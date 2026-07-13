#!/usr/bin/env python3

"""Generate or verify gint's committed single-header distribution.

The source model follows the established header-graph/include-expansion pattern
used by projects such as nlohmann/json. This implementation is intentionally
small and project-specific; it does not copy third-party generator code.
"""

from __future__ import print_function

import argparse
import errno
import hashlib
import os
import re
import stat
import sys
import tempfile


DEFAULT_INPUT = "src/gint/gint.hpp"
DEFAULT_OUTPUT = "include/gint/gint.h"
SOURCE_DIRECTORY = os.path.join("src", "gint")
FRAGMENT_MARKER = b"// GINT_REENTRANT_DEFINITION_PASS\n"

# This is an architectural classification, not a duplicate include graph. The
# quoted includes in src/gint remain the sole source of direct edges; the
# manifest and role matrix only constrain which directions those edges may take.
PROJECT_HEADER_MANIFEST = {
    "prelude.hpp": {"kind": "module", "role": "core", "order": 0},
    "configuration_pass.hpp": {"kind": "fragment", "role": "core", "order": 10},
    "primitives.hpp": {"kind": "module", "role": "core", "order": 20},
    "integer.hpp": {"kind": "module", "role": "core", "order": 30},
    "standard.hpp": {"kind": "module", "role": "core", "order": 40},
    "core.hpp": {"kind": "module", "role": "core-entry", "order": 50},
    "io_prelude.hpp": {"kind": "module", "role": "io", "order": 0},
    "string_stream.hpp": {"kind": "module", "role": "io", "order": 10},
    "fmt.hpp": {"kind": "module", "role": "io", "order": 20},
    "io.hpp": {"kind": "module", "role": "io-entry", "order": 30},
    "cleanup_pass.hpp": {"kind": "fragment", "role": "cleanup", "order": 0},
    "gint.hpp": {"kind": "module", "role": "distribution", "order": 0},
}
PROJECT_ROLE_DEPENDENCIES = {
    "core": frozenset(("core",)),
    "core-entry": frozenset(("cleanup", "core")),
    "io": frozenset(("core", "io")),
    "io-entry": frozenset(("cleanup", "core-entry", "io")),
    "cleanup": frozenset(),
    "distribution": frozenset(("io-entry",)),
}
PROJECT_FRAGMENT_CONTRACTS = {
    "configuration_pass.hpp": {
        "parents": {
            "fmt.hpp": 1,
            "primitives.hpp": 1,
            "string_stream.hpp": 1,
        },
        "must_be_last": False,
    },
    "cleanup_pass.hpp": {
        "parents": {"core.hpp": 1, "io.hpp": 1},
        "must_be_last": True,
    },
}
VALID_HEADER_KINDS = frozenset(("fragment", "module"))
HEADER_POLICY_FIELDS = frozenset(("kind", "order", "role"))
FRAGMENT_CONTRACT_FIELDS = frozenset(("must_be_last", "parents"))

INCLUDE_RE = re.compile(
    br'^[ \t\v\f]*#[ \t\v\f]*include[ \t\v\f]*"([^"\r\n]*)"[ \t\v\f]*(?://[^\r\n]*)?\n$'
)
ANGLE_INCLUDE_RE = re.compile(
    br"^[ \t\v\f]*#[ \t\v\f]*include[ \t\v\f]*<([^>\r\n]+)>[ \t\v\f]*(?://[^\r\n]*)?\n$"
)
ANY_INCLUDE_RE = re.compile(br"^[ \t\v\f]*#[ \t\v\f]*include(?:[ \t\v\f]|$)")
PRAGMA_ONCE_RE = re.compile(
    br"^[ \t\v\f]*#[ \t\v\f]*pragma[ \t\v\f]+once[ \t\v\f]*\n$"
)
PRAGMA_ONCE_LIKE_RE = re.compile(
    br"^[ \t\v\f]*#[ \t\v\f]*pragma[ \t\v\f]+once(?:[ \t\v\f]|$)"
)
QUOTED_INCLUDE_RE = re.compile(br'^[ \t\v\f]*#[ \t\v\f]*include[ \t\v\f]*"')
CONDITIONAL_START_RE = re.compile(
    br"^[ \t\v\f]*#[ \t\v\f]*(?:if|ifdef|ifndef)(?:[ \t\v\f]|$)"
)
CONDITIONAL_BRANCH_RE = re.compile(
    br"^[ \t\v\f]*#[ \t\v\f]*(?:elif|else)(?:[ \t\v\f]|$)"
)
CONDITIONAL_END_RE = re.compile(
    br"^[ \t\v\f]*#[ \t\v\f]*endif(?:[ \t\v\f]|$)"
)
PREPROCESSOR_DIRECTIVE_RE = re.compile(br"^[ \t\v\f]*(?:#|%:)")
DIGRAPH_DIRECTIVE_RE = re.compile(br"^[ \t\v\f]*%:")
DIRECTIVE_NAME_RE = re.compile(br"^[ \t\v\f]*#[ \t\v\f]*([A-Za-z_][A-Za-z_0-9]*)")
DEFINE_DIRECTIVE_RE = re.compile(
    br"^[ \t\v\f]*#[ \t\v\f]*define(?:[ \t\v\f]|$)"
)
UNSUPPORTED_INCLUDE_DIRECTIVE_RE = re.compile(
    br"^[ \t\v\f]*#[ \t\v\f]*(?:import|include_next)(?:[ \t\v\f]|$)"
)
FILE_SEARCH_OPERATOR_RE = re.compile(
    br"\b(?:__has_embed|__has_include|__has_include_next)\s*\("
)
TRIGRAPH_RE = re.compile(br"\?\?[=/'()!<>-]")
PRAGMA_OPERATOR_RE = re.compile(br"\b(?:_Pragma|__pragma)\s*\(")
RAW_STRING_RE = re.compile(br'(?:u8|u|U|L)?R"')
WHITESPACE_SPLICE_RE = re.compile(br"\\[ \t\v\f]+\n")
MODULE_CONTROL_LINE_RE = re.compile(
    br"^[ \t\v\f]*(?:export[ \t\v\f]+)?(?:import|module)(?:[ \t\v\f]|[<\";:]|$)"
)
ALLOWED_DIRECTIVES = frozenset(
    (
        b"define",
        b"elif",
        b"else",
        b"endif",
        b"error",
        b"if",
        b"ifdef",
        b"ifndef",
        b"include",
        b"pragma",
        b"undef",
    )
)


