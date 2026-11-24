#!/usr/bin/env python3
"""Frozen city review captures, their metadata, and the numbers that go with them.

The review shots are ordinary Arena promo specs under
tools/arena/promos/city_review/. This script runs them, records what produced
every frame (build, preset, hardware, camera, light, weather), and lays the
frames out so two captures can be judged side by side. See
docs/CITY_VISUAL_DIRECTION.md for the views and the scoring rubric.

  scripts/city-review.py capture tools/arena/promos/city_review/roman_block.json \
      --out artifacts/city-review/roman_block-<label>
  scripts/city-review.py map tools/arena/promos/city_review/rome_views.json \
      --out artifacts/city-review/rome-<label>
  scripts/city-review.py compare <before-dir> <after-dir> --out pairs.jpg
  scripts/city-review.py perf city_block_roman --out artifacts/city-review/perf \
      --quality high
  scripts/city-review.py audit artifacts/city-review/audit.json

Captures are raw in-engine frames: no grade, no edit. A capture needs a GL
context; run it under whatever display the machine renders on.
"""

from __future__ import annotations

import argparse
import datetime
import json
import os
import pathlib
import statistics
import subprocess
import sys
import time

ROOT = pathlib.Path(__file__).resolve().parent.parent
ARENA = ROOT / "build" / "bin" / "arena_app"

# Frames at the head of a run pay shader and pipeline warm-up.
WARMUP_FRAMES = 60


def git(*args: str) -> str:
    return subprocess.run(
        ["git", *args], cwd=ROOT, capture_output=True, text=True, check=False
    ).stdout.strip()


def build_identity() -> dict:
    return {
        "sha": git("rev-parse", "HEAD"),
        "branch": git("rev-parse", "--abbrev-ref", "HEAD"),
        "dirty": bool(git("status", "--porcelain")),
    }


def hardware() -> dict:
    cpu = ""
    try:
        for line in pathlib.Path("/proc/cpuinfo").read_text().splitlines():
            if line.startswith("model name"):
                cpu = line.split(":", 1)[1].strip()
                break
    except OSError:
        pass
    gpu = subprocess.run(
        ["nvidia-smi", "--query-gpu=name,driver_version", "--format=csv,noheader"],
        capture_output=True,
        text=True,
        check=False,
    ).stdout.strip()
    return {"cpu": cpu, "gpu": gpu or "unknown"}


def shot_metadata(spec: dict, shot: dict, quality: str) -> dict:
    keys = shot.get("camera", [])
    return {
        "name": shot.get("name"),
        "scenario": shot.get("scenario"),
        "seed": shot.get("seed"),
        "start_seconds": shot.get("start"),
        "duration_seconds": shot.get("duration"),
        "rig": shot.get("rig", "orbit"),
        "focus": shot.get("focus"),
        "camera_first": keys[0] if keys else None,
        "camera_last": keys[-1] if keys else None,
        "resolution": [spec.get("width"), spec.get("height")],
        "supersample": spec.get("supersample", 1),
        "fps": spec.get("fps"),
        "graphics_preset": quality,
        "lighting": shot.get("lighting", {}),
        "view": shot.get("view"),
        "weather": shot.get("weather", "scenario default"),
    }


def run_arena(binary: str | None, args: list[str], log: pathlib.Path) -> int:
    arena = pathlib.Path(binary).resolve() if binary else ARENA
    if not arena.exists():
        sys.exit(f"{arena} is missing; build the arena_app target first")
    # Promo capture insists on Ultra for marketing footage; review frames are
    # taken at the preset under review.
    env = dict(os.environ, SOI_PROMO_ALLOW_LOW_QUALITY="1")
    with log.open("w") as handle:
        return subprocess.run(
            [str(arena), *args],
            cwd=ROOT,
            env=env,
            stdout=handle,
            stderr=subprocess.STDOUT,
        ).returncode


