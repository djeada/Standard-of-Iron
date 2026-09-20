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

In place of (start, seconds, speed) an entry may carry a *list* of them, which
are concatenated into one clip. That is how a beat showing a mechanic keeps its
whole action: the segments are adjacent in the source, so the speed changes but
the footage never skips. Cutting from one source time to a much later one --
which is what a pair of separate clips does -- reads as a jump, and a jump over
the middle of a mechanic is a jump over the part the viewer is being shown.
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
    (
        "form_the_line",
        "formation_field.mp4",
        [(1.8, 7.2, 2.0), (9.0, 7.0, 4.0), (16.0, 14.0, 8.0), (30.0, 4.6, 3.2)],
    ),
    ("battle_line", "formation_field.mp4", 34.6, 12.2, 2.9),
    (
        "bridge_order",
        "palm_column.mp4",
        [(1.8, 6.2, 1.82), (8.0, 5.0, 3.6), (13.0, 6.0, 6.5), (19.0, 3.0, 2.6)],
    ),
    ("bridge_column", "palm_column.mp4", 22.0, 13.5, 2.93),
    ("town_wide", "forest_town.mp4", 0.3, 3.7),
    ("town_crews", "forest_town.mp4", 4.6, 5.0),
    ("town_build", "forest_town.mp4", 14.3, 8.0),
    ("town_busy", "forest_town.mp4", 22.4, 4.0),
]


def encode_args(crop: str | None, dst: Path) -> list[str]:
    return [
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
    ]


def cut_single(
    src: Path, dst: Path, start: float, seconds: float, speed: float, crop: str | None
) -> None:
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
            *encode_args(crop, dst),
        ],
        check=True,
    )


def cut_segments(
    src: Path, dst: Path, segments: list[tuple[float, float, float]], crop: str | None
) -> None:
    """One clip out of adjacent source spans played at different speeds.

    Trimming in a filter graph rather than with -ss keeps every span against the
    same decoded input, so the joins land on consecutive source frames and the
    ramp reads as a speed change instead of a cut.
    """
    parts = []
    labels = []
    for index, (start, seconds, speed) in enumerate(segments):
        label = f"s{index}"
        parts.append(
            f"[0:v]trim=start={start:.3f}:end={start + seconds:.3f},"
            f"setpts=(PTS-STARTPTS)/{speed:.4f}[{label}]"
        )
        labels.append(f"[{label}]")
    graph = ";".join(parts)
    graph += f";{''.join(labels)}concat=n={len(segments)}:v=1:a=0[joined]"
    graph += ";[joined]" + (f"crop={crop}," if crop else "")
    graph += "scale=1920:1080,setsar=1,fps=60[out]"
    subprocess.run(
        [
            "ffmpeg",
            "-v",
            "error",
            "-y",
            "-i",
            str(src),
            "-filter_complex",
            graph,
            "-map",
            "[out]",
            *encode_args(crop, dst),
        ],
        check=True,
    )


def main() -> int:
    OUT.mkdir(parents=True, exist_ok=True)
    only = set(sys.argv[1:])
    for name, source, third, *rest in CLIPS:
        segmented = isinstance(third, list)
        if segmented:
            segments = third
            crop = rest[0] if rest else None
        else:
            segments = [(third, rest[0], rest[1] if len(rest) > 1 else 1.0)]
            crop = rest[2] if len(rest) > 2 else None
        if only and name not in only:
            continue
        src = RAW / source
        if not src.is_file():
            print(f"skip {name}: {src} missing")
            continue
        dst = OUT / f"{name}.mp4"
        if segmented:
            cut_segments(src, dst, segments, crop)
            spans = " + ".join(
                f"{start:.1f}-{start + seconds:.1f}@{speed:g}x"
                for start, seconds, speed in segments
            )
            out_seconds = sum(seconds / speed for _, seconds, speed in segments)
            print(
                f"wrote {dst.name} ({out_seconds:.1f} s ramped from {source}: {spans}"
                + (f", crop {crop}" if crop else "")
                + ")"
            )
        else:
            start, seconds, speed = segments[0]
            cut_single(src, dst, start, seconds, speed, crop)
            print(
                f"wrote {dst.name} ({seconds / speed:.1f} s from {seconds:.1f} s of "
                f"{source} @ {start:.1f}, speed {speed:g}"
                + (f", crop {crop}" if crop else "")
                + ")"
            )
    return 0


if __name__ == "__main__":
    sys.exit(main())
