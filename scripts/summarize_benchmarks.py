#!/usr/bin/env python3

"""Normalize Google Benchmark JSON into a compact release evidence artifact."""

from __future__ import print_function

import argparse
import json
import os
import statistics
import sys


NOISY_CV_THRESHOLD = 0.05


def parse_assignment(value, option):
    if "=" not in value:
        raise ValueError("{0} must use NAME=VALUE syntax: {1}".format(option, value))
    name, assigned_value = value.split("=", 1)
    if not name or not assigned_value:
        raise ValueError("{0} must use non-empty NAME=VALUE syntax".format(option))
    return name, assigned_value


def summarize_document(document):
    benchmarks = document.get("benchmarks")
    if not isinstance(benchmarks, list) or not benchmarks:
        raise ValueError("benchmark document contains no benchmark rows")

    cvs = {}
    for row in benchmarks:
        if row.get("aggregate_name") == "cv":
            cvs[row["run_name"]] = row.get("real_time")

    medians = []
    seen = set()
    for row in benchmarks:
        if row.get("aggregate_name") != "median":
            continue
        name = row["run_name"]
        if name in seen:
            raise ValueError("duplicate median row: {0}".format(name))
        seen.add(name)
        medians.append(
            {
                "name": name,
                "median_real_time": row["real_time"],
                "median_cpu_time": row["cpu_time"],
                "time_unit": row["time_unit"],
                "repetitions": row.get("repetitions"),
                "real_time_cv": cvs.get(name),
            }
        )

    if not medians:
        raise ValueError("benchmark document contains no median aggregate rows")

    cv_values = [row["real_time_cv"] for row in medians if row["real_time_cv"] is not None]
    noisy = sorted(
        (
            {"name": row["name"], "real_time_cv": row["real_time_cv"]}
            for row in medians
            if row["real_time_cv"] is not None
            and row["real_time_cv"] > NOISY_CV_THRESHOLD
        ),
        key=lambda row: row["real_time_cv"],
        reverse=True,
    )

    return {
        "context": document.get("context", {}),
        "median_count": len(medians),
        "median_real_time_cv": statistics.median(cv_values) if cv_values else None,
        "max_real_time_cv": max(cv_values) if cv_values else None,
        "noisy_threshold": NOISY_CV_THRESHOLD,
        "noisy_count": len(noisy),
        "noisy_benchmarks": noisy,
        "benchmarks": medians,
    }


def percent(value):
    return "n/a" if value is None else "{0:.2f}%".format(value * 100.0)


def validate_suite(
    name,
    suite,
    expected_library_version=None,
    expected_count=None,
    expected_repetitions=None,
    required=(),
):
    if expected_library_version is not None:
        actual_version = suite["context"].get("library_version")
        if actual_version != expected_library_version:
            raise ValueError(
                "{0}: Google Benchmark version is {1!r}, expected {2!r}".format(
                    name, actual_version, expected_library_version
                )
            )
    if expected_count is not None and suite["median_count"] != expected_count:
        raise ValueError(
            "{0}: median count is {1}, expected {2}".format(
                name, suite["median_count"], expected_count
            )
        )
    if expected_repetitions is not None:
        wrong_repetitions = [
            row["name"]
            for row in suite["benchmarks"]
            if row["repetitions"] != expected_repetitions
        ]
        if wrong_repetitions:
            raise ValueError(
                "{0}: benchmark(s) do not report repetitions={1}: {2}".format(
                    name, expected_repetitions, ", ".join(wrong_repetitions)
                )
            )
        missing_cv = [
            row["name"]
            for row in suite["benchmarks"]
            if not isinstance(row["real_time_cv"], (int, float))
        ]
        if missing_cv:
            raise ValueError(
                "{0}: benchmark(s) are missing numeric CV rows: {1}".format(
                    name, ", ".join(missing_cv)
                )
            )
    names = set(row["name"] for row in suite["benchmarks"])
    missing = sorted(set(required) - names)
    if missing:
        raise ValueError("{0}: missing required benchmark(s): {1}".format(name, ", ".join(missing)))


