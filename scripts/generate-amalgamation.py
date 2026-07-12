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
import sys
import tempfile


DEFAULT_INPUT = "src/gint/gint.hpp"
DEFAULT_OUTPUT = "include/gint/gint.h"
SOURCE_DIRECTORY = os.path.join("src", "gint")

INCLUDE_RE = re.compile(
    br'^[ \t]*#[ \t]*include[ \t]*"([^"\r\n]+)"[ \t]*(?://[^\r\n]*)?\n$'
)
PRAGMA_ONCE_RE = re.compile(br"^[ \t]*#[ \t]*pragma[ \t]+once[ \t]*\n$")
QUOTED_INCLUDE_RE = re.compile(br'^[ \t]*#[ \t]*include[ \t]*"')


class AmalgamationError(ValueError):
    pass


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
        self.expanded = set()
        self.active = []

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
        if os.path.islink(absolute_path):
            raise AmalgamationError(
                "{0} must not be a symbolic link: {1}".format(
                    description, relative_to_root(self.root, absolute_path)
                )
            )
        if not os.path.isfile(absolute_path):
            raise AmalgamationError(
                "{0} does not exist: {1}".format(description, relative_to_root(self.root, absolute_path))
            )
        return absolute_path

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
        if not content.endswith(b"\n"):
            raise AmalgamationError("internal header must end with a newline: {0}".format(relative_path))
        return content

    def expand(self, absolute_path):
        absolute_path = self.validate_source_path(absolute_path, "internal include")
        if absolute_path in self.active:
            cycle = self.active[self.active.index(absolute_path) :] + [absolute_path]
            raise AmalgamationError(
                "internal header include cycle: {0}".format(
                    " -> ".join(relative_to_root(self.root, path) for path in cycle)
                )
            )
        if absolute_path in self.expanded:
            return b""

        self.active.append(absolute_path)
        self.expanded.add(absolute_path)
        output = []
        skip_boundary_blank = False
        try:
            for line_number, line in enumerate(self.read_header(absolute_path).splitlines(True), 1):
                if skip_boundary_blank:
                    skip_boundary_blank = False
                    if line == b"\n":
                        continue
                if PRAGMA_ONCE_RE.match(line):
                    skip_boundary_blank = True
                    continue
                include_match = INCLUDE_RE.match(line)
                if include_match:
                    try:
                        include_path = include_match.group(1).decode("utf-8")
                    except UnicodeDecodeError:
                        raise AmalgamationError(
                            "internal include path is not UTF-8 at {0}:{1}".format(
                                relative_to_root(self.root, absolute_path), line_number
                            )
                        )
                    dependency = os.path.abspath(os.path.join(os.path.dirname(absolute_path), include_path))
                    expanded_dependency = self.expand(dependency)
                    output.append(expanded_dependency)
                    continue
                if QUOTED_INCLUDE_RE.match(line):
                    raise AmalgamationError(
                        "unsupported internal include syntax at {0}:{1}".format(
                            relative_to_root(self.root, absolute_path), line_number
                        )
                    )
                output.append(line)
        finally:
            self.active.pop()
        return b"".join(output)

    def discovered_headers(self):
        headers = set()
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
                headers.add(absolute_path)
        return headers


def build_amalgamation(root, input_path=DEFAULT_INPUT):
    input_path = normalized_relative_path(input_path, "input path")
    graph = HeaderGraph(root)
    absolute_input = graph.validate_source_path(os.path.join(root, input_path), "input path")
    content = graph.expand(absolute_input)
    unreachable = sorted(graph.discovered_headers() - graph.expanded)
    if unreachable:
        raise AmalgamationError(
            "internal header(s) are unreachable from {0}: {1}".format(
                input_path, ", ".join(relative_to_root(root, path) for path in unreachable)
            )
        )
    # The internal entry header starts with #pragma once. Removing it leaves
    # leading blank lines that have no value in the distribution header.
    return content.lstrip(b"\n")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def first_difference(expected, actual):
    limit = min(len(expected), len(actual))
    for offset in range(limit):
        if expected[offset] != actual[offset]:
            return offset
    return limit


def check_output(root, output_path, expected):
    absolute_output = os.path.join(root, normalized_relative_path(output_path, "output path"))
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
    output_path = normalized_relative_path(output_path, "output path")
    absolute_output = os.path.join(root, output_path)
    output_dir = os.path.dirname(absolute_output)
    if not os.path.isdir(output_dir):
        os.makedirs(output_dir)

    current_mode = 0o644
    if os.path.exists(absolute_output):
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
            os.fsync(temporary_file.fileno())
        os.chmod(temporary, current_mode)
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
