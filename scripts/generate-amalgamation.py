#!/usr/bin/env python3

"""Generate or verify gint's committed single-header distribution.

The source model follows the established header-graph/include-expansion pattern
used by projects such as nlohmann/json. This implementation is intentionally
small and project-specific; it does not copy third-party generator code.
"""

from __future__ import print_function

import argparse
from collections import namedtuple
import errno
import hashlib
import os
import posixpath
import re
import stat
import sys
import tempfile


DEFAULT_INPUT = "src/gint/gint.hpp"
DEFAULT_OUTPUT = "include/gint/gint.h"
SOURCE_DIRECTORY = os.path.join("src", "gint")

# This is an architectural classification, not a duplicate include graph. The
# quoted includes in src/gint remain the sole source of direct edges; the
# manifest and role matrix only constrain which directions those edges may take.
PROJECT_HEADER_MANIFEST = {
    "prelude.hpp": {"role": "core", "order": 0},
    "configuration.hpp": {"role": "core", "order": 10},
    "limb_ops.hpp": {"role": "core", "order": 20},
    "integer.hpp": {"role": "core", "order": 30},
    "standard.hpp": {"role": "core", "order": 40},
    "string_stream.hpp": {"role": "io", "order": 0},
    "fmt.hpp": {"role": "io", "order": 10},
    "cleanup.hpp": {"role": "cleanup", "order": 0},
    "gint.hpp": {"role": "distribution", "order": 0},
}
PROJECT_ROLE_DEPENDENCIES = {
    "core": frozenset(("core",)),
    "io": frozenset(("core", "io")),
    "cleanup": frozenset(),
    "distribution": frozenset(("core", "io", "cleanup")),
}
HEADER_POLICY_FIELDS = frozenset(("order", "role"))

HORIZONTAL_SPACE = b" \t\v\f"
QUOTED_HEADER_RE = re.compile(br'^"([^"\r\n]*)"[ \t\v\f]*(?://[^\r\n]*)?$')
ANGLE_HEADER_RE = re.compile(br"^<([^>\r\n]+)>[ \t\v\f]*(?://[^\r\n]*)?$")
PRAGMA_ONCE_RE = re.compile(
    br"^[ \t\v\f]*#[ \t\v\f]*pragma[ \t\v\f]+once[ \t\v\f]*\n$"
)
DIAGNOSTIC_PRAGMA_RE = re.compile(
    br'^(?:GCC|clang)[ \t\v\f]+diagnostic[ \t\v\f]+'
    br'(?:(?:push|pop)|ignored[ \t\v\f]+"-W[A-Za-z_0-9-]+")$'
)
IDENTIFIER_RE = re.compile(br"[A-Za-z_][A-Za-z_0-9]*")
FILE_SEARCH_OPERATOR_RE = re.compile(br"\b(?:__has_embed|__has_include|__has_include_next)\b")
TRIGRAPH_RE = re.compile(br"\?\?[=/'()!<>-]")
DIGRAPH_RE = re.compile(br"(?:%:%:|<:|:>|<%|%>|%:)")
PRAGMA_OPERATOR_RE = re.compile(br"\b(?:_Pragma|__pragma)\b")
FILE_CONTEXT_MACRO_RE = re.compile(
    br"\b(?:__BASE_FILE__|__FILE__|__FILE_NAME__|__INCLUDE_LEVEL__|__LINE__|__TIMESTAMP__"
    br"|__builtin_(?:LINE|COLUMN|FILE|FILE_NAME|source_location))\b"
)
TOKEN_PASTE_RE = re.compile(br"(?:##|%:%:)")
ALLOWED_TOKEN_PASTE_RE = re.compile(
    br"^[ \t\v\f]*#[ \t\v\f]*define[ \t\v\f]+GINT_DETAIL_CONFIG_NAMESPACE_I"
    br"\([ \t\v\f]*divzero[ \t\v\f]*,[ \t\v\f]*gcc_tuned[ \t\v\f]*,"
    br"[ \t\v\f]*clang_tuned[ \t\v\f]*,[ \t\v\f]*aarch64_asm[ \t\v\f]*,"
    br"[ \t\v\f]*exceptions[ \t\v\f]*\)[ \t\v\f]+"
    br"config_d##divzero##_g##gcc_tuned##_c##clang_tuned##_a##aarch64_asm##_e##exceptions"
    br"[ \t\v\f]*\n$"
)
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
    """Normalize translation-phase line splices, retaining original bytes/locations."""
    whitespace_splice = WHITESPACE_SPLICE_RE.search(content)
    if whitespace_splice:
        line_number = content.count(b"\n", 0, whitespace_splice.start()) + 1
        raise AmalgamationError(
            "whitespace-separated line splice is not supported at {0}:{1}".format(
                description, line_number
            )
        )
    # Trigraph replacement precedes line splicing in the supported C++11
    # translation model. Reject it before treating backslash-newline pairs.
    trigraph = TRIGRAPH_RE.search(content)
    if trigraph:
        line_number = content.count(b"\n", 0, trigraph.start()) + 1
        raise AmalgamationError(
            "trigraphs are not supported at {0}:{1}".format(description, line_number)
        )
    # Only LF is a physical line boundary. Vertical tab/form feed are C++
    # horizontal whitespace and must not split a preprocessing directive.
    lines = [line + b"\n" for line in content.split(b"\n")[:-1]]
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
        groups.append((start_line, b"".join(raw_lines), logical_line, len(raw_lines) != 1))
    return groups


