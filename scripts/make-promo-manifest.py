#!/usr/bin/env python3
"""Rebuild ``shots.json`` for a capture directory from the spec and the clips.

``arena_app --promo-spec`` writes its manifest only when the whole run finishes,
so a reel assembled a scenario at a time -- with
``reshoot-promo-scenario.py``, or after a capture was interrupted -- has all its
footage on disk and nothing to describe it, and ``promo-edit.py`` refuses to
cut. This reads the real clip durations back out of the files and writes the
manifest the editor expects.

Clip length is measured rather than derived from the spec on purpose: a shot's
recorded length is what the edit has to lay on the timeline, and a clip that
came out short (a scenario that ended early, an interrupted encode) should show
up as a short clip rather than being silently assumed correct.

    scripts/make-promo-manifest.py
    scripts/promo-edit.py --spec tools/arena/promos/trailer.json \\
        --clips artifacts/promo/trailer
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def clip_seconds(path: Path) -> float:
    probe = subprocess.run(
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
        capture_output=True,
        text=True,
    )
    try:
        return float(probe.stdout.strip())
    except ValueError:
        return 0.0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--spec", type=Path, default=ROOT / "tools/arena/promos/trailer.json"
    )
    parser.add_argument("--clips", type=Path, default=ROOT / "artifacts/promo/trailer")
    args = parser.parse_args()

    spec = json.loads(args.spec.read_text())
    shots = []
    missing = []
    for number, shot in enumerate(spec["shots"], 1):
        name = shot["name"]
        clip = args.clips / f"{number:02d}_{name}.mp4"
        if not clip.is_file():
            found = list(args.clips.glob(f"*_{name}.mp4"))
            if not found:
                missing.append(name)
                continue
            clip = found[0]
        seconds = clip_seconds(clip)
        if seconds <= 0.0:
            missing.append(name)
            continue
        poster = clip.with_suffix(".png")
        shots.append(
            {
                "name": name,
                "scenario": shot["scenario"],
                "clip": clip.name,
                "poster": poster.name if poster.is_file() else "",
                "frames": int(round(seconds * float(spec.get("fps", 60)))),
                "scene_seconds": float(shot["duration"]),
                "clip_seconds": seconds,
            }
        )

    if missing:
        print(
            "make-promo-manifest: no usable clip for: " + ", ".join(missing),
            file=sys.stderr,
        )
        return 1

    manifest = {
        "id": spec.get("id", "promo"),
        "title": spec.get("title", ""),
        "width": spec.get("width", 1920),
        "height": spec.get("height", 1080),
        "fps": spec.get("fps", 60),
        "shots": shots,
    }
    out = args.clips / "shots.json"
    out.write_text(json.dumps(manifest, indent=2) + "\n")
    total = sum(shot["clip_seconds"] for shot in shots)
    print(
        f"make-promo-manifest: wrote {out} "
        f"({len(shots)} shot(s), {total:.1f}s of footage)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
