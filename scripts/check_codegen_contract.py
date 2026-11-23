#!/usr/bin/env python3

"""Validate stable structural properties of compiler-generated hot paths."""

from __future__ import print_function

import argparse
import json
import os
import re
import sys


FUNCTION_LABEL = re.compile(r"^_?(gint_perf_[A-Za-z0-9_]+):(?:\s|$)")
ASSEMBLY_LABEL = re.compile(r"^([.$A-Za-z_0-9]+):(?:\s|$)")
CALL_MNEMONICS = frozenset(("bl", "blr", "call", "calll", "callq"))
INDIRECT_TAIL_BRANCH_MNEMONICS = frozenset(("br", "braa", "braaz", "brab", "brabz"))
ELF_LOCAL_LABEL = re.compile(r"^(?:\.L.*|[0-9]+[fb]?)$")
LOCAL_LABEL = re.compile(r"^(?:\.L.*|LBB.*|Ltmp.*|Lloh.*|[0-9]+[fb]?)$")
MACHO_LOCAL_LABEL = re.compile(r"^L[A-Za-z0-9_$.]+$")
GOT_INDIRECT_SYMBOL = re.compile(
    r"^\*([.$A-Za-z_][.$A-Za-z_0-9]*)@GOT[A-Za-z0-9_]*\([^)]*\)$",
    re.IGNORECASE,
)
CONDITIONAL_BRANCH_MNEMONICS = frozenset(("cbz", "cbnz", "tbz", "tbnz"))
TERMINAL_MNEMONICS = frozenset(
    ("br", "braa", "braaz", "brab", "brabz", "brk", "eret", "hlt", "ret", "retq", "retn", "trap", "ud2")
)


def is_macho_target(target):
    """Return whether a target triple uses Mach-O assembly conventions."""
    if not target:
        return False
    lowered = target.lower()
    return "darwin" in lowered or "apple" in lowered


def is_local_label(label, is_macho=False, target=None):
    """Return whether a label is function-local rather than a new symbol."""
    if is_macho:
        return (
            ELF_LOCAL_LABEL.match(label) is not None
            or MACHO_LOCAL_LABEL.match(label) is not None
        )
    if target is not None:
        return ELF_LOCAL_LABEL.match(label) is not None
    return LOCAL_LABEL.match(label) is not None


def transfer_operand(mnemonic, instruction):
    """Return the target operand of a transfer."""
    operands = instruction.split(None, 1)
    if len(operands) == 1:
        return ""
    operand_text = operands[1]
    if mnemonic in CONDITIONAL_BRANCH_MNEMONICS:
        operand_text = operand_text.rsplit(",", 1)[-1].strip()
    return operand_text.split(None, 1)[0].rstrip(",") if operand_text else ""


def extract_transfer_target(instruction):
    """Return the callee symbol for a direct or GOT-indirect transfer."""
    operand = transfer_operand(instruction.split(None, 1)[0].lower(), instruction)
    if operand.startswith("*"):
        got_match = GOT_INDIRECT_SYMBOL.match(operand)
        return got_match.group(1) if got_match else ""
    return operand.split("@", 1)[0]


def is_direct_branch_mnemonic(mnemonic):
    """Return whether the mnemonic can branch to a label."""
    return (
        mnemonic.startswith(("j", "loop", "b."))
        or mnemonic == "b"
        or mnemonic in CONDITIONAL_BRANCH_MNEMONICS
    )


def is_unconditional_transfer(mnemonic):
    """Return whether execution cannot fall through to the next instruction."""
    return (
        mnemonic in TERMINAL_MNEMONICS
        or mnemonic in ("b", "b.al", "b.n", "b.w")
        or mnemonic.startswith("jmp")
    )


def resolve_branch_target(labels, branch):
    """Resolve a named or directional numeric local label to an instruction."""
    target = branch["target"]
    source = branch["source_index"]
    if len(target) > 1 and target[-1] in "bf" and target[:-1].isdigit():
        positions = labels.get(target[:-1], [])
        if target[-1] == "b":
            candidates = [position for position in positions if position <= source]
            return candidates[-1] if candidates else None
        candidates = [position for position in positions if position > source]
        return candidates[0] if candidates else None
    positions = labels.get(target, [])
    return positions[0] if positions else None


