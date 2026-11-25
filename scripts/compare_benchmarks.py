#!/usr/bin/env python3

"""Compare raw Google Benchmark repetitions for one suite, width, and environment."""

import argparse
import itertools
import json
import math
import os
import shlex
import statistics
import sys

from summarize_benchmarks import (
    NOISY_CV_THRESHOLD,
    is_finite_nonnegative_number,
    validate_repetition_rows,
    validate_status_flags,
)


METRICS = ("real_time", "cpu_time")
NS_PER_UNIT = {"ns": 1, "us": 1000, "ms": 1000000, "s": 1000000000}
META_TEXT_FIELDS = ("commit", "suite", "host", "cpu", "virtualization", "os", "arch", "compiler", "harness", "filter")
CONTEXT_HOST_FIELDS = ("host_name", "num_cpus", "caches", "cpu_scaling_enabled", "aslr_enabled")
MAX_EXACT_ASSIGNMENTS = 50000


def read_json(path):
    with open(path, encoding="utf-8") as source:
        document = json.load(source)
    if not isinstance(document, dict):
        raise ValueError("{0}: expected a JSON object".format(path))
    return document


def nonempty_text(value):
    return isinstance(value, str) and bool(value.strip())


def read_samples(document, label):
    context = document.get("context")
    if not isinstance(context, dict) or not nonempty_text(context.get("library_version")):
        raise ValueError("{0}: missing or invalid context.library_version".format(label))
    rows = document.get("benchmarks")
    if not isinstance(rows, list) or not rows:
        raise ValueError("{0}: no benchmark rows".format(label))

    names = set()
    iterations = {}
    for row in rows:
        if not isinstance(row, dict) or not nonempty_text(row.get("run_name")):
            raise ValueError("{0}: row without a valid run_name".format(label))
        name = row["run_name"]
        location = "{0}: {1}".format(label, name)
        names.add(name)
        run_type = row.get("run_type")
        if run_type == "iteration":
            iterations.setdefault(name, []).append(row)
        elif run_type == "aggregate":
            validate_status_flags(row, location)
        else:
            raise ValueError("{0}: invalid run_type={1!r}".format(location, run_type))

    missing = sorted(names - set(iterations))
    if missing:
        raise ValueError("{0}: no iteration rows for {1}".format(label, ", ".join(missing)))

    samples = {}
    for name, group in sorted(iterations.items()):
        repetitions = group[0].get("repetitions")
        if type(repetitions) is not int or repetitions < 1 or any(
            type(row.get("repetitions")) is not int or row["repetitions"] != repetitions
            for row in group
        ):
            raise ValueError("{0}: {1} has invalid or inconsistent repetitions".format(label, name))
        validate_repetition_rows(label, [name], group, repetitions)

        times = {metric: [] for metric in METRICS}
        for row in sorted(group, key=lambda item: item["repetition_index"]):
            unit = row.get("time_unit")
            if not isinstance(unit, str) or unit not in NS_PER_UNIT:
                raise ValueError("{0}: {1} has unknown time_unit={2!r}".format(label, name, unit))
            for metric in METRICS:
                time_ns = float(row[metric]) * NS_PER_UNIT[unit]
                if not is_finite_nonnegative_number(time_ns) or time_ns == 0:
                    raise ValueError("{0}: {1} has invalid {2} in ns".format(label, name, metric))
                times[metric].append(time_ns)
        samples[name] = times
    return context, samples


def read_metadata(path, label):
    if not path:
        return None, ["{0} metadata is missing".format(label)]
    try:
        meta = read_json(path)
        json.dumps(meta, allow_nan=False)
    except (OSError, ValueError) as error:
        raise ValueError("{0} metadata: {1}".format(label, error)) from error

    invalid = [field for field in META_TEXT_FIELDS if not nonempty_text(meta.get(field))]
    if type(meta.get("bits")) is not int or meta["bits"] <= 0:
        invalid.append("bits")
    flags = meta.get("flags")
    if not nonempty_text(flags) and not isinstance(flags, (list, dict)):
        invalid.append("flags")
    sampling = meta.get("sampling")
    if not nonempty_text(sampling) and not (isinstance(sampling, (list, dict)) and sampling):
        invalid.append("sampling")
    if invalid:
        return meta, ["{0} metadata has missing or invalid fields: {1}".format(label, ", ".join(invalid))]
    return meta, []


