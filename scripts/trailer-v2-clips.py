#!/usr/bin/env python3
"""Cut the game and editor recordings into the clips trailer_v2.json names.

Raw recordings live in artifacts/promo/film/raw; each entry below is
(output name, source, start seconds, duration seconds[, speed[, crop]]). Cuts
re-encode so the in-point is frame-accurate, at the same 1920x1080 / 60 fps the
arena clips use. A speed above 1 compresses that many source seconds into fewer
on the timeline, for a beat that needs its whole arc but not its whole running
time. A crop ("w:h:x:y" against the 1920x1080 frame) is a push-in: the window is
scaled back up to full frame, which is how a shot filmed wide enough to keep the
whole army on screen ends up filling it.
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
    ("editor_bridge", "editor_bridge.mp4", 2.0, 3.0),
    ("build_stronghold_place", "build_close.mp4", 1.2, 2.2),
    ("build_stronghold_rise", "build_close.mp4", 8.8, 3.0),
    # The formation beat is four shots across two maps: pick a line on the
    # parade ground and watch it land, then pick a column for a bridge and watch
    # that land. The first of each pair runs slowly enough to read the formation
    # panel -- that panel is the point of the beat -- and the second is held long
    # enough for the ranks to finish dressing. The push-in at the end of each
    # payoff is filmed in engine (the fixture moves the camera), not cropped in.
    ("form_the_line", "formation_field.mp4", 1.8, 7.2, 2.0),
    ("battle_line", "formation_field.mp4", 34.6, 12.2, 2.9),
    ("bridge_order", "palm_column.mp4", 1.8, 6.2, 1.82),
    ("bridge_column", "palm_column.mp4", 22.0, 13.5, 2.93),
    ("town_wide", "forest_town.mp4", 0.3, 3.7),
    ("town_crews", "forest_town.mp4", 4.6, 5.0),
    ("town_build", "forest_town.mp4", 10.8, 6.8),
    ("town_busy", "forest_town.mp4", 20.6, 4.0),
]


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    only = set(sys.argv[1:])
    for name, source, start, seconds, *rest in CLIPS:
        speed = rest[0] if rest else 1.0
        crop = rest[1] if len(rest) > 1 else None
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
                f"{seconds / speed:.3f}",
                "-vf",
                (
                    f"setpts=PTS/{speed:.4f},"
                    + (f"crop={crop}," if crop else "")
                    + "scale=1920:1080,setsar=1,fps=60"
                ),
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
        print(
            f"wrote {dst.name} ({seconds / speed:.1f} s from {seconds:.1f} s of "
            f"{source} @ {start:.1f}, speed {speed:g}"
            + (f", crop {crop}" if crop else "")
            + ")"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
