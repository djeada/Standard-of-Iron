#!/usr/bin/env python3
"""Capture a long promo spec in chunks under a thermal guard.

    scripts/promo-guarded-capture.py SPEC OUTDIR [--from N] [--chunk K]
        [--pause-at C] [--resume-at C] [--start-below C] [--cpus LIST]

A 30-minute film is minutes of sustained full load on CPU and GPU. Captured
in one ``arena_app`` run it once powered a workstation off mid-render. This
runner captures shots N.. of SPEC in chunks of K shots, each chunk in its own
``arena_app`` process, and:

* waits before each chunk until the CPU package is below ``--start-below``;
* SIGSTOPs the arena while the CPU package is above ``--pause-at`` or the GPU
  above 83 C, and SIGCONTs it once the CPU is below ``--resume-at`` and the
  GPU below 78 C -- promo capture steps the simulation per frame, so a paused
  arena loses nothing;
* kills and retries an arena whose log has not grown for five minutes;
* checks every clip's length, accepts the leading complete ones and retries
  from the first incomplete shot (three retries at most);
* rests 60 s between chunks.

Clips land in OUTDIR/<spec id>/ under their global shot numbers, and a
``shots.json`` that ``promo-edit.py`` can cut from is written once every shot
is present. Temperatures come from Linux ``coretemp`` and ``nvidia-smi``; when
either is missing it reads as 0 and that side of the guard is inert.
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import signal
import subprocess
import sys
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
ARENA = REPO / "build/bin/arena_app"
STALL_SECONDS = 300
MAX_RETRIES = 3


def cpu_temp() -> float:
    for name in glob.glob("/sys/class/hwmon/*/name"):
        if Path(name).read_text().strip() == "coretemp":
            sensor = Path(os.path.dirname(name)) / "temp1_input"
            return int(sensor.read_text()) / 1000
    return 0.0


def gpu_temp() -> float:
    try:
        out = subprocess.check_output(
            ["nvidia-smi", "--query-gpu=temperature.gpu", "--format=csv,noheader"],
            text=True,
        )
        return float(out.strip())
    except (OSError, subprocess.CalledProcessError, ValueError):
        return 0.0


def clip_seconds(path: Path) -> float:
    try:
        out = subprocess.check_output(
            [
                "ffprobe",
                "-v",
                "error",
                "-show_entries",
                "format=duration",
                "-of",
                "csv=p=0",
                str(path),
            ],
            text=True,
        )
        return float(out.strip())
    except (OSError, subprocess.CalledProcessError, ValueError):
        return 0.0


def log(message: str) -> None:
    print(time.strftime("%H:%M:%S"), message, flush=True)


def run_chunk(
    args, sub_spec: Path, log_path: Path, env: dict, peaks: list
) -> int | None:
    prefix = ["nice", "-n", "5"] + (["taskset", "-c", args.cpus] if args.cpus else [])
    with log_path.open("w") as handle:
        proc = subprocess.Popen(
            prefix
            + [str(ARENA), "--promo-spec", str(sub_spec), "--promo-out", args.out],
            cwd=REPO,
            env=env,
            stdout=handle,
            stderr=subprocess.STDOUT,
        )
        paused = False
        last_size, last_change = -1, time.time()
        while proc.poll() is None:
            size = log_path.stat().st_size
            if size != last_size or paused:
                last_size, last_change = size, time.time()
            elif time.time() - last_change > STALL_SECONDS:
                log("arena stalled for 5 min; killing it")
                proc.send_signal(signal.SIGCONT)
                proc.terminate()
                try:
                    proc.wait(timeout=20)
                except subprocess.TimeoutExpired:
                    proc.kill()
                break
            cpu, gpu = cpu_temp(), gpu_temp()
            peaks[0], peaks[1] = max(peaks[0], cpu), max(peaks[1], gpu)
            if not paused and (cpu > args.pause_at or gpu > 83):
                proc.send_signal(signal.SIGSTOP)
                paused = True
                log(f"pause: cpu {cpu:.0f} C, gpu {gpu:.0f} C")
            elif paused and cpu < args.resume_at and gpu < 78:
                proc.send_signal(signal.SIGCONT)
                paused = False
                log(f"resume: cpu {cpu:.0f} C, gpu {gpu:.0f} C")
            time.sleep(0.5)
        return proc.returncode


def write_manifest(spec: dict, final: Path) -> None:
    manifest = {
        "fps": spec["fps"],
        "height": spec["height"],
        "id": spec["id"],
        "title": spec.get("title", ""),
        "width": spec["width"],
        "shots": [],
    }
    for index, shot in enumerate(spec["shots"], start=1):
        stem = f"{index:02d}_{shot['name']}"
        if not (final / f"{stem}.mp4").exists():
            log(f"manifest: missing {stem}.mp4")
            sys.exit(1)
        manifest["shots"].append(
            {
                "clip": f"{stem}.mp4",
                "clip_seconds": shot["duration"],
                "edge_frames": 0,
                "frames": int(round(shot["duration"] * spec["fps"])),
                "name": shot["name"],
                "poster": f"{stem}.png",
                "scenario": shot["scenario"],
                "scene_seconds": shot["duration"],
            }
        )
    (final / "shots.json").write_text(json.dumps(manifest, indent=2))


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("spec")
    parser.add_argument("out")
    parser.add_argument("--from", dest="first", type=int, default=1)
    parser.add_argument("--chunk", type=int, default=8)
    parser.add_argument("--pause-at", type=float, default=95.0)
    parser.add_argument("--resume-at", type=float, default=88.0)
    parser.add_argument("--start-below", type=float, default=90.0)
    parser.add_argument(
        "--cpus", default="", help="taskset CPU list for arena_app, e.g. 0-7"
    )
    args = parser.parse_args()

    spec = json.loads(Path(args.spec).read_text())
    out = Path(args.out)
    final = out / spec["id"]
    final.mkdir(parents=True, exist_ok=True)
    shots = spec["shots"]
    env = dict(os.environ, PULSE_SERVER="unix:/nonexistent", SOI_AUDIO_OFFLINE="1")
    env.setdefault("DISPLAY", ":0")
    peaks = [0.0, 0.0]

    begin, retries = args.first - 1, 0
    while begin < len(shots):
        part = shots[begin : begin + args.chunk]
        while cpu_temp() > args.start_below:
            log(f"waiting to cool: cpu {cpu_temp():.0f} C")
            time.sleep(15)
        sub = dict(spec, id=f"{spec['id']}_part{begin + 1:02d}", shots=part)
        sub_spec = out / f"{sub['id']}.json"
        sub_spec.write_text(json.dumps(sub))
        log(f"chunk: shots {begin + 1}-{begin + len(part)}")
        code = run_chunk(args, sub_spec, out / f"{sub['id']}.log", env, peaks)
        log(f"chunk done: rc={code}, peak cpu {peaks[0]:.0f} C, gpu {peaks[1]:.0f} C")

        produced = out / sub["id"]
        accepted = 0
        for local, shot in enumerate(part, start=1):
            clip = produced / f"{local:02d}_{shot['name']}.mp4"
            seconds = clip_seconds(clip) if clip.exists() else 0.0
            if abs(seconds - shot["duration"]) > 0.2:
                log(f"incomplete: {shot['name']} ({seconds:.1f} s)")
                break
            for ext in ("mp4", "png"):
                piece = produced / f"{local:02d}_{shot['name']}.{ext}"
                if piece.exists():
                    piece.replace(final / f"{begin + local:02d}_{shot['name']}.{ext}")
            accepted += 1
        begin += accepted
        if accepted < len(part):
            retries += 1
            if retries > MAX_RETRIES:
                log(f"giving up after {MAX_RETRIES} retries")
                return 1
            log(f"retrying from shot {begin + 1} (retry {retries})")
        else:
            retries = 0
        if begin < len(shots):
            time.sleep(60)

    write_manifest(spec, final)
    log(f"done: {len(shots)} shots; peak cpu {peaks[0]:.0f} C, gpu {peaks[1]:.0f} C")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
