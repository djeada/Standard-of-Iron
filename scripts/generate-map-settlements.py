#!/usr/bin/env python3
"""Generate settlement geometry for Standard of Iron map JSON.

Each map carries a top-level ``settlements`` array describing authorial intent:
where a settlement sits, who owns it, and which tier it belongs to. This tool
stamps that intent into concrete ``structures`` - buildings, wall rings with
gates, and towers - so towns read as inhabited places rather than a barracks
with two houses beside it.

Tiers
  town            outer wall ring, inner citadel, street blocks of housing
  fortified_camp  single wall ring, two housing rows, corner towers
  marching_camp   palisade on the threat side only, no marketplace

A settlement marked ``on_hill`` is fitted inside the hill's flat crown, and the
hill feature is widened when the footprint needs more room. Slopes are not
walkable, so anything hanging off the crown would be unreachable.

The command is dry-run by default. Pass --write to replace only the top-level
``structures`` array; all other content in the document is retained.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Sequence

CAMPAIGN_MAPS = (
    "assets/maps/map_crossing_rhone.json",
    "assets/maps/map_crossing_alps.json",
    "assets/maps/map_battle_ticino.json",
    "assets/maps/map_battle_trebia.json",
    "assets/maps/map_battle_trasimene.json",
    "assets/maps/map_battle_cannae.json",
    "assets/maps/map_campania_campaign.json",
    "assets/maps/map_battle_zama.json",
)


BUILDING_SIZES = {
    "barracks": (4.0, 4.0),
    "home": (3.0, 3.0),
    "marketplace": (3.0, 3.0),
    "defense_tower": (3.0, 3.0),
    "wall_segment": (2.0, 2.0),
}


WALL_SEGMENT_SPACING = 2


BUILDING_CLEARANCE = 2.0


TERRAIN_INFLUENCE_MARGIN = 1.22

TIERS = ("town", "fortified_camp", "marching_camp")

FACINGS = {
    "north": (0.0, -1.0),
    "south": (0.0, 1.0),
    "east": (1.0, 0.0),
    "west": (-1.0, 0.0),
}


class SettlementError(RuntimeError):
    """Raised when a settlement cannot be placed legally."""


@dataclass
class Building:
    type: str
    x: float
    z: float
    player_id: int
    nation: str | None = None
    max_population: int | None = None

    def to_json(self) -> dict:
        entry: dict = {
            "type": self.type,
            "x": round(self.x, 2),
            "z": round(self.z, 2),
            "player_id": self.player_id,
        }
        if self.max_population is not None:
            entry["max_population"] = self.max_population
        if self.nation:
            entry["nation"] = self.nation
        return entry


@dataclass
class WallRun:
    start: tuple[float, float]
    end: tuple[float, float]
    player_id: int
    nation: str | None = None
    width: float = 2.0

    def entity_count(self) -> int:
        span = max(abs(self.end[0] - self.start[0]), abs(self.end[1] - self.start[1]))
        return int(span // WALL_SEGMENT_SPACING) + 1

    def to_json(self) -> dict:
        entry: dict = {
            "type": "wall_segment",
            "start": [round(self.start[0], 2), round(self.start[1], 2)],
            "end": [round(self.end[0], 2), round(self.end[1], 2)],
            "player_id": self.player_id,
            "width": self.width,
        }
        if self.nation:
            entry["nation"] = self.nation
        return entry


@dataclass
class Settlement:
    id: str
    player_id: int
    tier: str
    x: float
    z: float
    facing: str = "south"
    nation: str | None = None
    on_hill: bool | None = None
    homes: int | None = None
    max_population: int | None = None
    walls_only: bool = False
    palisade: bool = True
    outer_towers: int | None = None
    minimum_homes: int = 0
    scale: float = 1.0

    buildings: list[Building] = field(default_factory=list)
    walls: list[WallRun] = field(default_factory=list)
    hill: dict | None = None

    @staticmethod
    def from_json(entry: dict) -> "Settlement":
        missing = [
            key for key in ("id", "player_id", "tier", "x", "z") if key not in entry
        ]
        if missing:
            raise SettlementError(f"settlement is missing {', '.join(missing)}")
        tier = str(entry["tier"]).lower()
        if tier not in TIERS:
            raise SettlementError(f"unknown settlement tier: {tier}")
        facing = str(entry.get("facing", "south")).lower()
        if facing not in FACINGS:
            raise SettlementError(f"unknown facing: {facing}")
        return Settlement(
            id=str(entry["id"]),
            player_id=int(entry["player_id"]),
            tier=tier,
            x=float(entry["x"]),
            z=float(entry["z"]),
            facing=facing,
            nation=entry.get("nation"),
            on_hill=(None if "on_hill" not in entry else bool(entry["on_hill"])),
            homes=entry.get("homes"),
            max_population=entry.get("max_population"),
            walls_only=bool(entry.get("walls_only", False)),
            palisade=bool(entry.get("palisade", True)),
            outer_towers=(
                None if "outer_towers" not in entry else int(entry["outer_towers"])
            ),
            minimum_homes=max(0, int(entry.get("minimum_homes", 0))),
            scale=float(entry.get("scale", 1.0)),
        )


def _load_road_generator():
    """Import the road generator so its terrain rasteriser can be reused.

    generate-map-roads.py already rasterises hills, mountains, lakes and rivers
    into a passability field. Duplicating that here would let the two tools drift
    apart and disagree about what counts as blocked ground.
    """
    import types

    path = Path(__file__).resolve().parent / "generate-map-roads.py"
    module = types.ModuleType("soi_road_generator")
    module.__dict__["__name__"] = "soi_road_generator"
    sys.modules["soi_road_generator"] = module
    exec(compile(path.read_text(), str(path), "exec"), module.__dict__)
    return module


class TerrainMask:
    """Where buildings may stand, given the map's blocking terrain.

    A settlement sitting on a hill is allowed to use that hill's crown, so the
    hill it occupies is dropped from the mask; every other hill, mountain, lake
    and river still blocks.
    """

    def __init__(
        self, definition: dict, clearance: float, exclude_hill: dict | None = None
    ):
        roads = _load_road_generator()
        working = json.loads(json.dumps(definition))
        if exclude_hill is not None:
            working["terrain"] = [
                feature
                for feature in working.get("terrain") or []
                if not (
                    str(feature.get("type", "")).lower() == "hill"
                    and abs(float(feature.get("x", 0.0)) - float(exclude_hill["x"]))
                    < 0.01
                    and abs(float(feature.get("z", 0.0)) - float(exclude_hill["z"]))
                    < 0.01
                )
            ]

        for feature in working.get("terrain") or []:
            kind = str(feature.get("type", "")).lower()
            if kind not in {"hill", "mountain"}:
                continue
            half_width, half_depth = hill_extents(feature)
            if half_width <= 0.0 or half_depth <= 0.0:
                continue
            feature.pop("radius", None)
            feature["width"] = half_width * 2.0 * TERRAIN_INFLUENCE_MARGIN
            feature["depth"] = half_depth * 2.0 * TERRAIN_INFLUENCE_MARGIN

        working["roads"] = []
        self._field = roads.RoutingField(working, clearance)

    def clear_for(self, x: float, z: float, size: tuple[float, float]) -> bool:
        half_x = max(1, int(size[0] * 0.5))
        half_z = max(1, int(size[1] * 0.5))
        for dz in range(-half_z, half_z + 1):
            for dx in range(-half_x, half_x + 1):
                grid_x = int(round(x)) + dx
                grid_z = int(round(z)) + dz
                if not self._field.passable(grid_x, grid_z):
                    return False
                if self._field.water[self._field.index(grid_x, grid_z)]:
                    return False
        return True


def clip_walls_to_clear_ground(
    mask: TerrainMask, walls: Sequence[WallRun]
) -> list[WallRun]:
    """Split wall runs at river/lake clearance instead of crossing water."""
    result: list[WallRun] = []
    for wall in walls:
        delta_x = wall.end[0] - wall.start[0]
        delta_z = wall.end[1] - wall.start[1]
        span = math.hypot(delta_x, delta_z)
        sample_count = max(1, int(math.ceil(span / WALL_SEGMENT_SPACING)))
        samples = [
            (
                wall.start[0] + delta_x * index / sample_count,
                wall.start[1] + delta_z * index / sample_count,
            )
            for index in range(sample_count + 1)
        ]
        clear = [
            mask.clear_for(point[0], point[1], BUILDING_SIZES["wall_segment"])
            for point in samples
        ]

        run_start: int | None = None
        for index, is_clear in enumerate([*clear, False]):
            if is_clear and run_start is None:
                run_start = index
                continue
            if is_clear or run_start is None:
                continue
            run_end = index - 1
            result.append(
                WallRun(
                    samples[run_start],
                    samples[run_end],
                    wall.player_id,
                    wall.nation,
                    wall.width,
                )
            )
            run_start = None
    return result


def hill_height(width: float, depth: float, authored_height: float) -> float:
    """Mirror of the campaign-scale hill height in terrain.cpp."""
    footprint_height = min(width, depth) * 0.18
    return max(authored_height * 2.80, footprint_height)


def hill_crown_extent(
    width: float, depth: float, authored_height: float
) -> tuple[float, float]:
    """Half-extents of a hill's flat crown, in grid cells.

    Mirrors the plateau computation in terrain.cpp. The crown is the only
    walkable part of a hill, so a settlement has to fit inside it.
    """
    slope_width = max(2.0, width * 0.5)
    slope_depth = max(2.0, depth * 0.5)
    elevation_cells = max(hill_height(width, depth, authored_height), 0.25)
    slope_run = max(7.0, elevation_cells * 4.2)

    plateau_width = max(
        1.5,
        slope_width * 0.42,
        slope_width - min(slope_width * 0.62, slope_run),
    )
    plateau_depth = max(
        1.5,
        slope_depth * 0.42,
        slope_depth - min(slope_depth * 0.62, slope_run),
    )
    return plateau_width, plateau_depth


def usable_crown_extent(
    width: float, depth: float, authored_height: float, safety: float
) -> tuple[float, float]:
    """Crown extent reduced for the noise warping applied to the real crown.

    sample_hill() warps the crown ellipse with fbm noise and shifts it off
    centre, so the authored plateau is not reliable right up to its edge. The
    safety factor keeps placement well inside it; the authoritative check is the
    CampaignStructuresStandOnWalkableGround test, which uses the real terrain.
    """
    crown_x, crown_z = hill_crown_extent(width, depth, authored_height)
    return crown_x * safety, crown_z * safety


def crown_fits(
    width: float,
    depth: float,
    authored_height: float,
    half_x: float,
    half_z: float,
    safety: float,
) -> bool:
    """Whether a rectangular footprint sits inside the hill's elliptical crown."""
    crown_x, crown_z = usable_crown_extent(width, depth, authored_height, safety)

    return math.hypot(half_x / max(crown_x, 0.001), half_z / max(crown_z, 0.001)) <= 1.0