def contact_sheet(frames: list[pathlib.Path], out: pathlib.Path, cols: int) -> None:
    try:
        from PIL import Image, ImageDraw
    except ImportError:
        print("Pillow is not installed; skipping the contact sheet")
        return
    if not frames:
        return
    width = 960
    images = [Image.open(frame).convert("RGB") for frame in frames]
    height = int(width * images[0].height / images[0].width)
    label = 26
    rows = (len(images) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * width, rows * (height + label)), (24, 22, 20))
    draw = ImageDraw.Draw(sheet)
    for index, (frame, image) in enumerate(zip(frames, images, strict=True)):
        x = (index % cols) * width
        y = (index // cols) * (height + label)
        draw.text((x + 8, y + 6), frame.stem, fill=(232, 226, 210))
        sheet.paste(image.resize((width, height), Image.LANCZOS), (x, y + label))
    sheet.save(out, quality=90)


def cmd_capture(args: argparse.Namespace) -> None:
    spec_path = pathlib.Path(args.spec).resolve()
    spec = json.loads(spec_path.read_text())
    out = pathlib.Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)

    code = run_arena(
        args.arena,
        [
            "--promo-spec",
            str(spec_path),
            "--promo-out",
            str(out),
            "--graphics-quality",
            args.quality,
        ],
        out / "capture.log",
    )
    clips = out / spec["id"]
    frames = sorted(clips.glob("*.png"))
    log_text = (out / "capture.log").read_text(errors="replace")
    manifest = {
        "spec": (
            str(spec_path.relative_to(ROOT))
            if spec_path.is_relative_to(ROOT)
            else spec_path.name
        ),
        "captured_at": datetime.datetime.now(datetime.timezone.utc).isoformat(
            timespec="seconds"
        ),
        "build": build_identity(),
        "hardware": hardware(),
        "graphics_preset": args.quality,
        "arena_exit_code": code,
        "gl_errors_logged": log_text.count("GL error"),
        "raw_ungraded": True,
        "shots": [shot_metadata(spec, shot, args.quality) for shot in spec["shots"]],
        "frames": [frame.name for frame in frames],
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    stills = [
        frame
        for frame, shot in zip(frames, spec["shots"], strict=False)
        if shot.get("duration", 0) <= 1.0
    ]
    contact_sheet(stills or frames, out / "contact_sheet.jpg", args.cols)
    print(f"{len(frames)} frames in {clips}; manifest and contact sheet in {out}")
    if manifest["gl_errors_logged"]:
        print(f"warning: {manifest['gl_errors_logged']} GL errors in capture.log")
    if code != 0:
        sys.exit(code)


def cmd_map(args: argparse.Namespace) -> None:
    """Frozen views of a whole map, drawn as the game draws it.

    A views file names a shipped map and the cameras to look at it with. Each
    view is the map's own camera block, so what is framed here is what the map
    preview frames: no scenario, no script, just the map.
    """
    views_path = pathlib.Path(args.views).resolve()
    views = json.loads(views_path.read_text())
    map_path = ROOT / views["map"]
    source = json.loads(map_path.read_text())
    out = pathlib.Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    frames: list[pathlib.Path] = []
    shots = []
    for view in views["views"]:
        name = view["name"]
        staged = dict(source)
        staged["camera"] = {
            "center": [view["center"][0], 0, view["center"][1]],
            "distance": view["distance"],
            "tilt_deg": view["tilt"],
            "yaw": view["yaw"],
            "fov_y": view.get("fov", 45),
            "near": 1,
            "far": 1800,
        }
        if "hour" in view:
            # A view may fix its own hour (night pairs); the light is locked there.
            environment = dict(staged.get("environment", {}))
            environment.update(start_time=float(view["hour"]), time_mode="locked")
            staged["environment"] = environment
        work = out / f"_{name}"
        work.mkdir(exist_ok=True)
        staged_map = work / f"{name}.json"
        staged_map.write_text(json.dumps(staged))
        code = run_arena(
            args.arena,
            [
                "--batch",
                "--terrain-map",
                str(staged_map),
                "--map-preview-content",
                "--capture-interval",
                "0",
                "--graphics-quality",
                args.quality,
                "--artifact-dir",
                str(work),
            ],
            out / f"_{name}.log",
        )
        frame = work / name / "gameplay.png"
        if code != 0 or not frame.exists():
            sys.exit(f"{name}: no frame (exit {code}); see {out / f'_{name}.log'}")
        kept = out / f"{name}.png"
        kept.write_bytes(frame.read_bytes())
        frames.append(kept)
        shots.append({**view, "frame": kept.name})
        print(f"{name}: {kept}")
    manifest = {
        "views": (
            str(views_path.relative_to(ROOT))
            if views_path.is_relative_to(ROOT)
            else str(views_path)
        ),
        "map": views["map"],
        "quality": args.quality,
        "captured": datetime.datetime.now(datetime.timezone.utc).isoformat(
            timespec="seconds"
        ),
        "build": build_identity(),
        "hardware": hardware(),
        "shots": shots,
    }
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
    contact_sheet(frames, out / "contact_sheet.jpg", args.cols)


def cmd_compare(args: argparse.Namespace) -> None:
    from PIL import Image, ImageDraw

    def frames_of(directory: str) -> dict[str, pathlib.Path]:
        found = {}
        for frame in sorted(pathlib.Path(directory).rglob("*.png")):
            # 03_aur_B_gate.png -> aur_B_gate: pair by shot name, not by order.
            found[frame.stem.split("_", 1)[-1]] = frame
        return found

    before = frames_of(args.before)
    after = frames_of(args.after)
    names = [name for name in before if name in after]
    if args.only:
        names = [name for name in names if any(key in name for key in args.only)]
    if not names:
        sys.exit("the two captures share no shot names")
    width = 960
    label = 26
    rows = []
    for name in names:
        left = Image.open(before[name]).convert("RGB")
        right = Image.open(after[name]).convert("RGB")
        height = int(width * left.height / left.width)
        row = Image.new("RGB", (2 * width, height + label), (24, 22, 20))
        draw = ImageDraw.Draw(row)
        draw.text((8, 6), f"{name}  -  before", fill=(232, 226, 210))
        draw.text((width + 8, 6), f"{name}  -  after", fill=(232, 226, 210))
        row.paste(left.resize((width, height), Image.LANCZOS), (0, label))
        row.paste(right.resize((width, height), Image.LANCZOS), (width, label))
        rows.append(row)
    sheet = Image.new("RGB", (2 * width, sum(row.height for row in rows)))
    y = 0
    for row in rows:
        sheet.paste(row, (0, y))
        y += row.height
    out = pathlib.Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(out, quality=90)
    print(f"{len(names)} pairs in {out}")


def percentile(values: list[float], fraction: float) -> float:
    if not values:
        return 0.0
    ordered = sorted(values)
    position = fraction * (len(ordered) - 1)
    low = int(position)
    high = min(low + 1, len(ordered) - 1)
    return ordered[low] + (ordered[high] - ordered[low]) * (position - low)


def summarise(values: list[float]) -> dict:
    return {
        "p50": round(percentile(values, 0.50), 3),
        "p95": round(percentile(values, 0.95), 3),
        "p99": round(percentile(values, 0.99), 3),
        "min": round(min(values), 3) if values else 0.0,
        "mean": round(statistics.fmean(values), 3) if values else 0.0,
    }


def cmd_perf(args: argparse.Namespace) -> None:
    out = pathlib.Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    arena_args = [
        "--batch",
        "--scenario",
        args.scenario,
        "--profile",
        "--graphics-quality",
        args.quality,
        "--duration",
        str(args.seconds),
        "--fps",
        "60",
        "--watchdog-multiplier",
        "40",
        "--artifact-dir",
        str(out),
    ]
    started = time.monotonic()
    code = run_arena(args.arena, arena_args, out / f"{args.scenario}.log")
    wall_seconds = time.monotonic() - started
    run = out / args.scenario
    trace = run / "trace.jsonl"
    if not trace.exists():
        sys.exit(f"no trace written (arena exit {code}); see {out}/{args.scenario}.log")

    frames = [json.loads(line) for line in trace.read_text().splitlines() if line]
    measured = frames[WARMUP_FRAMES:] or frames
    breakdown: dict[str, list[float]] = {}
    for frame in measured:
        for stage, value in frame.get("frame_breakdown_ms", {}).items():
            breakdown.setdefault(stage, []).append(float(value))
    gpu: dict[str, list[float]] = {}
    for frame in measured:
        for stage, value in frame.get("gpu_ms", {}).items():
            gpu.setdefault(stage, []).append(float(value))

    report = json.loads((run / "report.json").read_text())
    config = json.loads((run / "run_config.json").read_text())
    totals = report.get("asset_counters", {}).get("total", {})
    summary = {
        "scenario": args.scenario,
        "graphics_preset": args.quality,
        "build": build_identity(),
        "hardware": config.get("reference_hardware", hardware()),
        "frames_measured": len(measured),
        "warmup_frames_dropped": len(frames) - len(measured),
        # Wall clock on a shared desktop is noisy: compare the minimum and the
        # counters before trusting a percentile.
        "frame_time_ms": summarise([float(f["frame_time_ms"]) for f in measured]),
        "render_cpu_ms": {
            stage: summarise(values) for stage, values in sorted(breakdown.items())
        },
        "gpu_ms": {stage: summarise(values) for stage, values in sorted(gpu.items())},
        "draw_calls": summarise([float(f.get("draw_calls", 0)) for f in measured]),
        "prepared_batches": summarise(
            [float(f.get("prepared_batches", 0)) for f in measured]
        ),
        "rigged_instances": summarise(
            [
                float(f.get("rigged_playback", {}).get("instanced_instances", 0))
                for f in measured
            ]
        ),
        "world_entities": {
            "units": len(measured[-1].get("units", [])),
            "soldiers": len(measured[-1].get("soldiers", [])),
        },
        "vram_proxy_bytes": {
            "gl_buffer_storage": totals.get("gl_buffer_storage_bytes"),
            "gl_texture_storage": totals.get("gl_texture_storage_bytes"),
        },
        "gl_programs_linked": totals.get("programs_linked"),
        # Process start to first frame: asset load, world build and bake, taken
        # as the wall clock the frames themselves do not account for.
        "startup_seconds": round(
            max(
                0.0,
                wall_seconds - sum(float(f["frame_time_ms"]) for f in frames) / 1000.0,
            ),
            2,
        ),
        "simulated_seconds": report.get("elapsed_seconds"),
        "shadows": report.get("shadows"),
    }
    target = out / f"{args.scenario}.{args.quality}.perf.json"
    target.write_text(json.dumps(summary, indent=2) + "\n")
    frame = summary["frame_time_ms"]
    print(
        f"{args.scenario} @ {args.quality}: frame p50 {frame['p50']} "
        f"p95 {frame['p95']} p99 {frame['p99']} min {frame['min']} ms, "
        f"draw calls p50 {summary['draw_calls']['p50']:.0f} -> {target}"
    )


def cmd_audit(args: argparse.Namespace) -> None:
    """Score a capture set against the rubric file and fail on open blockers."""
    audit = json.loads(pathlib.Path(args.audit).read_text())
    order = ["critical", "major", "moderate", "minor"]
    open_defects = [d for d in audit["defects"] if d.get("status", "open") == "open"]
    for severity in order:
        rows = [d for d in open_defects if d["severity"] == severity]
        if rows:
            print(f"{severity.upper()} ({len(rows)})")
            for row in rows:
                print(f"  {row['id']}  {row['title']}  [{', '.join(row['views'])}]")
    blocking = [d for d in open_defects if d["severity"] in ("critical", "major")]
    print(f"{len(open_defects)} open, {len(blocking)} blocking the exit gate")
    if args.gate and blocking:
        sys.exit(1)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    commands = parser.add_subparsers(dest="command", required=True)

    capture = commands.add_parser("capture", help="run a review spec")
    capture.add_argument("spec")
    capture.add_argument("--out", required=True)
    capture.add_argument("--quality", default="high")
    capture.add_argument("--cols", type=int, default=2)
    capture.set_defaults(run=cmd_capture)

    whole_map = commands.add_parser("map", help="frozen views of a shipped map")
    whole_map.add_argument("views")
    whole_map.add_argument("--out", required=True)
    whole_map.add_argument("--quality", default="high")
    whole_map.add_argument("--cols", type=int, default=2)
    whole_map.set_defaults(run=cmd_map)

    compare = commands.add_parser("compare", help="pair two captures by shot name")
    compare.add_argument("before")
    compare.add_argument("after")
    compare.add_argument("--out", required=True)
    compare.add_argument("--only", nargs="*", help="shot-name substrings to keep")
    compare.set_defaults(run=cmd_compare)

    perf = commands.add_parser("perf", help="profile a scenario and summarise it")
    perf.add_argument("scenario")
    perf.add_argument("--out", required=True)
    perf.add_argument("--quality", default="high")
    perf.add_argument("--seconds", type=float, default=12.0)
    perf.set_defaults(run=cmd_perf)

    audit = commands.add_parser("audit", help="list open defects from the rubric")
    audit.add_argument("audit")
    audit.add_argument("--gate", action="store_true")
    audit.set_defaults(run=cmd_audit)

    for command in (capture, perf, whole_map):
        command.add_argument(
            "--arena",
            help="arena_app to run instead of build/bin/arena_app, for example a "
            "copy kept from before a change so both captures share one tree",
        )

    args = parser.parse_args()
    os.chdir(ROOT)
    args.run(args)


if __name__ == "__main__":
    main()
