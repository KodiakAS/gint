#!/usr/bin/env python3

from __future__ import print_function

import contextlib
import errno
import importlib.util
import io
import os
import shutil
import tempfile
import unittest
from unittest import mock


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MODULE_PATH = os.path.join(ROOT, "scripts", "generate-amalgamation.py")
SPEC = importlib.util.spec_from_file_location("generate_amalgamation", MODULE_PATH)
AMALGAMATION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AMALGAMATION)


class GenerateAmalgamationTest(unittest.TestCase):
    def setUp(self):
        self.root = tempfile.mkdtemp(prefix="gint-amalgamation-test-")
        os.makedirs(os.path.join(self.root, "src", "gint"))
        os.makedirs(os.path.join(self.root, "include", "gint"))

    def tearDown(self):
        shutil.rmtree(self.root)

    def write_bytes(self, relative_path, content):
        absolute_path = os.path.join(self.root, relative_path)
        parent = os.path.dirname(absolute_path)
        if not os.path.isdir(parent):
            os.makedirs(parent)
        with open(absolute_path, "wb") as output_file:
            output_file.write(content)

    def build_with_policy(
        self,
        manifest,
        role_dependencies,
        fragment_contracts=None,
    ):
        return AMALGAMATION.build_amalgamation(
            self.root,
            header_manifest=manifest,
            role_dependencies=role_dependencies,
            fragment_contracts=fragment_contracts,
        )

    def test_expands_normal_header_graph_and_deduplicates_dependencies(self):
        self.write_bytes(
            "src/gint/gint.hpp",
            b'#pragma once\n\n#include "left.hpp"\n#include "right.hpp"\n\nroot\n',
        )
        self.write_bytes(
            "src/gint/left.hpp",
            b'#pragma once\n\n#include "shared.hpp"\n\nleft\n',
        )
        self.write_bytes(
            "src/gint/right.hpp",
            b'#pragma once\n\n#include "shared.hpp"\n\nright\n',
        )
        self.write_bytes(
            "src/gint/shared.hpp",
            b"#pragma once\n\n#include <cstdint>\n\nshared\n",
        )

        content = AMALGAMATION.build_amalgamation(self.root)
        self.assertEqual(content, b"#include <cstdint>\n\nshared\n\nleft\n\nright\n\nroot\n")

        with contextlib.redirect_stdout(io.StringIO()):
            AMALGAMATION.write_output(self.root, AMALGAMATION.DEFAULT_OUTPUT, content)
            self.assertTrue(AMALGAMATION.check_output(self.root, AMALGAMATION.DEFAULT_OUTPUT, content))

    def test_module_policy_allows_same_role_dependencies_only_toward_lower_order(self):
        self.write_bytes(
            "src/gint/gint.hpp",
            b'#pragma once\n#include "lower.hpp"\nroot\n',
        )
        self.write_bytes("src/gint/lower.hpp", b"#pragma once\nlower\n")
        roles = {"core": frozenset(("core",))}
        valid_manifest = {
            "gint.hpp": {"kind": "module", "role": "core", "order": 20},
            "lower.hpp": {"kind": "module", "role": "core", "order": 10},
        }
        self.assertEqual(
            self.build_with_policy(valid_manifest, roles),
            b"lower\nroot\n",
        )

        invalid_manifest = dict(valid_manifest)
        invalid_manifest["gint.hpp"] = {
            "kind": "module",
            "role": "core",
            "order": 5,
        }
        with self.assertRaisesRegex(
            AMALGAMATION.AmalgamationError,
            "same-role dependency must target a lower order",
        ):
            self.build_with_policy(invalid_manifest, roles)

    def test_module_policy_checks_direct_edges_even_after_dependency_expansion(self):
        self.write_bytes(
            "src/gint/gint.hpp",
            b'#pragma once\n#include "left.hpp"\n#include "shared.hpp"\nroot\n',
        )
        self.write_bytes(
            "src/gint/left.hpp",
            b'#pragma once\n#include "shared.hpp"\nleft\n',
        )
        self.write_bytes("src/gint/shared.hpp", b"#pragma once\nshared\n")
        manifest = {
            "gint.hpp": {"kind": "module", "role": "root", "order": 0},
            "left.hpp": {"kind": "module", "role": "branch", "order": 0},
            "shared.hpp": {"kind": "module", "role": "leaf", "order": 0},
        }
        roles = {
            "root": frozenset(("branch",)),
            "branch": frozenset(("leaf",)),
            "leaf": frozenset(),
        }
        with self.assertRaisesRegex(
            AMALGAMATION.AmalgamationError,
            r"module role dependency is not allowed: gint\.hpp .* -> shared\.hpp",
        ):
            self.build_with_policy(manifest, roles)

    def test_module_manifest_must_cover_every_discovered_header(self):
        self.write_bytes("src/gint/gint.hpp", b"#pragma once\nroot\n")
        self.write_bytes("src/gint/orphan.hpp", b"#pragma once\norphan\n")
        manifest = {
            "gint.hpp": {"kind": "module", "role": "root", "order": 0},
        }
        roles = {"root": frozenset()}
        with self.assertRaisesRegex(
            AMALGAMATION.AmalgamationError,
            "unclassified header.*orphan.hpp",
        ):
            self.build_with_policy(manifest, roles)

    def test_module_manifest_must_not_reference_missing_headers(self):
        self.write_bytes("src/gint/gint.hpp", b"#pragma once\nroot\n")
        manifest = {
            "gint.hpp": {"kind": "module", "role": "root", "order": 0},
            "missing.hpp": {"kind": "module", "role": "root", "order": 10},
        }
        roles = {"root": frozenset(("root",))}
        with self.assertRaisesRegex(
            AMALGAMATION.AmalgamationError,
            "missing manifest header.*missing.hpp",
        ):
            self.build_with_policy(manifest, roles)

    def test_reentrant_fragment_is_expanded_at_every_direct_include_site(self):
        self.write_bytes(
            "src/gint/gint.hpp",
            b'#pragma once\n#include "pass.hpp"\n#include "pass.hpp"\nroot\n',
        )
        self.write_bytes(
            "src/gint/pass.hpp",
            AMALGAMATION.FRAGMENT_MARKER + b"\nfragment\n",
        )
        manifest = {
            "gint.hpp": {"kind": "module", "role": "root", "order": 0},
            "pass.hpp": {"kind": "fragment", "role": "pass", "order": 0},
        }
        roles = {
            "root": frozenset(("pass",)),
            "pass": frozenset(),
        }
        self.assertEqual(
            self.build_with_policy(manifest, roles),
            b"fragment\nfragment\nroot\n",
        )

    def test_fragment_contract_enforces_direct_parent_counts(self):
        self.write_bytes(
            "src/gint/gint.hpp",
            b'#pragma once\n#include "pass.hpp"\nroot\n',
        )
        self.write_bytes(
            "src/gint/pass.hpp",
            AMALGAMATION.FRAGMENT_MARKER + b"fragment\n",
        )
        manifest = {
            "gint.hpp": {"kind": "module", "role": "root", "order": 0},
            "pass.hpp": {"kind": "fragment", "role": "pass", "order": 0},
        }
        roles = {
            "root": frozenset(("pass",)),
            "pass": frozenset(),
        }
        contracts = {
            "pass.hpp": {
                "parents": {"gint.hpp": 1},
                "must_be_last": False,
            },
        }
        self.assertEqual(
            self.build_with_policy(manifest, roles, contracts),
            b"fragment\nroot\n",
        )

        self.write_bytes(
            "src/gint/gint.hpp",
            b'#pragma once\n#include "pass.hpp"\n#include "pass.hpp"\nroot\n',
        )
        with self.assertRaisesRegex(
            AMALGAMATION.AmalgamationError,
            "fragment direct-parent contract mismatch.*gint.hpp x1.*gint.hpp x2",
        ):
            self.build_with_policy(manifest, roles, contracts)

    def test_fragment_contract_can_require_last_effective_parent_statement(self):
        self.write_bytes(
            "src/gint/gint.hpp",
            b'\n#pragma once\n#include "pass.hpp"\n// trailing comment\n\n',
        )
        self.write_bytes(
            "src/gint/pass.hpp",
            AMALGAMATION.FRAGMENT_MARKER + b"fragment\n",
        )
        manifest = {
            "gint.hpp": {"kind": "module", "role": "root", "order": 0},
            "pass.hpp": {"kind": "fragment", "role": "pass", "order": 0},
        }
        roles = {
            "root": frozenset(("pass",)),
            "pass": frozenset(),
        }
        contracts = {
            "pass.hpp": {
                "parents": {"gint.hpp": 1},
                "must_be_last": True,
            },
        }
        self.assertEqual(
            self.build_with_policy(manifest, roles, contracts),
            b"fragment\n// trailing comment\n\n",
        )

        self.write_bytes(
            "src/gint/gint.hpp",
            b'#pragma once\n#include "pass.hpp"\nroot\n',
        )
        with self.assertRaisesRegex(
            AMALGAMATION.AmalgamationError,
            "fragment include must be the parent's last effective statement",
        ):
            self.build_with_policy(manifest, roles, contracts)

    def test_reentrant_fragment_requires_one_canonical_leading_marker_and_no_pragma(self):
        self.write_bytes(
            "src/gint/gint.hpp",
            b'#pragma once\n#include "pass.hpp"\nroot\n',
        )
        manifest = {
            "gint.hpp": {"kind": "module", "role": "root", "order": 0},
            "pass.hpp": {"kind": "fragment", "role": "pass", "order": 0},
        }
        roles = {
            "root": frozenset(("pass",)),
            "pass": frozenset(),
        }
        cases = (
            (b"fragment\n", "canonical marker"),
            (b"\n" + AMALGAMATION.FRAGMENT_MARKER + b"fragment\n", "canonical marker"),
            (
                AMALGAMATION.FRAGMENT_MARKER
                + AMALGAMATION.FRAGMENT_MARKER
                + b"fragment\n",
                "canonical marker",
            ),
            (b"// GINT_REENTRANT_DEFINITION_PASS \nfragment\n", "canonical marker"),
            (
                AMALGAMATION.FRAGMENT_MARKER + b"#pragma once\nfragment\n",
                "must not contain #pragma once",
            ),
        )
        for content, expected_error in cases:
            with self.subTest(content=content):
                self.write_bytes("src/gint/pass.hpp", content)
                with self.assertRaisesRegex(
                    AMALGAMATION.AmalgamationError,
                    expected_error,
                ):
                    self.build_with_policy(manifest, roles)

    def test_normal_module_rejects_reentrant_fragment_marker(self):
        self.write_bytes(
            "src/gint/gint.hpp",
            AMALGAMATION.FRAGMENT_MARKER + b"root\n",
        )
        manifest = {
            "gint.hpp": {"kind": "module", "role": "root", "order": 0},
        }
        roles = {"root": frozenset()}
        with self.assertRaisesRegex(
            AMALGAMATION.AmalgamationError,
            "normal internal header must not contain the reentrant fragment marker",
        ):
            self.build_with_policy(manifest, roles)

    def test_reentrant_fragment_cycles_are_rejected(self):
        self.write_bytes(
            "src/gint/gint.hpp",
            b'#pragma once\n#include "first.hpp"\nroot\n',
        )
        self.write_bytes(
            "src/gint/first.hpp",
            AMALGAMATION.FRAGMENT_MARKER + b'#include "second.hpp"\nfirst\n',
        )
        self.write_bytes(
            "src/gint/second.hpp",
            AMALGAMATION.FRAGMENT_MARKER + b'#include "first.hpp"\nsecond\n',
        )
        manifest = {
            "gint.hpp": {"kind": "module", "role": "root", "order": 0},
            "first.hpp": {"kind": "fragment", "role": "first", "order": 0},
            "second.hpp": {"kind": "fragment", "role": "second", "order": 0},
        }
        roles = {
            "root": frozenset(("first",)),
            "first": frozenset(("second",)),
            "second": frozenset(("first",)),
        }
        with self.assertRaisesRegex(AMALGAMATION.AmalgamationError, "include cycle"):
            self.build_with_policy(manifest, roles)

    def test_cli_always_enforces_the_project_module_policy(self):
        self.write_bytes("src/gint/gint.hpp", b"#pragma once\nroot\n")
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            result = AMALGAMATION.main(("--root", self.root, "--check"))
        self.assertEqual(result, 2)
        self.assertIn("module manifest", stderr.getvalue())

    def test_project_wrapper_accepts_the_repository_graph_and_strips_fragment_markers(self):
        content = AMALGAMATION.build_project_amalgamation(ROOT)
        self.assertNotIn(AMALGAMATION.FRAGMENT_MARKER, content)
        self.assertNotIn(b'#include "', content)
        self.assertEqual(content.count(b"define GINT_DETAIL_HEADER_PASS_ACTIVE"), 3)
        self.assertEqual(content.count(b"undef GINT_DETAIL_HEADER_PASS_ACTIVE"), 2)

    def test_project_wrapper_always_supplies_all_production_contracts(self):
        with mock.patch.object(
            AMALGAMATION,
            "build_amalgamation",
            return_value=b"generated\n",
        ) as build:
            self.assertEqual(
                AMALGAMATION.build_project_amalgamation(self.root),
                b"generated\n",
            )
        build.assert_called_once_with(
            self.root,
            AMALGAMATION.DEFAULT_INPUT,
            header_manifest=AMALGAMATION.PROJECT_HEADER_MANIFEST,
            role_dependencies=AMALGAMATION.PROJECT_ROLE_DEPENDENCIES,
            fragment_contracts=AMALGAMATION.PROJECT_FRAGMENT_CONTRACTS,
        )

    def test_check_detects_stale_output_without_rewriting_it(self):
        self.write_bytes("src/gint/gint.hpp", b"#pragma once\ncurrent\n")
        self.write_bytes(AMALGAMATION.DEFAULT_OUTPUT, b"stale\n")

        content = AMALGAMATION.build_amalgamation(self.root)
        with contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            self.assertFalse(AMALGAMATION.check_output(self.root, AMALGAMATION.DEFAULT_OUTPUT, content))
        with open(os.path.join(self.root, AMALGAMATION.DEFAULT_OUTPUT), "rb") as output_file:
            self.assertEqual(output_file.read(), b"stale\n")

    def test_check_treats_missing_empty_output_as_stale(self):
        stderr = io.StringIO()
        with contextlib.redirect_stderr(stderr):
            self.assertFalse(
                AMALGAMATION.check_output(self.root, AMALGAMATION.DEFAULT_OUTPUT, b"")
            )
        self.assertIn("actual output is missing", stderr.getvalue())

    def test_check_propagates_output_read_errors(self):
        self.write_bytes(AMALGAMATION.DEFAULT_OUTPUT, b"current\n")
        read_error = IOError(errno.EACCES, "permission denied")
        with mock.patch("builtins.open", side_effect=read_error):
            with self.assertRaisesRegex(IOError, "permission denied"):
                AMALGAMATION.check_output(
                    self.root, AMALGAMATION.DEFAULT_OUTPUT, b"current\n"
                )

    def test_generation_does_not_replace_an_already_current_header(self):
        content = b"current\n"
        self.write_bytes(AMALGAMATION.DEFAULT_OUTPUT, content)
        with mock.patch.object(AMALGAMATION.os, "replace") as replace, contextlib.redirect_stdout(io.StringIO()):
            AMALGAMATION.write_output(self.root, AMALGAMATION.DEFAULT_OUTPUT, content)
        replace.assert_not_called()

    def test_new_output_has_distribution_file_permissions(self):
        output = "include/gint/new-gint.h"
        with contextlib.redirect_stdout(io.StringIO()):
            AMALGAMATION.write_output(self.root, output, b"current\n")
        mode = os.stat(os.path.join(self.root, output)).st_mode & 0o777
        self.assertEqual(mode, 0o644)

    def test_rejects_include_cycles(self):
        self.write_bytes("src/gint/gint.hpp", b'#pragma once\n#include "other.hpp"\n')
        self.write_bytes("src/gint/other.hpp", b'#pragma once\n#include "gint.hpp"\n')
        with self.assertRaisesRegex(AMALGAMATION.AmalgamationError, "include cycle"):
            AMALGAMATION.build_amalgamation(self.root)

    def test_rejects_missing_escaping_and_unsupported_quoted_includes(self):
        self.write_bytes("src/gint/gint.hpp", b'#pragma once\n#include "missing.hpp"\n')
        with self.assertRaises(AMALGAMATION.AmalgamationError):
            AMALGAMATION.build_amalgamation(self.root)

        self.write_bytes("outside.hpp", b"outside\n")
        self.write_bytes("src/gint/gint.hpp", b'#pragma once\n#include "../../outside.hpp"\n')
        with self.assertRaises(AMALGAMATION.AmalgamationError):
            AMALGAMATION.build_amalgamation(self.root)

        self.write_bytes("src/gint/gint.hpp", b'#pragma once\n#include "other.hpp" /* comment */\n')
        self.write_bytes("src/gint/other.hpp", b"other\n")
        with self.assertRaisesRegex(
            AMALGAMATION.AmalgamationError, "block comments are not supported"
        ):
            AMALGAMATION.build_amalgamation(self.root)

    def test_validates_raw_quoted_include_paths_before_resolving(self):
        self.write_bytes("src/gint/detail/part.hpp", b"#pragma once\npart\n")
        self.write_bytes(
            "src/gint/gint.hpp", b'#pragma once\n#include "detail/part.hpp"\n'
        )
        self.assertEqual(AMALGAMATION.build_amalgamation(self.root), b"part\n")

        invalid_paths = (
            (b"", "must not contain empty"),
            (b"./detail/part.hpp", "must not contain empty"),
            (b"detail//part.hpp", "must not contain empty"),
            (b"/absolute.hpp", "must be relative"),
        )
        for include_path, expected_error in invalid_paths:
            with self.subTest(include_path=include_path):
                self.write_bytes(
                    "src/gint/gint.hpp",
                    b'#pragma once\n#include "' + include_path + b'"\n',
                )
                with self.assertRaisesRegex(
                    AMALGAMATION.AmalgamationError, expected_error
                ):
                    AMALGAMATION.build_amalgamation(self.root)

    def test_resolves_safe_parent_include_within_source_directory(self):
        self.write_bytes(
            "src/gint/gint.hpp", b'#pragma once\n#include "detail/a.hpp"\nroot\n'
        )
        self.write_bytes(
            "src/gint/detail/a.hpp",
            b'#pragma once\n#include "../configuration.hpp"\na\n',
        )
        self.write_bytes(
            "src/gint/configuration.hpp", b"#pragma once\nconfiguration\n"
        )
        self.assertEqual(
            AMALGAMATION.build_amalgamation(self.root),
            b"configuration\na\nroot\n",
        )

    def test_rejects_parent_include_through_missing_directory(self):
        self.write_bytes(
            "src/gint/gint.hpp", b'#pragma once\n#include "detail/a.hpp"\n'
        )
        self.write_bytes(
            "src/gint/detail/a.hpp",
            b'#pragma once\n#include "missing/../target.hpp"\n',
        )
        self.write_bytes("src/gint/detail/target.hpp", b"#pragma once\ntarget\n")
        with self.assertRaisesRegex(
            AMALGAMATION.AmalgamationError,
            "internal include path component does not exist",
        ):
            AMALGAMATION.build_amalgamation(self.root)

    def test_rejects_parent_include_that_escapes_source_directory(self):
        self.write_bytes(
            "src/gint/gint.hpp", b'#pragma once\n#include "detail/a.hpp"\n'
        )
        self.write_bytes(
            "src/gint/detail/a.hpp",
            b'#pragma once\n#include "../../outside.hpp"\n',
        )
        self.write_bytes("src/outside.hpp", b"#pragma once\noutside\n")
        with self.assertRaisesRegex(
            AMALGAMATION.AmalgamationError, "internal include path escapes src/gint"
        ):
            AMALGAMATION.build_amalgamation(self.root)

    def test_rejects_parent_include_through_symbolic_link_directory(self):
        self.write_bytes(
            "src/gint/gint.hpp", b'#pragma once\n#include "detail/a.hpp"\n'
        )
        self.write_bytes(
            "src/gint/detail/a.hpp",
            b'#pragma once\n#include "link/../target.hpp"\n',
        )
        self.write_bytes(
            "src/gint/detail/target.hpp", b"#pragma once\nlexical target\n"
        )
        self.write_bytes(
            "src/gint/physical/target.hpp", b"#pragma once\nphysical target\n"
        )
        os.makedirs(os.path.join(self.root, "src", "gint", "physical", "deeper"))
        os.symlink(
            "../physical/deeper", os.path.join(self.root, "src", "gint", "detail", "link")
        )
        with self.assertRaisesRegex(
            AMALGAMATION.AmalgamationError,
            "internal include path must not traverse symbolic-link components",
        ):
            AMALGAMATION.build_amalgamation(self.root)

    def test_rejects_nested_internal_angle_include(self):
        self.write_bytes(
            "src/gint/gint.hpp", b'#pragma once\n#include "nested/a.hpp"\n'
        )
        self.write_bytes(
            "src/gint/nested/a.hpp", b"#pragma once\n#include <b.hpp>\na\n"
        )
        self.write_bytes("src/gint/nested/b.hpp", b"#pragma once\nb\n")
        with self.assertRaisesRegex(
            AMALGAMATION.AmalgamationError,
            "internal header must use a quoted include",
        ):
            AMALGAMATION.build_amalgamation(self.root)

    def test_nested_angle_include_does_not_match_unrelated_internal_basename(self):
        self.write_bytes(
            "src/gint/gint.hpp",
            b'#pragma once\n#include "nested/a.hpp"\n#include "other/external.hpp"\n',
        )
        self.write_bytes(
            "src/gint/nested/a.hpp", b"#pragma once\n#include <external.hpp>\na\n"
        )
        self.write_bytes(
            "src/gint/other/external.hpp", b"#pragma once\ninternal external\n"
        )
        self.assertEqual(
            AMALGAMATION.build_amalgamation(self.root),
            b"#include <external.hpp>\na\ninternal external\n",
        )

    def test_requires_one_canonical_leading_pragma_once(self):
        for content in (
            b"root\n",
            b"#pragma once\n#pragma once\nroot\n",
            b"root\n#pragma once\n",
            b"# pragma once // comment\nroot\n",
        ):
            self.write_bytes("src/gint/gint.hpp", content)
            with self.assertRaisesRegex(AMALGAMATION.AmalgamationError, "canonical #pragma once"):
                AMALGAMATION.build_amalgamation(self.root)

    def test_rejects_conditional_and_noncanonical_internal_includes(self):
        self.write_bytes("src/gint/other.hpp", b"#pragma once\nother\n")

        cases = (
            (
                b'#pragma once\n#if 0\n#include "other.hpp"\n#endif\n#include "other.hpp"\n',
                "internal include must be unconditional",
            ),
            (
                b"#pragma once\n#define GINT_HEADER \"other.hpp\"\n#include GINT_HEADER\n",
                "unsupported include syntax",
            ),
            (b"#pragma once\n#include <other.hpp>\n", "must use a quoted include"),
            (b'#pragma once\n#if __has_include("other.hpp")\n#endif\n', "file-search preprocessing operators"),
            (b'#pragma once\n#if __has_include_next("other.hpp")\n#endif\n', "file-search preprocessing operators"),
            (b'#pragma once\n#if __has_embed("other.bin")\n#endif\n', "file-search preprocessing operators"),
            (b'#pragma once\n#if __has_em\\\nbed("other.bin")\n#endif\n', "file-search preprocessing operators"),
            (b'#pragma once\n#inclu\\\nde "other.hpp"\n', "line-spliced preprocessing directive"),
            (b'#pragma once\n#include "other.hpp"\n#/**/include "other.hpp"\n', "block comments are not supported"),
            (b'#pragma once\n#include "other.hpp"\n#include/**/ "other.hpp"\n', "block comments are not supported"),
            (b'#pragma once\n#include "other.hpp"\n#\\\ninclude "other.hpp"\n', "line-spliced preprocessing directive"),
            (b'#pragma once\n#include "other.hpp"\n#in\\\nclude "other.hpp"\n', "line-spliced preprocessing directive"),
            (b'#pragma once\n#include "other.hpp"\n%:include "other.hpp"\n', "digraph preprocessing directive"),
            (b'#pragma once\n#include "other.hpp"\n??=include "other.hpp"\n', "trigraphs are not supported"),
            (b'#pragma once\n#include "other.hpp"\n_Pragma("once")\n', "pragma operators are not supported"),
            (b'#pragma once\n#include "other.hpp"\n#define P(x) _Pragma(#x)\nP(once)\n', "pragma operators are not supported"),
            (b'#pragma once\n#include "other.hpp"\n__pragma(once)\n', "pragma operators are not supported"),
            (b'#pragma once\n#include "other.hpp"\n_Pra\\\ngma("once")\n', "pragma operators are not supported"),
            (b'#pragma once\n#include "other.hpp"\n#import "other.hpp"\n', "#import and #include_next are not supported"),
            (b'#pragma once\n#include "other.hpp"\n#include_next "other.hpp"\n', "#import and #include_next are not supported"),
            (b'#pragma once\n#include "other.hpp"\n#unknown_directive\n', "unsupported preprocessing directive"),
            (b'#pragma once\n#include "other.hpp"\n#embed "other.bin"\n', "unsupported preprocessing directive"),
            (b'#pragma once\n#include "other.hpp"\n#inclu\\   \nde "other.hpp"\n', "whitespace-separated line splice"),
            (b'#pragma once\n\\ \n#include "other.hpp"\n', "whitespace-separated line splice"),
            (b'#pragma once\n\0#include "other.hpp"\n', "unsupported control byte"),
            (b'#pragma once\n#if\v0\n#include "other.hpp"\n#endif\n#include "other.hpp"\n', "internal include must be unconditional"),
            (b'#pragma once\n#if/**/ 0\n#include "other.hpp"\n#endif/**/\n#include "other.hpp"\n', "block comments are not supported"),
            (b'#pragma once\n/**/#if 0\n#include "other.hpp"\n/**/#endif\n#include "other.hpp"\n', "block comments are not supported"),
            (b'#pragma once\n/* leading\n*/#if 0\n#include "other.hpp"\n#endif\n#include "other.hpp"\n', "block comments are not supported"),
            (b'#pragma once\n/\\\n*\n#include "other.hpp"\n', "block comments are not supported"),
            (b'#pragma once\nstatic const char text[] = R"gint(\n#include "other.hpp"\n)gint";\n#include "other.hpp"\n', "raw string literals are not supported"),
            (b'#pragma once\nstatic const char text[] = R\\\n"(raw)";\n#include "other.hpp"\n', "raw string literals are not supported"),
            (b'#pragma once\n#include "other.hpp"\nimport "other.hpp";\n', "module control lines are not supported"),
            (b'#pragma once\n#include "other.hpp"\nimport <other.hpp>;\n', "module control lines are not supported"),
            (b'#pragma once\n#include "other.hpp"\n#define HEADER_UNIT "other.hpp"\nimport HEADER_UNIT;\n', "module control lines are not supported"),
            (b'#pragma once\n#include "other.hpp"\nexport import "other.hpp";\n', "module control lines are not supported"),
            (b'#pragma once\n#include "other.hpp"\nim\\\nport "other.hpp";\n', "module control lines are not supported"),
            (b'#pragma once\n#include "other.hpp"\nmodule;\n', "module control lines are not supported"),
            (b'#pragma once\n#include "other.hpp"\nmodule gint.internal;\n', "module control lines are not supported"),
            (b'#pragma once\n#include "other.hpp"\nexport module gint.internal;\n', "module control lines are not supported"),
        )
        for content, expected_error in cases:
            with self.subTest(expected_error=expected_error, content=content):
                self.write_bytes("src/gint/gint.hpp", content)
                with self.assertRaisesRegex(
                    AMALGAMATION.AmalgamationError, expected_error
                ):
                    AMALGAMATION.build_amalgamation(self.root)

    def test_preserves_line_spliced_macro_definitions(self):
        self.write_bytes(
            "src/gint/gint.hpp",
            b"#pragma once\n#define GINT_TEST_MACRO(x) \\\n+  ((x) + 1)\nGINT_TEST_MACRO(1)\n",
        )
        self.assertEqual(
            AMALGAMATION.build_amalgamation(self.root),
            b"#define GINT_TEST_MACRO(x) \\\n+  ((x) + 1)\nGINT_TEST_MACRO(1)\n",
        )

    def test_rejects_unreachable_internal_headers(self):
        self.write_bytes("src/gint/gint.hpp", b"#pragma once\nroot\n")
        self.write_bytes("src/gint/orphan.hpp", b"#pragma once\norphan\n")
        with self.assertRaisesRegex(AMALGAMATION.AmalgamationError, "unreachable"):
            AMALGAMATION.build_amalgamation(self.root)

    def test_rejects_symbolic_link_headers(self):
        self.write_bytes("src/gint/gint.hpp", b'#pragma once\n#include "link.hpp"\n')
        self.write_bytes("src/gint/target.hpp", b"target\n")
        os.symlink("target.hpp", os.path.join(self.root, "src", "gint", "link.hpp"))
        with self.assertRaisesRegex(AMALGAMATION.AmalgamationError, "symbolic-link"):
            AMALGAMATION.build_amalgamation(self.root)

    def test_rejects_hard_linked_header_aliases(self):
        self.write_bytes(
            "src/gint/gint.hpp",
            b'#pragma once\n#include "other.hpp"\n#include "alias.hpp"\n',
        )
        self.write_bytes("src/gint/other.hpp", b"#pragma once\nother\n")
        os.link(
            os.path.join(self.root, "src", "gint", "other.hpp"),
            os.path.join(self.root, "src", "gint", "alias.hpp"),
        )
        with self.assertRaisesRegex(AMALGAMATION.AmalgamationError, "same file"):
            AMALGAMATION.build_amalgamation(self.root)

    def test_rejects_nonexact_case_on_case_insensitive_filesystems(self):
        self.write_bytes("src/gint/gint.hpp", b'#pragma once\n#include "OTHER.hpp"\n')
        self.write_bytes("src/gint/other.hpp", b"#pragma once\nother\n")
        uppercase_path = os.path.join(self.root, "src", "gint", "OTHER.hpp")
        if not os.path.exists(uppercase_path):
            self.skipTest("filesystem is case-sensitive")
        with self.assertRaisesRegex(AMALGAMATION.AmalgamationError, "exact on-disk path spelling"):
            AMALGAMATION.build_amalgamation(self.root)

    def test_rejects_symbolic_link_source_directory_and_output(self):
        shutil.rmtree(os.path.join(self.root, "src", "gint"))
        os.makedirs(os.path.join(self.root, "external"))
        self.write_bytes("external/gint.hpp", b"#pragma once\nexternal\n")
        os.symlink("../../external", os.path.join(self.root, "src", "gint"))
        with self.assertRaisesRegex(AMALGAMATION.AmalgamationError, "symbolic-link components"):
            AMALGAMATION.build_amalgamation(self.root)

        os.unlink(os.path.join(self.root, "src", "gint"))
        os.makedirs(os.path.join(self.root, "src", "gint"))
        self.write_bytes("src/gint/gint.hpp", b"#pragma once\nroot\n")
        self.write_bytes("include/gint/target.h", b"target\n")
        os.symlink("target.h", os.path.join(self.root, AMALGAMATION.DEFAULT_OUTPUT))
        with self.assertRaisesRegex(AMALGAMATION.AmalgamationError, "regular file"):
            AMALGAMATION.check_output(self.root, AMALGAMATION.DEFAULT_OUTPUT, b"target\n")
        with self.assertRaisesRegex(AMALGAMATION.AmalgamationError, "regular file"):
            AMALGAMATION.write_output(self.root, AMALGAMATION.DEFAULT_OUTPUT, b"replacement\n")

        os.unlink(os.path.join(self.root, AMALGAMATION.DEFAULT_OUTPUT))
        os.symlink("include/gint", os.path.join(self.root, "linked-output"))
        with self.assertRaisesRegex(AMALGAMATION.AmalgamationError, "symbolic-link components"):
            AMALGAMATION.write_output(self.root, "linked-output/gint.h", b"replacement\n")

    def test_replace_failure_preserves_output_and_removes_temporary_file(self):
        self.write_bytes(AMALGAMATION.DEFAULT_OUTPUT, b"old\n")
        with mock.patch.object(AMALGAMATION.os, "replace", side_effect=OSError("replace failed")):
            with self.assertRaises(OSError), contextlib.redirect_stdout(io.StringIO()):
                AMALGAMATION.write_output(self.root, AMALGAMATION.DEFAULT_OUTPUT, b"new\n")
        with open(os.path.join(self.root, AMALGAMATION.DEFAULT_OUTPUT), "rb") as output_file:
            self.assertEqual(output_file.read(), b"old\n")
        self.assertEqual(sorted(os.listdir(os.path.join(self.root, "include", "gint"))), ["gint.h"])

    def test_rejects_empty_non_lf_and_unterminated_headers(self):
        for content in (b"", b"windows\r\n", b"missing newline"):
            self.write_bytes("src/gint/gint.hpp", content)
            with self.assertRaises(AMALGAMATION.AmalgamationError):
                AMALGAMATION.build_amalgamation(self.root)


if __name__ == "__main__":
    unittest.main()