def check_metadata(args):
    baseline, baseline_issues = read_metadata(args.baseline_meta, "baseline")
    result, result_issues = read_metadata(args.result_meta, "result")
    issues = baseline_issues + result_issues
    mismatches = []
    if baseline is not None and result is not None:
        for key in sorted((set(baseline) | set(result)) - {"commit"}):
            if key not in baseline or key not in result or (
                json.dumps(baseline[key], sort_keys=True) != json.dumps(result[key], sort_keys=True)
            ):
                mismatches.append(key)
        if mismatches:
            issues.append("metadata differs in: {0}".format(", ".join(mismatches)))
    return {
        "baseline": baseline,
        "result": result,
        "verified": not issues,
        "issues": issues,
        "mismatches": mismatches,
    }


def exact_assignment_count(total, group_size):
    """Return the number of label assignments, or None above the exact-test limit."""
    count = 1
    for size in range(1, min(group_size, total - group_size) + 1):
        count = count * (total - size + 1) // size
        if count > MAX_EXACT_ASSIGNMENTS:
            return None
    return count


def mann_whitney(baseline, result):
    """Return a two-sided, tie-aware p-value and its method."""
    n1, n2 = len(baseline), len(result)
    total = n1 + n2
    ranks = []
    observed = 0
    tie_term = 0
    start = 0
    pooled = sorted([(value, True) for value in baseline] + [(value, False) for value in result])
    for _, group in itertools.groupby(pooled, key=lambda sample: sample[0]):
        labels = [is_baseline for _, is_baseline in group]
        size = len(labels)
        # Use twice the midrank so the exact tail comparison stays integral.
        rank = 2 * start + size + 1
        ranks.extend([rank] * size)
        observed += rank * sum(labels)
        tie_term += size ** 3 - size
        start += size

    count = exact_assignment_count(total, n1)
    if count is not None:
        # Either group's rank sum gives the same distance from its expectation.
        group_size = min(n1, n2)
        expected = group_size * (total + 1)
        distance = abs(observed - n1 * (total + 1))
        if distance == 0:
            return 1.0, "exact"
        tail = sum(
            abs(sum(assignment) - expected) >= distance
            for assignment in itertools.combinations(ranks, group_size)
        )
        return tail / count, "exact"

    variance = n1 * n2 / 12.0 * (total + 1 - tie_term / (total * (total - 1)))
    if variance == 0:
        return 1.0, "asymptotic"
    distance = abs(observed - n1 * (total + 1)) / 2.0
    return math.erfc(max(0.0, distance - 0.5) / math.sqrt(2 * variance)), "asymptotic"


def sample_cv(values):
    if len(values) < 2:
        return None
    # Binary scaling keeps variance in range without rounding ordinary samples.
    _, exponent = math.frexp(max(values))
    scaled = [math.ldexp(value, -exponent) for value in values]
    return statistics.stdev(scaled) / statistics.mean(scaled)


def compare_case(name, baseline, result, args):
    n1, n2 = len(baseline[args.metric]), len(result[args.metric])
    measurements = {}
    for metric in METRICS:
        median_a = statistics.median(baseline[metric])
        median_b = statistics.median(result[metric])
        delta = median_b - median_a
        change = delta / median_a * 100
        if not all(math.isfinite(value) for value in (median_a, median_b, delta, change)):
            raise ValueError("{0}: {1} comparison exceeds numeric range".format(name, metric))
        measurements[metric] = {
            "baseline_median_ns": median_a,
            "result_median_ns": median_b,
            "delta_ns": delta,
            "change_pct": change,
        }

    cv_a = sample_cv(baseline[args.metric])
    cv_b = sample_cv(result[args.metric])
    p, method = mann_whitney(baseline[args.metric], result[args.metric])
    reasons = []
    if min(n1, n2) < args.min_repetitions:
        reasons.append("insufficient repetitions")
    if cv_a is None or cv_b is None:
        reasons.append("sample CV unavailable")
    if any(cv is not None and cv > args.max_cv_pct / 100 for cv in (cv_a, cv_b)):
        reasons.append("high CV")
    if p > args.alpha:
        reasons.append("p above alpha")

    primary = measurements[args.metric]
    ratio = primary["result_median_ns"] / primary["baseline_median_ns"]
    effect = args.regression_pct / 100
    status = "neutral"
    if ratio > 1 + effect:
        status = "retest" if reasons else "regression"
    elif ratio < 1 - effect and not reasons:
        status = "improvement"
    return {
        "name": name,
        "baseline_n": n1,
        "result_n": n2,
        "baseline_cv_pct": None if cv_a is None else cv_a * 100,
        "result_cv_pct": None if cv_b is None else cv_b * 100,
        "measurements": measurements,
        "p": p,
        "method": method,
        "status": status,
        "reasons": reasons,
    }


