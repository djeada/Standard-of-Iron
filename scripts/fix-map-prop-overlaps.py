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

Structures are not all equally fixed.  A wall ring is geometry -- its segments
are *meant* to abut and must never move -- and a settlement is laid out around
its barracks, temple and marketplace, so those are anchors too.  The homes and
towers dropped into the space between them are fill, and a tower standing
inside the barracks hall is a defect that moving the tower fixes.  Fill may
therefore be nudged, on a tighter travel budget than a prop and never onto a
road, and a fill building close enough to a wall to be part of the ring is
re-locked before anything moves.
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

SPAWN_BODY_RADIUS = 0.5
"""The default ``navigation_clearance`` in game/core/component.h.

A unit standing inside a tent or a tree trunk on the first frame is the same
defect as a tent inside a tent, so authored spawns are checked against the
world -- but never against each other.  Commanders are deliberately authored on
top of the unit they lead (nine pairs sit at distance 0.0), and the formation
pass spreads them on the first tick; separating them here would scatter every
starting army.
"""


PRIORITY_IMMOVABLE = 200
PRIORITY_ANCHOR = 120
PRIORITY_STRUCTURE_FILL = 70
PRIORITY_FIRECAMP = 60

STRUCTURE_PRIORITY = {
    "wall_segment": PRIORITY_IMMOVABLE,
    "wall_gate": PRIORITY_IMMOVABLE,
    "barracks": PRIORITY_ANCHOR,
    "temple": PRIORITY_ANCHOR,
    "marketplace": PRIORITY_ANCHOR,
    "farm": PRIORITY_ANCHOR,
    "home": PRIORITY_STRUCTURE_FILL,
    "defense_tower": PRIORITY_STRUCTURE_FILL,
}
STRUCTURE_PRIORITY_DEFAULT = PRIORITY_ANCHOR
PRIORITY_SPAWN = 1

WALL_LOCK_GAP = 1.0
"""A fill building this close to a wall is read as part of the ring.

Measured over every map in ``assets/maps``, the nearest a free-standing home or
tower sits to a wall body is 0.81 m and all but one clear 2 m, so this locks a
gatehouse or a corner tower without freezing ordinary settlement fill."""

DEFAULT_ROAD_WIDTH = 3.0
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
    travel_budget: float = 0.0
    avoids_roads: bool = False

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

    def separation_from(
        self, other: "Placeable", clearance: float
    ) -> tuple[float, float, float]:
        """How deep ``self`` sits in ``other``, and the shortest way out.

        Depth and direction are derived together so they can never disagree.
        The case that matters is a body whose centre is *inside* a building:
        clamping to the box returns the point itself, which reports a shallow
        overlap and no direction at all, so an archer standing in the middle of
        a barracks hall looked 0.65 m deep when it was 2.75 m from the nearest
        wall and had to be pushed out on a random bearing.  Inside the box, the
        way out is the nearest face.
        """
        if self.is_box and other.is_box:
            gap_x = abs(self.x - other.x) - (self.half_width + other.half_width)
            gap_z = abs(self.z - other.z) - (self.half_depth + other.half_depth)
            depth = clearance - max(gap_x, gap_z)
            if gap_x >= gap_z:
                return depth, (1.0 if self.x >= other.x else -1.0), 0.0
            return depth, 0.0, (1.0 if self.z >= other.z else -1.0)

        if not self.is_box and not other.is_box:
            dx = self.x - other.x
            dz = self.z - other.z
            distance = math.hypot(dx, dz)
            depth = (self.radius + other.radius + clearance) - distance
            if distance < 1e-4:
                return depth, 0.0, 0.0
            return depth, dx / distance, dz / distance

        box, disc = (self, other) if self.is_box else (other, self)
        dx = disc.x - box.x
        dz = disc.z - box.z
        inside_x = box.half_width - abs(dx)
        inside_z = box.half_depth - abs(dz)
        if inside_x > 0.0 and inside_z > 0.0:
            if inside_x <= inside_z:
                depth = inside_x + disc.radius + clearance
                exit_x, exit_z = (1.0 if dx >= 0.0 else -1.0), 0.0
            else:
                depth = inside_z + disc.radius + clearance
                exit_x, exit_z = 0.0, (1.0 if dz >= 0.0 else -1.0)
        else:
            near_x, near_z = box.closest_point(disc.x, disc.z)
            offset_x = disc.x - near_x
            offset_z = disc.z - near_z
            distance = math.hypot(offset_x, offset_z)
            depth = (disc.radius + clearance) - distance
            if distance < 1e-4:
                exit_x, exit_z = 0.0, 0.0
            else:
                exit_x, exit_z = offset_x / distance, offset_z / distance

        if self.is_box:
            return depth, -exit_x, -exit_z
        return depth, exit_x, exit_z

    def overlap_with(self, other: "Placeable", clearance: float) -> float:
        """How deep the two solid bodies intersect; <= 0 when they are clear."""
        return self.separation_from(other, clearance)[0]

    def exempt_from(self, other: "Placeable") -> bool:
        if self.kind == "spawn" and other.kind == "spawn":
            return True
        return self.body_type in WALL_TYPES and other.body_type in WALL_TYPES


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
                priority=STRUCTURE_PRIORITY.get(
                    str(structure.get("type", "")), STRUCTURE_PRIORITY_DEFAULT
                ),
                payload=structure,
                body_type=str(structure.get("type", "")),
                half_width=width * 0.5,
                half_depth=depth * 0.5,
                avoids_roads=True,
            )
        )

    for index, spawn in enumerate(map_data.get("spawns") or []):
        items.append(
            Placeable(
                key=f"spawns[{index}]",
                kind="spawn",
                name=str(spawn.get("id") or spawn.get("type") or "spawn"),
                x=float(spawn.get("x", 0.0)),
                z=float(spawn.get("z", 0.0)),
                radius=SPAWN_BODY_RADIUS,
                priority=PRIORITY_SPAWN,
                payload=spawn,
                body_type=str(spawn.get("type", "")),
                avoids_roads=False,
            )
        )

    lock_ring_structures(items)
    return items


