#!/usr/bin/env python3
"""Find and repair props that intersect each other in authored map JSON.

Two objects are treated as overlapping when their *solid* bodies intersect --
the same bodies the engine uses to block navigation, so a canopy hanging over a
tent is left alone while a tent standing inside a tent is not.  The radii mirror
``world_prop_ground_radius`` in game/map/map_definition.h and the building
bodies in game/systems/building_collision_registry.cpp; keep them in step.

Repairs push the *lower priority* object out along the separation axis: a tent
that overlaps a wall moves, the wall does not.  A push is only accepted when it
lands the object inside the map, clear of rivers, lakes and bridges, and no
deeper into anything else than it already was.
"""

from __future__ import annotations

import argparse
import itertools
import json
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path

WORLD_PROP_RENDER_SCALE = {
    "firecamp": 1.0,
    "tent": 3.0,
    "supply_cart": 1.28,
    "weapon_rack": 0.68,
    "ruins": 2.90,
    "dead_tree": 1.55,
    "boulder": 1.20,
    "pine_tree": 4.50,
    "olive_tree": 4.00,
    "cypress_tree": 3.60,
    "palm_tree": 4.20,
    "plant": 0.55,
    "iron_ore": 1.10,
    "magic_shrine": 1.80,
    "abandoned_home": 1.90,
    "statue": 1.05,
}

WORLD_PROP_MODEL_HALF_EXTENT = {
    "ruins": 0.94,
    "abandoned_home": 1.16,
    "supply_cart": 1.42,
    "weapon_rack": 0.88,
    "magic_shrine": 0.86,
    "statue": 0.55,
    "tent": 0.55,
    "firecamp": 0.90,
    "pine_tree": 1.0,
    "olive_tree": 1.0,
    "cypress_tree": 1.0,
    "palm_tree": 1.0,
    "dead_tree": 1.0,
}
WORLD_PROP_MODEL_HALF_EXTENT_DEFAULT = 0.55

TREE_TYPES = {"pine_tree", "olive_tree", "cypress_tree", "palm_tree", "dead_tree"}
STEM_FRACTION = 0.22
MIN_PROP_RADIUS = 0.5


BUILDING_BODIES = {
    "barracks": (8.65, 4.20),
    "home": (2.36, 2.42),
    "marketplace": (2.80, 2.80),
    "temple": (3.17, 2.32),
    "farm": (1.98, 2.04),
    "defense_tower": (2.60, 2.60),
    "wall_segment": (2.02, 0.76),
    "wall_gate": (2.02, 0.76),
}
BUILDING_BODY_DEFAULT = (3.0, 3.0)


WALL_TYPES = {"wall_segment", "wall_gate"}


PRIORITY_STRUCTURE = 100
PRIORITY_FIRECAMP = 60
PROP_PRIORITY = {
    "abandoned_home": 55,
    "ruins": 50,
    "magic_shrine": 50,
    "statue": 45,
    "firecamp": 40,
    "tent": 30,
    "supply_cart": 28,
    "weapon_rack": 26,
    "iron_ore": 20,
    "boulder": 18,
    "pine_tree": 12,
    "olive_tree": 12,
    "cypress_tree": 12,
    "palm_tree": 12,
    "dead_tree": 10,
    "plant": 5,
}
PROP_PRIORITY_DEFAULT = 25


def axis_aligned_body(
    body: tuple[float, float], facing_degrees: float
) -> tuple[float, float]:
    """The axis-aligned box a rotated building body fills, as the engine does it."""
    radians = math.radians(facing_degrees)
    cosine = abs(math.cos(radians))
    sine = abs(math.sin(radians))
    return (
        body[0] * cosine + body[1] * sine,
        body[0] * sine + body[1] * cosine,
    )


def prop_solid_radius(prop_type: str, scale: float) -> float:
    render_scale = WORLD_PROP_RENDER_SCALE.get(prop_type, 1.0)
    if prop_type in TREE_TYPES:
        fraction = STEM_FRACTION
    else:
        fraction = WORLD_PROP_MODEL_HALF_EXTENT.get(
            prop_type, WORLD_PROP_MODEL_HALF_EXTENT_DEFAULT
        )
    return max(MIN_PROP_RADIUS, render_scale * scale * fraction)


