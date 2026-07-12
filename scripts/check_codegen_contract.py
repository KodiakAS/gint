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
TAIL_TRANSFER_MNEMONICS = frozenset(("b", "jmp", "jmpl", "jmpq"))
LOCAL_LABEL = re.compile(r"^(?:\.L.*|LBB.*|Ltmp.*|Lloh.*|[0-9]+[fb]?)$")
CONDITIONAL_BRANCH_MNEMONICS = frozenset(("cbz", "cbnz", "tbz", "tbnz"))
TERMINAL_MNEMONICS = frozenset(
    ("br", "brk", "eret", "hlt", "ret", "retq", "retn", "trap", "ud2")
)


def is_out_of_line_transfer(mnemonic, instruction):
    """Return whether an instruction can leave the current function."""
    if mnemonic in CALL_MNEMONICS:
        return True
    if mnemonic not in TAIL_TRANSFER_MNEMONICS:
        return False
    operands = instruction.split(None, 1)
    if len(operands) == 1:
        return True
    target = operands[1].split(None, 1)[0].rstrip(",")
    if target.startswith("*"):
        return False
    return LOCAL_LABEL.match(target) is None


def local_branch_target(mnemonic, instruction):
    """Return the direct local-label operand for a branch, if any."""
    operands = instruction.split(None, 1)
    if len(operands) == 1:
        return None
    operand_text = operands[1].strip()
    if (
        mnemonic.startswith("j")
        or mnemonic.startswith("loop")
        or mnemonic == "b"
        or mnemonic.startswith("b.")
    ):
        target = operand_text.split(None, 1)[0].rstrip(",")
    elif mnemonic in CONDITIONAL_BRANCH_MNEMONICS:
        target = operand_text.rsplit(",", 1)[-1].strip().split(None, 1)[0]
    else:
        return None
    return target if LOCAL_LABEL.match(target) else None


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

    for branch in function["branches"]:
        target = resolve_branch_target(function["labels"], branch)
        if target is not None and target < instruction_count:
            graph[branch["source_index"]].add(target)
            branch_by_source.setdefault(branch["source_index"], []).append(
                (target, branch)
            )

    for index, instruction in enumerate(function["instructions"]):
        if index + 1 >= instruction_count:
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


def parse_assembly(text):
    """Return instruction mnemonics and call sites for contract functions."""
    functions = {}
    active = None

    for line_number, raw_line in enumerate(text.splitlines(), 1):
        line = raw_line.strip()
        match = FUNCTION_LABEL.match(line)
        if match:
            active = match.group(1)
            functions.setdefault(
                active,
                {
                    "mnemonics": [],
                    "calls": [],
                    "labels": {},
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
        label_match = ASSEMBLY_LABEL.match(line)
        if label_match:
            functions[active]["labels"].setdefault(label_match.group(1), []).append(
                len(functions[active]["instructions"])
            )
            continue
        if not line or line[0] in ".#;" or line.startswith("//"):
            continue

        mnemonic = line.split(None, 1)[0].lower()
        functions[active]["mnemonics"].append(mnemonic)
        source_index = len(functions[active]["instructions"])
        functions[active]["instructions"].append(
            {
                "line": line_number,
                "instruction": line,
                "mnemonic": mnemonic,
            }
        )
        target = local_branch_target(mnemonic, line)
        if target is not None:
            functions[active]["branches"].append(
                {
                    "line": line_number,
                    "instruction": line,
                    "target": target,
                    "source_index": source_index,
                }
            )
        if is_out_of_line_transfer(mnemonic, line):
            functions[active]["calls"].append(
                {"line": line_number, "instruction": line}
            )

    for function in functions.values():
        # Keep the report/contract field name for compatibility. These are now
        # cycle-forming branches, not branches selected by textual direction.
        function["back_edges"] = find_cycle_branches(function)
    return functions


def normalize_architecture(target):
    architecture = target.split("-", 1)[0].lower()
    if architecture in ("aarch64", "arm64"):
        return "aarch64"
    if architecture in ("amd64", "x86_64"):
        return "x86_64"
    raise ValueError("unsupported code-generation target: {0}".format(target))


def evaluate_contract(functions, contract, architecture):
    """Build a machine-readable report and return all violations."""
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
        functions = parse_assembly(assembly_file.read())
    with open(args.contract, "r") as contract_file:
        contract = json.load(contract_file)

    architecture = normalize_architecture(args.target)
    results, violations = evaluate_contract(functions, contract, architecture)
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
        print(
            "{0}: {1}/{2} instructions, calls={3}, back_edges={4}".format(
                name,
                result["instruction_count"],
                result["max_instructions"],
                len(result["calls"]),
                len(result["back_edges"]),
            )
        )
    if violations:
        for violation in violations:
            print("error: {0}".format(violation), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