class AmalgamationError(ValueError):
    pass


def logical_line_groups(content, description):
    """Return (line number, raw bytes, logical bytes, was spliced) groups."""
    whitespace_splice = WHITESPACE_SPLICE_RE.search(content)
    if whitespace_splice:
        line_number = content.count(b"\n", 0, whitespace_splice.start()) + 1
        raise AmalgamationError(
            "whitespace-separated line splice is not supported at {0}:{1}".format(
                description, line_number
            )
        )
    block_comment = content.find(b"/*")
    block_comment_end = content.find(b"*/")
    comment_offsets = [offset for offset in (block_comment, block_comment_end) if offset != -1]
    if comment_offsets:
        comment_offset = min(comment_offsets)
        line_number = content.count(b"\n", 0, comment_offset) + 1
        raise AmalgamationError(
            "block comments are not supported in internal headers at {0}:{1}".format(
                description, line_number
            )
        )
    pragma_operator = PRAGMA_OPERATOR_RE.search(content)
    if pragma_operator:
        line_number = content.count(b"\n", 0, pragma_operator.start()) + 1
        raise AmalgamationError(
            "pragma operators are not supported in internal headers at {0}:{1}".format(
                description, line_number
            )
        )
    trigraph = TRIGRAPH_RE.search(content)
    if trigraph:
        line_number = content.count(b"\n", 0, trigraph.start()) + 1
        raise AmalgamationError(
            "trigraphs are not supported at {0}:{1}".format(description, line_number)
        )
    lines = content.splitlines(True)
    groups = []
    index = 0
    while index < len(lines):
        start_line = index + 1
        raw_lines = []
        logical_parts = []
        while True:
            line = lines[index]
            raw_lines.append(line)
            if line.endswith(b"\\\n"):
                logical_parts.append(line[:-2])
                index += 1
                if index == len(lines):
                    raise AmalgamationError(
                        "dangling line splice at {0}:{1}".format(description, start_line)
                    )
                continue
            logical_parts.append(line)
            index += 1
            break
        logical_line = b"".join(logical_parts)
        if b"/*" in logical_line or b"*/" in logical_line:
            raise AmalgamationError(
                "block comments are not supported in internal headers at {0}:{1}".format(
                    description, start_line
                )
            )
        if RAW_STRING_RE.search(logical_line):
            raise AmalgamationError(
                "raw string literals are not supported in internal headers at {0}:{1}".format(
                    description, start_line
                )
            )
        if FILE_SEARCH_OPERATOR_RE.search(logical_line):
            raise AmalgamationError(
                "file-search preprocessing operators are not supported at {0}:{1}".format(
                    description, start_line
                )
            )
        if MODULE_CONTROL_LINE_RE.match(logical_line):
            raise AmalgamationError(
                "module control lines are not supported in internal headers at {0}:{1}".format(
                    description, start_line
                )
            )
        if PRAGMA_OPERATOR_RE.search(logical_line):
            raise AmalgamationError(
                "pragma operators are not supported in internal headers at {0}:{1}".format(
                    description, start_line
                )
            )
        groups.append((start_line, b"".join(raw_lines), logical_line, len(raw_lines) != 1))
    return groups