def render_markdown(suites, metadata):
    lines = ["## 性能采样", ""]
    if metadata:
        lines.append(
            "- " + ", ".join("{0}: `{1}`".format(key, value) for key, value in sorted(metadata.items()))
        )
        lines.append("")
    lines.extend(
        [
            "| 矩阵 | median 行数 | median CV | max CV | 高噪声行数 (>5%) |",
            "| --- | ---: | ---: | ---: | ---: |",
        ]
    )
    for name, suite in sorted(suites.items()):
        lines.append(
            "| {0} | {1} | {2} | {3} | {4} |".format(
                name,
                suite["median_count"],
                percent(suite["median_real_time_cv"]),
                percent(suite["max_real_time_cv"]),
                suite["noisy_count"],
            )
        )
    for name, suite in sorted(suites.items()):
        if not suite["noisy_benchmarks"]:
            continue
        lines.extend(["", "- `{0}` 高噪声用例：".format(name)])
        for row in suite["noisy_benchmarks"][:10]:
            lines.append("  - `{0}`: {1}".format(row["name"], percent(row["real_time_cv"])))
        remaining = len(suite["noisy_benchmarks"]) - 10
        if remaining > 0:
            lines.append("  - 其余 {0} 项见规范化 JSON。".format(remaining))
    lines.extend(
        [
            "",
            "共享 runner 的 wall-clock 结果仅作为采样证据，不作为阻断式回归阈值。",
            "",
        ]
    )
    return "\n".join(lines)


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--input",
        action="append",
        required=True,
        metavar="NAME=JSON",
        help="labeled Google Benchmark JSON input; may be repeated",
    )
    parser.add_argument(
        "--metadata",
        action="append",
        default=[],
        metavar="NAME=VALUE",
        help="metadata field to include; may be repeated",
    )
    parser.add_argument(
        "--expected-library-version",
        help="required Google Benchmark context.library_version for every input",
    )
    parser.add_argument(
        "--expected-repetitions",
        type=int,
        help="required repetitions value and numeric CV row for every median",
    )
    parser.add_argument(
        "--expected-median-count",
        action="append",
        default=[],
        metavar="NAME=COUNT",
        help="required median row count for an input label; may be repeated",
    )
    parser.add_argument(
        "--require-benchmark",
        action="append",
        default=[],
        metavar="NAME=BENCHMARK",
        help="required exact run_name for an input label; may be repeated",
    )
    parser.add_argument("--output", required=True, help="normalized JSON output")
    parser.add_argument("--markdown", help="optional Markdown summary output")
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    try:
        inputs = [parse_assignment(value, "--input") for value in args.input]
        metadata = dict(parse_assignment(value, "--metadata") for value in args.metadata)
        expected_counts = dict(
            (name, int(value))
            for name, value in (
                parse_assignment(item, "--expected-median-count")
                for item in args.expected_median_count
            )
        )
        required_benchmarks = {}
        for name, benchmark in (
            parse_assignment(item, "--require-benchmark")
            for item in args.require_benchmark
        ):
            required_benchmarks.setdefault(name, []).append(benchmark)
        suites = {}
        for name, path in inputs:
            if name in suites:
                raise ValueError("duplicate input label: {0}".format(name))
            with open(path, "r") as input_file:
                suites[name] = summarize_document(json.load(input_file))
        contract_labels = set(expected_counts) | set(required_benchmarks)
        unknown_labels = sorted(contract_labels - set(suites))
        if unknown_labels:
            raise ValueError(
                "benchmark contract references unknown input label(s): {0}".format(
                    ", ".join(unknown_labels)
                )
            )
        for name, suite in suites.items():
            validate_suite(
                name,
                suite,
                expected_library_version=args.expected_library_version,
                expected_count=expected_counts.get(name),
                expected_repetitions=args.expected_repetitions,
                required=required_benchmarks.get(name, []),
            )
    except (IOError, KeyError, TypeError, ValueError) as error:
        print("error: {0}".format(error), file=sys.stderr)
        return 2

    report = {
        "schema_version": 1,
        "metadata": metadata,
        "suites": suites,
    }
    output_dir = os.path.dirname(args.output)
    if output_dir and not os.path.isdir(output_dir):
        os.makedirs(output_dir)
    with open(args.output, "w") as output_file:
        json.dump(report, output_file, indent=2, sort_keys=True)
        output_file.write("\n")

    markdown = render_markdown(suites, metadata)
    if args.markdown:
        markdown_dir = os.path.dirname(args.markdown)
        if markdown_dir and not os.path.isdir(markdown_dir):
            os.makedirs(markdown_dir)
        with open(args.markdown, "w") as markdown_file:
            markdown_file.write(markdown)
    print(markdown)
    return 0


if __name__ == "__main__":
    sys.exit(main())
