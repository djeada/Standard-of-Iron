#!/usr/bin/env python3
"""Give straight rivers natural bends without breaking what crosses them.

A river is resampled and pushed sideways by two overlaid sine waves. The push is
zero at every pin - the river's ends, each bridge and ford, and each confluence -
and eases in from there, so decks still span the channel squarely. A displaced
point is pulled back toward the original line wherever it would enter a hill or
mountain, or come too close to a structure, a road, a lake or another river.

The result is written as `map_cli apply` operations, so the map file keeps every
byte the bends did not touch:

    scripts/map-meander.py assets/maps/map_battle_cannae.json --ops bends.json
    build/bin/map_cli apply assets/maps/map_battle_cannae.json bends.json

Rivers that already wander (sinuosity above --skip-above) are left alone.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import math
import subprocess
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

SAMPLE_SPACING = 10.0
PIN_EASE = 60.0
STRUCTURE_CLEARANCE = 9.0
ROAD_CLEARANCE = 7.0
WATER_CLEARANCE = 10.0
SPAWN_CLEARANCE = 10.0
PROP_CLEARANCE = 8.0
EDGE_MARGIN = 3.0


def load_plan_module():
    spec = importlib.util.spec_from_file_location(
        "map_plan", REPO_ROOT / "scripts" / "map-plan.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def polyline(element):
    points = element.get("waypoints")
    if points:
        return [(float(p[0]), float(p[1])) for p in points]
    if "start" in element and "end" in element:
        return [tuple(map(float, element["start"])), tuple(map(float, element["end"]))]
    return []


def length_of(points):
    return sum(math.dist(a, b) for a, b in zip(points, points[1:]))


def resample(points, spacing):
    total = length_of(points)
    count = max(2, int(round(total / spacing)) + 1)
    out, travelled, segment = [], 0.0, 0
    for i in range(count):
        target = total * i / (count - 1)
        while (
            segment < len(points) - 2
            and travelled + math.dist(points[segment], points[segment + 1]) < target
        ):
            travelled += math.dist(points[segment], points[segment + 1])
            segment += 1
        a, b = points[segment], points[segment + 1]
        span = max(math.dist(a, b), 1e-6)
        t = min(max((target - travelled) / span, 0.0), 1.0)
        out.append((a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t))
    return out


def point_segment_distance(p, a, b):
    ax, az = a
    bx, bz = b
    dx, dz = bx - ax, bz - az
    span = dx * dx + dz * dz
    t = (
        0.0
        if span < 1e-9
        else min(max(((p[0] - ax) * dx + (p[1] - az) * dz) / span, 0.0), 1.0)
    )
    return math.hypot(p[0] - (ax + dx * t), p[1] - (az + dz * t))


def distance_to_polyline(p, points):
    return min(point_segment_distance(p, a, b) for a, b in zip(points, points[1:]))


def smoothstep(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3.0 - 2.0 * t)


class Obstacles:
    def __init__(self, terrain, map_json, plan, river_index):
        self.width = terrain["width"]
        self.height = terrain["height"]
        self.types = terrain["types"]
        self.plan = plan
        self.structures = [
            (s["x"], s["z"]) for s in map_json.get("structures", []) if "x" in s
        ]
        self.walls = [
            polyline(s) for s in map_json.get("structures", []) if "start" in s
        ]
        self.bodies = [
            (s["x"], s["z"], SPAWN_CLEARANCE)
            for s in map_json.get("spawns", [])
            if "x" in s
        ]
        self.bodies += [
            (p["x"], p["z"], PROP_CLEARANCE)
            for p in map_json.get("world_props", []) + map_json.get("firecamps", [])
            if "x" in p
        ]
        self.bodies += [
            (z["x"], z["z"], float(z.get("radius", 8.0)))
            for z in map_json.get("undead_zones", [])
            if "x" in z
        ]
        self.roads = [polyline(r) for r in map_json.get("roads", [])]
        self.crossings = [polyline(b) for b in map_json.get("bridges", [])]
        for ford in map_json.get("fords", []):
            if "start" in ford:
                self.crossings.append([tuple(ford["start"]), tuple(ford["end"])])
            elif "position" in ford:
                p = tuple(ford["position"])
                self.crossings.append([p, p])
        self.lakes = [
            (
                lake["x"],
                lake["z"],
                max(lake.get("width", 0), lake.get("depth", 0)) * 0.5,
            )
            for lake in map_json.get("lakes", [])
            if "x" in lake
        ]
        self.other_rivers = [
            (polyline(r), float(r.get("width", 6.0)))
            for i, r in enumerate(map_json.get("rivers", []))
            if i != river_index and polyline(r)
        ]

    def blocked(self, p, half_width, near_crossing):
        x, z = p
        if (
            x < EDGE_MARGIN
            or z < EDGE_MARGIN
            or x > self.width - EDGE_MARGIN
            or z > self.height - EDGE_MARGIN
        ):
            return True
        reach = int(half_width) + 2
        for dz in (-reach, 0, reach):
            for dx in (-reach, 0, reach):
                cx, cz = int(x + dx), int(z + dz)
                if 0 <= cx < self.width and 0 <= cz < self.height:
                    kind = self.types[cz * self.width + cx]
                    if kind in (self.plan.TERRAIN_HILL, self.plan.TERRAIN_MOUNTAIN):
                        return True
        for sx, sz in self.structures:
            if math.hypot(x - sx, z - sz) < half_width + STRUCTURE_CLEARANCE:
                return True
        for bx, bz, clearance in self.bodies:
            if math.hypot(x - bx, z - bz) < half_width + clearance:
                return True
        for wall in self.walls:
            if (
                len(wall) >= 2
                and distance_to_polyline(p, wall) < half_width + STRUCTURE_CLEARANCE
            ):
                return True
        if not near_crossing:
            for road in self.roads:
                if (
                    len(road) >= 2
                    and distance_to_polyline(p, road) < half_width + ROAD_CLEARANCE
                ):
                    return True
        for lx, lz, radius in self.lakes:
            if math.hypot(x - lx, z - lz) < radius + half_width + WATER_CLEARANCE:
                return True
        for other, other_width in self.other_rivers:
            if (
                distance_to_polyline(p, other)
                < half_width + other_width * 0.5 + WATER_CLEARANCE
            ):
                return True
        return False


def pin_distances(samples, river_points, obstacles, half_width):
    """Arc distance from each sample to the nearest point that must not move."""
    arc = [0.0]
    for a, b in zip(samples, samples[1:]):
        arc.append(arc[-1] + math.dist(a, b))
    pinned = {0, len(samples) - 1}
    for i, p in enumerate(samples):
        for crossing in obstacles.crossings:
            reach = half_width + 6.0
            near = (
                math.dist(p, crossing[0]) < reach
                if crossing[0] == crossing[-1]
                else distance_to_polyline(p, crossing) < reach
            )
            if near:
                pinned.add(i)
        for other, other_width in obstacles.other_rivers:
            for end in (other[0], other[-1]):
                if math.dist(p, end) < half_width + other_width + 8.0:
                    pinned.add(i)
        for lx, lz, radius in obstacles.lakes:
            if math.hypot(p[0] - lx, p[1] - lz) < radius + half_width + 14.0:
                pinned.add(i)
    ordered = sorted(pinned)
    return [
        min(abs(arc[i] - arc[j]) for j in ordered) for i in range(len(samples))
    ], pinned


def meander(river, obstacles, seed, amplitude_scale):
    original = polyline(river)
    width = float(river.get("width", 6.0))
    half_width = width * 0.5
    total = length_of(original)
    samples = resample(original, SAMPLE_SPACING)
    distances, pinned = pin_distances(samples, original, obstacles, half_width)

    amplitude = min(0.055 * total, 2.6 * width + 12.0, 30.0) * amplitude_scale
    wave_long = max(150.0, min(0.42 * total, 300.0))
    wave_short = wave_long * 0.43
    phase_long = (seed * 0.6180339887) % 1.0 * math.tau
    phase_short = (seed * 0.4142135623) % 1.0 * math.tau

    arc = [0.0]
    for a, b in zip(samples, samples[1:]):
        arc.append(arc[-1] + math.dist(a, b))

    offsets = []
    for i in range(len(samples)):
        envelope = smoothstep(distances[i] / PIN_EASE)
        wave = 0.68 * math.sin(
            arc[i] / wave_long * math.tau + phase_long
        ) + 0.32 * math.sin(arc[i] / wave_short * math.tau + phase_short)
        offsets.append(amplitude * envelope * wave)

    def displaced(i, offset):
        a = samples[max(i - 1, 0)]
        b = samples[min(i + 1, len(samples) - 1)]
        dx, dz = b[0] - a[0], b[1] - a[1]
        span = max(math.hypot(dx, dz), 1e-6)
        return (samples[i][0] - dz / span * offset, samples[i][1] + dx / span * offset)

    def enforce():
        for i in range(len(samples)):
            if i in pinned:
                offsets[i] = 0.0
                continue
            near_crossing = distances[i] < half_width + 14.0
            for _ in range(7):
                here = displaced(i, offsets[i])
                before = displaced(i - 1, offsets[i - 1]) if i > 0 else here
                midway = ((here[0] + before[0]) * 0.5, (here[1] + before[1]) * 0.5)
                if not obstacles.blocked(
                    here, half_width, near_crossing
                ) and not obstacles.blocked(midway, half_width, near_crossing):
                    break
                offsets[i] *= 0.55
            else:
                offsets[i] = 0.0

    enforce()
    for _ in range(3):
        relaxed = offsets[:]
        for i in range(1, len(offsets) - 1):
            if i in pinned:
                continue
            blended = 0.25 * offsets[i - 1] + 0.5 * offsets[i] + 0.25 * offsets[i + 1]
            relaxed[i] = blended if abs(blended) <= abs(offsets[i]) else offsets[i]
        offsets[:] = relaxed
        enforce()

    return [displaced(i, offsets[i]) for i in range(len(samples))], max(
        abs(o) for o in offsets
    )


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("map", type=Path)
    parser.add_argument(
        "--ops", type=Path, required=True, help="write map_cli operations here"
    )
    parser.add_argument(
        "--skip-above",
        type=float,
        default=1.10,
        help="leave rivers at least this sinuous alone",
    )
    parser.add_argument("--min-length", type=float, default=160.0)
    parser.add_argument(
        "--amplitude", type=float, default=1.0, help="scale the bend size"
    )
    parser.add_argument(
        "--only", type=int, action="append", help="river index to bend (repeatable)"
    )
    parser.add_argument(
        "--map-cli", type=Path, default=REPO_ROOT / "build" / "bin" / "map_cli"
    )
    args = parser.parse_args()

    plan = load_plan_module()
    map_json = json.loads(args.map.read_text(encoding="utf-8"))
    terrain = plan.probe(args.map, REPO_ROOT / "build" / "bin" / "terrain_probe")

    listing = subprocess.run(
        [str(args.map_cli), "list", str(args.map), "linear", "--json"],
        capture_output=True,
        text=True,
        check=True,
        env={"QT_QPA_PLATFORM": "offscreen", "LC_ALL": "C"},
    )
    linear_rivers = [
        row["index"] for row in json.loads(listing.stdout) if row["type"] == "river"
    ]
    rivers = map_json.get("rivers", [])
    with_lines = [i for i, r in enumerate(rivers) if polyline(r)]
    if len(linear_rivers) == len(rivers):
        linear_rivers = [linear_rivers[i] for i in with_lines]
    elif len(linear_rivers) != len(with_lines):
        raise SystemExit(
            f"the editor lists {len(linear_rivers)} rivers but the map carries {len(with_lines)} with a course"
        )

    ops = []
    for order, river_index in enumerate(with_lines):
        river = rivers[river_index]
        points = polyline(river)
        total = length_of(points)
        sinuosity = total / max(math.dist(points[0], points[-1]), 1.0)
        label = river.get("id", f"river {river_index}")
        if args.only and river_index not in args.only:
            continue
        if sinuosity >= args.skip_above or total < args.min_length:
            print(
                f"{label}: left alone (length {total:.0f}, sinuosity {sinuosity:.2f})"
            )
            continue
        obstacles = Obstacles(terrain, map_json, plan, river_index)
        seed = sum(ord(c) for c in str(label)) + river_index * 17 + len(points)
        bent, peak = meander(river, obstacles, seed, args.amplitude)
        new_length = length_of(bent)
        new_sinuosity = new_length / max(math.dist(bent[0], bent[-1]), 1.0)
        if peak < 2.0:
            print(f"{label}: no room to bend (peak offset {peak:.1f})")
            continue
        waypoints = [[round(x, 2), round(z, 2)] for x, z in bent]
        ops.append(
            {
                "op": "set",
                "kind": "linear",
                "index": linear_rivers[order],
                "fields": {
                    "start": waypoints[0],
                    "end": waypoints[-1],
                    "waypoints": waypoints,
                },
            }
        )
        print(
            f"{label}: sinuosity {sinuosity:.2f} -> {new_sinuosity:.2f}, peak offset {peak:.1f}"
        )

    args.ops.write_text(json.dumps(ops, indent=1) + "\n", encoding="utf-8")
    print(f"{len(ops)} river(s) bent, operations in {args.ops}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
