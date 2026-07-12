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
    jmpq *(%rax,%rcx,8)
    br x16
.Ldone:
    ret
    .cfi_endproc
"""
        functions = CODEGEN.parse_assembly(assembly)
        calls = functions["gint_perf_equal256"]["calls"]
        self.assertEqual(
            [call["instruction"].split()[0] for call in calls],
            ["callq", "bl", "jmpq", "b"],
        )

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
