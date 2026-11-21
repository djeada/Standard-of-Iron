#!/usr/bin/env python3
"""Plan view and open-ground report for one battlefield map.

The engine's hills are not the ellipses they are authored as, so this asks the
engine: it runs `terrain_probe` for the real walkability and terrain types, then
lays the map's roads, structures, forests and props over them.

    scripts/map-plan.py assets/maps/map_forest.json --png plan.png
    scripts/map-plan.py assets/maps/map_forest.json --open 12 --min-radius 8

`--png` writes a top-down plan with grid coordinates (the same x/z the map JSON
and `map_cli` use). `--open N` prints the N largest discs of free ground: walkable
cells that keep clear of roads, structures, forests, props and the map edge.
"""

from __future__ import annotations

import argparse
import json
import math
import struct
import subprocess
import sys
import tempfile
from itertools import pairwise
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

TERRAIN_FLAT, TERRAIN_HILL, TERRAIN_MOUNTAIN = 0, 1, 2
TERRAIN_RIVER, TERRAIN_FOREST, TERRAIN_LAKE = 3, 4, 5

ROAD_CLEARANCE = 3.0
STRUCTURE_CLEARANCE = 6.0
PROP_CLEARANCE = 2.0
EDGE_CLEARANCE = 4

OWNER_COLOURS = {
    0: (200, 200, 200),
    1: (70, 130, 255),
    2: (235, 70, 70),
    3: (80, 200, 110),
    4: (240, 200, 60),
}


def probe(map_path: Path, tool: Path) -> dict:
    with tempfile.TemporaryDirectory() as scratch:
        out = Path(scratch) / "terrain.bin"
        result = subprocess.run(
            [str(tool), str(map_path), str(out)],
            capture_output=True,
            text=True,
            check=False,
        )
        if result.returncode != 0:
            raise SystemExit(f"terrain_probe failed: {result.stderr.strip()}")
        blob = out.read_bytes()
    header, _, body = blob.partition(b"\n")
    meta = json.loads(header)
    width, height = meta["width"], meta["height"]
    cells = width * height
    heights = struct.unpack(f"<{cells}f", body[: cells * 4])
    offset = cells * 4
    entrances = body[offset : offset + cells]
    types = body[offset + cells : offset + 2 * cells]
    walkable = body[offset + 2 * cells : offset + 3 * cells]
    return {
        "width": width,
        "height": height,
        "heights": heights,
        "entrances": entrances,
        "types": types,
        "walkable": walkable,
    }


def polyline(element: dict) -> list[tuple[float, float]]:
    points = element.get("waypoints")
    if points:
        return [(float(p[0]), float(p[1])) for p in points]
    return [tuple(map(float, element["start"])), tuple(map(float, element["end"]))]


def stamp_disc(
    mask: bytearray, width: int, height: int, cx: float, cz: float, r: float
):
    x0, x1 = max(0, int(cx - r)), min(width - 1, int(cx + r) + 1)
    z0, z1 = max(0, int(cz - r)), min(height - 1, int(cz + r) + 1)
    r_sq = r * r
    for z in range(z0, z1 + 1):
        dz = z + 0.5 - cz
        row = z * width
        for x in range(x0, x1 + 1):
            dx = x + 0.5 - cx
            if dx * dx + dz * dz <= r_sq:
                mask[row + x] = 1


def stamp_line(mask, width, height, points, radius):
    for (ax, az), (bx, bz) in pairwise(points):
        length = math.hypot(bx - ax, bz - az)
        steps = max(1, int(length))
        for i in range(steps + 1):
            t = i / steps
            stamp_disc(
                mask, width, height, ax + (bx - ax) * t, az + (bz - az) * t, radius
            )


def blocked_mask(terrain: dict, map_json: dict, flat_only: bool = False) -> bytearray:
    width, height = terrain["width"], terrain["height"]
    mask = bytearray(width * height)
    walkable = terrain["walkable"]
    types = terrain["types"]
    for index in range(width * height):
        if not walkable[index] or (flat_only and types[index] != TERRAIN_FLAT):
            mask[index] = 1
    for z in range(height):
        for x in range(width):
            if (
                x < EDGE_CLEARANCE
                or z < EDGE_CLEARANCE
                or x >= width - EDGE_CLEARANCE
                or z >= height - EDGE_CLEARANCE
            ):
                mask[z * width + x] = 1
    for road in map_json.get("roads", []):
        stamp_line(
            mask,
            width,
            height,
            polyline(road),
            road.get("width", 3.0) / 2 + ROAD_CLEARANCE,
        )
    for bridge in map_json.get("bridges", []):
        stamp_line(mask, width, height, polyline(bridge), bridge.get("width", 8.0))
    for structure in map_json.get("structures", []):
        if "x" in structure:
            stamp_disc(
                mask, width, height, structure["x"], structure["z"], STRUCTURE_CLEARANCE
            )
        elif "start" in structure:
            stamp_line(mask, width, height, polyline(structure), 3.0)
    for spawn in map_json.get("spawns", []):
        if "x" in spawn:
            stamp_disc(mask, width, height, spawn["x"], spawn["z"], 4.0)
    for forest in map_json.get("forests", []):
        stamp_disc(
            mask, width, height, forest["x"], forest["z"], forest.get("radius", 8.0)
        )
    for prop in map_json.get("world_props", []) + map_json.get("firecamps", []):
        if "x" in prop:
            stamp_disc(mask, width, height, prop["x"], prop["z"], PROP_CLEARANCE)
    for zone in map_json.get("undead_zones", []):
        if "x" in zone:
            stamp_disc(
                mask, width, height, zone["x"], zone["z"], zone.get("radius", 8.0)
            )
    return mask