def validate_directive_spelling(logical_line, was_spliced, description, line_number):
    if not PREPROCESSOR_DIRECTIVE_RE.match(logical_line):
        return
    if DIGRAPH_DIRECTIVE_RE.match(logical_line):
        raise AmalgamationError(
            "digraph preprocessing directive is not supported at {0}:{1}".format(
                description, line_number
            )
        )
    if UNSUPPORTED_INCLUDE_DIRECTIVE_RE.match(logical_line):
        raise AmalgamationError(
            "#import and #include_next are not supported at {0}:{1}".format(
                description, line_number
            )
        )
    directive_name = DIRECTIVE_NAME_RE.match(logical_line)
    if not directive_name or directive_name.group(1) not in ALLOWED_DIRECTIVES:
        raise AmalgamationError(
            "unsupported preprocessing directive at {0}:{1}".format(
                description, line_number
            )
        )
    if was_spliced and not DEFINE_DIRECTIVE_RE.match(logical_line):
        raise AmalgamationError(
            "line-spliced preprocessing directive is not supported at {0}:{1}".format(
                description, line_number
            )
        )


def repository_root():
    return os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def normalized_relative_path(path, description):
    if os.path.isabs(path):
        raise AmalgamationError("{0} must be repository-root relative: {1}".format(description, path))
    normalized = os.path.normpath(path)
    if normalized == ".." or normalized.startswith(".." + os.sep):
        raise AmalgamationError("{0} escapes the repository root: {1}".format(description, path))
    return normalized


def relative_to_root(root, absolute_path):
    return os.path.relpath(absolute_path, root)


