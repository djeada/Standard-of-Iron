#!/usr/bin/env python3
"""Build the 'Same army. Different formation.' reel from its two captures.

reel_08_formation_a (piecemeal) and reel_08_formation_b (one plan) are captured
by arena_app as ordinary specs. This stacks their opening shots into one
top/bottom split, keeps each plan's play-out full screen, and writes a clip
folder plus an edit-only spec that scripts/promo-edit.py cuts like any other.
Run from the repo root after both captures exist.
"""

import json
import subprocess
from pathlib import Path

CLIPS = Path("artifacts/reels/clips")
A = CLIPS / "reel_08_formation_a"
B = CLIPS / "reel_08_formation_b"
OUT = CLIPS / "reel_08_formation"
SPEC_IN = Path("tools/arena/promos/reels/reel_08_formation_a.json")
SPEC_OUT = Path("tools/arena/promos/reels/edit/reel_08_formation.json")
FONT = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"


def label(text, y):
    return (
        f"drawtext=fontfile={FONT}:text='{text}':fontsize=54:fontcolor=white:"
        f"borderw=3:bordercolor=black@0.8:x=(w-text_w)/2:y={y}"
    )


def run(args):
    subprocess.run(args, check=True)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    split = OUT / "01_split.mp4"
    graph = (
        "[0:v]crop=1080:960:0:480,"
        + label("A  -  ONE AT A TIME", 40)
        + "[top];[1:v]crop=1080:960:0:480,"
        + label("B  -  ONE PLAN", 40)
        + "[bottom];[top][bottom]vstack=inputs=2,"
        "drawbox=x=0:y=956:w=1080:h=8:color=white@0.9:t=fill[v]"
    )
    run(
        [
            "ffmpeg",
            "-v",
            "error",
            "-y",
            "-i",
            str(A / "01_split.mp4"),
            "-i",
            str(B / "01_split.mp4"),
            "-filter_complex",
            graph,
            "-map",
            "[v]",
            "-map",
            "0:a?",
            "-c:v",
            "libx264",
            "-crf",
            "14",
            "-preset",
            "medium",
            "-pix_fmt",
            "yuv420p",
            "-c:a",
            "aac",
            "-b:a",
            "192k",
            str(split),
        ]
    )
    pieces = [
        ("split", split, "reel_formation_piecemeal"),
        ("one_at_a_time", A / "02_play.mp4", "reel_formation_piecemeal"),
        ("one_plan", B / "02_play.mp4", "reel_formation_combined"),
    ]
    shots = []
    for index, (name, source, scenario) in enumerate(pieces, start=1):
        target = OUT / f"{index:02d}_{name}.mp4"
        if source != target:
            run(
                [
                    "ffmpeg",
                    "-v",
                    "error",
                    "-y",
                    "-i",
                    str(source),
                    "-c",
                    "copy",
                    str(target),
                ]
            )
        seconds = float(
            subprocess.run(
                [
                    "ffprobe",
                    "-v",
                    "error",
                    "-show_entries",
                    "format=duration",
                    "-of",
                    "csv=p=0",
                    str(target),
                ],
                capture_output=True,
                text=True,
                check=True,
            ).stdout.strip()
        )
        poster = target.with_suffix(".png")
        run(
            [
                "ffmpeg",
                "-v",
                "error",
                "-y",
                "-ss",
                str(seconds / 2),
                "-i",
                str(target),
                "-frames:v",
                "1",
                str(poster),
            ]
        )
        shots.append(
            {
                "clip": target.name,
                "clip_seconds": seconds,
                "edge_frames": 0,
                "frames": round(seconds * 30),
                "name": name,
                "poster": poster.name,
                "scenario": scenario,
                "scene_seconds": seconds,
            }
        )
    (OUT / "shots.json").write_text(
        json.dumps(
            {
                "fps": 30,
                "height": 1920,
                "width": 1080,
                "id": "reel_08_formation",
                "title": "STANDARD OF IRON",
                "shots": shots,
            },
            indent=1,
        )
    )

    spec = json.loads(SPEC_IN.read_text())
    spec["id"] = "reel_08_formation"
    captions = {
        "split": "SAME ARMY. DIFFERENT FORMATION.",
        "one_at_a_time": "A: ONE AT A TIME. THE LINE HOLDS.",
        "one_plan": "B: ONE PLAN. THE LINE BREAKS.",
    }
    spec["shots"] = [
        {
            "name": shot["name"],
            "scenario": shot["scenario"],
            "duration": shot["clip_seconds"],
            "caption": captions[shot["name"]],
        }
        for shot in shots
    ]
    SPEC_OUT.parent.mkdir(parents=True, exist_ok=True)
    SPEC_OUT.write_text(json.dumps(spec, indent=2) + "\n")
    print(f"wrote {OUT} and {SPEC_OUT}")


if __name__ == "__main__":
    main()