def clearance(mask: bytearray, width: int, height: int) -> list[float]:
    """Two-pass chamfer distance to the nearest blocked cell."""
    far = float(width + height)
    dist = [0.0 if mask[i] else far for i in range(width * height)]
    diagonal = math.sqrt(2.0)
    for z in range(height):
        row = z * width
        for x in range(width):
            index = row + x
            best = dist[index]
            if best == 0.0:
                continue
            if x > 0:
                best = min(best, dist[index - 1] + 1.0)
            if z > 0:
                best = min(best, dist[index - width] + 1.0)
                if x > 0:
                    best = min(best, dist[index - width - 1] + diagonal)
                if x < width - 1:
                    best = min(best, dist[index - width + 1] + diagonal)
            dist[index] = best
    for z in range(height - 1, -1, -1):
        row = z * width
        for x in range(width - 1, -1, -1):
            index = row + x
            best = dist[index]
            if best == 0.0:
                continue
            if x < width - 1:
                best = min(best, dist[index + 1] + 1.0)
            if z < height - 1:
                best = min(best, dist[index + width] + 1.0)
                if x < width - 1:
                    best = min(best, dist[index + width + 1] + diagonal)
                if x > 0:
                    best = min(best, dist[index + width - 1] + diagonal)
            dist[index] = best
    return dist


def open_ground(terrain, map_json, count, min_radius, flat_only=False):
    width, height = terrain["width"], terrain["height"]
    dist = clearance(blocked_mask(terrain, map_json, flat_only), width, height)
    order = sorted(range(width * height), key=dist.__getitem__, reverse=True)
    picked = []
    for index in order:
        radius = dist[index]
        if radius < min_radius or len(picked) >= count:
            break
        x, z = index % width + 0.5, index // width + 0.5
        if all(math.hypot(x - px, z - pz) >= radius + pr for px, pz, pr in picked):
            picked.append((x, z, radius))
    return picked