def canonical_value(value):
    return json.dumps(value, sort_keys=True, allow_nan=False)


def compare_contexts(baseline_context, result_context):
    mismatches = []
    for field in CONTEXT_HOST_FIELDS:
        in_baseline = field in baseline_context and baseline_context[field] is not None
        in_result = field in result_context and result_context[field] is not None
        if not in_baseline and not in_result:
            continue
        if in_baseline != in_result or (
            canonical_value(baseline_context[field]) != canonical_value(result_context[field])
        ):
            mismatches.append(field)
    return mismatches


def tokenize_sampling(value):
    if isinstance(value, str):
        try:
            return shlex.split(value)
        except ValueError:
            return value.split()
    if isinstance(value, list):
        return value
    return []


def parse_repetitions_tokens(tokens):
    values = []
    index = 0
    while index < len(tokens):
        token = tokens[index]
        if not isinstance(token, str):
            index += 1
            continue
        raw = None
        if token.startswith("--benchmark_repetitions="):
            raw = token.split("=", 1)[1]
        elif token == "--benchmark_repetitions":
            if index + 1 >= len(tokens) or not isinstance(tokens[index + 1], str):
                return None, "missing value for --benchmark_repetitions"
            raw = tokens[index + 1]
            index += 1
        if raw is not None:
            try:
                parsed = int(raw, 10)
            except ValueError:
                return None, "invalid --benchmark_repetitions value {0!r}".format(raw)
            if parsed < 1:
                return None, "invalid --benchmark_repetitions value {0!r}".format(raw)
            values.append(parsed)
        index += 1
    if not values:
        return None, None
    if len(set(values)) > 1:
        return None, "conflicting --benchmark_repetitions values"
    return values[0], None


def extract_declared_repetitions(sampling):
    found = []
    if isinstance(sampling, dict):
        if "repetitions" in sampling:
            rep = sampling["repetitions"]
            if type(rep) is not int or rep < 1:
                return None, "invalid sampling.repetitions"
            found.append(rep)
        for key in ("options", "args"):
            if key in sampling:
                parsed, error = parse_repetitions_tokens(tokenize_sampling(sampling[key]))
                if error:
                    return None, error
                if parsed is not None:
                    found.append(parsed)
    elif isinstance(sampling, (list, str)):
        parsed, error = parse_repetitions_tokens(tokenize_sampling(sampling))
        if error:
            return None, error
        if parsed is not None:
            found.append(parsed)
    if not found:
        return None, None
    if len(set(found)) > 1:
        return None, "conflicting sampling repetition declarations"
    return found[0], None


def format_name_sample(names, limit=5):
    if len(names) <= limit:
        return ", ".join(names)
    return "{0} (+{1} more)".format(", ".join(names[:limit]), len(names) - limit)