def build_control_flow_graph(function):
    """Build an instruction-level CFG from direct branches and fallthroughs."""
    instruction_count = len(function["instructions"])
    graph = [set() for _ in range(instruction_count)]
    branch_by_source = {}
    fragment_entries = function["fragment_entries"]

    for branch in function["branches"]:
        target = resolve_branch_target(function["labels"], branch)
        if target is not None and target < instruction_count:
            graph[branch["source_index"]].add(target)
            branch_by_source.setdefault(branch["source_index"], []).append(
                (target, branch)
            )

    for index, instruction in enumerate(function["instructions"]):
        if index + 1 >= instruction_count or index + 1 in fragment_entries:
            continue
        if not is_unconditional_transfer(instruction["mnemonic"]):
            graph[index].add(index + 1)

    return graph, branch_by_source


def reachable_nodes(graph):
    """Return instruction nodes reachable from the function entry."""
    if not graph:
        return set()
    pending = [0]
    reachable = set()
    while pending:
        node = pending.pop()
        if node in reachable:
            continue
        reachable.add(node)
        pending.extend(graph[node] - reachable)
    return reachable


def cyclic_components(graph):
    """Return reachable strongly connected components that contain a cycle."""
    reachable = reachable_nodes(graph)
    indices = {}
    lowlinks = {}
    stack = []
    on_stack = set()
    next_index = [0]
    components = []

    def visit(node):
        indices[node] = next_index[0]
        lowlinks[node] = next_index[0]
        next_index[0] += 1
        stack.append(node)
        on_stack.add(node)

        for successor in graph[node]:
            if successor not in reachable:
                continue
            if successor not in indices:
                visit(successor)
                lowlinks[node] = min(lowlinks[node], lowlinks[successor])
            elif successor in on_stack:
                lowlinks[node] = min(lowlinks[node], indices[successor])

        if lowlinks[node] != indices[node]:
            return
        component = set()
        while True:
            member = stack.pop()
            on_stack.remove(member)
            component.add(member)
            if member == node:
                break
        if len(component) > 1 or node in graph[node]:
            components.append(component)

    for node in sorted(reachable):
        if node not in indices:
            visit(node)
    return components


def find_cycle_branches(function):
    """Return direct branches that participate in reachable CFG cycles."""
    graph, branch_by_source = build_control_flow_graph(function)
    components = cyclic_components(graph)
    cycle_branches = []
    for component in components:
        for source in sorted(component):
            for target, branch in branch_by_source.get(source, []):
                if target in component:
                    cycle_branches.append(
                        {
                            "line": branch["line"],
                            "instruction": branch["instruction"],
                            "target": branch["target"],
                        }
                    )
    return cycle_branches


def parse_assembly(text, target=None):
    """Return instruction mnemonics and call sites for probes and their callees."""
    is_macho = is_macho_target(target)
    functions = {}
    active = None

    for line_number, raw_line in enumerate(text.splitlines(), 1):
        line = raw_line.strip()
        label_match = ASSEMBLY_LABEL.match(line)
        if label_match and not is_local_label(
            label_match.group(1), is_macho=is_macho, target=target
        ):
            symbol = label_match.group(1)
            # GCC emits a second symbol and CFI range when partitioning a function.
            if symbol.endswith(".cold") and symbol[:-5] in functions:
                active = symbol[:-5]
                function = functions[active]
                entry = len(function["instructions"])
                function["labels"][symbol] = [entry]
                function["fragment_entries"].add(entry)
                continue
            match = FUNCTION_LABEL.match(line)
            active = match.group(1) if match else symbol
            functions.setdefault(
                active,
                {
                    "mnemonics": [],
                    "calls": [],
                    "indirect_transfers": [],
                    "labels": {symbol: [0]},
                    "fragment_entries": set(),
                    "branches": [],
                    "instructions": [],
                },
            )
            continue

        if active is None:
            continue
        if line.startswith(".cfi_endproc"):
            active = None
            continue
        if label_match:
            functions[active]["labels"].setdefault(label_match.group(1), []).append(
                len(functions[active]["instructions"])
            )
            continue
        if not line or line[0] in ".#;" or line.startswith("//"):
            continue

        mnemonic = line.split(None, 1)[0].lower()
        function = functions[active]
        function["mnemonics"].append(mnemonic)
        source_index = len(function["instructions"])
        function["instructions"].append(
            {
                "line": line_number,
                "instruction": line,
                "mnemonic": mnemonic,
            }
        )
        if mnemonic in CALL_MNEMONICS:
            function["calls"].append({"line": line_number, "instruction": line})
        elif mnemonic in INDIRECT_TAIL_BRANCH_MNEMONICS:
            function["indirect_transfers"].append({"line": line_number, "instruction": line})
        elif is_direct_branch_mnemonic(mnemonic):
            operand = transfer_operand(mnemonic, line)
            if operand.startswith("*"):
                transfers = "calls" if GOT_INDIRECT_SYMBOL.match(operand) else "indirect_transfers"
                function[transfers].append({"line": line_number, "instruction": line})
                continue
            function["branches"].append(
                {
                    "line": line_number,
                    "instruction": line,
                    "target": operand,
                    "source_index": source_index,
                }
            )

    for name, function in functions.items():
        local_branches = []
        for branch in function["branches"]:
            if resolve_branch_target(function["labels"], branch) is None:
                function["calls"].append(
                    {"line": branch["line"], "instruction": branch["instruction"]}
                )
            else:
                local_branches.append(branch)
        function["branches"] = local_branches
        function["calls"].sort(key=lambda call: call["line"])
        if name.startswith("gint_perf_"):
            # Keep the report/contract field name for compatibility.
            function["back_edges"] = find_cycle_branches(function)
    return functions