class HeaderGraph(object):
    """Expand the constrained internal C++ header graph deterministically."""

    def __init__(
        self,
        root,
        source_directory=SOURCE_DIRECTORY,
        header_manifest=None,
        role_dependencies=None,
        fragment_contracts=None,
    ):
        self.root = os.path.abspath(root)
        self.source_directory = os.path.abspath(os.path.join(self.root, source_directory))
        self.reject_symlink_components(self.root, self.source_directory, "source directory")
        self.header_manifest = None
        self.role_dependencies = None
        self.fragment_contracts = None
        self.validate_policy(header_manifest, role_dependencies, fragment_contracts)
        self.expanded_identities = set()
        self.reached_identities = set()
        self.active = []
        self.direct_dependency_sites = []

    def validate_policy(self, header_manifest, role_dependencies, fragment_contracts):
        if header_manifest is None and role_dependencies is None and fragment_contracts is None:
            return
        if header_manifest is None or role_dependencies is None:
            raise AmalgamationError(
                "header manifest and role dependency policy must be provided together"
            )
        if not isinstance(header_manifest, dict) or not isinstance(role_dependencies, dict):
            raise AmalgamationError("header manifest and role dependency policy must be mappings")

        normalized_roles = {}
        for role, dependencies in role_dependencies.items():
            if not isinstance(role, str) or not role:
                raise AmalgamationError("module role names must be non-empty strings")
            try:
                normalized_dependencies = frozenset(dependencies)
            except TypeError:
                raise AmalgamationError(
                    "allowed dependencies for role {0} must be an iterable of roles".format(role)
                )
            if any(not isinstance(dependency, str) or not dependency for dependency in normalized_dependencies):
                raise AmalgamationError(
                    "allowed dependencies for role {0} must be non-empty strings".format(role)
                )
            normalized_roles[role] = normalized_dependencies
        unknown_target_roles = sorted(
            dependency
            for dependencies in normalized_roles.values()
            for dependency in dependencies
            if dependency not in normalized_roles
        )
        if unknown_target_roles:
            raise AmalgamationError(
                "role dependency policy references unknown role(s): {0}".format(
                    ", ".join(sorted(set(unknown_target_roles)))
                )
            )

        normalized_manifest = {}
        for path, policy in header_manifest.items():
            if not isinstance(path, str) or not path:
                raise AmalgamationError("header manifest paths must be non-empty strings")
            if (
                os.path.isabs(path)
                or "\\" in path
                or os.path.normpath(path) != path
                or path == ".."
                or path.startswith(".." + os.sep)
                or not path.endswith(".hpp")
            ):
                raise AmalgamationError(
                    "header manifest path must be a canonical src/gint-relative .hpp path: {0}".format(
                        path
                    )
                )
            if not isinstance(policy, dict) or frozenset(policy) != HEADER_POLICY_FIELDS:
                raise AmalgamationError(
                    "header manifest entry must contain exactly kind, role, and order: {0}".format(
                        path
                    )
                )
            kind = policy["kind"]
            role = policy["role"]
            order = policy["order"]
            if kind not in VALID_HEADER_KINDS:
                raise AmalgamationError(
                    "header manifest has unsupported kind for {0}: {1}".format(path, kind)
                )
            if role not in normalized_roles:
                raise AmalgamationError(
                    "header manifest has unknown role for {0}: {1}".format(path, role)
                )
            if isinstance(order, bool) or not isinstance(order, int) or order < 0:
                raise AmalgamationError(
                    "header manifest order must be a non-negative integer: {0}".format(path)
                )
            normalized_manifest[path] = {"kind": kind, "role": role, "order": order}

        self.header_manifest = normalized_manifest
        self.role_dependencies = normalized_roles
        self.validate_fragment_contracts(fragment_contracts)

    def validate_fragment_contracts(self, fragment_contracts):
        if fragment_contracts is None:
            return
        if not isinstance(fragment_contracts, dict):
            raise AmalgamationError("fragment contracts must be a mapping")
        fragment_paths = set(
            path
            for path, policy in self.header_manifest.items()
            if policy["kind"] == "fragment"
        )
        contract_paths = set(fragment_contracts)
        if contract_paths != fragment_paths:
            raise AmalgamationError(
                "fragment contracts must classify every and only manifest fragment: {0}".format(
                    ", ".join(sorted(fragment_paths ^ contract_paths))
                )
            )

        normalized_contracts = {}
        for path, contract in fragment_contracts.items():
            if not isinstance(contract, dict) or frozenset(contract) != FRAGMENT_CONTRACT_FIELDS:
                raise AmalgamationError(
                    "fragment contract must contain exactly parents and must_be_last: {0}".format(
                        path
                    )
                )
            if not isinstance(contract["must_be_last"], bool):
                raise AmalgamationError(
                    "fragment must_be_last policy must be boolean: {0}".format(path)
                )
            parents = contract["parents"]
            if not isinstance(parents, dict) or not parents:
                raise AmalgamationError(
                    "fragment contract parents must be a non-empty mapping: {0}".format(path)
                )
            normalized_parents = {}
            for parent, count in parents.items():
                if parent not in self.header_manifest:
                    raise AmalgamationError(
                        "fragment contract references unknown parent {0}: {1}".format(
                            parent, path
                        )
                    )
                if self.header_manifest[parent]["kind"] != "module":
                    raise AmalgamationError(
                        "fragment contract parent must be a normal module {0}: {1}".format(
                            parent, path
                        )
                    )
                if isinstance(count, bool) or not isinstance(count, int) or count <= 0:
                    raise AmalgamationError(
                        "fragment parent include count must be a positive integer {0}: {1}".format(
                            parent, path
                        )
                    )
                normalized_parents[parent] = count
            normalized_contracts[path] = {
                "parents": normalized_parents,
                "must_be_last": contract["must_be_last"],
            }
        self.fragment_contracts = normalized_contracts

    def source_relative_path(self, absolute_path):
        return os.path.relpath(absolute_path, self.source_directory).replace(os.sep, "/")

    def header_policy(self, absolute_path):
        if self.header_manifest is None:
            return {"kind": "module", "role": None, "order": 0}
        relative_path = self.source_relative_path(absolute_path)
        if relative_path not in self.header_manifest:
            raise AmalgamationError(
                "internal header is not classified by the module manifest: {0}".format(
                    relative_to_root(self.root, absolute_path)
                )
            )
        return self.header_manifest[relative_path]

    def record_direct_dependency(
        self,
        including_header,
        dependency,
        line_number,
        is_last_effective,
    ):
        source = self.source_relative_path(including_header)
        target = self.source_relative_path(dependency)
        self.direct_dependency_sites.append(
            {
                "source": source,
                "target": target,
                "line": line_number,
                "is_last_effective": is_last_effective,
            }
        )
        if self.header_manifest is None:
            return

        source_policy = self.header_policy(including_header)
        target_policy = self.header_policy(dependency)
        allowed_roles = self.role_dependencies[source_policy["role"]]
        if target_policy["role"] not in allowed_roles:
            raise AmalgamationError(
                "module role dependency is not allowed: {0} ({1}) -> {2} ({3})".format(
                    source,
                    source_policy["role"],
                    target,
                    target_policy["role"],
                )
            )
        if (
            source_policy["role"] == target_policy["role"]
            and target_policy["order"] >= source_policy["order"]
        ):
            raise AmalgamationError(
                "same-role dependency must target a lower order: {0} ({1}) -> {2} ({3})".format(
                    source,
                    source_policy["order"],
                    target,
                    target_policy["order"],
                )
            )

    def validate_manifest_coverage(self, discovered_headers):
        if self.header_manifest is None:
            return
        discovered_paths = set(
            self.source_relative_path(path) for path in discovered_headers.values()
        )
        manifest_paths = set(self.header_manifest)
        unclassified = sorted(discovered_paths - manifest_paths)
        missing = sorted(manifest_paths - discovered_paths)
        if unclassified or missing:
            details = []
            if unclassified:
                details.append("unclassified header(s): {0}".format(", ".join(unclassified)))
            if missing:
                details.append("missing manifest header(s): {0}".format(", ".join(missing)))
            raise AmalgamationError("module manifest does not match src/gint: {0}".format("; ".join(details)))

    def validate_fragment_structure(self):
        if self.fragment_contracts is None:
            return
        for fragment, contract in self.fragment_contracts.items():
            actual_parents = {}
            for site in self.direct_dependency_sites:
                if site["target"] != fragment:
                    continue
                source = site["source"]
                actual_parents[source] = actual_parents.get(source, 0) + 1
                if contract["must_be_last"] and not site["is_last_effective"]:
                    raise AmalgamationError(
                        "fragment include must be the parent's last effective statement: "
                        "{0}:{1} -> {2}".format(source, site["line"], fragment)
                    )
            if actual_parents != contract["parents"]:
                def format_parents(parents):
                    return ", ".join(
                        "{0} x{1}".format(parent, parents[parent])
                        for parent in sorted(parents)
                    ) or "none"

                raise AmalgamationError(
                    "fragment direct-parent contract mismatch for {0}: expected {1}; "
                    "actual {2}".format(
                        fragment,
                        format_parents(contract["parents"]),
                        format_parents(actual_parents),
                    )
                )

    def reject_symlink_components(self, base, path, description):
        base = os.path.abspath(base)
        path = os.path.abspath(path)
        try:
            relative = os.path.relpath(path, base)
        except ValueError:
            raise AmalgamationError("{0} is outside its expected root: {1}".format(description, path))
        if relative == ".." or relative.startswith(".." + os.sep):
            raise AmalgamationError("{0} is outside its expected root: {1}".format(description, path))
        current = base
        for component in [] if relative == "." else relative.split(os.sep):
            next_path = os.path.join(current, component)
            if os.path.isdir(current):
                entries = os.listdir(current)
                if component not in entries and os.path.lexists(next_path):
                    raise AmalgamationError(
                        "{0} must use exact on-disk path spelling: {1}".format(
                            description, relative_to_root(self.root, next_path)
                        )
                    )
            current = next_path
            if os.path.islink(current):
                raise AmalgamationError(
                    "{0} must not contain symbolic-link components: {1}".format(
                        description, relative_to_root(self.root, current)
                    )
                )

    def validate_source_path(self, absolute_path, description):
        absolute_path = os.path.abspath(absolute_path)
        try:
            inside_source = os.path.commonpath([self.source_directory, absolute_path]) == self.source_directory
        except ValueError:
            inside_source = False
        if not inside_source or not absolute_path.endswith(".hpp"):
            raise AmalgamationError(
                "{0} must resolve to a src/gint/*.hpp file: {1}".format(
                    description, relative_to_root(self.root, absolute_path)
                )
            )
        self.reject_symlink_components(self.source_directory, absolute_path, description)
        if not os.path.isfile(absolute_path):
            raise AmalgamationError(
                "{0} does not exist: {1}".format(description, relative_to_root(self.root, absolute_path))
            )
        return absolute_path

    def file_identity(self, absolute_path):
        metadata = os.stat(absolute_path)
        return (metadata.st_dev, metadata.st_ino)

    def is_internal_angle_include(self, include_path, including_header=None):
        if include_path.startswith("gint/"):
            return True
        candidates = [os.path.join(self.source_directory, include_path)]
        if including_header is not None:
            candidates.append(os.path.join(os.path.dirname(including_header), include_path))
        for candidate in candidates:
            absolute_candidate = os.path.abspath(candidate)
            try:
                inside_source = (
                    os.path.commonpath([self.source_directory, absolute_candidate])
                    == self.source_directory
                )
            except ValueError:
                inside_source = False
            if inside_source and os.path.isfile(candidate):
                return True
        return False

    def resolve_include_path(
        self, including_header, include_path, description, line_number
    ):
        if os.path.isabs(include_path):
            raise AmalgamationError(
                "internal include path must be relative at {0}:{1}".format(
                    description, line_number
                )
            )
        components = include_path.split("/")
        if any(component in ("", ".") for component in components):
            raise AmalgamationError(
                "internal include path must not contain empty or '.' components "
                "at {0}:{1}".format(description, line_number)
            )

        current = os.path.dirname(including_header)
        for index, component in enumerate(components):
            if component == "..":
                if current == self.source_directory:
                    raise AmalgamationError(
                        "internal include path escapes src/gint at {0}:{1}".format(
                            description, line_number
                        )
                    )
                if os.path.islink(current):
                    raise AmalgamationError(
                        "internal include path must not traverse symbolic-link "
                        "components at {0}:{1}".format(description, line_number)
                    )
                if not os.path.isdir(current):
                    raise AmalgamationError(
                        "internal include parent is not an existing directory at "
                        "{0}:{1}".format(description, line_number)
                    )
                current = os.path.dirname(current)
                continue

            candidate = os.path.join(current, component)
            if index != len(components) - 1:
                if (
                    component not in os.listdir(current)
                    and os.path.lexists(candidate)
                ):
                    raise AmalgamationError(
                        "internal include path must use exact on-disk path spelling "
                        "at {0}:{1}: {2}".format(
                            description, line_number, component
                        )
                    )
                if os.path.islink(candidate):
                    raise AmalgamationError(
                        "internal include path must not traverse symbolic-link "
                        "components at {0}:{1}".format(description, line_number)
                    )
                if not os.path.exists(candidate):
                    raise AmalgamationError(
                        "internal include path component does not exist at "
                        "{0}:{1}: {2}".format(description, line_number, component)
                    )
                if not os.path.isdir(candidate):
                    raise AmalgamationError(
                        "internal include path component is not a directory at "
                        "{0}:{1}: {2}".format(description, line_number, component)
                    )
            current = candidate
        return current

    def read_header(self, absolute_path):
        relative_path = relative_to_root(self.root, absolute_path)
        try:
            with open(absolute_path, "rb") as header_file:
                content = header_file.read()
        except IOError as error:
            raise AmalgamationError("cannot read internal header {0}: {1}".format(relative_path, error))
        if not content:
            raise AmalgamationError("internal header is empty: {0}".format(relative_path))
        if b"\r" in content:
            raise AmalgamationError("internal header must use LF line endings: {0}".format(relative_path))
        for offset, value in enumerate(bytearray(content)):
            if value < 32 and value not in (9, 10, 11, 12):
                line_number = content.count(b"\n", 0, offset) + 1
                raise AmalgamationError(
                    "internal header contains an unsupported control byte at {0}:{1}".format(
                        relative_path, line_number
                    )
                )
        if not content.endswith(b"\n"):
            raise AmalgamationError("internal header must end with a newline: {0}".format(relative_path))
        return content

    def expand(self, absolute_path):
        absolute_path = self.validate_source_path(absolute_path, "internal include")
        policy = self.header_policy(absolute_path)
        identity = self.file_identity(absolute_path)
        active_identities = [entry[0] for entry in self.active]
        if identity in active_identities:
            cycle_start = active_identities.index(identity)
            cycle = [entry[1] for entry in self.active[cycle_start:]] + [absolute_path]
            raise AmalgamationError(
                "internal header include cycle: {0}".format(
                    " -> ".join(relative_to_root(self.root, path) for path in cycle)
                )
            )
        if policy["kind"] == "module" and identity in self.expanded_identities:
            return b""

        self.active.append((identity, absolute_path))
        self.reached_identities.add(identity)
        if policy["kind"] == "module":
            self.expanded_identities.add(identity)
        output = []
        skip_boundary_blank = False
        try:
            relative_path = relative_to_root(self.root, absolute_path)
            groups = logical_line_groups(self.read_header(absolute_path), relative_path)
            pragma_lines = [
                index
                for index, group in enumerate(groups)
                if not group[3] and PRAGMA_ONCE_RE.match(group[2])
            ]
            fragment_marker_lines = [
                index
                for index, group in enumerate(groups)
                if not group[3] and group[1] == FRAGMENT_MARKER
            ]
            first_content_line = next(
                (index for index, group in enumerate(groups) if group[1].strip()), None
            )
            effective_lines = [
                index
                for index, group in enumerate(groups)
                if group[2].strip() and not group[2].lstrip().startswith(b"//")
            ]
            last_effective_line = effective_lines[-1] if effective_lines else None
            if policy["kind"] == "module":
                if fragment_marker_lines:
                    raise AmalgamationError(
                        "normal internal header must not contain the reentrant fragment marker: {0}".format(
                            relative_path
                        )
                    )
                if pragma_lines != [first_content_line]:
                    raise AmalgamationError(
                        "internal header must begin with exactly one canonical #pragma once: {0}".format(
                            relative_path
                        )
                    )
            else:
                if pragma_lines:
                    raise AmalgamationError(
                        "reentrant fragment must not contain #pragma once: {0}".format(
                            relative_path
                        )
                    )
                if fragment_marker_lines != [0] or first_content_line != 0:
                    raise AmalgamationError(
                        "reentrant fragment must begin with exactly one canonical marker: {0}".format(
                            relative_path
                        )
                    )

            conditional_depth = 0
            for group_index, (
                line_number,
                raw_line,
                logical_line,
                was_spliced,
            ) in enumerate(groups):
                validate_directive_spelling(
                    logical_line, was_spliced, relative_path, line_number
                )
                if skip_boundary_blank:
                    skip_boundary_blank = False
                    if raw_line == b"\n":
                        continue
                if PRAGMA_ONCE_RE.match(logical_line):
                    skip_boundary_blank = True
                    continue
                if raw_line == FRAGMENT_MARKER:
                    skip_boundary_blank = True
                    continue
                if PRAGMA_ONCE_LIKE_RE.match(logical_line):
                    raise AmalgamationError(
                        "unsupported #pragma once syntax at {0}:{1}".format(
                            relative_path, line_number
                        )
                    )
                if CONDITIONAL_START_RE.match(logical_line):
                    conditional_depth += 1
                    output.append(raw_line)
                    continue
                if CONDITIONAL_BRANCH_RE.match(logical_line):
                    if conditional_depth == 0:
                        raise AmalgamationError(
                            "unmatched conditional branch at {0}:{1}".format(
                                relative_path, line_number
                            )
                        )
                    output.append(raw_line)
                    continue
                if CONDITIONAL_END_RE.match(logical_line):
                    if conditional_depth == 0:
                        raise AmalgamationError(
                            "unmatched #endif at {0}:{1}".format(
                                relative_path, line_number
                            )
                        )
                    conditional_depth -= 1
                    output.append(raw_line)
                    continue
                include_match = INCLUDE_RE.match(logical_line)
                if include_match:
                    if conditional_depth:
                        raise AmalgamationError(
                            "internal include must be unconditional at {0}:{1}".format(
                                relative_path, line_number
                            )
                        )
                    try:
                        include_path = include_match.group(1).decode("utf-8")
                    except UnicodeDecodeError:
                        raise AmalgamationError(
                            "internal include path is not UTF-8 at {0}:{1}".format(
                                relative_path, line_number
                            )
                        )
                    dependency = self.resolve_include_path(
                        absolute_path, include_path, relative_path, line_number
                    )
                    self.record_direct_dependency(
                        absolute_path,
                        dependency,
                        line_number,
                        group_index == last_effective_line,
                    )
                    expanded_dependency = self.expand(dependency)
                    output.append(expanded_dependency)
                    continue
                angle_include_match = ANGLE_INCLUDE_RE.match(logical_line)
                if angle_include_match:
                    try:
                        angle_include_path = angle_include_match.group(1).decode("utf-8", "strict")
                    except UnicodeDecodeError:
                        raise AmalgamationError(
                            "angle include path is not UTF-8 at {0}:{1}".format(
                                relative_path, line_number
                            )
                        )
                    if self.is_internal_angle_include(angle_include_path, absolute_path):
                        raise AmalgamationError(
                            "internal header must use a quoted include at {0}:{1}".format(
                                relative_path, line_number
                            )
                        )
                    output.append(raw_line)
                    continue
                if ANY_INCLUDE_RE.match(logical_line) or QUOTED_INCLUDE_RE.match(logical_line):
                    raise AmalgamationError(
                        "unsupported include syntax at {0}:{1}".format(
                            relative_path, line_number
                        )
                    )
                output.append(raw_line)
            if conditional_depth:
                raise AmalgamationError(
                    "unterminated conditional block in {0}".format(
                        relative_to_root(self.root, absolute_path)
                    )
                )
        finally:
            self.active.pop()
        return b"".join(output)

    def discovered_headers(self):
        headers = {}
        if not os.path.isdir(self.source_directory):
            raise AmalgamationError("source directory does not exist: {0}".format(SOURCE_DIRECTORY))
        for directory, subdirectories, filenames in os.walk(self.source_directory):
            symbolic_directories = [
                name for name in subdirectories if os.path.islink(os.path.join(directory, name))
            ]
            if symbolic_directories:
                raise AmalgamationError(
                    "source tree must not contain symbolic-link directories: {0}".format(
                        ", ".join(sorted(symbolic_directories))
                    )
                )
            for filename in filenames:
                if not filename.endswith(".hpp"):
                    continue
                absolute_path = os.path.join(directory, filename)
                self.validate_source_path(absolute_path, "internal header")
                identity = self.file_identity(absolute_path)
                if identity in headers:
                    raise AmalgamationError(
                        "multiple internal header paths refer to the same file: {0}, {1}".format(
                            relative_to_root(self.root, headers[identity]),
                            relative_to_root(self.root, absolute_path),
                        )
                    )
                headers[identity] = absolute_path
        return headers


