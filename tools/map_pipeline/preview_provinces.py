#!/usr/bin/env python3
"""Render provinces.json to a PNG so province shapes can be eyeballed.

Usage: python3 tools/map_pipeline/preview_provinces.py [out.png]
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
PROVINCES = ROOT / "assets" / "campaign_map" / "provinces.json"
LAND = ROOT / "assets" / "campaign_map" / "land_uv.geojson"

WIDTH = 1600
HEIGHT = 1000

PALETTE = [
    (198, 62, 48),
    (222, 140, 60),
    (86, 130, 92),
    (72, 104, 168),
    (150, 92, 160),
    (206, 176, 78),
    (108, 160, 176),
    (170, 96, 78),
    (96, 152, 108),
    (128, 128, 200),
    (196, 118, 148),
    (146, 146, 96),
    (72, 148, 148),
    (188, 96, 120),
    (120, 120, 120),
]


def px(u: float, v: float) -> tuple[float, float]:
    return (u * WIDTH, (1.0 - v) * HEIGHT)


def land_mask() -> Image.Image:
    """White over land, black over water. Province fills are composited through
    this, so a fill that strays into the sea shows up as missing colour rather
    than being quietly painted over the water."""
    mask = Image.new("L", (WIDTH, HEIGHT), 0)
    if not LAND.exists():
        return Image.new("L", (WIDTH, HEIGHT), 255)
    draw = ImageDraw.Draw(mask)
    geo = json.loads(LAND.read_text())
    for feature in geo.get("features", []):
        geom = feature["geometry"]
        polys = (
            [geom["coordinates"]] if geom["type"] == "Polygon" else geom["coordinates"]
        )
        for rings in polys:
            for index, ring in enumerate(rings):
                pts = [px(u, v) for u, v in ring]
                if len(pts) >= 3:
                    draw.polygon(pts, fill=255 if index == 0 else 0)
    return mask


def main() -> None:
    out = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "province_preview.png"
    data = json.loads(PROVINCES.read_text())

    mask = land_mask()
    sea = Image.new("RGB", (WIDTH, HEIGHT), (30, 52, 78))
    land = Image.new("RGB", (WIDTH, HEIGHT), (232, 226, 208))

    fills = Image.new("RGBA", (WIDTH, HEIGHT), (0, 0, 0, 0))
    fill_draw = ImageDraw.Draw(fills, "RGBA")
    for index, province in enumerate(data.get("provinces", [])):
        color = PALETTE[index % len(PALETTE)] + (190,)
        tris = province.get("triangles", [])
        for i in range(0, len(tris) - 2, 3):
            fill_draw.polygon(
                [px(*tris[i]), px(*tris[i + 1]), px(*tris[i + 2])], fill=color
            )

    land.paste(fills, (0, 0), fills)
    image = Image.composite(land, sea, mask)

    draw = ImageDraw.Draw(image, "RGBA")
    for border in data.get("borders", []):
        pts = [px(u, v) for u, v in border]
        if len(pts) >= 2:
            draw.line(pts, fill=(20, 20, 20, 220), width=2)

    for province in data.get("provinces", []):
        u, v = province.get("label_uv", [0.0, 0.0])
        draw.text(px(u, v), province.get("id", ""), fill=(20, 20, 20))

    image.save(out)
    print(f"Wrote {out}")


if __name__ == "__main__":
    main()
