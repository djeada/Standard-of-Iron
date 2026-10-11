"""One frame from the middle of every cut, as labelled contact sheets.

    python3 scripts/drama/contact_sheet.py --plan DIR --clips DIR --out DIR [--scenes s03 s04]
"""

import argparse
import json
import subprocess
from pathlib import Path

from PIL import Image, ImageDraw

TILE = (480, 270)
COLS = 4


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--plan", required=True)
    parser.add_argument("--clips", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--scenes", nargs="*")
    args = parser.parse_args()
    cutlist = json.loads((Path(args.plan) / "cutlist.json").read_text())
    clips = Path(args.clips)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    for scene in cutlist["scenes"]:
        if args.scenes and scene["scene"] not in args.scenes:
            continue
        tiles = []
        for i, seg in enumerate(scene["segments"]):
            if "shot" not in seg:
                continue
            found = sorted(clips.glob(f"**/[0-9][0-9]_{seg['shot']}.mp4"))
            if not found:
                continue
            t = seg["clip_offset"] + (seg["to"] - seg["from"]) / 2
            frame = out / f"{scene['scene']}_{i:02d}.png"
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-ss", f"{t:.2f}", "-i", str(found[-1]),
                            "-frames:v", "1", "-vf", f"scale={TILE[0]}:{TILE[1]}", str(frame)], check=False)
            if frame.exists():
                tiles.append((frame, f"{i:02d} {seg['cam']} @{seg['from']:.1f}"))
        if not tiles:
            continue
        rows = (len(tiles) + COLS - 1) // COLS
        sheet = Image.new("RGB", (TILE[0] * COLS, TILE[1] * rows))
        for k, (path, label) in enumerate(tiles):
            im = Image.open(path)
            ImageDraw.Draw(im).text((6, 6), label, fill=(255, 255, 0))
            sheet.paste(im, ((k % COLS) * TILE[0], (k // COLS) * TILE[1]))
            path.unlink()
        sheet.save(out / f"{scene['scene']}.png")
        print(out / f"{scene['scene']}.png", len(tiles))


if __name__ == "__main__":
    main()