def render_plan(terrain, map_json, out_path, open_discs):
    from PIL import Image, ImageDraw

    width, height = terrain["width"], terrain["height"]
    scale = max(1, min(8, 1400 // max(width, height)))
    margin = 26
    image = Image.new(
        "RGB", (width * scale + margin * 2, height * scale + margin * 2), (16, 16, 16)
    )
    pixels = image.load()
    types, walkable, entrances = (
        terrain["types"],
        terrain["walkable"],
        terrain["entrances"],
    )
    heights = terrain["heights"]
    low, high = min(heights), max(heights)
    span = max(high - low, 1e-3)
    for z in range(height):
        for x in range(width):
            index = z * width + x
            kind = types[index]
            lift = (heights[index] - low) / span
            if kind in (TERRAIN_RIVER, TERRAIN_LAKE):
                colour = (40, 100, 170)
            elif kind == TERRAIN_MOUNTAIN:
                shade = int(70 + 70 * lift)
                colour = (shade, shade, shade + 8)
            elif kind == TERRAIN_HILL:
                colour = (150, 130, 70) if walkable[index] else (96, 78, 48)
                if entrances[index]:
                    colour = (215, 190, 110)
            else:
                shade = int(28 * lift)
                colour = (
                    (46 + shade, 86 + shade, 52 + shade)
                    if walkable[index]
                    else (60, 52, 44)
                )
            for dz in range(scale):
                for dx in range(scale):
                    pixels[margin + x * scale + dx, margin + z * scale + dz] = colour

    draw = ImageDraw.Draw(image)

    def at(x, z):
        return (margin + x * scale, margin + z * scale)

    for forest in map_json.get("forests", []):
        r = forest.get("radius", 8.0) * scale
        cx, cz = at(forest["x"], forest["z"])
        draw.ellipse((cx - r, cz - r, cx + r, cz + r), outline=(60, 220, 90), width=2)
        draw.text(
            (cx - r + 3, cz - 6),
            str(forest.get("id", "forest"))[:18],
            fill=(150, 255, 170),
        )
    for road in map_json.get("roads", []):
        draw.line(
            [at(*p) for p in polyline(road)],
            fill=(205, 175, 125),
            width=max(2, int(road.get("width", 3) * scale * 0.6)),
        )
    for river in map_json.get("rivers", []):
        draw.line([at(*p) for p in polyline(river)], fill=(90, 160, 235), width=2)
    for bridge in map_json.get("bridges", []):
        draw.line(
            [at(*p) for p in polyline(bridge)],
            fill=(245, 245, 245),
            width=max(3, scale * 3),
        )
    for ford in map_json.get("fords", []):
        if "start" in ford:
            draw.line(
                [at(*ford["start"]), at(*ford["end"])],
                fill=(160, 235, 235),
                width=max(3, scale * 3),
            )
        elif "position" in ford:
            cx, cz = at(*ford["position"])
            draw.ellipse(
                (cx - 5, cz - 5, cx + 5, cz + 5), outline=(160, 235, 235), width=2
            )
    for prop in map_json.get("world_props", []) + map_json.get("firecamps", []):
        if "x" in prop:
            cx, cz = at(prop["x"], prop["z"])
            draw.rectangle((cx - 1, cz - 1, cx + 1, cz + 1), fill=(190, 150, 230))
    for structure in map_json.get("structures", []):
        colour = OWNER_COLOURS.get(structure.get("player_id", 0), (255, 255, 255))
        if "x" in structure:
            cx, cz = at(structure["x"], structure["z"])
            half = max(3, scale * 2)
            draw.rectangle(
                (cx - half, cz - half, cx + half, cz + half), outline=colour, width=2
            )
        elif "start" in structure:
            draw.line([at(*p) for p in polyline(structure)], fill=colour, width=2)
    for spawn in map_json.get("spawns", []):
        if "x" in spawn:
            colour = OWNER_COLOURS.get(spawn.get("player_id", 0), (255, 255, 255))
            cx, cz = at(spawn["x"], spawn["z"])
            draw.ellipse((cx - 2, cz - 2, cx + 2, cz + 2), fill=colour)
    for feature in map_json.get("terrain", []):
        if "x" in feature and feature.get("type") in ("hill", "mountain"):
            cx, cz = at(feature["x"], feature["z"])
            label = str(feature.get("id", feature["type"]))[:20]
            draw.text((cx - 3 * len(label), cz - 5), label, fill=(255, 240, 200))
    for x, z, radius in open_discs:
        cx, cz = at(x, z)
        r = radius * scale
        draw.ellipse((cx - r, cz - r, cx + r, cz + r), outline=(255, 80, 200), width=1)
        draw.text((cx - 14, cz - 5), f"{x:.0f},{z:.0f}", fill=(255, 150, 225))

    step = 20 if max(width, height) <= 300 else 50
    for tick in range(0, width + 1, step):
        draw.text((margin + tick * scale - 8, 6), str(tick), fill=(210, 210, 210))
        draw.line(
            (margin + tick * scale, margin - 4, margin + tick * scale, margin),
            fill=(210, 210, 210),
        )
    for tick in range(0, height + 1, step):
        draw.text((2, margin + tick * scale - 5), str(tick), fill=(210, 210, 210))
        draw.line(
            (margin - 4, margin + tick * scale, margin, margin + tick * scale),
            fill=(210, 210, 210),
        )
    image.save(out_path)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("map", type=Path)
    parser.add_argument("--png", type=Path, help="write a plan view here")
    parser.add_argument(
        "--open",
        type=int,
        default=0,
        metavar="N",
        help="report the N largest free discs",
    )
    parser.add_argument("--min-radius", type=float, default=6.0)
    parser.add_argument(
        "--flat-only",
        action="store_true",
        help="ignore hill crowns when looking for free ground",
    )
    parser.add_argument(
        "--terrain-probe",
        type=Path,
        default=REPO_ROOT / "build" / "bin" / "terrain_probe",
    )
    args = parser.parse_args()

    map_json = json.loads(args.map.read_text(encoding="utf-8"))
    if (
        str(map_json.get("coord_system", map_json.get("coordSystem", "grid"))).lower()
        != "grid"
    ):
        raise SystemExit("map-plan.py reads grid-coordinate maps only")
    terrain = probe(args.map, args.terrain_probe)
    discs = (
        open_ground(terrain, map_json, args.open, args.min_radius, args.flat_only)
        if args.open > 0
        else []
    )
    for x, z, radius in discs:
        print(f"{x:7.1f} {z:7.1f}  radius {radius:5.1f}")
    if args.png:
        render_plan(terrain, map_json, args.png, discs)
    total = terrain["width"] * terrain["height"]
    walkable = sum(terrain["walkable"])
    print(
        f"walkable {100.0 * walkable / total:.0f}% of {terrain['width']}x{terrain['height']}",
        file=sys.stderr,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