def build_report(args, baseline_context, baseline, result_context, result, metadata):
    version = baseline_context["library_version"]
    if result_context["library_version"] != version:
        raise ValueError("Google Benchmark library versions differ")
    if args.expected_library_version is not None and version != args.expected_library_version:
        raise ValueError("Google Benchmark version is {0!r}, expected {1!r}".format(
            version, args.expected_library_version
        ))

    metadata = {
        "baseline": metadata["baseline"],
        "result": metadata["result"],
        "verified": metadata["verified"],
        "issues": list(metadata["issues"]),
        "mismatches": list(metadata["mismatches"]),
    }

    env_issues = []
    context_mismatches = compare_contexts(baseline_context, result_context)
    if context_mismatches:
        env_issues.append("raw context differs in: {0}".format(", ".join(context_mismatches)))

    for label, meta_doc, samples in (
        ("baseline", metadata["baseline"], baseline),
        ("result", metadata["result"], result),
    ):
        if meta_doc is None or "sampling" not in meta_doc:
            continue
        declared, error = extract_declared_repetitions(meta_doc["sampling"])
        if error:
            env_issues.append("{0} metadata: {1}".format(label, error))
        elif declared is not None:
            mismatched = [
                name for name in sorted(samples)
                if len(samples[name][METRICS[0]]) != declared
            ]
            if mismatched:
                env_issues.append(
                    "{0}: repetition count does not match metadata sampling ({1}) for: {2}".format(
                        label, declared, format_name_sample(mismatched)
                    )
                )

    same_scope = not set(metadata["mismatches"]).intersection(("suite", "bits"))
    common = set(baseline).intersection(result) if same_scope else set()
    added = sorted(set(result) - common)
    removed = sorted(set(baseline) - common)
    rows = [compare_case(name, baseline[name], result[name], args) for name in sorted(common)]

    unequal_reps = [row["name"] for row in rows if row["baseline_n"] != row["result_n"]]
    if unequal_reps:
        env_issues.append(
            "repetition counts differ between baseline and result for: {0}".format(
                format_name_sample(unequal_reps)
            )
        )

    if env_issues:
        metadata["issues"].extend(env_issues)
        metadata["verified"] = False

    issues = list(metadata["issues"])
    for label, context in (("baseline", baseline_context), ("result", result_context)):
        if context.get("library_build_type") != "release":
            issues.append("{0}: expected context.library_build_type=release".format(label))
    if added or removed:
        issues.append("benchmark sets differ")

    primary = [row["measurements"][args.metric] for row in rows]
    geomean = None
    worst = None
    if rows:
        geomean = math.exp(math.fsum(
            math.log(row["result_median_ns"]) - math.log(row["baseline_median_ns"])
            for row in primary
        ) / len(primary))
        if not (math.isfinite(geomean) and geomean > 0.0):
            raise ValueError("geometric mean exceeds numeric range")
        worst = max(rows, key=lambda row: row["measurements"][args.metric]["change_pct"])["name"]

    counts = {status: 0 for status in ("regression", "retest", "improvement", "neutral")}
    for row in rows:
        counts[row["status"]] += 1
    if counts["retest"]:
        issues.append("unresolved regression candidates require retest")
    exit_code = 0
    if args.fail_on_regression:
        if issues:
            exit_code = 2
        elif counts["regression"]:
            exit_code = 1

    return {
        "schema_version": 1,
        "parameters": {
            "metric": args.metric,
            "regression_pct": args.regression_pct,
            "max_cv_pct": args.max_cv_pct,
            "alpha": args.alpha,
            "min_repetitions": args.min_repetitions,
            "expected_library_version": args.expected_library_version,
            "fail_on_regression": args.fail_on_regression,
        },
        "metadata": metadata,
        "library_version": version,
        "library_build_type": {
            "baseline": baseline_context.get("library_build_type"),
            "result": result_context.get("library_build_type"),
        },
        "coverage": {"common_count": len(rows), "added": added, "removed": removed},
        "summary": {"geomean_ratio": geomean, "worst": worst, "counts": counts},
        "benchmarks": rows,
        "issues": issues,
        "exit_code": exit_code,
    }