def lock_ring_structures(items: list[Placeable]) -> None:
    """Freeze fill buildings that are close enough to a wall to be part of it.

    A corner tower or a gatehouse is ring geometry even though its type says
    fill, and sliding it off the ring opens a hole no wall segment covers.
    """
    walls = [item for item in items if item.body_type in WALL_TYPES]
    if not walls:
        return
    for item in items:
        if item.priority != PRIORITY_STRUCTURE_FILL:
            continue
        if any(item.overlap_with(wall, WALL_LOCK_GAP) > 0.0 for wall in walls):
            item.priority = PRIORITY_IMMOVABLE


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


def collect_roads(map_data: dict) -> list[Segment]:
    """Roads a *building* must not be nudged onto.

    Props are left alone here on purpose -- a boulder or a dead tree beside a
    road is authored that way -- but a home shifted into the middle of the road
    it faces is a defect swapped for a worse one.
    """
    segments: list[Segment] = []
    for road in map_data.get("roads") or []:
        points = road.get("waypoints") or [road.get("start"), road.get("end")]
        points = [point for point in points if point]
        half_width = float(road.get("width", DEFAULT_ROAD_WIDTH)) * 0.5
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
    roads: list[Segment],
    margin: float,
) -> bool:
    if not (margin <= x <= width - margin and margin <= z <= height - margin):
        return False
    for segment in water:
        if segment.distance(x, z) < segment.half_width + item.radius:
            return False
    if item.avoids_roads:
        for segment in roads:
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


ESCAPE_FAN_DEGREES = (
    0.0,
    18.0,
    -18.0,
    36.0,
    -36.0,
    55.0,
    -55.0,
    75.0,
    -75.0,
    95.0,
    -95.0,
    120.0,
    -120.0,
    150.0,
    -150.0,
    180.0,
)
ESCAPE_MARGIN = 0.01
ESCAPE_STEP_GROWTH = 1.35


