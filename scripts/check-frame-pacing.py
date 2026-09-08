#!/usr/bin/env python3
"""Run sequential real-mission frame-pacing gates and retain every artifact."""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import math
import os
import platform
import shutil
import subprocess
import sys
import time
from pathlib import Path

MISSIONS = (
    "second_punic_war/crossing_the_rhone",
    "second_punic_war/crossing_the_alps",
    "second_punic_war/battle_of_ticino",
)


def read_report(path: Path, preset: str, camera_cycle: bool = False) -> list[str]:
    try:
        report = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        return [f"missing or invalid report: {error}"]
    if not isinstance(report, dict) or report.get("valid") is not True:
        return ["runtime report is invalid"]
    pacing = report.get("frame_pacing")
    if not isinstance(pacing, dict):
        return ["frame pacing was not measured"]
    failures = []
    visible = report.get("visible_soldiers_average")
    if type(visible) not in (int, float) or not math.isfinite(visible) or visible <= 0:
        failures.append("no visible soldiers were measured")
    if pacing.get("preset") != preset:
        failures.append("measured preset does not match requested preset")
    if pacing.get("passed") is not True:
        failures.append("frame-pacing budget failed")
        failures.extend(str(value) for value in pacing.get("failures", []))
    assets = report.get("asset_counters", {})
    if not isinstance(assets, dict) or assets.get("load_barrier_marked") is not True:
        failures.append("post-load asset work was not measured")

    checks = pacing.get("checks", {})
    swap_check = (
        checks.get("untimed_presentation_frames", {})
        if isinstance(checks, dict)
        else {}
    )
    if (
        pacing.get("interval_source") != "QQuickWindow.frameSwapped"
        or not isinstance(swap_check, dict)
        or swap_check.get("passed") is not True
    ):
        failures.append("presentation timing failed or was not measured")
    asset_check = (
        checks.get("post_playable_asset_work", {}) if isinstance(checks, dict) else {}
    )
    if not isinstance(asset_check, dict) or asset_check.get("passed") is not True:
        failures.append("post-playable asset work failed or was not measured")
    if camera_cycle:
        cycle = report.get("presentation_cycle", {})
        completed = cycle.get("completed_cycles") if isinstance(cycle, dict) else None
        if type(completed) is not int or completed < 1:
            failures.append("camera cycle did not complete")
    return failures


