#!/usr/bin/env python3

from __future__ import print_function

import importlib.util
import os
import unittest


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MODULE_PATH = os.path.join(ROOT, "scripts", "check_codegen_contract.py")
SPEC = importlib.util.spec_from_file_location("check_codegen_contract", MODULE_PATH)
CODEGEN = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CODEGEN)


class ParseAssemblyTest(unittest.TestCase):
    def test_parses_gnu_and_apple_function_labels(self):
        assembly = """
gint_perf_add256:
.LFB0:
    .cfi_startproc
    addq %rax, %rbx
    adcq %rcx, %rdx
    ret
    .cfi_endproc
_gint_perf_sub256: ; @gint_perf_sub256
    subs x0, x0, x1
    sbcs x2, x2, x3
    ret
    .cfi_endproc
"""
        functions = CODEGEN.parse_assembly(assembly)
        self.assertEqual(
            functions["gint_perf_add256"]["mnemonics"],
            ["addq", "adcq", "ret"],
        )
        self.assertEqual(
            functions["gint_perf_sub256"]["mnemonics"],
            ["subs", "sbcs", "ret"],
        )

    def test_records_calls_and_tail_transfers_but_not_local_branches(self):
        assembly = """
gint_perf_equal256:
    bls .Lfast
    callq helper
.Lfast:
    bl runtime_helper
    jmp .Ldone
LBB0_1:
    b LBB0_2
    jmpq helper_tail
    b _other_tail
    jmpq *__udivti3@GOTPCREL(%rip)
    jmpq *(%rax,%rcx,8)
    br x16
LBB0_2:
.Ldone:
    ret
    .cfi_endproc
"""
        functions = CODEGEN.parse_assembly(assembly)
        calls = functions["gint_perf_equal256"]["calls"]
        self.assertEqual(
            [call["instruction"].split()[0] for call in calls],
            ["callq", "bl", "jmpq", "b", "jmpq"],
        )

    def test_preserves_probe_instructions_across_macho_debug_labels(self):
        assembly = """
_gint_perf_add256:
Lfunc_begin0:
    .cfi_startproc
    ret
Lfunc_end0:
    .cfi_endproc
"""
        functions = CODEGEN.parse_assembly(assembly, target="arm64-apple-darwin")
        self.assertEqual(functions["gint_perf_add256"]["mnemonics"], ["ret"])
        self.assertNotIn("Lfunc_begin0", functions)
        self.assertNotIn("Lfunc_end0", functions)

    def test_counts_calls_in_gcc_cold_partition_of_the_same_function(self):
        assembly = """
gint_perf_mul256:
    je .Lcold
.Ldone:
    ret
    .cfi_endproc
    .section .text.unlikely
gint_perf_mul256.cold:
.Lcold:
    callq mul_limbs4_u64
    jmp .Ldone
    .cfi_endproc
"""
        functions = CODEGEN.parse_assembly(assembly, target="x86_64-linux-gnu")
        function = functions["gint_perf_mul256"]
        self.assertEqual(function["mnemonics"], ["je", "ret", "callq", "jmp"])
        self.assertEqual([call["instruction"] for call in function["calls"]], ["callq mul_limbs4_u64"])
        self.assertEqual(function["back_edges"], [])

    def test_does_not_treat_backward_common_tail_branches_as_loops(self):
        assembly = """
gint_perf_add256:
    testq %rax, %rax
    jne .Lright
.Lleft:
    addq %rax, %rbx
.Ljoin:
    ret
.Lright:
    subq %rcx, %rdx
    jmp .Ljoin
    .cfi_endproc
_gint_perf_sub256:
    cbnz x0, LBB1_2
LBB1_1:
    add x1, x1, x2
LBB1_3:
    ret
LBB1_2:
    sub x1, x1, x2
    b LBB1_3
    .cfi_endproc
"""
        functions = CODEGEN.parse_assembly(assembly)
        self.assertEqual(functions["gint_perf_add256"]["back_edges"], [])
        self.assertEqual(functions["gint_perf_sub256"]["back_edges"], [])

    def test_records_reachable_self_and_multi_node_cycles(self):
        assembly = """
gint_perf_add256:
.Lloop:
    jmp .Lloop
    .cfi_endproc
_gint_perf_sub256:
LBB1_1:
    subs x0, x0, x1
    tbz x0, #0, LBB1_3
LBB1_2:
    add x1, x1, x2
    b LBB1_1
LBB1_3:
    ret
    .cfi_endproc
"""
        functions = CODEGEN.parse_assembly(assembly)
        self.assertEqual(len(functions["gint_perf_add256"]["back_edges"]), 1)
        self.assertEqual(len(functions["gint_perf_sub256"]["back_edges"]), 1)

    def test_ignores_unreachable_and_indirect_cycles(self):
        assembly = """
gint_perf_add256:
    ret
.Ldead:
    jne .Ldead
    .cfi_endproc
_gint_perf_sub256:
    br x16
LBB1_1:
    b LBB1_1
    .cfi_endproc
"""
        functions = CODEGEN.parse_assembly(assembly)
        self.assertEqual(functions["gint_perf_add256"]["back_edges"], [])
        self.assertEqual(functions["gint_perf_sub256"]["back_edges"], [])

    def test_resolves_repeated_numeric_labels_by_direction_and_proximity(self):
        assembly = """
gint_perf_add256:
    jmp 1f
1:
    addq %rax, %rbx
    jne 2f
    ret
2:
    subq %rcx, %rdx
    jmp 1b
1:
    ret
    .cfi_endproc
"""
        functions = CODEGEN.parse_assembly(assembly)
        cycle_targets = {
            branch["target"]
            for branch in functions["gint_perf_add256"]["back_edges"]
        }
        self.assertEqual(cycle_targets, {"1b", "2f"})