FORBIDDEN_LOGICAL_FORMS = (
    (re.compile(br"/\*|\*/"), "block comments are not supported in internal headers"),
    (TRIGRAPH_RE, "trigraph spellings are not supported"),
    (DIGRAPH_RE, "digraphs are not supported"),
    (RAW_STRING_RE, "raw string literals are not supported in internal headers"),
    (FILE_SEARCH_OPERATOR_RE, "file-search preprocessing operators are not supported"),
    (FILE_CONTEXT_MACRO_RE, "file-context predefined macros and builtins are not supported"),
    (PRAGMA_OPERATOR_RE, "pragma operators are not supported in internal headers"),
    (MODULE_CONTROL_LINE_RE, "module control lines are not supported in internal headers"),
)


def reject_at(message, description, line_number):
    raise AmalgamationError("{0} at {1}:{2}".format(message, description, line_number))


def validate_logical_line(logical_line, description, line_number):
    # This is a deliberately conservative byte-level dialect: forbidden forms
    # are also rejected in comments/literals. Apply every token restriction
    # after splicing so split spellings and unsplit spellings have one policy.
    for pattern, message in FORBIDDEN_LOGICAL_FORMS:
        if pattern.search(logical_line):
            reject_at(message, description, line_number)
    if TOKEN_PASTE_RE.search(logical_line) and not ALLOWED_TOKEN_PASTE_RE.match(logical_line):
        reject_at("token-pasting operators are not supported", description, line_number)


def without_line_comment(text):
    """Remove a trailing // comment without interpreting // inside a literal."""
    quote = None
    index = 0
    while index < len(text):
        char = text[index:index + 1]
        if quote is not None:
            if char == b"\\":
                index += 2
                continue
            if char == quote:
                quote = None
        elif char in (b'"', b"'"):
            quote = char
        elif text[index:index + 2] == b"//":
            return text[:index]
        index += 1
    return text


Directive = namedtuple("Directive", "name argument include_kind include_path")


class SourceLine(namedtuple("SourceLineBase", "number raw logical was_spliced directive")):
    __slots__ = ()

    @property
    def is_once(self):
        return self.directive is not None and self.directive.name == b"pragma" and self.directive.argument == b"once"