def build_amalgamation(
    root,
    input_path=DEFAULT_INPUT,
    header_manifest=None,
    role_dependencies=None,
    fragment_contracts=None,
):
    input_path = normalized_relative_path(input_path, "input path")
    graph = HeaderGraph(
        root,
        header_manifest=header_manifest,
        role_dependencies=role_dependencies,
        fragment_contracts=fragment_contracts,
    )
    absolute_input = graph.validate_source_path(os.path.join(root, input_path), "input path")
    content = graph.expand(absolute_input)
    discovered_headers = graph.discovered_headers()
    graph.validate_manifest_coverage(discovered_headers)
    graph.validate_fragment_structure()
    unreachable = sorted(
        path
        for identity, path in discovered_headers.items()
        if identity not in graph.reached_identities
    )
    if unreachable:
        raise AmalgamationError(
            "internal header(s) are unreachable from {0}: {1}".format(
                input_path, ", ".join(relative_to_root(root, path) for path in unreachable)
            )
        )
    # The internal entry header starts with #pragma once. Removing it leaves
    # leading blank lines that have no value in the distribution header.
    content = content.lstrip(b"\n")
    for line_number, raw_line, logical_line, was_spliced in logical_line_groups(
        content, "generated header"
    ):
        validate_directive_spelling(
            logical_line, was_spliced, "generated header", line_number
        )
        if (
            PRAGMA_ONCE_LIKE_RE.match(logical_line)
            or QUOTED_INCLUDE_RE.match(logical_line)
            or raw_line == FRAGMENT_MARKER
        ):
            raise AmalgamationError(
                "generated header retains an internal directive at line {0}".format(line_number)
            )
        angle_include_match = ANGLE_INCLUDE_RE.match(logical_line)
        if angle_include_match:
            try:
                angle_include_path = angle_include_match.group(1).decode("utf-8", "strict")
            except UnicodeDecodeError:
                raise AmalgamationError(
                    "generated header retains a non-UTF-8 angle include at line {0}".format(
                        line_number
                    )
                )
            if graph.is_internal_angle_include(angle_include_path):
                raise AmalgamationError(
                    "generated header retains an internal angle include at line {0}".format(line_number)
                )
    return content


