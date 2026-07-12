#!/usr/bin/env python3

"""Generate or verify gint's committed single-header distribution.

The source model follows the established header-graph/include-expansion pattern
used by projects such as nlohmann/json. This implementation is intentionally
small and project-specific; it does not copy third-party generator code.
"""

from __future__ import print_function

import argparse
import hashlib
import os
import re
import stat
import sys
import tempfile


DEFAULT_INPUT = "src/gint/gint.hpp"
DEFAULT_OUTPUT = "include/gint/gint.h"
SOURCE_DIRECTORY = os.path.join("src", "gint")

INCLUDE_RE = re.compile(
    br'^[ \t\v\f]*#[ \t\v\f]*include[ \t\v\f]*"([^"\r\n]+)"[ \t\v\f]*(?://[^\r\n]*)?\n$'
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
    """Expand a normal internal C++ header graph into one deterministic file."""

    def __init__(self, root, source_directory=SOURCE_DIRECTORY):
        self.root = os.path.abspath(root)
        self.source_directory = os.path.abspath(os.path.join(self.root, source_directory))
        self.reject_symlink_components(self.root, self.source_directory, "source directory")
        self.expanded_identities = set()
        self.active = []

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

    def is_internal_angle_include(self, include_path):
        return include_path.startswith("gint/") or os.path.isfile(
            os.path.join(self.source_directory, include_path)
        )

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
            first_content_line = next(
                (index for index, group in enumerate(groups) if group[1].strip()), None
            )
            if pragma_lines != [first_content_line]:
                raise AmalgamationError(
                    "internal header must begin with exactly one canonical #pragma once: {0}".format(
                        relative_path
                    )
                )

            conditional_depth = 0
            for line_number, raw_line, logical_line, was_spliced in groups:
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
                    dependency = os.path.abspath(os.path.join(os.path.dirname(absolute_path), include_path))
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
                    if self.is_internal_angle_include(angle_include_path):
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


def build_amalgamation(root, input_path=DEFAULT_INPUT):
    input_path = normalized_relative_path(input_path, "input path")
    graph = HeaderGraph(root)
    absolute_input = graph.validate_source_path(os.path.join(root, input_path), "input path")
    content = graph.expand(absolute_input)
    discovered_headers = graph.discovered_headers()
    unreachable = sorted(
        path
        for identity, path in discovered_headers.items()
        if identity not in graph.expanded_identities
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
        if PRAGMA_ONCE_LIKE_RE.match(logical_line) or QUOTED_INCLUDE_RE.match(logical_line):
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
    except IOError:
        actual = b""
    if actual == expected:
        print("amalgamated header is current: {0} ({1})".format(output_path, sha256(expected)))
        return True

    offset = first_difference(expected, actual)
    print("error: amalgamated header is stale: {0}".format(output_path), file=sys.stderr)
    print("error: expected sha256 {0}".format(sha256(expected)), file=sys.stderr)
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
        content = build_amalgamation(root, args.input)
        if args.check:
            return 0 if check_output(root, args.output, content) else 1
        write_output(root, args.output, content)
        return 0
    except (AmalgamationError, OSError) as error:
        print("error: {0}".format(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
