#!/usr/bin/env python3

import contextlib
import copy
import io
import json
import math
import os
import sys
import tempfile
import unittest


SCRIPTS = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "scripts"))
sys.path.insert(0, SCRIPTS)
try:
    import compare_benchmarks as compare
finally:
    sys.path.pop(0)


NAME = "ToString/Base10/gint"
CONTROL = "Div/NativeU64/gint"
META = {
    "commit": "baseline",
    "suite": "gint",
    "bits": 1024,
    "host": "test-machine",
    "cpu": "test-cpu",
    "virtualization": "bare metal",
    "os": "test-os",
    "arch": "aarch64",
    "compiler": "test-compiler 1.0",
    "flags": ["-O3", "-DNDEBUG"],
    "harness": "benchmark fixture v1",
    "filter": ".*",
    "sampling": {"min_time": "0.2s", "repetitions": 7},
}


def document(cases, unit="ns"):
    rows = []
    for name, wall, cpu in cases:
        for index in reversed(range(len(wall))):
            rows.append({
                "run_name": name,
                "run_type": "iteration",
                "repetitions": len(wall),
                "repetition_index": index,
                "iterations": 100,
                "real_time": wall[index],
                "cpu_time": cpu[index],
                "time_unit": unit,
            })
        # Raw repetitions supply the samples even if aggregates exist.
        rows.append({"run_name": name, "run_type": "aggregate", "aggregate_name": "median"})
    return {
        "context": {"library_version": "v1.9.5", "library_build_type": "release"},
        "benchmarks": rows,
    }


def constant(wall, cpu=None, name=NAME, repetitions=7):
    return name, [wall] * repetitions, [wall if cpu is None else cpu] * repetitions


class MannWhitneyTest(unittest.TestCase):
    def test_exact_seven_per_side_completely_separated(self):
        p_value, method = compare.mann_whitney(list(range(7)), list(range(7, 14)))
        self.assertEqual(method, "exact")
        self.assertEqual(p_value, 2 / 3432)

    def test_all_ties_small_and_large(self):
        for size, method in ((7, "exact"), (15, "asymptotic")):
            self.assertEqual(compare.mann_whitney([1] * size, [1] * size), (1, method))

    def test_exact_mixed_ties(self):
        # Midranks: 1.5, 1.5, 3.5, 3.5, 5.5, 5.5.
        # Two labelings hit the low tail (6.5) and two the high tail (14.5).
        self.assertEqual(compare.mann_whitney([1, 1, 2], [2, 3, 3]), (4 / 20, "exact"))

    def test_unequal_sample_sizes(self):
        self.assertEqual(compare.mann_whitney([0] * 2, [1] * 3), (1 / 10, "exact"))
        self.assertEqual(compare.mann_whitney([1] * 3, [0] * 2), (1 / 10, "exact"))

    def test_asymptotic_tie_and_continuity_corrections(self):
        first = [1] * 10 + [2] * 5
        second = [2] * 5 + [3] * 10
        variance = 15 * 15 / 12 * (31 - 3 * (10 ** 3 - 10) / (30 * 29))
        expected = math.erfc((100 - 0.5) / math.sqrt(2 * variance))
        self.assertEqual(compare.mann_whitney(first, second), (expected, "asymptotic"))
        self.assertEqual(compare.exact_assignment_count(18, 9), 48620)
        self.assertIsNone(compare.exact_assignment_count(19, 9))


