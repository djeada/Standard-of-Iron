#!/usr/bin/env python3
"""Pair two look-survey runs into before/after images.

    scripts/look-survey-compare.py <before-dir> <after-dir> <out-dir>

Each directory is the output of scripts/look-survey.sh. For every frame present
in both runs this writes <out-dir>/<name>.jpg: the before frame on the left, the
after frame on the right, each labelled. It also writes index.json, which lists
the pairs together with each side's mean luma and saturation so that a grade
change shows up as a number as well as a picture.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
from pathlib import Path

try:
    from PIL import Image, ImageDraw, ImageStat
except ImportError:
    sys.exit("look-survey-compare needs Pillow (pip install pillow)")

TILE_W, TILE_H = 960, 540


def frames(run: Path) -> dict[str, Path]:
    found: dict[str, Path] = {}
    pngs = [
        Path(folder) / name
        for folder, _, names in os.walk(run, followlinks=True)
        for name in names
        if name.endswith(".png")
    ]
    for png in sorted(pngs):
        if png.parent.name == "game":
            found[f"game_{png.stem}"] = png
        else:
            found[png.stem] = png
    return found


def measure(image: Image.Image) -> dict[str, float]:
    luma = ImageStat.Stat(image.convert("L")).mean[0] / 255.0
    saturation = ImageStat.Stat(image.convert("HSV")).mean[1] / 255.0
    return {"luma": round(luma, 4), "saturation": round(saturation, 4)}


def pair(name: str, before: Path, after: Path, out: Path) -> dict:
    left = Image.open(before).convert("RGB")
    right = Image.open(after).convert("RGB")
    sheet = Image.new("RGB", (TILE_W * 2, TILE_H), (0, 0, 0))
    sheet.paste(left.resize((TILE_W, TILE_H), Image.LANCZOS), (0, 0))
    sheet.paste(right.resize((TILE_W, TILE_H), Image.LANCZOS), (TILE_W, 0))
    draw = ImageDraw.Draw(sheet)
    for x, label in ((0, "BEFORE"), (TILE_W, "AFTER")):
        draw.rectangle([x, 0, x + 90, 22], fill=(0, 0, 0))
        draw.text((x + 6, 5), label, fill=(240, 200, 120))
    target = out / f"{name}.jpg"
    sheet.save(target, quality=85, optimize=True)
    return {
        "name": name,
        "image": target.name,
        "before": measure(left),
        "after": measure(right),
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("before", type=Path)
    parser.add_argument("after", type=Path)
    parser.add_argument("out", type=Path)
    args = parser.parse_args()

    before, after = frames(args.before), frames(args.after)
    shared = sorted(before.keys() & after.keys())
    if not shared:
        print("no frames in common", file=sys.stderr)
        return 1
    args.out.mkdir(parents=True, exist_ok=True)
    index = [pair(name, before[name], after[name], args.out) for name in shared]
    (args.out / "index.json").write_text(json.dumps(index, indent=1))
    missing = sorted(before.keys() ^ after.keys())
    print(f"{len(index)} pairs written to {args.out}")
    if missing:
        print("only in one run: " + ", ".join(missing))
    return 0


if __name__ == "__main__":
    sys.exit(main())
