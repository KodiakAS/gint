#!/usr/bin/env python3

from __future__ import print_function

import importlib.util
import os
import unittest


ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
MODULE_PATH = os.path.join(ROOT, "scripts", "summarize_benchmarks.py")
SPEC = importlib.util.spec_from_file_location("summarize_benchmarks", MODULE_PATH)
SUMMARY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(SUMMARY)


def aggregate(name, aggregate_name, real_time):
    return {
        "name": name + "_" + aggregate_name,
        "run_name": name,
        "aggregate_name": aggregate_name,
        "real_time": real_time,
        "cpu_time": real_time * 0.99,
        "time_unit": "ns",
        "repetitions": 7,
    }


def iteration(name, index, **fields):
    row = {
        "name": name,
        "run_name": name,
        "run_type": "iteration",
        "repetition_index": index,
        "iterations": 100,
        "real_time": 1.0,
        "cpu_time": 0.99,
        "time_unit": "ns",
    }
    row.update(fields)
    return row


class SummarizeDocumentTest(unittest.TestCase):
    def test_extracts_medians_and_noise(self):
        document = {
            "context": {"library_version": "v1"},
            "benchmarks": [
                aggregate("Add/gint", "median", 1.0),
                aggregate("Add/gint", "cv", 0.01),
                aggregate("Div/gint", "median", 10.0),
                aggregate("Div/gint", "cv", 0.08),
            ],
        }
        result = SUMMARY.summarize_document(document)
        self.assertEqual(result["median_count"], 2)
        self.assertEqual(result["noisy_count"], 1)
        self.assertEqual(result["noisy_benchmarks"][0]["name"], "Div/gint")
        self.assertAlmostEqual(result["median_real_time_cv"], 0.045)

    def test_rejects_results_without_median_rows(self):
        with self.assertRaises(ValueError):
            SUMMARY.summarize_document(
                {"benchmarks": [aggregate("Add/gint", "mean", 1.0)]}
            )

    def test_rejects_invalid_median_measurements(self):
        name = "Div/gint"
        for field in ("real_time", "cpu_time"):
            for value in (-1, float("nan"), float("inf"), True):
                with self.subTest(field=field, value=value):
                    row = aggregate(name, "median", 1.0)
                    row[field] = value
                    with self.assertRaisesRegex(ValueError, "median row.*invalid measurements"):
                        SUMMARY.summarize_document({"benchmarks": [row]})

    def test_rejects_failed_or_skipped_medians(self):
        for flag in ("error_occurred", "skipped"):
            with self.subTest(flag=flag, value=True):
                row = aggregate("Div/gint", "median", 1.0)
                row[flag] = True
                with self.assertRaisesRegex(ValueError, "median row.*failed or was skipped"):
                    SUMMARY.summarize_document({"benchmarks": [row]})
            for invalid in (None, 0, ""):
                with self.subTest(flag=flag, value=invalid):
                    row = aggregate("Div/gint", "median", 1.0)
                    row[flag] = invalid
                    with self.assertRaisesRegex(ValueError, "median row.*invalid " + flag):
                        SUMMARY.summarize_document({"benchmarks": [row]})

    def test_rejects_invalid_cv_measurements(self):
        name = "Div/gint"
        median = aggregate(name, "median", 1.0)
        for field in ("real_time", "cpu_time"):
            with self.subTest(field=field, missing=True):
                cv = aggregate(name, "cv", 0.01)
                del cv[field]
                with self.assertRaisesRegex(ValueError, "CV row.*invalid measurements"):
                    SUMMARY.summarize_document({"benchmarks": [median, cv]})
            for value in (-1, float("nan"), float("inf"), True, False):
                with self.subTest(field=field, value=value):
                    cv = aggregate(name, "cv", 0.01)
                    cv[field] = value
                    with self.assertRaisesRegex(ValueError, "CV row.*invalid measurements"):
                        SUMMARY.summarize_document({"benchmarks": [median, cv]})

    def test_rejects_duplicate_cv_rows(self):
        name = "Div/gint"
        with self.assertRaisesRegex(ValueError, "duplicate CV row.*Div/gint"):
            SUMMARY.summarize_document(
                {"benchmarks": [
                    aggregate(name, "median", 1.0),
                    aggregate(name, "cv", 0.01),
                    aggregate(name, "cv", 0.09),
                ]}
            )

    def test_rejects_orphan_cv_rows(self):
        with self.assertRaisesRegex(ValueError, "CV row\\(s\\) without a median.*Ghost/gint"):
            SUMMARY.summarize_document(
                {"benchmarks": [
                    aggregate("Div/gint", "median", 1.0),
                    aggregate("Div/gint", "cv", 0.01),
                    aggregate("Ghost/gint", "cv", 0.02),
                ]}
            )

    def test_validates_library_version_count_and_required_names(self):
        name = "FromString/Base16/gint"
        suite = SUMMARY.summarize_document(
            {
                "context": {"library_version": "v1.9.5"},
                "benchmarks": [
                    aggregate(name, "median", 1.0),
                    aggregate(name, "cv", 0.01),
                ],
            }
        )
        rows = [iteration(name, index) for index in reversed(range(7))]
        SUMMARY.validate_suite(
            "gint",
            suite,
            expected_library_version="v1.9.5",
            expected_count=1,
            expected_repetitions=7,
            required=[name],
            raw_rows=rows,
        )
        with self.assertRaises(ValueError):
            SUMMARY.validate_suite("gint", suite, expected_library_version="v1.9.4")
        with self.assertRaises(ValueError):
            SUMMARY.validate_suite("gint", suite, expected_count=2)
        with self.assertRaises(ValueError):
            SUMMARY.validate_suite("gint", suite, expected_repetitions=5)
        with self.assertRaises(ValueError):
            SUMMARY.validate_suite("gint", suite, required=["DivMod/SimilarMagnitude/gint"])

        for value in (None, -1, float("nan"), float("inf"), True):
            with self.subTest(value=value):
                suite["benchmarks"][0]["real_time_cv"] = value
                with self.assertRaisesRegex(ValueError, "missing or invalid CV"):
                    SUMMARY.validate_suite("gint", suite, expected_repetitions=7, raw_rows=rows)

    def test_requires_each_successful_repetition_for_each_benchmark(self):
        medians = [
            aggregate(name, statistic, value)
            for name in ("Add/gint", "Div/gint")
            for statistic, value in (("median", 1.0), ("cv", 0.01))
        ]
        rows = [
            iteration(name, index)
            for index in reversed(range(7))
            for name in ("Add/gint", "Div/gint")
        ]
        suite = SUMMARY.summarize_document({"benchmarks": medians + rows})
        SUMMARY.validate_suite("gint", suite, expected_repetitions=7, raw_rows=medians + rows)

        with self.assertRaisesRegex(ValueError, "missing successful iteration rows.*Div/gint"):
            SUMMARY.validate_suite("gint", suite, expected_repetitions=7, raw_rows=medians + rows[:-1])

        legacy_suite = SUMMARY.summarize_document({"benchmarks": medians})
        SUMMARY.validate_suite("gint", legacy_suite)
        with self.assertRaisesRegex(ValueError, "missing successful iteration rows"):
            SUMMARY.validate_suite("gint", legacy_suite, expected_repetitions=7, raw_rows=medians)

    def test_rejects_invalid_repetition_rows_even_with_complete_aggregates(self):
        name = "Add/gint"
        suite = SUMMARY.summarize_document(
            {"benchmarks": [aggregate(name, "median", 1.0), aggregate(name, "cv", 0.01)]}
        )
        valid = [iteration(name, index) for index in range(6)]
        cases = (
            (iteration(name, 6, error_occurred=True), "failed or was skipped"),
            (iteration(name, 6, skipped=True), "failed or was skipped"),
            (iteration(name, 6, error_occurred=None), "invalid error_occurred"),
            (iteration(name, 6, skipped=0), "invalid skipped"),
            (iteration(name, 6, error_occurred=""), "invalid error_occurred"),
            (iteration(name, 6, iterations=0), "invalid measurements"),
            (iteration(name, 6, real_time=float("nan")), "invalid measurements"),
            (iteration(name, 6, cpu_time=None), "invalid measurements"),
            (iteration(name, 0), "duplicate repetition_index"),
            (iteration(name, 7), "invalid repetition_index"),
            (iteration(name, -1), "invalid repetition_index"),
            (iteration(name, "6"), "invalid repetition_index"),
            (iteration(name, 6.0), "invalid repetition_index"),
            (iteration(name, False), "invalid repetition_index"),
            (iteration(name, None), "invalid repetition_index"),
            (iteration("Div/gint", 6), "iteration rows without a median"),
        )
        for row, message in cases:
            with self.subTest(row=row):
                with self.assertRaisesRegex(ValueError, message):
                    SUMMARY.validate_suite("gint", suite, expected_repetitions=7, raw_rows=valid + [row])

    def test_markdown_lists_noisy_benchmark_names(self):
        suite = SUMMARY.summarize_document(
            {
                "benchmarks": [
                    aggregate("Div/gint", "median", 10.0),
                    aggregate("Div/gint", "cv", 0.08),
                ]
            }
        )
        markdown = SUMMARY.render_markdown({"gint": suite}, {})
        self.assertIn("`Div/gint`: 8.00%", markdown)


if __name__ == "__main__":
    unittest.main()
