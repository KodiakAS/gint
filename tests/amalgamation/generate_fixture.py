#!/usr/bin/env python3

"""Generate the compiler fixture through the real graph expander."""

import argparse
import importlib.util
import os


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SPEC = importlib.util.spec_from_file_location(
    "generate_amalgamation", os.path.join(ROOT, "scripts", "generate-amalgamation.py")
)
AMALGAMATION = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(AMALGAMATION)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", required=True)
    args = parser.parse_args()
    fixture = os.path.join(ROOT, "tests", "amalgamation", "fixture")
    content = AMALGAMATION.build_amalgamation(fixture)
    AMALGAMATION.write_output(os.path.abspath(args.output_dir), "gint/gint.hpp", content)


if __name__ == "__main__":
    main()