def fit_hill_to_footprint(
    feature: dict,
    half_x: float,
    half_z: float,
    safety: float,
    max_width: float,
    max_depth: float,
) -> bool:
    """Widen a hill until its usable crown contains the settlement footprint.

    Growth is capped: a hill wide enough to carry a full town would span half the
    map and stop reading as terrain. Callers shrink the settlement instead when
    this returns False.
    """
    authored_height = float(feature.get("height", 2.0))
    changed = False
    for _ in range(96):
        width = float(feature.get("width", 0.0))
        depth = float(feature.get("depth", 0.0))
        if crown_fits(width, depth, authored_height, half_x, half_z, safety):
            return changed
        if width >= max_width and depth >= max_depth:
            return changed

        crown_x, crown_z = usable_crown_extent(width, depth, authored_height, safety)
        want_x = half_x / max(crown_x, 0.001)
        want_z = half_z / max(crown_z, 0.001)
        grew = False
        if want_x >= want_z and width < max_width:
            feature["width"] = round(min(width * 1.10 + 4.0, max_width), 2)
            grew = True
        elif want_z > want_x and depth < max_depth:
            feature["depth"] = round(min(depth * 1.10 + 4.0, max_depth), 2)
            grew = True
        elif width < max_width:
            feature["width"] = round(min(width * 1.10 + 4.0, max_width), 2)
            grew = True
        elif depth < max_depth:
            feature["depth"] = round(min(depth * 1.10 + 4.0, max_depth), 2)
            grew = True

        if not grew:
            return changed
        changed = True
    return changed