class CompareBenchmarksTest(unittest.TestCase):
    def write_json(self, stem, content):
        path = os.path.join(self.tmp.name, stem + ".json")
        with open(path, "w", encoding="utf-8") as output:
            json.dump(content, output)
        return path

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)

    def run_comparison(self, baseline, result, options=(), metadata=(META, dict(META, commit="result"))):
        output = os.path.join(self.tmp.name, "report.json")
        markdown = os.path.join(self.tmp.name, "report.md")
        for path in (output, markdown):
            if os.path.exists(path):
                os.unlink(path)
        args = [
            "--baseline", self.write_json("baseline", baseline),
            "--result", self.write_json("result", result),
            "--markdown", markdown,
            "--json", output,
        ]
        for label, meta in zip(("baseline", "result"), metadata):
            if meta is not None:
                args.extend(["--" + label + "-meta", self.write_json(label + "-meta", meta)])
        stdout = io.StringIO()
        stderr = io.StringIO()
        with contextlib.redirect_stdout(stdout), contextlib.redirect_stderr(stderr):
            code = compare.main(args + list(options))
        if not os.path.isfile(output):
            return code, None, stderr.getvalue()
        with open(output, encoding="utf-8") as source:
            report = json.load(source)
        with open(markdown, encoding="utf-8") as source:
            self.assertEqual(source.read(), stdout.getvalue())
        self.assertEqual(report["exit_code"], code)
        self.assertEqual(report["schema_version"], 1)
        return code, report, stdout.getvalue()

    def test_units_and_identical_samples(self):
        for unit, measurement in (("ns", 1000), ("us", 1), ("ms", 0.001), ("s", 0.000001)):
            with self.subTest(unit=unit):
                code, report, text = self.run_comparison(
                    document([constant(1000)]), document([constant(measurement)], unit),
                    ["--fail-on-regression", "--expected-library-version", "v1.9.5"],
                )
                self.assertEqual(code, 0)
                row = report["benchmarks"][0]
                self.assertEqual(row["status"], "neutral")
                self.assertEqual(row["p"], 1)
                self.assertEqual(row["measurements"]["real_time"]["result_median_ns"], 1000)
                self.assertEqual(row["baseline_n"], 7)
                self.assertEqual(row["baseline_cv_pct"], 0)
                self.assertIn(NAME, text)

    def test_primary_metric_and_direction(self):
        baseline = document([constant(1000, 1000)])
        result = document([constant(1.2, 0.8)], "us")
        code, report, text = self.run_comparison(baseline, result)
        self.assertEqual(code, 0)
        row = report["benchmarks"][0]
        self.assertEqual(row["status"], "regression")
        self.assertEqual(row["measurements"]["real_time"]["delta_ns"], 200)
        self.assertEqual(row["measurements"]["cpu_time"]["change_pct"], -20)
        self.assertIn("regression: `" + NAME, text)
        self.assertIn("+200.000 ns/op", text)
        code, report, _ = self.run_comparison(baseline, result, ["--metric", "cpu_time", "--fail-on-regression"])
        self.assertEqual(code, 0)
        self.assertEqual(report["benchmarks"][0]["status"], "improvement")
        self.assertAlmostEqual(report["summary"]["geomean_ratio"], 0.8)
        code, report, _ = self.run_comparison(result, baseline, ["--metric", "real_time"])
        self.assertEqual(code, 0)
        self.assertEqual(report["benchmarks"][0]["status"], "improvement")

    def test_geometric_mean_cannot_mask_single_regression(self):
        code, report, text = self.run_comparison(
            document([constant(100), constant(100, name=CONTROL)]),
            document([constant(120), constant(50, name=CONTROL)]),
            ["--fail-on-regression"],
        )
        self.assertEqual(code, 1)
        self.assertAlmostEqual(report["summary"]["geomean_ratio"], math.sqrt(0.6))
        self.assertEqual(report["coverage"]["common_count"], 2)
        self.assertEqual(report["summary"]["worst"], NAME)
        self.assertEqual([row["name"] for row in report["benchmarks"]], [CONTROL, NAME])
        self.assertIn("regression: `" + NAME, text)

    def test_threshold_equalities(self):
        for value in (1.08, 0.92):
            code, report, _ = self.run_comparison(document([constant(1)]), document([constant(value)]))
            self.assertEqual(code, 0)
            self.assertEqual(report["benchmarks"][0]["status"], "neutral")

        samples = [95, 100, 105]
        baseline = document([(NAME, samples, samples)])
        result = document([constant(150, repetitions=3)])
        meta_3 = dict(META, sampling={"min_time": "0.2s", "repetitions": 3})
        code, report, _ = self.run_comparison(
            baseline, result,
            ["--min-repetitions", "3", "--alpha", "0.1", "--fail-on-regression"],
            (meta_3, dict(meta_3, commit="result")),
        )
        self.assertEqual(code, 1)
        self.assertAlmostEqual(report["benchmarks"][0]["baseline_cv_pct"], 5.0)
        self.assertEqual(report["benchmarks"][0]["p"], 0.1)

    def test_retest_conditions_take_precedence_over_regression(self):
        meta_min = dict(META, sampling={"min_time": "0.2s"})
        metadata = (meta_min, dict(meta_min, commit="result"))
        for base_reps, values, reason in (
            (7, [110, 110, 110, 150, 150, 150, 150], "high CV"),
            (4, [120] * 4, "insufficient repetitions"),
            (7, [120] * 7, "p above alpha"),
        ):
            with self.subTest(reason=reason):
                params = ["--alpha", "0.0001"] if reason == "p above alpha" else []
                # The control has more samples so it remains significant at the lower alpha.
                baseline = document([constant(100, repetitions=base_reps), constant(100, name=CONTROL, repetitions=10)])
                result = document([(NAME, values, values), constant(150, name=CONTROL, repetitions=10)])
                code, report, text = self.run_comparison(baseline, result, params + ["--fail-on-regression"], metadata)
                self.assertEqual(code, 2)
                self.assertEqual(report["issues"], ["unresolved regression candidates require retest"])
                self.assertEqual(report["summary"]["counts"]["regression"], 1)
                self.assertEqual(report["benchmarks"][1]["status"], "retest")
                self.assertIn(reason, report["benchmarks"][1]["reasons"])
                self.assertIn("retest: `" + NAME, text)
                code, _, _ = self.run_comparison(baseline, result, params, metadata)
                self.assertEqual(code, 0)

    def test_metadata_required_for_gate(self):
        doc = document([constant(100)])
        conflicting = dict(META, compiler="other")
        missing_field = dict(META)
        del missing_field["sampling"]
        for metadata in ((None, META), (META, conflicting), (missing_field, META)):
            with self.subTest(metadata=metadata):
                code, report, text = self.run_comparison(doc, doc, metadata=metadata)
                self.assertEqual(code, 0)
                self.assertFalse(report["metadata"]["verified"])
                self.assertIn("unverified", text)
                code, report, _ = self.run_comparison(doc, doc, ["--fail-on-regression"], metadata)
                self.assertEqual(code, 2)
                self.assertEqual(report["coverage"]["common_count"], 1)

    def test_same_name_in_different_suite_or_width_does_not_pair(self):
        doc = document([constant(100)])
        for other in (dict(META, bits=512), dict(META, suite="other-suite")):
            code, report, _ = self.run_comparison(doc, doc, ["--fail-on-regression"], (META, other))
            self.assertEqual(code, 2)
            self.assertEqual(report["coverage"], {"common_count": 0, "added": [NAME], "removed": [NAME]})

    def test_invalid_supplied_metadata(self):
        doc = document([constant(100)])
        code, report, text = self.run_comparison(doc, doc, metadata=(META, []))
        self.assertEqual(code, 2)
        self.assertIsNone(report)
        self.assertIn("result metadata", text)

    def test_changed_coverage_keeps_common_results(self):
        baseline = document([constant(100), constant(100, name="old")])
        result = document([constant(100), constant(100, name="new")])
        for options, expected in (([], 0), (["--fail-on-regression"], 2)):
            code, report, text = self.run_comparison(baseline, result, options)
            self.assertEqual(code, expected)
            self.assertEqual(report["coverage"], {"common_count": 1, "added": ["new"], "removed": ["old"]})
            self.assertIn("Added: new. Removed: old.", text)

    def test_library_context(self):
        doc = document([constant(100)])
        for version in (None, "", "v1.9.4", False):
            changed = copy.deepcopy(doc)
            changed["context"]["library_version"] = version
            self.assertEqual(self.run_comparison(doc, changed)[0], 2)
        self.assertEqual(self.run_comparison(doc, doc, ["--expected-library-version", "v1.9.4"])[0], 2)
        for build_type in ("debug", None):
            changed = copy.deepcopy(doc)
            changed["context"]["library_build_type"] = build_type
            self.assertEqual(self.run_comparison(doc, changed)[0], 0)
            code, report, _ = self.run_comparison(doc, changed, ["--fail-on-regression"])
            self.assertEqual(code, 2)
            self.assertEqual(len(report["benchmarks"]), 1)

    def test_rejects_invalid_iteration_fields(self):
        doc = document([constant(100)])
        for field, value in (
            ("error_occurred", True), ("skipped", True), ("error_occurred", 0),
            ("real_time", 0), ("real_time", -1), ("cpu_time", float("nan")),
            ("real_time", float("inf")), ("real_time", True), ("time_unit", "ticks"),
            ("time_unit", []), ("repetition_index", 0), ("repetition_index", 7),
            ("repetition_index", False), ("repetitions", 6), ("repetitions", True),
            ("iterations", 0), ("run_type", "unknown"),
        ):
            with self.subTest(field=field, value=value):
                bad = copy.deepcopy(doc)
                bad["benchmarks"][0][field] = value
                code, report, text = self.run_comparison(doc, bad)
                self.assertEqual(code, 2)
                self.assertIsNone(report)
                self.assertIn(NAME, text)

    def test_rejects_missing_iterations_even_with_aggregates(self):
        doc = document([constant(100)])
        for rows in (doc["benchmarks"][1:], doc["benchmarks"][-1:]):
            changed = dict(doc, benchmarks=rows)
            self.assertEqual(self.run_comparison(doc, changed)[0], 2)

    def test_refuses_to_overwrite_inputs(self):
        path = self.write_json("input", document([constant(100)]))
        with contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(compare.main(["--baseline", path, "--result", path, "--json", path]), 2)
        with open(path, encoding="utf-8") as source:
            self.assertIn("benchmarks", json.load(source))

    def test_raw_context_host_identity_conflict_fails_gate(self):
        baseline = document([constant(100)])
        result = document([constant(100)])
        baseline["context"]["host_name"] = "host-a"
        result["context"]["host_name"] = "host-b"
        baseline["context"]["num_cpus"] = 4
        result["context"]["num_cpus"] = 8
        code, report, text = self.run_comparison(baseline, result)
        self.assertEqual(code, 0)
        self.assertFalse(report["metadata"]["verified"])
        self.assertTrue(any("host_name" in issue and "num_cpus" in issue for issue in report["issues"]))
        self.assertIn("unverified", text)
        code, report, _ = self.run_comparison(baseline, result, ["--fail-on-regression"])
        self.assertEqual(code, 2)
        self.assertFalse(report["metadata"]["verified"])

        # Identical raw host context stays verified.
        result["context"]["host_name"] = "host-a"
        result["context"]["num_cpus"] = 4
        code, report, _ = self.run_comparison(baseline, result, ["--fail-on-regression"])
        self.assertEqual(code, 0)
        self.assertTrue(report["metadata"]["verified"])

    def test_unequal_repetitions_and_declared_sampling_mismatch(self):
        baseline = document([constant(100, repetitions=7)])
        result = document([constant(100, repetitions=5)])
        # Report mode still computes statistics for unequal sample sizes.
        code, report, _ = self.run_comparison(baseline, result)
        self.assertEqual(code, 0)
        self.assertFalse(report["metadata"]["verified"])
        self.assertEqual(report["benchmarks"][0]["baseline_n"], 7)
        self.assertEqual(report["benchmarks"][0]["result_n"], 5)
        self.assertTrue(any("differ between baseline and result" in issue for issue in report["issues"]))
        self.assertTrue(any("does not match metadata sampling (7)" in issue for issue in report["issues"]))
        # Gate mode exits 2.
        code, _, _ = self.run_comparison(baseline, result, ["--fail-on-regression"])
        self.assertEqual(code, 2)

        # Even without declared repetitions in metadata, unequal raw repetitions fail the gate.
        meta_plain = dict(META, sampling={"min_time": "0.2s"})
        code, report, _ = self.run_comparison(
            baseline, result, ["--fail-on-regression"], (meta_plain, dict(meta_plain, commit="result"))
        )
        self.assertEqual(code, 2)
        self.assertFalse(report["metadata"]["verified"])
        self.assertTrue(any("differ between baseline and result" in issue for issue in report["issues"]))

        # Equal raw repetitions (5 vs 5) that contradict CLI-option declared repetitions (7) fail the gate.
        meta_cli = dict(META, sampling={"options": ["--gint_full", "--benchmark_repetitions=7"]})
        code, report, _ = self.run_comparison(
            result, result, ["--fail-on-regression"], (meta_cli, dict(meta_cli, commit="result"))
        )
        self.assertEqual(code, 2)
        self.assertFalse(report["metadata"]["verified"])
        self.assertTrue(any("does not match metadata sampling (7)" in issue for issue in report["issues"]))

        # Matching CLI-option declared repetitions pass the gate.
        meta_cli_5 = dict(META, sampling={"options": ["--gint_full", "--benchmark_repetitions=5"]})
        code, report, _ = self.run_comparison(
            result, result, ["--fail-on-regression"], (meta_cli_5, dict(meta_cli_5, commit="result"))
        )
        self.assertEqual(code, 0)
        self.assertTrue(report["metadata"]["verified"])

    def test_rejects_geomean_underflow(self):
        baseline = document([constant(1e300)])
        result = document([constant(1e-300)])
        code, report, text = self.run_comparison(baseline, result, ["--fail-on-regression"])
        self.assertEqual(code, 2)
        self.assertIsNone(report)
        self.assertIn("geometric mean exceeds numeric range", text)


if __name__ == "__main__":
    unittest.main()
