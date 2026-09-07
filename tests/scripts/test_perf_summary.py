"""A failing repeat must never be hidden by successful repeats."""

import importlib.util
import unittest
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    "perf_summary",
    Path(__file__).resolve().parents[2] / "scripts/summarize-perf-suite.py",
)
summary = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(summary)


class RuntimePacingSummaryTest(unittest.TestCase):
    def test_every_repeat_must_have_passing_pacing(self):
        good = {
            "valid": True,
            "budget": {"passed": True, "failures": []},
            "frame_pacing": {"passed": True, "failures": []},
        }
        self.assertTrue(summary.runtime_row("battle", [good, good])["passed"])
        for bad in (
            {**good, "frame_pacing": {"passed": False, "failures": []}},
            {"valid": True, "budget": good["budget"]},
            {**good, "valid": False},
            {**good, "budget": {"passed": False}},
        ):
            with self.subTest(bad=bad):
                self.assertFalse(summary.runtime_row("battle", [good, bad])["passed"])


if __name__ == "__main__":
    unittest.main()
