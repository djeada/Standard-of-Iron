"""The hardware lane must fail closed on missing measurements and asset work."""

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    "pacing_runner",
    Path(__file__).resolve().parents[2] / "scripts/check-frame-pacing.py",
)
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class PacingRunnerTest(unittest.TestCase):
    def check(self, report, preset="medium", cycle=False):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            path.write_text(json.dumps(report))
            return runner.read_report(path, preset, cycle)

    def good(self):
        return {
            "valid": True,
            "visible_soldiers_average": 224,
            "frame_pacing": {
                "passed": True,
                "preset": "medium",
                "interval_source": "QQuickWindow.frameSwapped",
                "checks": {
                    "post_playable_asset_work": {"passed": True, "measured": 0},
                    "untimed_presentation_frames": {"passed": True, "measured": 0},
                },
            },
            "asset_counters": {
                "load_barrier_marked": True,
                "post_load_asset_work": 0,
            },
        }

    def test_non_ultra_does_not_require_ultra_budget(self):
        self.assertEqual(self.check(self.good()), [])

    def test_missing_bad_or_mismatched_reports_fail(self):
        for report in ([], {}, {"valid": False}, {"valid": True}):
            self.assertTrue(self.check(report))
        self.assertTrue(self.check(self.good(), "ultra"))
        with tempfile.TemporaryDirectory() as directory:
            self.assertTrue(runner.read_report(Path(directory) / "missing", "medium"))

    def test_missing_or_nonzero_asset_work_fails(self):
        for assets in (
            {},
            [],
            {"load_barrier_marked": False},
        ):
            self.assertTrue(self.check({**self.good(), "asset_counters": assets}))

    def test_missing_presentation_timing_cannot_pass(self):
        report = self.good()
        del report["frame_pacing"]["interval_source"]
        self.assertTrue(self.check(report))
        for check in ({}, {"passed": False, "measured": 1}):
            report = self.good()
            report["frame_pacing"]["checks"]["untimed_presentation_frames"] = check
            self.assertTrue(self.check(report))

    def test_empty_battle_cannot_pass(self):
        for visible in (0, -1, None, float("nan")):
            self.assertTrue(
                self.check({**self.good(), "visible_soldiers_average": visible})
            )

    def test_playable_asset_gate_is_required(self):
        for check in ({}, {"passed": False, "measured": 1}):
            report = self.good()
            report["frame_pacing"]["checks"]["post_playable_asset_work"] = check
            self.assertTrue(self.check(report))

    def test_overlay_prewarm_does_not_fail_playable_gate(self):
        report = self.good()
        report["asset_counters"]["post_load_asset_work"] = 100
        self.assertEqual(self.check(report), [])

    def test_requested_camera_cycle_must_complete(self):
        self.assertTrue(self.check(self.good(), cycle=True))
        self.assertEqual(
            self.check(
                {**self.good(), "presentation_cycle": {"completed_cycles": 1}},
                cycle=True,
            ),
            [],
        )


if __name__ == "__main__":
    unittest.main()