def capture(command: list[str]) -> str:
    try:
        result = subprocess.run(command, capture_output=True, text=True, timeout=10)
        return (
            result.stdout.strip() if result.returncode == 0 else result.stderr.strip()
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        return str(error)


def copy_mission_fixture(source: Path, output: Path, index: int):
    source = source.resolve()
    raw = source.read_bytes()
    mission = json.loads(raw)
    map_path = mission.get("map_path") if isinstance(mission, dict) else None
    if not isinstance(map_path, str) or not map_path:
        raise ValueError(f"mission has no map_path: {source}")
    original = output / f"fixture{index}.source.json"
    original.write_bytes(raw)
    dependencies = []
    if not map_path.startswith(":/"):
        map_source = Path(map_path)
        if not map_source.is_absolute():
            map_source = source.parent / map_source
        map_bytes = map_source.read_bytes()
        map_artifact = output / f"fixture{index}.map.json"
        map_artifact.write_bytes(map_bytes)
        mission["map_path"] = str(map_artifact.resolve())
        dependencies.append(
            {
                "artifact": map_artifact.name,
                "sha256": hashlib.sha256(map_bytes).hexdigest(),
            }
        )
    destination = output / f"fixture{index}.mission.json"
    destination.write_text(json.dumps(mission, indent=2) + "\n")
    return destination, {
        "source": str(source),
        "original": original.name,
        "source_sha256": hashlib.sha256(raw).hexdigest(),
        "artifact": destination.name,
        "sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
        "maps": dependencies,
    }


def competing_processes(excluded=(), proc_root=Path("/proc")) -> list[dict]:
    """Linux qualification lane: detect competing games and build workers."""
    names = {
        "standard_of_iron",
        "arena_app",
        "soi_pacing_probe",
        "cc1plus",
        "cc1",
        "lto1",
        "ninja",
        "make",
        "ld",
        "ld.lld",
    }
    found = []
    if not proc_root.is_dir():
        return found
    for entry in proc_root.iterdir():
        if not entry.name.isdigit() or int(entry.name) in excluded:
            continue
        try:
            executable = Path(os.readlink(entry / "exe")).name.removesuffix(
                " (deleted)"
            )
        except OSError:
            continue
        if executable in names:
            found.append({"pid": int(entry.name), "executable": executable})
    return sorted(found, key=lambda item: item["pid"])


def run_monitored(command, log, environment, timeout, competitors) -> int:
    with subprocess.Popen(
        command, stdout=log, stderr=subprocess.STDOUT, env=environment
    ) as process:
        deadline = time.monotonic() + timeout
        while True:
            for rival in competing_processes({process.pid}):
                competitors[rival["pid"]] = rival
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                process.kill()
                process.wait()
                raise subprocess.TimeoutExpired(command, timeout)
            try:
                return process.wait(timeout=min(1.0, remaining))
            except subprocess.TimeoutExpired:
                pass


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--binary", type=Path, default=Path("build/bin/standard_of_iron")
    )
    parser.add_argument(
        "--preset", action="append", choices=("low", "medium", "high", "ultra")
    )
    fixtures = parser.add_mutually_exclusive_group()
    fixtures.add_argument(
        "--mission", action="append", help="campaign mission ID; repeatable"
    )
    fixtures.add_argument(
        "--replay",
        action="append",
        type=Path,
        help="recorded runtime replay; repeatable, copied into artifacts",
    )
    fixtures.add_argument(
        "--mission-file",
        action="append",
        type=Path,
        help="custom mission JSON; copied and hashed with its local map",
    )
    parser.add_argument(
        "--camera-cycle", action="store_true", help="repeat a 20-second pan/zoom path"
    )
    parser.add_argument(
        "--allow-contended",
        action="store_true",
        help="collect diagnostic data despite competitors; the gate still fails",
    )
    parser.add_argument("--seconds", type=int, default=60)
    parser.add_argument("--repeats", type=int, default=3)
    parser.add_argument(
        "--timeout", type=int, default=300, help="per-process watchdog seconds"
    )
    parser.add_argument(
        "--output", type=Path, help="new artifact directory (must not exist)"
    )
    args = parser.parse_args()
    if args.seconds < 30 or args.repeats < 1 or args.timeout <= args.seconds + 2:
        parser.error("requires seconds >= 30, repeats >= 1, timeout > seconds + 2")
    binary = args.binary.resolve()
    if not binary.is_file() or not os.access(binary, os.X_OK):
        parser.error(f"binary is not executable: {binary}")
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    output = args.output or Path("artifacts/frame-pacing") / stamp
    try:
        output.mkdir(parents=True, exist_ok=False)
    except OSError as error:
        parser.error(str(error))
    output = output.resolve()
    presets = args.preset or ["low", "medium", "high", "ultra"]
    missions = args.mission or list(MISSIONS)
    replay_manifest = []
    mission_manifest = []
    if args.mission_file:
        missions = []
        for index, source in enumerate(args.mission_file):
            try:
                destination, metadata = copy_mission_fixture(source, output, index)
            except (OSError, ValueError) as error:
                parser.error(str(error))
            missions.append(str(destination))
            mission_manifest.append(metadata)
    if args.replay:
        missions = []
        for index, source in enumerate(args.replay):
            destination = output / f"fixture{index}.soireplay"
            try:
                shutil.copyfile(source, destination)
            except OSError as error:
                parser.error(str(error))
            replay_manifest.append(
                {
                    "source": str(source.resolve()),
                    "artifact": destination.name,
                    "sha256": hashlib.sha256(destination.read_bytes()).hexdigest(),
                }
            )
            missions.append(str(destination))
    environment = os.environ.copy()
    environment["SOI_BENCHMARK_CAMERA_CYCLE"] = "1" if args.camera_cycle else "0"
    environment["SOI_SWAP_INTERVAL"] = "1"
    manifest = {
        "utc": stamp,
        "binary": str(binary),
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "load_average": os.getloadavg() if hasattr(os, "getloadavg") else None,
        "platform": platform.platform(),
        "graphics_environment": {
            key: environment.get(key)
            for key in (
                "DISPLAY",
                "WAYLAND_DISPLAY",
                "SOI_SWAP_INTERVAL",
                "QSG_RENDER_LOOP",
                "QT_XCB_GL_INTEGRATION",
                "__GL_SYNC_TO_VBLANK",
                "__GL_MaxFramesAllowed",
                "vblank_mode",
                "SOI_PROFILE_SIMULATION",
            )
        },
        "cpu": capture(["lscpu"]),
        "gpu": capture(["glxinfo", "-B"]),
        "display": capture(["xrandr", "--current"]),
        "revision": capture(["git", "rev-parse", "HEAD"]),
        "working_tree": capture(["git", "status", "--short"]),
        "presets": presets,
        "missions": missions,
        "replays": replay_manifest,
        "mission_files": mission_manifest,
        "seconds": args.seconds,
        "repeats": args.repeats,
        "coverage": {
            "campaign": not bool(args.replay or args.mission_file),
            "camera_cycle": args.camera_cycle,
            "ui_actions": False,
        },
    }
    (output / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    rows = []
    for mission_index, mission in enumerate(missions):
        for repeat in range(args.repeats):
            for preset in presets:
                label = f"mission{mission_index}.{preset}.run{repeat + 1}"
                report_path = output / f"{label}.json"
                command = [
                    str(binary),
                    (
                        "--replay"
                        if args.replay
                        else (
                            "--mission-file"
                            if args.mission_file
                            else "--campaign-mission"
                        )
                    ),
                    mission,
                    "--skip-briefing",
                    "--graphics-preset",
                    preset,
                    "--benchmark-seconds",
                    str(args.seconds),
                    "--benchmark-output",
                    str(report_path),
                ]
                print(
                    f"Measuring {mission} / {preset} / repeat {repeat + 1}", flush=True
                )
                failures = []
                load_before = os.getloadavg() if hasattr(os, "getloadavg") else None
                competitors = {item["pid"]: item for item in competing_processes()}
                with (output / f"{label}.log").open("w") as log:
                    if competitors and not args.allow_contended:
                        log.write(
                            "Skipped: competing game or build processes detected.\n"
                        )
                    else:
                        try:
                            returncode = run_monitored(
                                command, log, environment, args.timeout, competitors
                            )
                            if returncode != 0:
                                failures.append(f"process exited {returncode}")
                        except (OSError, subprocess.TimeoutExpired) as error:
                            failures.append(str(error))
                if competitors:
                    failures.append("competing game or build processes detected")
                failures.extend(read_report(report_path, preset, args.camera_cycle))
                rows.append(
                    {
                        "mission": mission,
                        "preset": preset,
                        "repeat": repeat + 1,
                        "command": command,
                        "load_average_before": load_before,
                        "load_average_after": (
                            os.getloadavg() if hasattr(os, "getloadavg") else None
                        ),
                        "competing_workloads": list(competitors.values()),
                        "report": report_path.name,
                        "passed": not failures,
                        "failures": failures,
                    }
                )

                summary = {"passed": False, "complete": False, "runs": rows}
                (output / "summary.json").write_text(
                    json.dumps(summary, indent=2) + "\n"
                )
    passed = all(row["passed"] for row in rows)
    (output / "summary.json").write_text(
        json.dumps({"passed": passed, "complete": True, "runs": rows}, indent=2) + "\n"
    )
    print(f"{'PASS' if passed else 'FAIL'}: {output / 'summary.json'}")
    return 0 if passed else 1


if __name__ == "__main__":
    sys.exit(main())