@dataclass
class Placeable:
    key: str
    kind: str
    name: str
    x: float
    z: float
    radius: float
    priority: int
    payload: dict = field(repr=False, default_factory=dict)

    half_width: float = 0.0
    half_depth: float = 0.0
    body_type: str = ""
    x_field: str = "x"
    z_field: str = "z"
    moved: float = 0.0

    @property
    def is_box(self) -> bool:
        return self.half_width > 0.0 and self.half_depth > 0.0

    def closest_point(self, x: float, z: float) -> tuple[float, float]:
        if not self.is_box:
            return self.x, self.z
        return (
            min(max(x, self.x - self.half_width), self.x + self.half_width),
            min(max(z, self.z - self.half_depth), self.z + self.half_depth),
        )

    def overlap_with(self, other: "Placeable", clearance: float) -> float:
        """How deep the two solid bodies intersect; <= 0 when they are clear."""
        if self.is_box and other.is_box:
            gap_x = abs(self.x - other.x) - (self.half_width + other.half_width)
            gap_z = abs(self.z - other.z) - (self.half_depth + other.half_depth)
            return clearance - max(gap_x, gap_z)
        if not self.is_box and not other.is_box:
            distance = math.hypot(self.x - other.x, self.z - other.z)
            return (self.radius + other.radius + clearance) - distance
        box, disc = (self, other) if self.is_box else (other, self)
        near_x, near_z = box.closest_point(disc.x, disc.z)
        distance = math.hypot(disc.x - near_x, disc.z - near_z)
        return (disc.radius + clearance) - distance

    def exempt_from(self, other: "Placeable") -> bool:
        return self.body_type in WALL_TYPES and other.body_type in WALL_TYPES

    def push_direction_from(self, anchor: "Placeable") -> tuple[float, float]:
        near_x, near_z = anchor.closest_point(self.x, self.z)
        dx = self.x - near_x
        dz = self.z - near_z
        length = math.hypot(dx, dz)
        if length < 1e-4:
            return 0.0, 0.0
        return dx / length, dz / length


def collect(map_data: dict) -> list[Placeable]:
    items: list[Placeable] = []

    for index, prop in enumerate(map_data.get("world_props") or []):
        prop_type = str(prop.get("type", ""))
        scale = float(prop.get("scale", 1.0) or 1.0)
        items.append(
            Placeable(
                key=f"world_props[{index}]",
                kind="world_prop",
                name=str(prop.get("id") or prop_type),
                x=float(prop.get("x", 0.0)),
                z=float(prop.get("z", 0.0)),
                radius=prop_solid_radius(prop_type, scale),
                priority=PROP_PRIORITY.get(prop_type, PROP_PRIORITY_DEFAULT),
                payload=prop,
                body_type=prop_type,
            )
        )

    for index, camp in enumerate(map_data.get("firecamps") or []):
        items.append(
            Placeable(
                key=f"firecamps[{index}]",
                kind="firecamp",
                name=str(camp.get("id") or "firecamp"),
                x=float(camp.get("x", 0.0)),
                z=float(camp.get("z", 0.0)),
                radius=prop_solid_radius("firecamp", 1.0),
                priority=PRIORITY_FIRECAMP,
                payload=camp,
                body_type="firecamp",
            )
        )

    for index, structure in enumerate(map_data.get("structures") or []):
        body = BUILDING_BODIES.get(
            str(structure.get("type", "")), BUILDING_BODY_DEFAULT
        )
        width, depth = axis_aligned_body(
            body, float(structure.get("facing", structure.get("rotation", 0.0)) or 0.0)
        )
        items.append(
            Placeable(
                key=f"structures[{index}]",
                kind="structure",
                name=str(structure.get("id") or structure.get("type")),
                x=float(structure.get("x", 0.0)),
                z=float(structure.get("z", 0.0)),
                radius=0.5 * math.hypot(width, depth),
                priority=PRIORITY_STRUCTURE,
                payload=structure,
                body_type=str(structure.get("type", "")),
                half_width=width * 0.5,
                half_depth=depth * 0.5,
            )
        )

    return items


@dataclass
class Segment:
    x1: float
    z1: float
    x2: float
    z2: float
    half_width: float

    def distance(self, x: float, z: float) -> float:
        dx = self.x2 - self.x1
        dz = self.z2 - self.z1
        length_sq = dx * dx + dz * dz
        if length_sq <= 1e-9:
            return math.hypot(x - self.x1, z - self.z1)
        t = ((x - self.x1) * dx + (z - self.z1) * dz) / length_sq
        t = max(0.0, min(1.0, t))
        return math.hypot(x - (self.x1 + dx * t), z - (self.z1 + dz * t))


