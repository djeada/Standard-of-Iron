"""The hardware lane must fail closed on missing measurements and asset work."""

import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from unittest.mock import MagicMock, patch

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

    def test_competitor_detection_excludes_benchmark_and_handles_exits(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            for pid, executable in (
                (1, "standard_of_iron"),
                (2, "standard_of_iron"),
                (3, "ninja"),
                (4, "python3"),
                (5, "cc1plus (deleted)"),
            ):
                entry = root / str(pid)
                entry.mkdir()
                (entry / "exe").symlink_to("/bin/" + executable)
            (root / "6").mkdir()
            self.assertEqual(
                runner.competing_processes({1}, root),
                [
                    {"pid": 2, "executable": "standard_of_iron"},
                    {"pid": 3, "executable": "ninja"},
                    {"pid": 5, "executable": "cc1plus"},
                ],
            )

    def test_monitor_retains_competitor_that_starts_during_run(self):
        process = MagicMock(pid=123)
        process.__enter__.return_value = process
        process.wait.side_effect = [runner.subprocess.TimeoutExpired(["game"], 1), 0]
        rivals = {}
        with (
            patch.object(runner.subprocess, "Popen", return_value=process),
            patch.object(
                runner,
                "competing_processes",
                side_effect=[[], [{"pid": 456, "executable": "ninja"}]],
            ),
        ):
            self.assertEqual(runner.run_monitored(["game"], None, {}, 30, rivals), 0)
        self.assertEqual(rivals, {456: {"pid": 456, "executable": "ninja"}})

    def test_monitor_kills_and_reaps_timed_out_benchmark(self):
        process = MagicMock(pid=123)
        process.__enter__.return_value = process
        with (
            patch.object(runner.subprocess, "Popen", return_value=process),
            patch.object(runner, "competing_processes", return_value=[]),
            patch.object(runner.time, "monotonic", side_effect=[0, 31]),
        ):
            with self.assertRaises(runner.subprocess.TimeoutExpired):
                runner.run_monitored(["game"], None, {}, 30, {})
        process.kill.assert_called_once()
        process.wait.assert_called_once()

    def test_custom_mission_copies_and_hashes_local_map(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "artifacts"
            output.mkdir()
            source = root / "mission.json"
            source.write_text(json.dumps({"id": "fixture", "map_path": "map.json"}))
            (root / "map.json").write_text('{"name":"test map"}')
            destination, metadata = runner.copy_mission_fixture(source, output, 0)
            copied_map = Path(json.loads(destination.read_text())["map_path"])
            self.assertEqual(copied_map.parent, output)
            self.assertEqual(copied_map.read_bytes(), (root / "map.json").read_bytes())
            self.assertEqual(
                (output / metadata["original"]).read_bytes(), source.read_bytes()
            )
            self.assertEqual(
                metadata["maps"][0]["sha256"],
                runner.hashlib.sha256(copied_map.read_bytes()).hexdigest(),
            )

    def test_custom_mission_preserves_embedded_map_reference(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "mission.json"
            source.write_text('{"map_path":":/assets/maps/map_forest.json"}')
            destination, metadata = runner.copy_mission_fixture(source, root, 0)
            self.assertEqual(
                json.loads(destination.read_text())["map_path"],
                ":/assets/maps/map_forest.json",
            )
            self.assertEqual(metadata["maps"], [])

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