def build_project_amalgamation(root, input_path=DEFAULT_INPUT):
    return build_amalgamation(
        root,
        input_path,
        header_manifest=PROJECT_HEADER_MANIFEST,
        role_dependencies=PROJECT_ROLE_DEPENDENCIES,
        fragment_contracts=PROJECT_FRAGMENT_CONTRACTS,
    )


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def first_difference(expected, actual):
    limit = min(len(expected), len(actual))
    for offset in range(limit):
        if expected[offset] != actual[offset]:
            return offset
    return limit


def validated_output_path(root, output_path):
    output_path = normalized_relative_path(output_path, "output path")
    absolute_output = os.path.abspath(os.path.join(root, output_path))
    output_dir = os.path.dirname(absolute_output)

    current = os.path.abspath(root)
    relative_directory = os.path.relpath(output_dir, current)
    if relative_directory == ".." or relative_directory.startswith(".." + os.sep):
        raise AmalgamationError("output path escapes the repository root: {0}".format(output_path))
    for component in [] if relative_directory == "." else relative_directory.split(os.sep):
        current = os.path.join(current, component)
        if os.path.islink(current):
            raise AmalgamationError(
                "output directory must not contain symbolic-link components: {0}".format(output_path)
            )

    if os.path.lexists(absolute_output):
        output_mode = os.lstat(absolute_output).st_mode
        if stat.S_ISLNK(output_mode) or not stat.S_ISREG(output_mode):
            raise AmalgamationError("output must be a regular file, not a symbolic link: {0}".format(output_path))
    return output_path, absolute_output