def keep_hill_entrances_outside_crown(feature: dict) -> None:
    """Push entrances back out to the enlarged hill's slope.

    Entrances authored for the original hill end up buried inside a widened
    crown, where they carve nothing. Each one keeps its bearing - the direction
    it faces is the route it defends - and slides out to the new slope edge.
    """
    entrances = feature.get("entrances") or []
    if not entrances:
        return

    centre_x = float(feature.get("x", 0.0))
    centre_z = float(feature.get("z", 0.0))
    half_width = float(feature.get("width", 0.0)) * 0.5
    half_depth = float(feature.get("depth", 0.0)) * 0.5

    for entrance in entrances:
        dx = float(entrance.get("x", centre_x)) - centre_x
        dz = float(entrance.get("z", centre_z)) - centre_z
        if abs(dx) < 1e-6 and abs(dz) < 1e-6:
            dx = -1.0
        scale = math.hypot(dx / max(half_width, 0.001), dz / max(half_depth, 0.001))
        if scale <= 0.0:
            continue
        entrance["x"] = round(centre_x + dx / scale, 2)
        entrance["z"] = round(centre_z + dz / scale, 2)


def hill_extents(feature: dict) -> tuple[float, float]:
    """Half-extents of a hill, whichever way it was authored.

    Campaign maps use `width`/`depth` in some places and a single `radius` in
    others; both reach the same terrain code, so both have to be understood here.
    """
    radius = float(feature.get("radius") or 0.0)
    width = float(feature.get("width") or radius * 2.0)
    depth = float(feature.get("depth") or radius * 2.0)
    return width * 0.5, depth * 0.5


def hill_containing(terrain: Sequence[dict], x: float, z: float) -> dict | None:
    """The hill a point already stands on, if any."""
    for feature in terrain:
        if str(feature.get("type", "")).lower() != "hill":
            continue
        half_width, half_depth = hill_extents(feature)
        if half_width <= 0.0 or half_depth <= 0.0:
            continue
        offset_x = (x - float(feature.get("x", 0.0))) / half_width
        offset_z = (z - float(feature.get("z", 0.0))) / half_depth
        if math.hypot(offset_x, offset_z) <= 1.0:
            return feature
    return None


def normalise_hill_dimensions(feature: dict) -> None:
    """Rewrite a radius-authored hill as explicit width/depth so it can be grown."""
    half_width, half_depth = hill_extents(feature)
    feature["width"] = round(half_width * 2.0, 2)
    feature["depth"] = round(half_depth * 2.0, 2)
    feature.pop("radius", None)