def collect_water(map_data: dict) -> list[Segment]:
    """Rivers and bridges a prop must not be pushed into."""
    segments: list[Segment] = []
    for river in map_data.get("rivers") or []:
        points = river.get("waypoints") or [river.get("start"), river.get("end")]
        half_width = float(river.get("width", 4.0)) * 0.5
        points = [point for point in points if point]
        for first, second in zip(points, points[1:], strict=False):
            segments.append(
                Segment(
                    float(first[0]),
                    float(first[1]),
                    float(second[0]),
                    float(second[1]),
                    half_width,
                )
            )
    for bridge in map_data.get("bridges") or []:
        start = bridge.get("start")
        end = bridge.get("end")
        if not start or not end:
            continue
        segments.append(
            Segment(
                float(start[0]),
                float(start[1]),
                float(end[0]),
                float(end[1]),
                float(bridge.get("width", 8.0)) * 0.5,
            )
        )
    return segments


def collect_lakes(map_data: dict) -> list[tuple[float, float, float]]:
    lakes = []
    for lake in map_data.get("lakes") or []:
        center = lake.get("center") or [lake.get("x"), lake.get("z")]
        if not center or center[0] is None:
            continue
        radius = float(lake.get("radius", lake.get("radius_x", 4.0)) or 4.0)
        lakes.append((float(center[0]), float(center[1]), radius))
    return lakes


def is_placeable_spot(
    item: Placeable,
    x: float,
    z: float,
    width: float,
    height: float,
    water: list[Segment],
    lakes: list[tuple[float, float, float]],
    margin: float,
) -> bool:
    if not (margin <= x <= width - margin and margin <= z <= height - margin):
        return False
    for segment in water:
        if segment.distance(x, z) < segment.half_width + item.radius:
            return False
    for lake_x, lake_z, lake_radius in lakes:
        if math.hypot(x - lake_x, z - lake_z) < lake_radius + item.radius:
            return False
    return True