class EvaluateContractTest(unittest.TestCase):
    def check_outlined_division(self, assembly, architecture="x86_64", target=None):
        contract = {
            "functions": {
                "gint_perf_div_u32": {
                    "max_instructions": {"x86_64": 3, "aarch64": 3},
                    "max_total_instructions": {"x86_64": 6, "aarch64": 6},
                    "external_call_budget": {"x86_64": {}, "aarch64": {"__udivti3": 1}},
                }
            }
        }
        return CODEGEN.evaluate_contract(
            CODEGEN.parse_assembly(assembly, target=target),
            contract,
            architecture,
            target=target,
        )

    def test_checks_transitive_calls_and_counts_callee_instructions(self):
        assembly = """
gint_perf_div_u32:
    callq _div_mod_small
    ret
    .cfi_endproc
_div_mod_small:
    jmp _kernel.constprop.0
    .cfi_endproc
_kernel.constprop.0:
    callq __udivmodti4@PLT
    ret
    .cfi_endproc
"""
        results, violations = self.check_outlined_division(assembly)
        result = results["gint_perf_div_u32"]
        self.assertEqual(result["total_instruction_count"], 5)
        self.assertEqual(result["callee_instruction_counts"], {"_div_mod_small": 1, "_kernel.constprop.0": 2})
        self.assertEqual(len(result["external_calls"]), 1)
        self.assertEqual(len(violations), 1)
        self.assertIn("unexpected external call: callq __udivmodti4@PLT", violations[0])

    def test_rejects_got_indirect_tail_call(self):
        assembly = """
gint_perf_div_u32:
    jmpq *__udivti3@GOTPCREL(%rip)
    .cfi_endproc
"""
        results, violations = self.check_outlined_division(
            assembly, "x86_64", target="x86_64-pc-linux-gnu"
        )
        self.assertEqual(len(results["gint_perf_div_u32"]["external_calls"]), 1)
        self.assertEqual(len(violations), 1)
        self.assertIn("unexpected external call: jmpq *__udivti3@GOTPCREL(%rip)", violations[0])

    def test_rejects_elf_tail_call_matching_macho_debug_label_prefix(self):
        assembly = """
gint_perf_div_u32:
    jmp Lfunc_begin_helper@PLT
    .cfi_endproc
"""
        results, violations = self.check_outlined_division(
            assembly, "x86_64", target="x86_64-pc-linux-gnu"
        )
        self.assertEqual(len(results["gint_perf_div_u32"]["external_calls"]), 1)
        self.assertEqual(len(violations), 1)
        self.assertIn("unexpected external call: jmp Lfunc_begin_helper@PLT", violations[0])

    def test_rejects_elf_plt_tail_call_matching_macho_block_label_prefix(self):
        assembly = """
gint_perf_div_u32:
    jmp LBBhelper@PLT
    .cfi_endproc
"""
        results, violations = self.check_outlined_division(
            assembly, "x86_64", target="x86_64-linux-gnu"
        )
        self.assertEqual(len(results["gint_perf_div_u32"]["external_calls"]), 1)
        self.assertEqual(len(violations), 1)
        self.assertIn("unexpected external call: jmp LBBhelper@PLT", violations[0])

    def test_rejects_unresolved_indirect_tail_transfers_in_call_tree(self):
        x86_assembly = """
gint_perf_div_u32:
    movq __udivti3@GOTPCREL(%rip), %rax
    jmp *%rax
    .cfi_endproc
"""
        results, violations = self.check_outlined_division(
            x86_assembly, "x86_64", target="x86_64-linux-gnu"
        )
        self.assertEqual(len(results["gint_perf_div_u32"]["external_calls"]), 1)
        self.assertEqual(len(violations), 1)
        self.assertIn("unresolved indirect transfer: jmp *%rax", violations[0])

        aarch64_assembly = """
gint_perf_div_u32:
    adrp x16, :got:__umodti3
    ldr x16, [x16, :got_lo12:__umodti3]
    br x16
    .cfi_endproc
"""
        results, violations = self.check_outlined_division(
            aarch64_assembly, "aarch64", target="aarch64-linux-gnu"
        )
        self.assertEqual(len(results["gint_perf_div_u32"]["external_calls"]), 1)
        self.assertEqual(len(violations), 1)
        self.assertIn("unresolved indirect transfer: br x16", violations[0])

    def test_rejects_unresolved_indirect_transfers_under_call_limits(self):
        for architecture, transfer in (("x86_64", "jmp *%rax"), ("aarch64", "br x9")):
            target = architecture + "-linux-gnu"
            assembly = "gint_perf_div_u32:\n    " + transfer
            functions = CODEGEN.parse_assembly(assembly, target=target)
            for call_limit in ({"forbid_calls": True}, {"max_calls": 1}):
                with self.subTest(transfer=transfer, call_limit=call_limit):
                    requirements = dict(call_limit, max_instructions={architecture: 3})
                    contract = {"functions": {"gint_perf_div_u32": requirements}}
                    _, violations = CODEGEN.evaluate_contract(
                        functions, contract, architecture, target=target
                    )
                    self.assertEqual(len(violations), 1)
                    self.assertIn("unresolved indirect transfer: " + transfer, violations[0])

    def test_checks_external_conditional_transfers_under_call_limits(self):
        cases = (
            ("x86_64-linux-gnu", "jne .Ldone", "jne helper"),
            ("aarch64-linux-gnu", "b.ne .Ldone", "b.ne helper"),
            ("arm64-apple-darwin", "cbnz x0, LBB0_1", "cbnz x0, _helper"),
            ("aarch64-linux-gnu", "tbz x0, #0, 1f", "tbz x0, #0, .Loutside"),
        )
        for target, local_branch, external_branch in cases:
            with self.subTest(target=target, external_branch=external_branch):
                assembly = """
gint_perf_div_u32:
    {local}
    {external}
1:
.Ldone:
{macho_label}    ret
    .cfi_endproc
other_function:
.Loutside:
    ret
""".format(
                    local=local_branch,
                    external=external_branch,
                    macho_label="LBB0_1:\n" if "apple" in target else "",
                )
                architecture = CODEGEN.normalize_architecture(target)
                functions = CODEGEN.parse_assembly(assembly, target=target)
                calls = functions["gint_perf_div_u32"]["calls"]
                self.assertEqual([call["instruction"] for call in calls], [external_branch])
                for call_limit in ({"forbid_calls": True}, {"max_calls": 0}):
                    requirements = dict(call_limit, max_instructions={architecture: 3})
                    contract = {"functions": {"gint_perf_div_u32": requirements}}
                    _, violations = CODEGEN.evaluate_contract(
                        functions, contract, architecture, target=target
                    )
                    self.assertEqual(len(violations), 1)
                    self.assertIn("out-of-line", violations[0])
                _, violations = self.check_outlined_division(assembly, architecture, target=target)
                self.assertEqual(len(violations), 1)
                self.assertIn("unexpected external call: " + external_branch, violations[0])

    def test_rejects_a_missing_outlined_body(self):
        _, violations = self.check_outlined_division("gint_perf_div_u32:\n    jmp _div_mod_small")
        self.assertEqual(len(violations), 1)
        self.assertIn("unexpected external call: jmp _div_mod_small", violations[0])

    def test_allows_one_aarch64_reciprocal_setup_outside_loops(self):
        assembly = """
_gint_perf_div_u32:
    bl _div_mod_small
    ret
    .cfi_endproc
_div_mod_small:
    bl ___udivti3
LBB1_1:
    subs x0, x0, x1
    b.ne LBB1_1
    ret
    .cfi_endproc
"""
        _, violations = self.check_outlined_division(
            assembly, "aarch64", target="arm64-apple-darwin"
        )
        self.assertEqual(violations, [])

        _, elf_violations = self.check_outlined_division(
            assembly.replace("LBB1_1", ".LBB1_1"), "aarch64", target="aarch64-unknown-linux-gnu"
        )
        self.assertEqual(len(elf_violations), 1)
        self.assertIn("unexpected external call: bl ___udivti3", elf_violations[0])

        inside_loop = assembly.replace("    bl ___udivti3\nLBB1_1:", "LBB1_1:\n    bl ___udivti3")
        _, violations = self.check_outlined_division(
            inside_loop, "aarch64", target="arm64-apple-darwin"
        )
        self.assertEqual(len(violations), 1)
        self.assertIn("call inside a loop", violations[0])

        duplicate_call = assembly.replace("    bl ___udivti3", "    bl ___udivti3\n    bl ___udivti3")
        _, violations = self.check_outlined_division(
            duplicate_call, "aarch64", target="arm64-apple-darwin"
        )
        self.assertEqual(len(violations), 2)
        self.assertIn("unexpected external call", violations[0])
        self.assertIn("total instructions exceeds budget", violations[1])

    def test_reports_missing_calls_and_instruction_budget(self):
        functions = {
            "gint_perf_add256": {
                "mnemonics": ["add", "callq", "ret"],
                "calls": [{"line": 2, "instruction": "callq helper"}],
            }
        }
        contract = {
            "functions": {
                "gint_perf_add256": {
                    "max_instructions": 2,
                    "forbid_calls": True,
                },
                "gint_perf_sub256": {
                    "max_instructions": 4,
                    "forbid_calls": True,
                },
            }
        }
        for requirements in contract["functions"].values():
            requirements["max_instructions"] = {"x86_64": requirements["max_instructions"]}
        results, violations = CODEGEN.evaluate_contract(functions, contract, "x86_64")
        self.assertTrue(results["gint_perf_add256"]["present"])
        self.assertFalse(results["gint_perf_sub256"]["present"])
        self.assertEqual(len(violations), 3)

    def test_normalizes_supported_target_architectures(self):
        self.assertEqual(CODEGEN.normalize_architecture("arm64-apple-darwin"), "aarch64")
        self.assertEqual(CODEGEN.normalize_architecture("aarch64-linux-gnu"), "aarch64")
        self.assertEqual(CODEGEN.normalize_architecture("x86_64-linux-gnu"), "x86_64")
        with self.assertRaises(ValueError):
            CODEGEN.normalize_architecture("riscv64-linux-gnu")

    def test_allows_only_named_division_helper(self):
        functions = {
            "gint_perf_div_u32": {
                "mnemonics": ["callq", "ret"],
                "calls": [
                    {"line": 1, "instruction": "callq _integer_div_mod_small"}
                ],
            }
        }
        contract = {
            "functions": {
                "gint_perf_div_u32": {
                    "max_instructions": {"x86_64": 4},
                    "max_calls": 1,
                    "allowed_call_substrings": ["div_mod_small"],
                }
            }
        }
        _, violations = CODEGEN.evaluate_contract(functions, contract, "x86_64")
        self.assertEqual(violations, [])
        functions["gint_perf_div_u32"]["calls"][0]["instruction"] = "callq __udivti3"
        _, violations = CODEGEN.evaluate_contract(functions, contract, "x86_64")
        self.assertEqual(len(violations), 1)

    def test_rejects_back_edges_over_budget(self):
        functions = {
            "gint_perf_add256": {
                "mnemonics": ["add", "jne", "ret"],
                "calls": [],
                "back_edges": [
                    {"line": 2, "instruction": "jne .Lloop", "target": ".Lloop"}
                ],
            }
        }
        contract = {
            "functions": {
                "gint_perf_add256": {
                    "max_instructions": {"x86_64": 4},
                    "max_back_edges": 0,
                }
            }
        }
        _, violations = CODEGEN.evaluate_contract(functions, contract, "x86_64")
        self.assertEqual(len(violations), 1)


if __name__ == "__main__":
    unittest.main()
