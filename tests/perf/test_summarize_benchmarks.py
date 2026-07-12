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

    def test_validates_library_version_count_and_required_names(self):
        suite = SUMMARY.summarize_document(
            {
                "context": {"library_version": "v1.9.5"},
                "benchmarks": [
                    aggregate("FromString/Base16/gint", "median", 1.0),
                    aggregate("FromString/Base16/gint", "cv", 0.01),
                ],
            }
        )
        SUMMARY.validate_suite(
            "gint",
            suite,
            expected_library_version="v1.9.5",
            expected_count=1,
            expected_repetitions=7,
            required=["FromString/Base16/gint"],
        )
        with self.assertRaises(ValueError):
            SUMMARY.validate_suite("gint", suite, expected_library_version="v1.9.4")
        with self.assertRaises(ValueError):
            SUMMARY.validate_suite("gint", suite, expected_count=2)
        with self.assertRaises(ValueError):
            SUMMARY.validate_suite("gint", suite, expected_repetitions=5)
        with self.assertRaises(ValueError):
            SUMMARY.validate_suite("gint", suite, required=["DivMod/SimilarMagnitude/gint"])

        suite["benchmarks"][0]["real_time_cv"] = None
        with self.assertRaises(ValueError):
            SUMMARY.validate_suite("gint", suite, expected_repetitions=7)

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
