#!/usr/bin/env python3
"""Turn game --film takes into a promo-edit clip folder and edit-only spec.

Reels 10 and 14 are filmed from the real game (scripts/film-game.sh), not the
arena, so there is no arena capture manifest. This cuts the named segments out
of the takes, centre-crops them to 1080x1920, gives them a silent audio track
(film takes have none) and writes the manifest and spec scripts/promo-edit.py
expects. Run from the repo root: compose_film.py reel_10_campaign | reel_14_command
"""

import json
import subprocess
import sys
from pathlib import Path

FILM = Path("artifacts/reels/film")
CLIPS = Path("artifacts/reels/clips")
HEADER_FROM = Path("tools/arena/promos/reels/reel_02_cannae.json")
EDIT = Path("tools/arena/promos/reels/edit")

REELS = {
    "reel_10_campaign": {
        "music": "assets/audio/music/campaign/campaign_crossing_of_the_alps.ogg",
        "segments": [
            (
                "the_alps",
                "montage_crossing_the_alps.mp4",
                0.2,
                2.4,
                "TAKE HANNIBAL FROM THE RHONE TO ZAMA.",
            ),
            (
                "the_rhone",
                "montage_crossing_the_rhone.mp4",
                0.6,
                2.2,
                "218 BC. THE RHONE.",
            ),
            ("trebia", "montage_battle_of_trebia.mp4", 0.6, 2.2, "TREBIA."),
            ("trasimene", "montage_battle_of_trasimene.mp4", 0.6, 2.2, "TRASIMENE."),
            ("cannae", "montage_battle_of_cannae.mp4", 0.6, 2.2, "CANNAE."),
            ("zama", "montage_battle_of_zama.mp4", 0.4, 2.6, "202 BC. ZAMA."),
        ],
    },
    "reel_14_command": {
        "music": "assets/audio/music/menu/main_theme_iron_kingdom.ogg",
        "segments": [
            (
                "the_army",
                "reel_14_raw.mp4",
                0.0,
                1.3,
                "COMMAND 500 SOLDIERS IN 10 SECONDS.",
            ),
            ("select", "reel_14_raw.mp4", 1.3, 2.9, "SELECT."),
            ("formation", "reel_14_raw.mp4", 2.9, 5.7, "DRAW THE LINE."),
            ("move", "reel_14_raw.mp4", 5.7, 8.2, "MOVE."),
            ("charge", "reel_14_raw.mp4", 8.2, 11.4, "CHARGE."),
        ],
    },
}


def probe_size(path):
    out = subprocess.run(
        [
            "ffprobe",
            "-v",
            "error",
            "-select_streams",
            "v:0",
            "-show_entries",
            "stream=width,height",
            "-of",
            "csv=p=0",
            str(path),
        ],
        capture_output=True,
        text=True,
        check=True,
    ).stdout.strip()
    width, height = (int(v) for v in out.split(","))
    return width, height


def main():
    reel_id = sys.argv[1]
    reel = REELS[reel_id]
    out = CLIPS / reel_id
    out.mkdir(parents=True, exist_ok=True)
    shots = []
    for index, (name, source, start, end, caption) in enumerate(reel["segments"], 1):
        source_path = FILM / source
        width, height = probe_size(source_path)
        crop_w, crop_h = min(width, height * 9 // 16), height
        if height * 9 > width * 16:
            crop_w, crop_h = width, width * 16 // 9
        crop = (
            f"crop={crop_w}:{crop_h}:{(width - crop_w) // 2}:{(height - crop_h) // 2}"
        )
        target = out / f"{index:02d}_{name}.mp4"
        duration = end - start
        subprocess.run(
            [
                "ffmpeg",
                "-v",
                "error",
                "-y",
                "-ss",
                f"{start:.3f}",
                "-t",
                f"{duration:.3f}",
                "-i",
                str(source_path),
                "-f",
                "lavfi",
                "-t",
                f"{duration:.3f}",
                "-i",
                "anullsrc=r=48000:cl=stereo",
                "-vf",
                f"{crop},scale=1080:1920:flags=lanczos,fps=30",
                "-map",
                "0:v",
                "-map",
                "1:a",
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
                "-shortest",
                str(target),
            ],
            check=True,
        )
        poster = target.with_suffix(".png")
        subprocess.run(
            [
                "ffmpeg",
                "-v",
                "error",
                "-y",
                "-ss",
                f"{duration / 2:.3f}",
                "-i",
                str(target),
                "-frames:v",
                "1",
                str(poster),
            ],
            check=True,
        )
        shots.append(
            {
                "clip": target.name,
                "clip_seconds": duration,
                "edge_frames": 0,
                "frames": round(duration * 30),
                "name": name,
                "poster": poster.name,
                "scenario": "film",
                "scene_seconds": duration,
                "caption": caption,
            }
        )
    (out / "shots.json").write_text(
        json.dumps(
            {
                "fps": 30,
                "width": 1080,
                "height": 1920,
                "id": reel_id,
                "title": "STANDARD OF IRON",
                "shots": [
                    {k: v for k, v in s.items() if k != "caption"} for s in shots
                ],
            },
            indent=1,
        )
    )
    spec = json.loads(HEADER_FROM.read_text())
    spec["id"] = reel_id
    spec["music"] = reel["music"]
    spec["music_start"] = 0.0
    spec["shots"] = [
        {
            "name": s["name"],
            "scenario": "film",
            "duration": s["clip_seconds"],
            "caption": s["caption"],
        }
        for s in shots
    ]
    EDIT.mkdir(parents=True, exist_ok=True)
    (EDIT / f"{reel_id}.json").write_text(json.dumps(spec, indent=2) + "\n")
    print(f"wrote {out} and {EDIT / (reel_id + '.json')}")


if __name__ == "__main__":
    main()