def nearest_clear_anchor(
    mask: "TerrainMask",
    x: float,
    z: float,
    size: tuple[float, float],
    reach: float = 90.0,
) -> tuple[float, float] | None:
    """Nearest position that can carry a settlement core."""
    step = 6.0
    rings = int(reach // step)
    for ring in range(1, rings + 1):
        for angle_index in range(8 * ring):
            angle = (2.0 * math.pi * angle_index) / (8 * ring)
            candidate_x = x + math.cos(angle) * step * ring
            candidate_z = z + math.sin(angle) * step * ring
            if mask.clear_for(candidate_x, candidate_z, size):
                return round(candidate_x, 2), round(candidate_z, 2)
    return None


def push_settlement_off_hill(
    settlement: "Settlement", feature: dict, margin: float = 26.0
) -> None:
    """Slide a settlement anchor out past a hill's skirt, keeping its bearing."""
    centre_x = float(feature.get("x", 0.0))
    centre_z = float(feature.get("z", 0.0))
    half_width, half_depth = hill_extents(feature)

    offset_x = settlement.x - centre_x
    offset_z = settlement.z - centre_z
    if abs(offset_x) < 1e-6 and abs(offset_z) < 1e-6:
        offset_x, offset_z = FACINGS[opposite(settlement.facing)]

    length = math.hypot(offset_x, offset_z)
    if length < 1e-6:
        offset_x, offset_z, length = 1.0, 0.0, 1.0
    direction_x = offset_x / length
    direction_z = offset_z / length

    reach = math.hypot(direction_x * half_width, direction_z * half_depth) + margin
    settlement.x = round(centre_x + direction_x * reach, 2)
    settlement.z = round(centre_z + direction_z * reach, 2)


def distance_to_water(definition: dict, x: float, z: float) -> float:
    """Clearance from a point to the nearest river channel or lake edge."""
    best = float("inf")

    for river in definition.get("rivers") or []:
        points = river.get("waypoints") or [river.get("start"), river.get("end")]
        points = [point for point in points if point]
        half_width = float(river.get("width", 3.0)) * 0.5
        for start, end in zip(points, points[1:]):
            delta_x = float(end[0]) - float(start[0])
            delta_z = float(end[1]) - float(start[1])
            length_sq = delta_x * delta_x + delta_z * delta_z
            if length_sq <= 0.0:
                continue
            travel = max(
                0.0,
                min(
                    1.0,
                    ((x - float(start[0])) * delta_x + (z - float(start[1])) * delta_z)
                    / length_sq,
                ),
            )
            near_x = float(start[0]) + delta_x * travel
            near_z = float(start[1]) + delta_z * travel
            best = min(best, math.hypot(x - near_x, z - near_z) - half_width)

    lakes = list(definition.get("lakes") or [])
    for feature in definition.get("terrain") or []:
        if str(feature.get("type", "")).lower() == "lake":
            lakes.append(feature)
    for lake in lakes:
        centre = lake.get("center")
        lake_x = float(centre[0]) if centre else float(lake.get("x", 0.0))
        lake_z = float(centre[1]) if centre else float(lake.get("z", 0.0))
        radius = float(lake.get("radius") or 0.0)
        extent = (
            max(
                float(lake.get("width") or radius * 2.0),
                float(lake.get("depth") or radius * 2.0),
            )
            * 0.5
        )
        best = min(best, math.hypot(x - lake_x, z - lake_z) - extent)

    return best


def ensure_hill_approaches(feature: dict, minimum: int = 4) -> None:
    """Give an enlarged hill enough ramps to stay permeable.

    A widened hill with only two entrances becomes a wall across the map and can
    cut the road network in half. Compass approaches keep it a defended position
    rather than a barrier, and satisfy the two-approach rule in
    terrain_topology_audit.cpp with room to spare.
    """
    entrances = list(feature.get("entrances") or [])
    if len(entrances) >= minimum:
        return

    centre_x = float(feature.get("x", 0.0))
    centre_z = float(feature.get("z", 0.0))
    half_width, half_depth = hill_extents(feature)

    existing = [
        (float(item.get("x", 0.0)) - centre_x, float(item.get("z", 0.0)) - centre_z)
        for item in entrances
    ]
    for direction_x, direction_z in ((-1.0, 0.0), (1.0, 0.0), (0.0, -1.0), (0.0, 1.0)):
        if len(entrances) >= minimum:
            break
        candidate_x = direction_x * half_width
        candidate_z = direction_z * half_depth
        if any(
            math.hypot(candidate_x - other_x, candidate_z - other_z)
            < max(half_width, half_depth) * 0.5
            for other_x, other_z in existing
        ):
            continue
        entrances.append(
            {
                "x": round(centre_x + candidate_x, 2),
                "z": round(centre_z + candidate_z, 2),
                "radius": 2.0,
            }
        )
        existing.append((candidate_x, candidate_z))

    feature["entrances"] = entrances


def find_hill_at(
    terrain: Sequence[dict], x: float, z: float, radius: float = 90.0
) -> dict | None:
    best = None
    best_distance = radius
    for feature in terrain:
        if str(feature.get("type", "")).lower() != "hill":
            continue
        distance = math.hypot(
            float(feature.get("x", 0.0)) - x, float(feature.get("z", 0.0)) - z
        )
        if distance < best_distance:
            best = feature
            best_distance = distance
    return best


@dataclass
class TierSpec:
    outer_half_x: float
    outer_half_z: float
    inner_half_x: float | None
    home_spacing: float
    street_period: int
    wall_inset: float
    citadel_clearance: float
    core_clearance: float
    outer_towers: int
    inner_towers: int
    marketplace: bool
    full_ring: bool


TIER_SPECS = {
    "town": TierSpec(
        outer_half_x=58.0,
        outer_half_z=50.0,
        inner_half_x=17.0,
        home_spacing=6.0,
        street_period=4,
        wall_inset=8.0,
        citadel_clearance=7.0,
        core_clearance=0.0,
        outer_towers=4,
        inner_towers=2,
        marketplace=True,
        full_ring=True,
    ),
    "fortified_camp": TierSpec(
        outer_half_x=38.0,
        outer_half_z=32.0,
        inner_half_x=None,
        home_spacing=6.0,
        street_period=4,
        wall_inset=7.0,
        citadel_clearance=0.0,
        core_clearance=11.0,
        outer_towers=4,
        inner_towers=0,
        marketplace=True,
        full_ring=True,
    ),
    "marching_camp": TierSpec(
        outer_half_x=26.0,
        outer_half_z=22.0,
        inner_half_x=None,
        home_spacing=6.0,
        street_period=0,
        wall_inset=6.0,
        citadel_clearance=0.0,
        core_clearance=10.0,
        outer_towers=2,
        inner_towers=0,
        marketplace=False,
        full_ring=False,
    ),
}


def gated_run(
    fixed: float,
    span_start: float,
    span_end: float,
    gate_centre: float,
    gate_width: float,
    horizontal: bool,
    player_id: int,
    nation: str | None,
) -> list[WallRun]:
    """One wall side split into two runs with a gate gap between them."""
    gate_low = gate_centre - gate_width * 0.5
    gate_high = gate_centre + gate_width * 0.5
    runs: list[WallRun] = []

    def make(a: float, b: float) -> None:
        if b - a < WALL_SEGMENT_SPACING:
            return
        if horizontal:
            runs.append(WallRun((a, fixed), (b, fixed), player_id, nation))
        else:
            runs.append(WallRun((fixed, a), (fixed, b), player_id, nation))

    make(span_start, gate_low)
    make(gate_high, span_end)
    return runs


def wall_ring(
    cx: float,
    cz: float,
    half_x: float,
    half_z: float,
    player_id: int,
    nation: str | None,
    gates: Iterable[str],
    gate_width: float = 14.0,
) -> list[WallRun]:
    """Axis-aligned rectangular ring with gaps on the named sides."""
    gate_set = set(gates)
    left, right = cx - half_x, cx + half_x
    top, bottom = cz - half_z, cz + half_z
    runs: list[WallRun] = []

    for side, fixed, a, b, horizontal, gate_centre in (
        ("north", top, left, right, True, cx),
        ("south", bottom, left, right, True, cx),
        ("west", left, top, bottom, False, cz),
        ("east", right, top, bottom, False, cz),
    ):
        if side in gate_set:
            runs.extend(
                gated_run(
                    fixed, a, b, gate_centre, gate_width, horizontal, player_id, nation
                )
            )
        elif horizontal:
            runs.append(WallRun((a, fixed), (b, fixed), player_id, nation))
        else:
            runs.append(WallRun((fixed, a), (fixed, b), player_id, nation))

    return runs


def opposite(facing: str) -> str:
    return {"north": "south", "south": "north", "east": "west", "west": "east"}[facing]


def perpendicular(facing: str) -> str:
    return {"north": "east", "south": "west", "east": "south", "west": "north"}[facing]


def outer_wall_runs(
    cx: float,
    cz: float,
    half_x: float,
    half_z: float,
    facing: str,
    owner: int,
    nation: str | None,
    full_ring: bool,
) -> list[WallRun]:
    if full_ring:
        return wall_ring(
            cx, cz, half_x, half_z, owner, nation, [facing, opposite(facing)]
        )

    walls: list[WallRun] = []
    dx, dz = FACINGS[facing]
    if dz != 0.0:
        fixed = cz + dz * half_z
        walls.extend(
            gated_run(
                fixed,
                cx - half_x,
                cx + half_x,
                cx,
                12.0,
                True,
                owner,
                nation,
            )
        )
        walls.append(WallRun((cx - half_x, fixed), (cx - half_x, cz), owner, nation))
        walls.append(WallRun((cx + half_x, fixed), (cx + half_x, cz), owner, nation))
    else:
        fixed = cx + dx * half_x
        walls.extend(
            gated_run(
                fixed,
                cz - half_z,
                cz + half_z,
                cz,
                12.0,
                False,
                owner,
                nation,
            )
        )
        walls.append(WallRun((fixed, cz - half_z), (cx, cz - half_z), owner, nation))
        walls.append(WallRun((fixed, cz + half_z), (cx, cz + half_z), owner, nation))
    return walls


def layout_settlement(
    settlement: Settlement,
    spec: TierSpec,
    mask: TerrainMask | None = None,
    crown: tuple[float, float] | None = None,
) -> None:
    """Fill in buildings and walls for one settlement."""
    cx, cz = settlement.x, settlement.z
    scale = settlement.scale
    half_x = spec.outer_half_x * scale
    half_z = spec.outer_half_z * scale
    nation = settlement.nation
    owner = settlement.player_id

    buildings: list[Building] = []
    walls: list[WallRun] = []

    def ground_is_clear(x: float, z: float, building_type: str) -> bool:
        if crown is not None:

            size = BUILDING_SIZES.get(building_type, (3.0, 3.0))
            reach_x = (abs(x - cx) + size[0] * 0.5) / max(crown[0], 0.001)
            reach_z = (abs(z - cz) + size[1] * 0.5) / max(crown[1], 0.001)
            if math.hypot(reach_x, reach_z) > 1.0:
                return False
        if mask is None:
            return True
        return mask.clear_for(x, z, BUILDING_SIZES.get(building_type, (3.0, 3.0)))

    def site_is_free(x: float, z: float, building_type: str) -> bool:
        if not ground_is_clear(x, z, building_type):
            return False
        probe = Building(building_type, x, z, owner)
        return not any(overlapping(probe, placed) for placed in buildings)

    def nearest_clear(
        x: float, z: float, building_type: str, reach: float = 18.0
    ) -> tuple[float, float] | None:
        """Nudge a building off blocking terrain, keeping it near its anchor.

        Candidates have to clear both the terrain and everything already placed,
        or a displaced tower ends up sitting inside the barracks it guards.
        """
        if site_is_free(x, z, building_type):
            return x, z
        step = 3.0
        rings = int(reach // step)
        for ring in range(1, rings + 1):
            for angle_index in range(8 * ring):
                angle = (2.0 * math.pi * angle_index) / (8 * ring)
                candidate_x = x + math.cos(angle) * step * ring
                candidate_z = z + math.sin(angle) * step * ring
                if site_is_free(candidate_x, candidate_z, building_type):
                    return candidate_x, candidate_z
        return None

    if settlement.walls_only:
        settlement.buildings = []
        authored_walls = outer_wall_runs(
            cx, cz, half_x, half_z, settlement.facing, owner, nation, spec.full_ring
        )
        settlement.walls = (
            clip_walls_to_clear_ground(mask, authored_walls)
            if mask is not None
            else authored_walls
        )
        return

    barracks_site = nearest_clear(cx, cz - 6.0, "barracks")
    if barracks_site is None:
        raise SettlementError(
            f"{settlement.id}: no clear ground for a barracks near "
            f"{cx:.0f},{cz:.0f}"
        )
    buildings.append(
        Building(
            "barracks",
            barracks_site[0],
            barracks_site[1],
            owner,
            nation,
            settlement.max_population,
        )
    )
    if spec.marketplace:
        market_site = nearest_clear(cx, cz + 6.0, "marketplace")
        if market_site is not None:
            buildings.append(
                Building("marketplace", market_site[0], market_site[1], owner, nation)
            )

    if spec.inner_half_x is not None:
        inner_half_x = spec.inner_half_x * scale
        inner_half_z = (spec.inner_half_x - 2.0) * scale

        inner_gate = perpendicular(settlement.facing)
        walls.extend(
            wall_ring(
                cx,
                cz,
                inner_half_x,
                inner_half_z,
                owner,
                nation,
                [inner_gate],
                gate_width=10.0,
            )
        )
        for index in range(spec.inner_towers):
            offset = inner_half_x + 4.0
            sign = 1.0 if index % 2 == 0 else -1.0
            if inner_gate in ("east", "west"):
                tower_x = cx + (offset if inner_gate == "east" else -offset)
                tower_z = cz + sign * (inner_half_z - 2.0)
            else:
                tower_x = cx + sign * (inner_half_x - 2.0)
                tower_z = cz + (offset if inner_gate == "south" else -offset)
            tower_site = nearest_clear(tower_x, tower_z, "defense_tower", reach=12.0)
            if tower_site is None:
                continue
            buildings.append(
                Building("defense_tower", tower_site[0], tower_site[1], owner, nation)
            )

    spacing = spec.home_spacing
    ring_inset = spec.wall_inset * scale
    citadel_clear = (
        (spec.inner_half_x + spec.citadel_clearance) * scale
        if spec.inner_half_x
        else 0.0
    )
    core_clear = spec.core_clearance * scale

    usable_x = half_x - ring_inset
    usable_z = half_z - ring_inset
    columns = int(usable_x // spacing)
    rows = int(usable_z // spacing)

    gate_corridor = 9.0 * scale

    plots: list[tuple[float, float]] = []
    for row in range(-rows, rows + 1):
        if spec.street_period and row % spec.street_period == 0:
            continue
        for column in range(-columns, columns + 1):
            if spec.street_period and column % spec.street_period == 0:
                continue
            offset_x = column * spacing
            offset_z = row * spacing
            if abs(offset_x) > usable_x or abs(offset_z) > usable_z:
                continue
            if (
                citadel_clear
                and abs(offset_x) < citadel_clear
                and abs(offset_z) < citadel_clear
            ):
                continue
            if not citadel_clear and max(abs(offset_x), abs(offset_z)) < core_clear:
                continue

            if (
                settlement.facing in ("north", "south")
                and abs(offset_x) < gate_corridor
            ):
                continue
            if settlement.facing in ("east", "west") and abs(offset_z) < gate_corridor:
                continue
            plot_x = cx + offset_x
            plot_z = cz + offset_z

            if not ground_is_clear(plot_x, plot_z, "home"):
                continue
            plots.append((plot_x, plot_z))

    plots.sort(key=lambda plot: abs(plot[0] - cx) + abs(plot[1] - cz))
    home_budget = settlement.homes if settlement.homes is not None else len(plots)
    placed_homes = 0
    for x, z in plots:
        if placed_homes >= home_budget:
            break

        if not site_is_free(x, z, "home"):
            continue
        buildings.append(Building("home", x, z, owner, nation))
        placed_homes += 1

    fallback_home_offsets = (
        (0.0, -12.0),
        (0.0, 12.0),
        (-12.0, 0.0),
        (12.0, 0.0),
    )
    for offset_x, offset_z in fallback_home_offsets:
        if placed_homes >= settlement.minimum_homes:
            break
        home_site = nearest_clear(cx + offset_x, cz + offset_z, "home", reach=9.0)
        if home_site is None:
            continue
        buildings.append(Building("home", home_site[0], home_site[1], owner, nation))
        placed_homes += 1
    if placed_homes < settlement.minimum_homes:
        raise SettlementError(
            f"{settlement.id}: placed {placed_homes} homes, "
            f"below required minimum {settlement.minimum_homes}"
        )

    if settlement.palisade:
        walls.extend(
            outer_wall_runs(
                cx,
                cz,
                half_x,
                half_z,
                settlement.facing,
                owner,
                nation,
                spec.full_ring,
            )
        )

    corner_inset = 3.0
    corners = [
        (cx - half_x + corner_inset, cz - half_z + corner_inset),
        (cx + half_x - corner_inset, cz - half_z + corner_inset),
        (cx - half_x + corner_inset, cz + half_z - corner_inset),
        (cx + half_x - corner_inset, cz + half_z - corner_inset),
    ]
    outer_tower_count = (
        spec.outer_towers
        if settlement.outer_towers is None
        else max(0, settlement.outer_towers)
    )
    for corner in corners[:outer_tower_count]:
        corner_site = nearest_clear(corner[0], corner[1], "defense_tower", reach=12.0)
        if corner_site is None:
            continue
        buildings.append(
            Building("defense_tower", corner_site[0], corner_site[1], owner, nation)
        )

    settlement.buildings = buildings
    settlement.walls = (
        clip_walls_to_clear_ground(mask, walls) if mask is not None else walls
    )


def settlement_footprint(settlement: Settlement) -> tuple[float, float]:
    """Half-extents actually occupied, including walls."""
    half_x = 0.0
    half_z = 0.0
    for building in settlement.buildings:
        size = BUILDING_SIZES.get(building.type, (3.0, 3.0))
        half_x = max(half_x, abs(building.x - settlement.x) + size[0] * 0.5)
        half_z = max(half_z, abs(building.z - settlement.z) + size[1] * 0.5)
    for wall in settlement.walls:
        for point in (wall.start, wall.end):
            half_x = max(half_x, abs(point[0] - settlement.x) + 1.0)
            half_z = max(half_z, abs(point[1] - settlement.z) + 1.0)
    return half_x, half_z


@dataclass
class ValidationResult:
    errors: list[str] = field(default_factory=list)
    warnings: list[str] = field(default_factory=list)
    wall_entities: int = 0
    buildings: int = 0

    def passed(self) -> bool:
        return not self.errors


def overlapping(a: Building, b: Building) -> bool:
    size_a = BUILDING_SIZES.get(a.type, (3.0, 3.0))
    size_b = BUILDING_SIZES.get(b.type, (3.0, 3.0))
    gap_x = abs(a.x - b.x) - (size_a[0] + size_b[0]) * 0.5
    gap_z = abs(a.z - b.z) - (size_a[1] + size_b[1]) * 0.5
    return gap_x < BUILDING_CLEARANCE and gap_z < BUILDING_CLEARANCE


def validate(
    definition: dict,
    settlements: Sequence[Settlement],
    max_player_homes: int,
    wall_budget: int,
) -> ValidationResult:
    result = ValidationResult()

    grid = definition.get("grid") or {}
    width = int(grid.get("width", 0))
    height = int(grid.get("height", 0))

    all_buildings: list[Building] = []
    for settlement in settlements:
        all_buildings.extend(settlement.buildings)
        result.wall_entities += sum(wall.entity_count() for wall in settlement.walls)

    result.buildings = len(all_buildings)

    owners = {settlement.player_id for settlement in settlements}
    for owner in sorted(owners):
        owned = [b for b in all_buildings if b.player_id == owner]
        requires_barracks = any(
            settlement.player_id == owner and not settlement.walls_only
            for settlement in settlements
        )
        if requires_barracks and not any(b.type == "barracks" for b in owned):
            result.errors.append(f"player {owner} has no barracks")

    player_homes = sum(
        1 for b in all_buildings if b.player_id == 1 and b.type == "home"
    )
    if player_homes > max_player_homes:
        result.errors.append(
            f"player owns {player_homes} homes, above the {max_player_homes} cap "
            "that keeps manpower refill a choice rather than automatic"
        )

    for building in all_buildings:
        if not (0 < building.x < width and 0 < building.z < height):
            result.errors.append(
                f"{building.type} at {building.x:.1f},{building.z:.1f} is off the map"
            )

    for index, first in enumerate(all_buildings):
        for second in all_buildings[index + 1 :]:
            if overlapping(first, second):
                result.errors.append(
                    f"{first.type} at {first.x:.1f},{first.z:.1f} overlaps "
                    f"{second.type} at {second.x:.1f},{second.z:.1f}"
                )

    if result.wall_entities > wall_budget:
        result.warnings.append(
            f"{result.wall_entities} wall entities exceeds the {wall_budget} budget"
        )

    return result


def build_structures(settlements: Sequence[Settlement]) -> list[dict]:
    """Serialise every settlement, buildings before walls.

    The map editor canonicalises structures into point buildings followed by
    line walls, so emitting them in that order keeps an editor round-trip
    byte-stable (see MapEditorMapDataTest.RealMapRoundTrips...).
    """
    entries: list[dict] = []
    for settlement in settlements:
        entries.extend(building.to_json() for building in settlement.buildings)
    for settlement in settlements:
        entries.extend(wall.to_json() for wall in settlement.walls)
    return entries


def process_map(
    path: Path,
    *,
    write: bool,
    validate_only: bool,
    crown_safety: float,
    max_player_homes: int,
    wall_budget: int,
    max_hill_width: float,
    max_hill_depth: float,
    max_hill_growth: float,
    terrain_clearance: float,
) -> bool:
    definition = json.loads(path.read_text())
    raw_settlements = definition.get("settlements")
    if not raw_settlements:
        print(f"{path}: SKIP: no settlements block")
        return True

    settlements = [Settlement.from_json(entry) for entry in raw_settlements]
    terrain = definition.get("terrain") or []

    for settlement in settlements:

        if settlement.tier == "town" or settlement.on_hill is False:
            standing_on = hill_containing(terrain, settlement.x, settlement.z)
            if standing_on is not None:
                push_settlement_off_hill(settlement, standing_on)
            settlement.on_hill = False
            continue

        if settlement.on_hill is None:
            standing_on = hill_containing(terrain, settlement.x, settlement.z)
            if standing_on is None:
                settlement.on_hill = False
                continue
            settlement.on_hill = True
        spec = TIER_SPECS[settlement.tier]
        hill = find_hill_at(terrain, settlement.x, settlement.z)
        if hill is None:
            raise SettlementError(
                f"{settlement.id} is marked on_hill but no hill is near it"
            )
        normalise_hill_dimensions(hill)

        growth_width = min(max_hill_width, float(hill["width"]) * max_hill_growth)
        growth_depth = min(max_hill_depth, float(hill["depth"]) * max_hill_growth)

        water_gap = distance_to_water(definition, float(hill["x"]), float(hill["z"]))
        if math.isfinite(water_gap):
            water_limit = max(float(hill["width"]), 2.0 * max(water_gap - 6.0, 8.0))
            growth_width = min(growth_width, water_limit)
            growth_depth = min(growth_depth, water_limit)
        settlement.x = float(hill["x"])
        settlement.z = float(hill["z"])

        authored_height = float(hill.get("height", 2.0))
        for _ in range(24):
            layout_settlement(settlement, spec)
            half_x, half_z = settlement_footprint(settlement)
            fit_hill_to_footprint(
                hill, half_x, half_z, crown_safety, growth_width, growth_depth
            )
            if crown_fits(
                float(hill["width"]),
                float(hill["depth"]),
                authored_height,
                half_x,
                half_z,
                crown_safety,
            ):
                break
            settlement.scale *= 0.92
        else:
            raise SettlementError(
                f"{settlement.id} will not fit on its hill even at minimum scale"
            )
        keep_hill_entrances_outside_crown(hill)
        ensure_hill_approaches(hill)
        settlement.hill = hill

    base_mask = TerrainMask(definition, terrain_clearance)
    for settlement in settlements:
        spec = TIER_SPECS[settlement.tier]
        mask = base_mask
        crown: tuple[float, float] | None = None
        if settlement.on_hill:
            mask = TerrainMask(
                definition, terrain_clearance, exclude_hill=settlement.hill
            )
            crown = usable_crown_extent(
                float(settlement.hill["width"]),
                float(settlement.hill["depth"]),
                float(settlement.hill.get("height", 2.0)),
                crown_safety,
            )
        elif not settlement.walls_only and not mask.clear_for(
            settlement.x, settlement.z, BUILDING_SIZES["barracks"]
        ):

            moved = nearest_clear_anchor(
                mask, settlement.x, settlement.z, BUILDING_SIZES["barracks"]
            )
            if moved is None:
                raise SettlementError(
                    f"{settlement.id}: no clear ground near "
                    f"{settlement.x:.0f},{settlement.z:.0f} for a settlement"
                )
            settlement.x, settlement.z = moved
        layout_settlement(settlement, spec, mask, crown)

    result = validate(definition, settlements, max_player_homes, wall_budget)

    label = "VALIDATE" if validate_only else "GENERATE"
    status = "PASS" if result.passed() else "FAIL"
    print(
        f"{path} [{label}] {status}: settlements={len(settlements)}, "
        f"buildings={result.buildings}, wall_entities={result.wall_entities}"
    )
    for warning in result.warnings:
        print(f"  WARNING: {warning}")
    for error in result.errors:
        print(f"  ERROR: {error}", file=sys.stderr)

    if not result.passed():
        return False

    if write and not validate_only:
        definition["structures"] = build_structures(settlements)
        path.write_text(json.dumps(definition, indent=2) + "\n")
        print(f"  wrote {len(definition['structures'])} structure entries")

    return True


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("maps", nargs="*", type=Path, help="map JSON files")
    parser.add_argument(
        "--campaign",
        action="store_true",
        help="process all eight campaign maps in campaign order",
    )
    parser.add_argument(
        "--write",
        action="store_true",
        help="replace the structures array after successful validation",
    )
    parser.add_argument(
        "--validate-only",
        action="store_true",
        help="validate the generated layout without writing it",
    )
    parser.add_argument(
        "--crown-safety",
        type=float,
        default=0.80,
        help="fraction of a hill's plateau treated as usable, absorbing crown noise "
        "(measured range is 0.94-1.10 of the formula; see HillCrownGeometryTest)",
    )
    parser.add_argument(
        "--max-player-homes",
        type=int,
        default=10,
        help="cap on homes owned by the local player across a map",
    )
    parser.add_argument(
        "--wall-budget",
        type=int,
        default=1400,
        help="soft cap on generated wall entities per map",
    )
    parser.add_argument(
        "--max-hill-growth",
        type=float,
        default=2.4,
        help="largest multiple of its authored size a hill may be grown to",
    )
    parser.add_argument(
        "--terrain-clearance",
        type=float,
        default=4.0,
        help="extra clearance kept between buildings and blocking terrain",
    )
    parser.add_argument(
        "--max-hill-width",
        type=float,
        default=150.0,
        help="largest a hill may be grown to carry a settlement",
    )
    parser.add_argument(
        "--max-hill-depth",
        type=float,
        default=130.0,
        help="largest a hill may be grown to carry a settlement",
    )
    args = parser.parse_args(argv)
    if args.write and args.validate_only:
        parser.error("--write and --validate-only cannot be combined")
    if not 0.0 < args.crown_safety <= 1.0:
        parser.error("crown safety must be within (0, 1]")
    if not args.campaign and not args.maps:
        parser.error("provide at least one map or use --campaign")
    return args


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    repo_root = Path(__file__).resolve().parents[1]
    paths = list(args.maps)
    if args.campaign:
        paths.extend(repo_root / relative for relative in CAMPAIGN_MAPS)
    unique_paths = list(dict.fromkeys(path.resolve() for path in paths))

    failures = 0
    for path in unique_paths:
        try:
            if not process_map(
                path,
                write=args.write,
                validate_only=args.validate_only,
                crown_safety=args.crown_safety,
                max_player_homes=args.max_player_homes,
                wall_budget=args.wall_budget,
                max_hill_width=args.max_hill_width,
                max_hill_depth=args.max_hill_depth,
                max_hill_growth=args.max_hill_growth,
                terrain_clearance=args.terrain_clearance,
            ):
                failures += 1
        except (OSError, json.JSONDecodeError, SettlementError) as error:
            print(f"{path}: ERROR: {error}", file=sys.stderr)
            failures += 1
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