def parse_directive(logical_line, was_spliced, description, line_number):
    text = logical_line.lstrip(HORIZONTAL_SPACE)
    if not text.startswith(b"#"):
        return None
    text = text[1:].lstrip(HORIZONTAL_SPACE)
    match = IDENTIFIER_RE.match(text)
    if not match:
        reject_at("unsupported preprocessing directive", description, line_number)
    name = match.group()
    if name in (b"import", b"include_next"):
        reject_at("#import and #include_next are not supported", description, line_number)
    if name not in ALLOWED_DIRECTIVES:
        reject_at("unsupported preprocessing directive", description, line_number)
    if was_spliced and name != b"define":
        reject_at("line-spliced preprocessing directive is not supported", description, line_number)

    argument = text[match.end():].rstrip(b"\n").lstrip(HORIZONTAL_SPACE)
    if name == b"include":
        # Header names form their own preprocessing token. In particular, //
        # inside <...> is part of the path, not a line-comment delimiter.
        quoted = QUOTED_HEADER_RE.match(argument)
        angle = ANGLE_HEADER_RE.match(argument)
        if not quoted and not angle:
            reject_at("unsupported include syntax", description, line_number)
        path_bytes = (quoted or angle).group(1)
        if b"\\" in path_bytes or any(byte <= 32 or byte == 127 for byte in bytearray(path_bytes)):
            reject_at("unsupported include path spelling", description, line_number)
        try:
            path = path_bytes.decode("utf-8")
        except UnicodeDecodeError:
            reject_at("include path is not UTF-8", description, line_number)
        return Directive(name, argument, "quoted" if quoted else "angle", path)

    argument = without_line_comment(argument).strip(HORIZONTAL_SPACE)
    if name in (b"ifdef", b"ifndef", b"undef"):
        identifier = IDENTIFIER_RE.match(argument)
        if not identifier or identifier.end() != len(argument):
            reject_at("directive requires exactly one identifier", description, line_number)
    elif name in (b"if", b"elif"):
        if not argument:
            reject_at("conditional directive requires an expression", description, line_number)
    elif name in (b"else", b"endif"):
        if argument:
            reject_at("unexpected tokens after conditional directive", description, line_number)
    elif name == b"define":
        if not IDENTIFIER_RE.match(argument):
            reject_at("macro definition requires an identifier", description, line_number)
    elif name == b"pragma":
        if argument == b"once":
            if not PRAGMA_ONCE_RE.match(logical_line):
                reject_at("unsupported #pragma once syntax; expected canonical #pragma once", description, line_number)
        elif not DIAGNOSTIC_PRAGMA_RE.match(argument):
            reject_at("unsupported pragma; only once and GCC/clang diagnostic pragmas are supported", description, line_number)
    return Directive(name, argument, None, None)


def parse_source(content, description):
    """Produce the only directive representation consumed by graph/output checks."""
    lines = []
    for number, raw, logical, spliced in logical_line_groups(content, description):
        validate_logical_line(logical, description, number)
        directive = parse_directive(logical, spliced, description, number)
        lines.append(SourceLine(number, raw, logical, spliced, directive))
    return lines