def render_markdown(report):
    metric = report["parameters"]["metric"]
    coverage = report["coverage"]
    summary = report["summary"]
    rows = report["benchmarks"]
    lines = ["## Benchmark comparison", ""]
    if summary["worst"] is not None:
        worst = next(row for row in rows if row["name"] == summary["worst"])
        time = worst["measurements"][metric]
        lines.append("- Worst ({0}): `{1}` {2:+.3f} ns/op ({3:+.2f}%).".format(
            metric, worst["name"], time["delta_ns"], time["change_pct"]
        ))
        lines.append("- Geometric mean: {0:.6g}× across {1} common benchmarks.".format(
            summary["geomean_ratio"], coverage["common_count"]
        ))
    else:
        lines.append("- No common benchmarks.")
    lines.append("- Environment metadata: {0}.".format("verified" if report["metadata"]["verified"] else "unverified"))
    lines.append("- Added: {0}. Removed: {1}.".format(
        ", ".join(coverage["added"]) or "none", ", ".join(coverage["removed"]) or "none"
    ))
    for row in rows:
        if row["status"] in ("regression", "retest"):
            time = row["measurements"][metric]
            reason = ", ".join(row["reasons"]) or "effect, CV, sample count and significance meet thresholds"
            lines.append("- {0}: `{1}` {2:+.3f} ns/op ({3:+.2f}%), {4}.".format(
                row["status"], row["name"], time["delta_ns"], time["change_pct"], reason
            ))
    lines.extend("- {0}.".format(issue) for issue in report["issues"])
    params = report["parameters"]
    lines.extend([
        "",
        "Thresholds: effect > {regression_pct:g}%, CV ≤ {max_cv_pct:g}%, p ≤ {alpha:g}, "
        "minimum repetitions {min_repetitions:d} per side.".format(**params),
        "",
        "| Benchmark | Wall median A → B (ns/op) | Wall Δ ns/op (%) | CPU median A → B (ns/op) | CPU Δ ns/op (%) | n A / B | CV A / B | p (method) | Status |",
        "| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | --- |",
    ])
    for row in rows:
        cells = ["`{0}`".format(row["name"].replace("|", "\\|"))]
        for field in METRICS:
            time = row["measurements"][field]
            cells.extend([
                "{baseline_median_ns:.3f} → {result_median_ns:.3f}".format(**time),
                "{delta_ns:+.3f} ({change_pct:+.2f}%)".format(**time),
            ])
        cvs = (row["baseline_cv_pct"], row["result_cv_pct"])
        cells.extend([
            "{baseline_n} / {result_n}".format(**row),
            " / ".join("n/a" if cv is None else "{0:.2f}%".format(cv) for cv in cvs),
            "{p:.6g} ({method})".format(**row),
            row["status"],
        ])
        lines.append("| " + " | ".join(cells) + " |")
    return "\n".join(lines) + "\n"


def parse_args(argv):
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument("--baseline", required=True, help="baseline raw JSON")
    parser.add_argument("--result", required=True, help="result raw JSON")
    parser.add_argument("--baseline-meta", help="baseline environment JSON sidecar")
    parser.add_argument("--result-meta", help="result environment JSON sidecar")
    parser.add_argument("--expected-library-version", help="required context.library_version")
    parser.add_argument("--metric", choices=METRICS, default="real_time", help="primary metric")
    parser.add_argument("--regression-pct", type=float, default=8.0, help="effect threshold in percent")
    parser.add_argument("--max-cv-pct", type=float, default=NOISY_CV_THRESHOLD * 100, help="maximum sample CV in percent")
    parser.add_argument("--alpha", type=float, default=0.05, help="maximum two-sided p-value")
    parser.add_argument("--min-repetitions", type=int, default=5, help="minimum repetitions per side")
    parser.add_argument("--markdown", help="write Markdown report (also printed to stdout)")
    parser.add_argument("--json", help="write versioned JSON report")
    parser.add_argument("--fail-on-regression", action="store_true", help="gate on complete, verified comparisons")
    args = parser.parse_args(argv)
    if not is_finite_nonnegative_number(args.regression_pct) or not is_finite_nonnegative_number(args.max_cv_pct):
        parser.error("effect and CV thresholds must be finite and nonnegative")
    if not 0 < args.alpha <= 1 or args.min_repetitions < 1:
        parser.error("alpha must be in (0, 1] and min-repetitions must be positive")
    return args


def write_report(path, contents):
    directory = os.path.dirname(path)
    if directory:
        os.makedirs(directory, exist_ok=True)
    with open(path, "w", encoding="utf-8") as output:
        output.write(contents)


def validate_output_paths(args):
    paths = [path for path in (args.baseline, args.result, args.baseline_meta, args.result_meta) if path]
    for output in (args.markdown, args.json):
        if not output:
            continue
        for path in paths:
            if os.path.realpath(output) == os.path.realpath(path) or (
                os.path.exists(output) and os.path.exists(path) and os.path.samefile(output, path)
            ):
                raise ValueError("report path must be distinct from input and other output paths: {0}".format(output))
        paths.append(output)


def main(argv=None):
    args = parse_args(argv)
    try:
        validate_output_paths(args)
        baseline_context, baseline = read_samples(read_json(args.baseline), "baseline")
        result_context, result = read_samples(read_json(args.result), "result")
        report = build_report(args, baseline_context, baseline, result_context, result, check_metadata(args))
        markdown = render_markdown(report)
        if args.json:
            write_report(args.json, json.dumps(report, indent=2, sort_keys=True, allow_nan=False) + "\n")
        if args.markdown:
            write_report(args.markdown, markdown)
        print(markdown, end="")
        return report["exit_code"]
    except (OSError, ValueError, OverflowError) as error:
        print("error: {0}".format(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