def find_overlaps(
    items: list[Placeable], clearance: float
) -> list[tuple[Placeable, Placeable, float]]:
    """All intersecting pairs, worst first.

    A uniform grid keeps this near-linear: the biggest maps carry a few hundred
    props and a plain double loop is fine, but the bucket keeps the repair loop
    cheap when it runs many passes.
    """
    if not items:
        return []
    cell = max(4.0, 2.0 * max(item.radius for item in items) + clearance)
    buckets: dict[tuple[int, int], list[Placeable]] = {}
    for item in items:
        buckets.setdefault((int(item.x // cell), int(item.z // cell)), []).append(item)

    seen: set[tuple[str, str]] = set()
    pairs: list[tuple[Placeable, Placeable, float]] = []
    for (bucket_x, bucket_z), bucket in buckets.items():
        neighbours: list[Placeable] = []
        for offset_x in (-1, 0, 1):
            for offset_z in (-1, 0, 1):
                neighbours.extend(
                    buckets.get((bucket_x + offset_x, bucket_z + offset_z), [])
                )
        for item in bucket:
            for other in neighbours:
                if item.key == other.key:
                    continue
                pair_key = tuple(sorted((item.key, other.key)))
                if pair_key in seen:
                    continue
                seen.add(pair_key)
                if item.exempt_from(other):
                    continue
                overlap = item.overlap_with(other, clearance)
                if overlap > 1e-3:
                    pairs.append((item, other, overlap))
    pairs.sort(key=lambda entry: entry[2], reverse=True)
    return pairs


def repair(
    items: list[Placeable],
    map_data: dict,
    clearance: float,
    max_passes: int,
    max_travel: float,
) -> tuple[int, int]:
    grid = map_data.get("grid") or {}
    width = float(grid.get("width", 0) or 0)
    height = float(grid.get("height", 0) or 0)
    water = collect_water(map_data)
    lakes = collect_lakes(map_data)

    resolved = 0
    for _ in range(max_passes):
        pairs = find_overlaps(items, clearance)
        if not pairs:
            break
        progressed = False
        for item, other, overlap in pairs:

            overlap = item.overlap_with(other, clearance)
            if overlap <= 1e-3:
                continue
            mover, anchor = (
                (item, other) if item.priority <= other.priority else (other, item)
            )
            if mover.priority >= PRIORITY_STRUCTURE:
                continue

            dx = mover.x - anchor.x
            dz = mover.z - anchor.z
            length = math.hypot(dx, dz)
            if length < 1e-4:

                angle = (hash(mover.name) % 360) * math.pi / 180.0
                dx, dz, length = math.cos(angle), math.sin(angle), 1.0
            step = overlap + 1e-2
            candidates = [(dx / length, dz / length)]

            candidates.append((-dz / length, dx / length))
            candidates.append((dz / length, -dx / length))

            for unit_x, unit_z in candidates:
                new_x = mover.x + unit_x * step
                new_z = mover.z + unit_z * step
                if mover.moved + step > max_travel:
                    continue
                if (
                    width > 0
                    and height > 0
                    and not is_placeable_spot(
                        mover, new_x, new_z, width, height, water, lakes, 1.0
                    )
                ):
                    continue
                previous_x, previous_z = mover.x, mover.z
                mover.x, mover.z = new_x, new_z
                if any(
                    mover.overlap_with(candidate, clearance) > 1e-3
                    and candidate.key != anchor.key
                    and candidate.priority > mover.priority
                    for candidate in items
                    if candidate.key != mover.key
                ):
                    mover.x, mover.z = previous_x, previous_z
                    continue
                mover.moved += step
                progressed = True
                resolved += 1
                break
        if not progressed:
            break

    remaining = len(find_overlaps(items, clearance))
    return resolved, remaining


def detect_format(source: str, map_data: dict) -> tuple[int, bool, bool] | None:
    """The indent/sort/newline that reproduces this file byte for byte.

    Maps in this repo are authored in two styles, and rewriting one in the
    other buries a two-line fix under a whole-file diff.
    """
    for indent, sort_keys, trailing_newline in itertools.product(
        (2, 4), (False, True), (True, False)
    ):
        candidate = json.dumps(map_data, indent=indent, sort_keys=sort_keys) + (
            "\n" if trailing_newline else ""
        )
        if candidate == source:
            return indent, sort_keys, trailing_newline
    return None


def apply(items: list[Placeable]) -> int:
    changed = 0
    for item in items:
        if item.moved <= 0.0:
            continue
        item.payload[item.x_field] = round(item.x, 2)
        item.payload[item.z_field] = round(item.z, 2)
        changed += 1
    return changed


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "maps",
        nargs="*",
        help="Map JSON files (default: every map in assets/maps).",
    )
    parser.add_argument(
        "--clearance",
        type=float,
        default=0.15,
        help="Extra gap demanded between two solid bodies, in world units.",
    )
    parser.add_argument(
        "--max-travel",
        type=float,
        default=6.0,
        help="Furthest a single object may be nudged from where it was authored.",
    )
    parser.add_argument("--max-passes", type=int, default=12)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Report overlaps and exit non-zero without writing anything.",
    )
    parser.add_argument("--quiet", action="store_true")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    paths = (
        [Path(entry) for entry in args.maps]
        if args.maps
        else sorted((repo_root / "assets" / "maps").glob("*.json"))
    )

    total_before = 0
    total_remaining = 0
    total_moved = 0
    for path in paths:
        try:
            source = path.read_text()
            map_data = json.loads(source)
        except (OSError, json.JSONDecodeError) as error:
            print(f"{path}: cannot read ({error})", file=sys.stderr)
            continue
        if not isinstance(map_data, dict) or "grid" not in map_data:
            continue

        items = collect(map_data)
        before = find_overlaps(items, args.clearance)
        total_before += len(before)
        if not before:
            continue

        if args.check:
            print(f"{path.name}: {len(before)} overlapping pair(s)")
            if not args.quiet:
                for item, other, overlap in before[:12]:
                    print(
                        f"    {item.name} ({item.kind}) x {other.name} "
                        f"({other.kind}) overlap {overlap:.2f}"
                    )
                if len(before) > 12:
                    print(f"    ... {len(before) - 12} more")
            total_remaining += len(before)
            continue

        style = detect_format(source, map_data)
        if style is None:
            print(
                f"{path.name}: unrecognised formatting, refusing to rewrite it",
                file=sys.stderr,
            )
            total_remaining += len(before)
            continue

        _, remaining = repair(
            items, map_data, args.clearance, args.max_passes, args.max_travel
        )
        moved = apply(items)
        total_remaining += remaining
        total_moved += moved
        indent, sort_keys, trailing_newline = style
        path.write_text(
            json.dumps(map_data, indent=indent, sort_keys=sort_keys)
            + ("\n" if trailing_newline else "")
        )
        print(
            f"{path.name}: {len(before)} overlapping pair(s) -> {remaining} left, "
            f"{moved} object(s) nudged"
        )

    if args.check:
        print(f"total: {total_before} overlapping pair(s)")
        return 1 if total_before else 0

    print(
        f"total: {total_before} overlapping pair(s) -> {total_remaining} left, "
        f"{total_moved} object(s) nudged"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