class ConditionalState(object):
    """Track syntax only; local includes are forbidden in every conditional arm."""

    def __init__(self):
        self.else_seen = []

    @property
    def active(self):
        return bool(self.else_seen)

    def consume(self, line, description):
        if line.directive is None:
            return False
        name = line.directive.name
        if name in (b"if", b"ifdef", b"ifndef"):
            self.else_seen.append(False)
        elif name in (b"elif", b"else"):
            if not self.else_seen:
                reject_at("unmatched conditional branch", description, line.number)
            if self.else_seen[-1]:
                reject_at("conditional branch follows #else", description, line.number)
            self.else_seen[-1] = name == b"else"
        elif name == b"endif":
            if not self.else_seen:
                reject_at("unmatched #endif", description, line.number)
            self.else_seen.pop()
        else:
            return False
        return True

    def finish(self, description):
        if self.else_seen:
            raise AmalgamationError("unterminated conditional block in {0}".format(description))


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
    ):
        self.root = os.path.abspath(root)
        self.source_directory = os.path.abspath(os.path.join(self.root, source_directory))
        self.reject_symlink_components(self.root, self.source_directory, "source directory")
        self.header_manifest = None
        self.role_dependencies = None
        self.validate_policy(header_manifest, role_dependencies)
        self.expanded_identities = set()
        self.reached_identities = set()
        self.source_identities = set()
        self.active = []

    def validate_policy(self, header_manifest, role_dependencies):
        if header_manifest is None and role_dependencies is None:
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
                    "header manifest entry must contain exactly role and order: {0}".format(
                        path
                    )
                )
            role = policy["role"]
            order = policy["order"]
            if role not in normalized_roles:
                raise AmalgamationError(
                    "header manifest has unknown role for {0}: {1}".format(path, role)
                )
            if isinstance(order, bool) or not isinstance(order, int) or order < 0:
                raise AmalgamationError(
                    "header manifest order must be a non-negative integer: {0}".format(path)
                )
            normalized_manifest[path] = {"role": role, "order": order}

        self.header_manifest = normalized_manifest
        self.role_dependencies = normalized_roles

    def source_relative_path(self, absolute_path):
        return os.path.relpath(absolute_path, self.source_directory).replace(os.sep, "/")

    def header_policy(self, absolute_path):
        if self.header_manifest is None:
            return {"role": None, "order": 0}
        relative_path = self.source_relative_path(absolute_path)
        if relative_path not in self.header_manifest:
            raise AmalgamationError(
                "internal header is not classified by the module manifest: {0}".format(
                    relative_to_root(self.root, absolute_path)
                )
            )
        return self.header_manifest[relative_path]

    def validate_direct_dependency(self, including_header, dependency):
        source = self.source_relative_path(including_header)
        target = self.source_relative_path(dependency)
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
        # Reserve the internal include namespace on both case-sensitive and
        # case-insensitive filesystems. Classification never rewrites output.
        namespace = os.path.basename(self.source_directory).casefold()
        prefixes = (include_path.split("/", 1)[0], posixpath.normpath(include_path).split("/", 1)[0])
        if any(prefix.casefold() == namespace for prefix in prefixes):
            return True
        candidates = [
            os.path.join(os.path.dirname(self.source_directory), include_path),
            os.path.join(self.source_directory, include_path),
        ]
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
            if os.path.isfile(candidate):
                if inside_source or self.file_identity(candidate) in self.source_identities:
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
        self.header_policy(absolute_path)
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
        if identity in self.expanded_identities:
            return b""

        self.active.append((identity, absolute_path))
        self.reached_identities.add(identity)
        self.expanded_identities.add(identity)
        output = []
        skip_boundary_blank = False
        try:
            relative_path = relative_to_root(self.root, absolute_path)
            groups = parse_source(self.read_header(absolute_path), relative_path)
            pragma_lines = [
                index
                for index, group in enumerate(groups)
                if group.is_once
            ]
            first_content_line = next(
                (index for index, group in enumerate(groups) if group.raw.strip()), None
            )
            if pragma_lines != [first_content_line]:
                raise AmalgamationError(
                    "internal header must begin with exactly one canonical #pragma once: {0}".format(
                        relative_path
                    )
                )

            conditional = ConditionalState()
            for line in groups:
                raw_line = line.raw
                directive = line.directive
                if skip_boundary_blank:
                    skip_boundary_blank = False
                    if raw_line == b"\n":
                        continue
                if line.is_once:
                    skip_boundary_blank = True
                    continue
                if conditional.consume(line, relative_path):
                    output.append(raw_line)
                    continue
                if directive is not None and directive.include_kind == "quoted":
                    if conditional.active:
                        raise AmalgamationError(
                            "internal include must be unconditional at {0}:{1}".format(
                                relative_path, line.number
                            )
                        )
                    dependency = self.resolve_include_path(
                        absolute_path, directive.include_path, relative_path, line.number
                    )
                    self.validate_direct_dependency(absolute_path, dependency)
                    expanded_dependency = self.expand(dependency)
                    output.append(expanded_dependency)
                    continue
                if directive is not None and directive.include_kind == "angle":
                    if self.is_internal_angle_include(directive.include_path, absolute_path):
                        raise AmalgamationError(
                            "internal header must use a quoted include at {0}:{1}".format(
                                relative_path, line.number
                            )
                        )
                    output.append(raw_line)
                    continue
                output.append(raw_line)
            conditional.finish(relative_path)
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
):
    input_path = normalized_relative_path(input_path, "input path")
    graph = HeaderGraph(
        root,
        header_manifest=header_manifest,
        role_dependencies=role_dependencies,
    )
    absolute_input = graph.validate_source_path(os.path.join(root, input_path), "input path")
    discovered_headers = graph.discovered_headers()
    graph.source_identities = set(discovered_headers)
    graph.validate_manifest_coverage(discovered_headers)
    content = graph.expand(absolute_input)
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
    conditional = ConditionalState()
    for line in parse_source(content, "generated header"):
        directive = line.directive
        conditional.consume(line, "generated header")
        if (
            line.is_once
            or (directive is not None and directive.include_kind == "quoted")
        ):
            raise AmalgamationError(
                "generated header retains an internal directive at line {0}".format(line.number)
            )
        if directive is not None and directive.include_kind == "angle":
            if graph.is_internal_angle_include(directive.include_path):
                raise AmalgamationError(
                    "generated header retains an internal angle include at line {0}".format(line.number)
                )
    conditional.finish("generated header")
    return b"#pragma once\n\n" + content


def build_project_amalgamation(root, input_path=DEFAULT_INPUT):
    return build_amalgamation(
        root,
        input_path,
        header_manifest=PROJECT_HEADER_MANIFEST,
        role_dependencies=PROJECT_ROLE_DEPENDENCIES,
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