def check_output(root, output_path, expected):
    output_path, absolute_output = validated_output_path(root, output_path)
    try:
        with open(absolute_output, "rb") as output_file:
            actual = output_file.read()
    except IOError as error:
        if error.errno != errno.ENOENT:
            raise
        actual = None
    if actual is not None and actual == expected:
        print("amalgamated header is current: {0} ({1})".format(output_path, sha256(expected)))
        return True

    offset = first_difference(expected, actual if actual is not None else b"")
    print("error: amalgamated header is stale: {0}".format(output_path), file=sys.stderr)
    print("error: expected sha256 {0}".format(sha256(expected)), file=sys.stderr)
    if actual is None:
        print("error: actual output is missing", file=sys.stderr)
    else:
        print("error: actual   sha256 {0}".format(sha256(actual)), file=sys.stderr)
    print("error: first differing byte offset {0}; run scripts/generate-amalgamation.py".format(offset), file=sys.stderr)
    return False


def write_output(root, output_path, content):
    output_path, absolute_output = validated_output_path(root, output_path)
    output_dir = os.path.dirname(absolute_output)
    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)
        output_path, absolute_output = validated_output_path(root, output_path)

    current_mode = 0o644
    if os.path.lexists(absolute_output):
        with open(absolute_output, "rb") as output_file:
            if output_file.read() == content:
                print("amalgamated header is already current: {0} ({1})".format(output_path, sha256(content)))
                return
        current_mode = os.stat(absolute_output).st_mode & 0o777

    temporary = None
    try:
        with tempfile.NamedTemporaryFile(prefix=".gint-amalgamation-", dir=output_dir, delete=False) as temporary_file:
            temporary = temporary_file.name
            temporary_file.write(content)
            temporary_file.flush()
            os.fchmod(temporary_file.fileno(), current_mode)
            os.fsync(temporary_file.fileno())
        os.replace(temporary, absolute_output)
        temporary = None
    finally:
        if temporary is not None and os.path.exists(temporary):
            os.unlink(temporary)
    print("generated {0} ({1})".format(output_path, sha256(content)))


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--check", action="store_true", help="verify the committed header without changing files")
    parser.add_argument("--root", default=repository_root(), help="repository root (primarily for tests)")
    parser.add_argument("--input", default=DEFAULT_INPUT, help="repository-root-relative internal entry header")
    parser.add_argument("--output", default=DEFAULT_OUTPUT, help="repository-root-relative generated header path")
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    root = os.path.abspath(args.root)
    try:
        content = build_project_amalgamation(root, args.input)
        if args.check:
            return 0 if check_output(root, args.output, content) else 1
        write_output(root, args.output, content)
        return 0
    except (AmalgamationError, OSError) as error:
        print("error: {0}".format(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