def normalize_architecture(target):
    architecture = target.split("-", 1)[0].lower()
    if architecture in ("aarch64", "arm64"):
        return "aarch64"
    if architecture in ("amd64", "x86_64"):
        return "x86_64"
    raise ValueError("unsupported code-generation target: {0}".format(target))


def inspect_call_tree(name, functions, external_budget, violations, is_macho=False):
    """Inspect each emitted body once and check every external call site."""
    bodies = {}
    external_calls = []
    counts = {}
    pending = [name]
    while pending:
        symbol = pending.pop()
        if symbol in bodies:
            continue

        function = functions[symbol]
        bodies[symbol] = len(function["mnemonics"])
        indirect_transfers = function.get("indirect_transfers", [])
        if not function["calls"] and not indirect_transfers:
            continue
        graph, _ = build_control_flow_graph(function)
        cycle_lines = {
            function["instructions"][index]["line"]
            for component in cyclic_components(graph)
            for index in component
        }
        for call in function["calls"]:
            instruction = call["instruction"]
            line = call["line"]
            target = extract_transfer_target(instruction)
            if target.startswith("_gint_perf_"):
                target = target[1:]
            location = "{0}: {1} at line {2}".format(name, symbol, line)
            if line in cycle_lines:
                violations.append("{0}: call inside a loop: {1}".format(location, instruction))
            if target in functions and functions[target]["mnemonics"]:
                pending.append(target)
                continue

            # Mach-O adds one leading underscore to C symbols.
            if is_macho and target.startswith("_"):
                target = target[1:]
            external_calls.append({"function": symbol, "line": line, "instruction": instruction})
            counts[target] = counts.get(target, 0) + 1
            if counts[target] > external_budget.get(target, 0):
                violations.append("{0}: unexpected external call: {1}".format(location, instruction))
        for transfer in indirect_transfers:
            instruction = transfer["instruction"]
            line = transfer["line"]
            location = "{0}: {1} at line {2}".format(name, symbol, line)
            if line in cycle_lines:
                violations.append("{0}: call inside a loop: {1}".format(location, instruction))
            external_calls.append({"function": symbol, "line": line, "instruction": instruction})
            violations.append("{0}: unresolved indirect transfer: {1}".format(location, instruction))
    return bodies, external_calls