def escape_moves(
    overlap: float, unit_x: float, unit_z: float, remaining: float
) -> list[tuple[float, float, float]]:
    """Every push worth trying, shortest first.

    Three fixed directions could not free an object wedged in a dense ruins
    field, so this fans out around the shortest way out.  How far a swung push
    has to travel to clear depends on the shape of both bodies, so rather than
    predict it the ladder walks outward from the minimum and lets the caller
    check the real geometry at each rung.  Stepping the ladder in the outer
    loop keeps the accepted repair the shortest one available, and ordering the
    fan by deviation keeps it the most natural of the ties.
    """
    if remaining <= 0.0:
        return []
    directions = []
    for degrees in ESCAPE_FAN_DEGREES:
        radians = math.radians(degrees)
        cosine = math.cos(radians)
        sine = math.sin(radians)
        directions.append(
            ((unit_x * cosine) - (unit_z * sine), (unit_x * sine) + (unit_z * cosine))
        )

    steps: list[float] = []
    step = max(overlap + ESCAPE_MARGIN, ESCAPE_MARGIN)
    while step < remaining:
        steps.append(step)
        step *= ESCAPE_STEP_GROWTH
    steps.append(remaining)

    return [
        (direction_x, direction_z, rung)
        for rung in steps
        for direction_x, direction_z in directions
    ]


def travel_budget_for(item: Placeable, budgets: dict[str, float]) -> float:
    return budgets.get(item.kind, budgets["world_prop"])


def repair(
    items: list[Placeable],
    map_data: dict,
    clearance: float,
    max_passes: int,
    budgets: dict[str, float],
) -> tuple[int, int]:
    grid = map_data.get("grid") or {}
    width = float(grid.get("width", 0) or 0)
    height = float(grid.get("height", 0) or 0)
    water = collect_water(map_data)
    lakes = collect_lakes(map_data)
    roads = collect_roads(map_data)
    bounded = width > 0.0 and height > 0.0

    resolved = 0
    for _ in range(max_passes):
        pairs = find_overlaps(items, clearance)
        if not pairs:
            break
        progressed = False
        for item, other, _ in pairs:
            if item.overlap_with(other, clearance) <= 1e-3:
                continue
            mover, anchor = (
                (item, other) if item.priority <= other.priority else (other, item)
            )
            if mover.priority >= PRIORITY_ANCHOR:
                continue

            budget = travel_budget_for(mover, budgets)
            overlap, unit_x, unit_z = mover.separation_from(anchor, clearance)
            if math.hypot(unit_x, unit_z) < 1e-4:
                angle = (hash(mover.name) % 360) * math.pi / 180.0
                unit_x, unit_z = math.cos(angle), math.sin(angle)

            depth_before = {
                candidate.key: mover.overlap_with(candidate, clearance)
                for candidate in items
                if candidate.key != mover.key
                and not mover.exempt_from(candidate)
                and mover.overlap_with(candidate, clearance) > 1e-3
            }

            previous_x, previous_z = mover.x, mover.z
            for unit_dx, unit_dz, reach in escape_moves(
                overlap, unit_x, unit_z, budget - mover.moved
            ):
                new_x = previous_x + (unit_dx * reach)
                new_z = previous_z + (unit_dz * reach)
                if bounded and not is_placeable_spot(
                    mover, new_x, new_z, width, height, water, lakes, roads, 1.0
                ):
                    continue
                mover.x, mover.z = new_x, new_z
                if mover.overlap_with(anchor, clearance) > 1e-3:
                    continue
                if any(
                    mover.overlap_with(candidate, clearance)
                    > max(depth_before.get(candidate.key, 0.0), 1e-3)
                    for candidate in items
                    if candidate.key != mover.key
                    and candidate.key != anchor.key
                    and candidate.priority >= mover.priority
                    and not mover.exempt_from(candidate)
                ):
                    continue
                mover.moved += reach
                progressed = True
                resolved += 1
                break
            else:
                mover.x, mover.z = previous_x, previous_z
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
        help="Furthest a prop may be nudged from where it was authored.",
    )
    parser.add_argument(
        "--max-travel-structure",
        type=float,
        default=3.0,
        help="Furthest a fill building may be nudged. Kept short: a building is "
        "an authored anchor, and a long push relocates it across the plaza.",
    )
    parser.add_argument(
        "--max-travel-spawn",
        type=float,
        default=3.0,
        help="Furthest a unit spawn may be nudged, so it stays with its camp.",
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

    budgets = {
        "world_prop": args.max_travel,
        "firecamp": args.max_travel,
        "structure": args.max_travel_structure,
        "spawn": args.max_travel_spawn,
    }

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

        _, remaining = repair(items, map_data, args.clearance, args.max_passes, budgets)
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
