#!/usr/bin/env python3
"""Cut the game and editor recordings into the clips trailer_v2.json names.

Raw recordings live in artifacts/promo/film/raw; each entry below is
(output name, source, start seconds, duration seconds). Cuts re-encode so the
in-point is frame-accurate, at the same 1920x1080 / 60 fps the arena clips use.
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
RAW = ROOT / "artifacts" / "promo" / "film" / "raw"
OUT = ROOT / "artifacts" / "promo" / "film"

CLIPS = [
    ("build_place", "build_60.mp4", 0.5, 6.5),
    ("build_rise", "build_60.mp4", 15.0, 2.5),
    ("commander_select", "cmd_60.mp4", 0.6, 3.6),
    ("commander_strike", "cmd_60.mp4", 9.5, 3.6),
    ("river_formation", "river_60.mp4", 0.3, 7.0),
    ("river_cavalry", "river_60.mp4", 8.3, 3.6),
    ("river_commander", "river_60.mp4", 13.6, 11.5),
    ("editor_route", "editor_route.mp4", 1.2, 5.5),
    ("editor_mission", "editor_mission.mp4", 1.5, 5.0),
]


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    only = set(sys.argv[1:])
    for name, source, start, seconds in CLIPS:
        if only and name not in only:
            continue
        src = RAW / source
        if not src.is_file():
            print(f"skip {name}: {src} missing")
            continue
        dst = OUT / f"{name}.mp4"
        subprocess.run(
            [
                "ffmpeg",
                "-v",
                "error",
                "-y",
                "-ss",
                f"{start:.3f}",
                "-i",
                str(src),
                "-t",
                f"{seconds:.3f}",
                "-vf",
                "scale=1920:1080,fps=60",
                "-c:v",
                "libx264",
                "-preset",
                "slow",
                "-crf",
                "16",
                "-pix_fmt",
                "yuv420p",
                "-an",
                str(dst),
            ],
            check=True,
        )
        print(f"wrote {dst.name} ({seconds:.1f} s from {source} @ {start:.1f})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