def evaluate_contract(functions, contract, architecture, target=None):
    """Build a machine-readable report and return all violations."""
    is_macho = is_macho_target(target)
    results = {}
    violations = []

    for name, requirements in contract["functions"].items():
        maximum = requirements["max_instructions"][architecture]
        parsed = functions.get(name)
        if parsed is None:
            violations.append("missing function: {0}".format(name))
            results[name] = {
                "present": False,
                "instruction_count": 0,
                "max_instructions": maximum,
                "calls": [],
                "back_edges": [],
            }
            continue

        count = len(parsed["mnemonics"])
        calls = parsed["calls"]
        back_edges = parsed.get("back_edges", [])
        results[name] = {
            "present": True,
            "instruction_count": count,
            "max_instructions": maximum,
            "calls": calls,
            "back_edges": back_edges,
        }

        if count == 0:
            violations.append("{0}: no instructions parsed".format(name))
        if count > maximum:
            violations.append(
                "{0}: {1} instructions exceeds budget {2}".format(
                    name, count, maximum
                )
            )
        if requirements.get("forbid_calls", False) and calls:
            violations.append(
                "{0}: contains {1} out-of-line call/transfer(s)".format(
                    name, len(calls)
                )
            )
        if "max_calls" in requirements and len(calls) > requirements["max_calls"]:
            violations.append(
                "{0}: {1} out-of-line calls exceeds budget {2}".format(
                    name, len(calls), requirements["max_calls"]
                )
            )
        # Call tree inspection already rejects unresolved transfers when enabled.
        if "max_total_instructions" not in requirements and (
            requirements.get("forbid_calls", False) or "max_calls" in requirements
        ):
            for transfer in parsed.get("indirect_transfers", []):
                violations.append(
                    "{0} at line {1}: unresolved indirect transfer: {2}".format(
                        name, transfer["line"], transfer["instruction"]
                    )
                )
        if "max_back_edges" in requirements and len(back_edges) > requirements["max_back_edges"]:
            violations.append(
                "{0}: {1} back edge(s) exceeds budget {2}".format(
                    name, len(back_edges), requirements["max_back_edges"]
                )
            )
        allowed_calls = requirements.get("allowed_call_substrings", [])
        for call in calls:
            if allowed_calls and not any(
                allowed in call["instruction"] for allowed in allowed_calls
            ):
                violations.append(
                    "{0}: unexpected call at line {1}: {2}".format(
                        name, call["line"], call["instruction"]
                    )
                )
        if "max_total_instructions" in requirements:
            total_maximum = requirements["max_total_instructions"][architecture]
            external_budget = requirements["external_call_budget"][architecture]
            bodies, external_calls = inspect_call_tree(
                name, functions, external_budget, violations, is_macho=is_macho
            )
            total_count = sum(bodies.values())
            results[name].update(
                total_instruction_count=total_count,
                max_total_instructions=total_maximum,
                callee_instruction_counts={symbol: size for symbol, size in bodies.items() if symbol != name},
                external_calls=external_calls,
            )
            if total_count > total_maximum:
                violations.append(
                    "{0}: {1} total instructions exceeds budget {2}".format(name, total_count, total_maximum)
                )

    return results, violations


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--assembly", required=True, help="compiler-generated assembly")
    parser.add_argument("--contract", required=True, help="JSON contract")
    parser.add_argument("--compiler", required=True, help="compiler version string")
    parser.add_argument("--target", required=True, help="compiler target triple")
    parser.add_argument("--output", required=True, help="machine-readable JSON report")
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    with open(args.assembly, "r") as assembly_file:
        functions = parse_assembly(assembly_file.read(), target=args.target)
    with open(args.contract, "r") as contract_file:
        contract = json.load(contract_file)

    architecture = normalize_architecture(args.target)
    results, violations = evaluate_contract(
        functions, contract, architecture, target=args.target
    )
    report = {
        "schema_version": 1,
        "status": "fail" if violations else "pass",
        "language_standard": contract["language_standard"],
        "architecture": architecture,
        "compiler": args.compiler,
        "target": args.target,
        "assembly": os.path.basename(args.assembly),
        "functions": results,
        "violations": violations,
    }

    output_dir = os.path.dirname(args.output)
    if output_dir and not os.path.isdir(output_dir):
        os.makedirs(output_dir)
    with open(args.output, "w") as output_file:
        json.dump(report, output_file, indent=2, sort_keys=True)
        output_file.write("\n")

    print("compiler: {0}".format(args.compiler))
    print("target: {0}".format(args.target))
    for name in sorted(results):
        result = results[name]
        summary = "{0}: {1}/{2} instructions, calls={3}, back_edges={4}".format(
            name,
            result["instruction_count"],
            result["max_instructions"],
            len(result["calls"]),
            len(result["back_edges"]),
        )
        if "total_instruction_count" in result:
            summary += ", {0}/{1} total instructions, external calls={2}".format(
                result["total_instruction_count"], result["max_total_instructions"], len(result["external_calls"])
            )
        print(summary)
    if violations:
        for violation in violations:
            print("error: {0}".format(violation), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
