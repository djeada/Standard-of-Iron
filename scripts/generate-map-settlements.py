#!/usr/bin/env python3
"""Generate settlement geometry for Standard of Iron map JSON.

Each map carries a top-level ``settlements`` array describing authorial intent:
where a settlement sits, who owns it, and which tier it belongs to. This tool
stamps that intent into concrete ``structures`` - buildings, wall rings with
gates, and towers - so towns read as inhabited places rather than a barracks
with two houses beside it.

Tiers
  town            outer wall ring, inner citadel with a temple, housing blocks
  fortified_camp  single wall ring, two housing rows, corner towers
  marching_camp   closed rampart with two gates, no marketplace

A town raises a temple in its citadel. Any settlement may override that with a
``temple`` boolean when a map wants a sanctuary at a camp, or a town without one.

Wall rings are laid cell by cell on the runtime's own 2-unit wall grid, so a ring
is closed by construction: the only cells left out are the ones a gate covers and
the ones no unit can walk anyway (a hill core, a lake, a river channel). A wall
that merely stops short of a river bank leaves a gap units walk straight through,
which is why clearance margins are not allowed to decide where a rampart ends.

A gate spans ``GATE_SPAN`` along the wall it closes, so the opening is sized to
match; anything wider leaves walkable ground beside the gate and the ring stops
meaning anything. Gate yaw follows the wall: 0 spans x, 90 spans z. Sides and
positions come from where roads actually cross the ring, because a gateway that
is not on the road makes formations walk the length of a curtain wall to get in;
when no road crosses, the gate is aimed at the nearest road instead of being
dropped at the midpoint of a side.

A settlement marked ``on_hill`` is fitted inside the hill's flat crown, and the
hill feature is widened when the footprint needs more room. Slopes are not
walkable, so anything hanging off the crown would be unreachable. Set
``grow_hill: false`` where the hill's size is load bearing for something else -
a road threading past it, a pass it forms one wall of - and the settlement is
shrunk to the crown it already has instead.

The command is dry-run by default. Pass --write to replace only the top-level
``structures`` array; all other content in the document is retained. Structures
carrying a ``landmark`` key belong to generate-map-landmarks.py and are carried
across untouched, so the two tools can be run in either order.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Sequence

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
    "temple": (3.0, 3.0),
    "wall_segment": (2.0, 2.0),
    "wall_gate": (6.0, 2.0),
}


GATE_SPAN = 6.0


WALL_SEGMENT_SPACING = 2


BUILDING_CLEARANCE = 2.0


BUILDING_GRID_PADDING = 1.0


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
    rotation: float | None = None

    def to_json(self) -> dict:
        entry: dict = {
            "type": self.type,
            "x": round(self.x, 2),
            "z": round(self.z, 2),
            "player_id": self.player_id,
        }
        if self.rotation is not None:
            entry["rotation"] = round(self.rotation, 2)
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
    temple: bool | None = None
    grow_hill: bool = True
    gateways: dict[str, float] | None = None

    buildings: list[Building] = field(default_factory=list)
    walls: list[WallRun] = field(default_factory=list)
    hill: dict | None = None
    seal: "TerrainMask | None" = None

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
            temple=(None if "temple" not in entry else bool(entry["temple"])),
            grow_hill=bool(entry.get("grow_hill", True)),
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
        self,
        definition: dict,
        clearance: float,
        exclude_hill: dict | None = None,
        influence: float = TERRAIN_INFLUENCE_MARGIN,
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
            feature["width"] = half_width * 2.0 * influence
            feature["depth"] = half_depth * 2.0 * influence

        working["roads"] = []
        self._field = roads.RoutingField(working, clearance)

    def walkable(self, x: float, z: float) -> bool:
        """Whether a unit could stand on this cell, ignoring buildings."""
        grid_x = int(round(x))
        grid_z = int(round(z))
        if not self._field.passable(grid_x, grid_z):
            return False
        return not self._field.water[self._field.index(grid_x, grid_z)]

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


def snap_wall_coordinate(value: float) -> int:
    """The lattice cell a wall authored at ``value`` actually spawns on.

    MapTransformer rounds a wall's position to the grid and then to the 2-unit
    wall lattice, so an off-lattice coordinate in the file is a fiction: it
    describes a wall somewhere the game will never put one. Authoring on the
    lattice is what makes a ring's gaps countable.
    """
    return int(math.floor(value / WALL_SEGMENT_SPACING + 0.5)) * WALL_SEGMENT_SPACING


def lattice_range(low: int, high: int) -> list[int]:
    return list(range(low, high + 1, WALL_SEGMENT_SPACING))


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
        for start, end in zip(points, points[1:], strict=False):
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


def clamp_hill_entrances(feature: dict, definition: dict, margin: float = 3.0) -> None:
    """Keep approaches on the map after a hill has been widened.

    A hill grown to carry a settlement can push its own compass entrances past
    the map edge, where nothing can use them and the routing field has no cells
    to carve. Sliding them back inside keeps the approach real.
    """
    grid = definition.get("grid") or {}
    width = float(grid.get("width", 0)) or None
    height = float(grid.get("height", 0)) or None
    if width is None or height is None:
        return
    for entrance in feature.get("entrances") or []:
        entrance["x"] = round(min(max(float(entrance["x"]), margin), width - margin), 2)
        entrance["z"] = round(
            min(max(float(entrance["z"]), margin), height - margin), 2
        )


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
    temple: bool


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
        temple=True,
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
        temple=False,
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
        temple=False,
    ),
}


SIDE_AXIS = {"north": "z", "south": "z", "west": "x", "east": "x"}


def opposite(facing: str) -> str:
    return {"north": "south", "south": "north", "east": "west", "west": "east"}[facing]


def perpendicular(facing: str) -> str:
    return {"north": "east", "south": "west", "east": "south", "west": "north"}[facing]


@dataclass
class RingEdge:
    """One side of a wall ring, as the lattice cells that side owns."""

    side: str
    horizontal: bool
    fixed: int
    cells: list[int]

    def point(self, along: int) -> tuple[int, int]:
        return (along, self.fixed) if self.horizontal else (self.fixed, along)


def ring_edges(cx: float, cz: float, half_x: float, half_z: float) -> list[RingEdge]:
    """The four sides of a ring, with each corner owned by exactly one side.

    Corners belong to the north and south runs, so the east and west runs stop
    two cells short of them. Without that the corner cell would be emitted by two
    runs and the wall entity count would drift away from what the ring costs.
    """
    left = snap_wall_coordinate(cx - half_x)
    right = snap_wall_coordinate(cx + half_x)
    top = snap_wall_coordinate(cz - half_z)
    bottom = snap_wall_coordinate(cz + half_z)
    flank = lattice_range(top + WALL_SEGMENT_SPACING, bottom - WALL_SEGMENT_SPACING)
    return [
        RingEdge("north", True, top, lattice_range(left, right)),
        RingEdge("south", True, bottom, lattice_range(left, right)),
        RingEdge("west", False, left, flank),
        RingEdge("east", False, right, flank),
    ]


GATE_CELLS = int(GATE_SPAN // WALL_SEGMENT_SPACING)


def gate_cells_on(edge: RingEdge, along: float) -> list[int]:
    """The lattice cells a gate covers on this side, or [] if it will not fit.

    A gate needs a wall cell left standing on either side of it, otherwise the
    "gate" is just the end of the wall and units walk around it.
    """
    if len(edge.cells) < GATE_CELLS + 2:
        return []
    reach = (GATE_CELLS // 2) * WALL_SEGMENT_SPACING
    low = edge.cells[0] + reach + WALL_SEGMENT_SPACING
    high = edge.cells[-1] - reach - WALL_SEGMENT_SPACING
    centre = min(max(snap_wall_coordinate(along), low), high)
    return [
        centre + offset * WALL_SEGMENT_SPACING
        for offset in range(-(GATE_CELLS // 2), GATE_CELLS // 2 + 1)
    ]


def build_ring(
    cx: float,
    cz: float,
    half_x: float,
    half_z: float,
    player_id: int,
    nation: str | None,
    gateways: dict[str, float],
    seal: TerrainMask | None = None,
) -> tuple[list[WallRun], list[Building]]:
    """A closed rectangular ring, laid one lattice cell at a time.

    Only two things take a cell out of the ring: a gate covering it, and ground
    no unit can walk on anyway. Everything else gets a wall, so the enclosure a
    map claims is the enclosure the game builds. Rings used to be emitted as four
    long runs and then clipped back from blocking terrain by a clearance margin,
    which opened a walkable gap between where the wall stopped and where the
    river or hill actually started.
    """
    walls: list[WallRun] = []
    gates: list[Building] = []

    for edge in ring_edges(cx, cz, half_x, half_z):
        if not edge.cells:
            continue
        covered: set[int] = set()
        if edge.side in gateways:
            covered = set(gate_cells_on(edge, gateways[edge.side]))
            if covered:
                centre = sorted(covered)[len(covered) // 2]
                gate_x, gate_z = edge.point(centre)
                if seal is None or seal.walkable(gate_x, gate_z):
                    gates.append(
                        Building(
                            "wall_gate",
                            gate_x,
                            gate_z,
                            player_id,
                            nation,
                            rotation=0.0 if edge.horizontal else 90.0,
                        )
                    )
                else:
                    covered = set()

        run: list[int] = []

        def flush(run: list[int] = run, edge: RingEdge = edge) -> None:
            if not run:
                return
            walls.append(
                WallRun(edge.point(run[0]), edge.point(run[-1]), player_id, nation)
            )
            run.clear()

        for along in edge.cells:
            point = edge.point(along)
            if along in covered or (seal is not None and not seal.walkable(*point)):
                flush()
                continue
            run.append(along)
        flush()

    return walls, gates


def nearest_road_point(
    definition: dict, x: float, z: float
) -> tuple[float, float] | None:
    """The road vertex closest to a point, for aiming a gate that has no crossing."""
    best: tuple[float, float] | None = None
    best_distance = float("inf")
    for road in definition.get("roads") or []:
        raw = road.get("waypoints") or [road.get("start"), road.get("end")]
        for point in raw:
            if not point:
                continue
            candidate = (float(point[0]), float(point[1]))
            distance = math.hypot(candidate[0] - x, candidate[1] - z)
            if distance < best_distance:
                best = candidate
                best_distance = distance
    return best


def road_gateways(
    definition: dict, settlement: Settlement, half_x: float, half_z: float
) -> dict[str, float]:
    """Where the road network crosses the ring, per side.

    A gateway that is not on the road makes formations walk the length of a
    curtain wall to get in, so both the side and the position along it are read
    off where the approaches actually cross the wall line. A ring always ends up
    with two gates: where roads supply only one, or none, the remaining gate is
    aimed at the nearest road rather than parked at the midpoint of a side, so a
    garrison always has a sortie route and a besieger always has a second front.
    """
    left, right = settlement.x - half_x, settlement.x + half_x
    top, bottom = settlement.z - half_z, settlement.z + half_z
    sides = {
        "north": ("z", top, left, right),
        "south": ("z", bottom, left, right),
        "west": ("x", left, top, bottom),
        "east": ("x", right, top, bottom),
    }

    crossings: dict[str, list[tuple[float, float]]] = {name: [] for name in sides}
    for road in definition.get("roads") or []:
        raw = road.get("waypoints") or [road.get("start"), road.get("end")]
        points = [tuple(map(float, point[:2])) for point in raw if point]
        width = float(road.get("width", 3.0))
        for a, b in zip(points, points[1:], strict=False):
            for name, (axis, fixed, span_low, span_high) in sides.items():
                p0, p1 = (a[1], b[1]) if axis == "z" else (a[0], b[0])
                if (p0 - fixed) * (p1 - fixed) > 0.0 or abs(p1 - p0) < 1e-6:
                    continue
                t = (fixed - p0) / (p1 - p0)
                along = (
                    a[0] + (b[0] - a[0]) * t
                    if axis == "z"
                    else a[1] + (b[1] - a[1]) * t
                )
                if span_low + GATE_SPAN <= along <= span_high - GATE_SPAN:
                    crossings[name].append((width, along))

    gateways: dict[str, float] = {}
    for name, hits in crossings.items():
        if hits:
            gateways[name] = max(hits)[1]

    if len(gateways) > 2:

        ranked = sorted(
            gateways,
            key=lambda name: -max(w for w, _ in crossings[name]),
        )
        chosen = [ranked[0]]
        chosen.append(
            next((n for n in ranked[1:] if n == opposite(ranked[0])), ranked[1])
        )
        gateways = {name: gateways[name] for name in chosen}

    while len(gateways) < 2:
        gateways.update(
            aimed_gateway(definition, settlement, sides, set(gateways))
            or fallback_gateway(settlement, sides, set(gateways))
        )
    return gateways


def aimed_gateway(
    definition: dict,
    settlement: Settlement,
    sides: dict[str, tuple[str, float, float, float]],
    taken: set[str],
) -> dict[str, float]:
    """A gate on the side facing the nearest road, positioned to point at it."""
    target = nearest_road_point(definition, settlement.x, settlement.z)
    if target is None:
        return {}
    offset_x = target[0] - settlement.x
    offset_z = target[1] - settlement.z
    order = [
        "east" if offset_x >= 0.0 else "west",
        "south" if offset_z >= 0.0 else "north",
    ]
    if abs(offset_z) > abs(offset_x):
        order.reverse()
    order.extend(opposite(side) for side in list(order))
    for side in order:
        if side in taken:
            continue
        axis, _fixed, span_low, span_high = sides[side]
        along = target[1] if axis == "x" else target[0]
        return {side: min(max(along, span_low + GATE_SPAN), span_high - GATE_SPAN)}
    return {}


def fallback_gateway(
    settlement: Settlement,
    sides: dict[str, tuple[str, float, float, float]],
    taken: set[str],
) -> dict[str, float]:
    """The gate a ring gets when the map has no roads at all."""
    for side in (
        settlement.facing,
        opposite(settlement.facing),
        "north",
        "south",
        "east",
        "west",
    ):
        if side in taken:
            continue
        axis, _fixed, span_low, span_high = sides[side]
        return {side: (span_low + span_high) * 0.5}
    return {}


def ring_box(
    settlement: Settlement, spec: TierSpec
) -> tuple[float, float, float, float]:
    """The outer wall ring as a rectangle, with a margin for the wall itself."""
    half_x = spec.outer_half_x * settlement.scale + WALL_SEGMENT_SPACING
    half_z = spec.outer_half_z * settlement.scale + WALL_SEGMENT_SPACING
    return (
        settlement.x - half_x,
        settlement.z - half_z,
        settlement.x + half_x,
        settlement.z + half_z,
    )


def inside_any_ring(
    boxes: Sequence[tuple[float, float, float, float]], x: float, z: float
) -> bool:
    return any(
        left <= x <= right and top <= z <= bottom for left, top, right, bottom in boxes
    )


def move_entrances_out_of_rings(
    terrain: Sequence[dict], boxes: Sequence[tuple[float, float, float, float]]
) -> list[str]:
    """Keep hill ramps from opening inside somebody's walls.

    Where a hill's flank closes a side of a wall ring, the wall there is left out
    because the ground is not walkable - but an entrance carves a ramp through
    exactly that ground. A ramp inside the ring and another outside it is a way
    in that never passes a gate, and it is invisible to a check that treats hills
    as solid. Each offending entrance is walked around the hill's edge to the
    nearest point outside every ring, keeping the hill's approach count.
    """
    moved: list[str] = []
    if not boxes:
        return moved
    for feature in terrain:
        if str(feature.get("type", "")).lower() != "hill":
            continue
        entrances = feature.get("entrances") or []
        if not entrances:
            continue
        centre_x = float(feature.get("x", 0.0))
        centre_z = float(feature.get("z", 0.0))
        half_width, half_depth = hill_extents(feature)
        if half_width <= 0.0 or half_depth <= 0.0:
            continue
        for entrance in entrances:
            x = float(entrance.get("x", centre_x))
            z = float(entrance.get("z", centre_z))
            if not inside_any_ring(boxes, x, z):
                continue
            start = math.atan2((z - centre_z) / half_depth, (x - centre_x) / half_width)
            best: tuple[float, float] | None = None
            for step in range(1, 73):
                for direction in (1.0, -1.0):
                    angle = start + direction * step * math.pi / 36.0
                    candidate_x = centre_x + math.cos(angle) * half_width
                    candidate_z = centre_z + math.sin(angle) * half_depth
                    if not inside_any_ring(boxes, candidate_x, candidate_z):
                        best = (candidate_x, candidate_z)
                        break
                if best is not None:
                    break
            if best is None:
                continue
            entrance["x"] = round(best[0], 2)
            entrance["z"] = round(best[1], 2)
            moved.append(
                f"hill {centre_x:.0f},{centre_z:.0f}: ramp moved from "
                f"{x:.0f},{z:.0f} to {best[0]:.0f},{best[1]:.0f}, out of a wall ring"
            )
    return moved


def layout_settlement(
    settlement: Settlement,
    spec: TierSpec,
    mask: TerrainMask | None = None,
    crown: tuple[float, float] | None = None,
    seal: TerrainMask | None = None,
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

    gateways = settlement.gateways or {
        settlement.facing: cz if settlement.facing in ("east", "west") else cx,
        opposite(settlement.facing): (
            cz if settlement.facing in ("east", "west") else cx
        ),
    }

    if settlement.walls_only:
        settlement.walls, settlement.buildings = build_ring(
            cx, cz, half_x, half_z, owner, nation, gateways, seal
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

    wants_temple = spec.temple if settlement.temple is None else settlement.temple
    if wants_temple:

        temple_site = nearest_clear(cx - 9.0, cz, "temple")
        if temple_site is not None:
            buildings.append(
                Building("temple", temple_site[0], temple_site[1], owner, nation)
            )

    if spec.inner_half_x is not None:
        inner_half_x = spec.inner_half_x * scale
        inner_half_z = (spec.inner_half_x - 2.0) * scale

        inner_gate = perpendicular(settlement.facing)
        inner_along = cz if inner_gate in ("east", "west") else cx
        citadel_walls, citadel_gates = build_ring(
            cx,
            cz,
            inner_half_x,
            inner_half_z,
            owner,
            nation,
            {inner_gate: inner_along},
            seal,
        )
        walls.extend(citadel_walls)
        buildings.extend(citadel_gates)
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
        outer_walls, outer_gates = build_ring(
            cx, cz, half_x, half_z, owner, nation, gateways, seal
        )
        walls.extend(outer_walls)
        buildings.extend(outer_gates)

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

    settlement.walls = walls
    settlement.buildings = buildings


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


class OccupancyGrid:
    """Which cells the runtime's collision registry will call blocked.

    Structures are authored in grid coordinates and loaded into a world whose
    origin sits at the middle of the map, so a wall's footprint straddles cell
    boundaries that grid coordinates alone do not show. Reproducing that shift is
    the only way a gap counted here is a gap a unit can actually walk through.
    """

    def __init__(self, definition: dict):
        grid = definition.get("grid") or {}
        self.shift_x = float(grid.get("width", 0)) * 0.5 - 0.5
        self.shift_z = float(grid.get("height", 0)) * 0.5 - 0.5
        self.blocked: dict[tuple[int, int], str] = {}

    def _axis(self, centre: float, shift: float, half: float, padding: float) -> range:
        world = centre - shift
        base = int(round(shift + 0.5))
        low = math.floor(world - half - padding) + base
        high = math.ceil(world + half + padding) + base
        return range(low, high)

    def add(
        self,
        kind: str,
        x: float,
        z: float,
        size: tuple[float, float],
        padding: float,
    ) -> None:
        for cell_x in self._axis(x, self.shift_x, size[0] * 0.5, padding):
            for cell_z in self._axis(z, self.shift_z, size[1] * 0.5, padding):
                self.blocked.setdefault((cell_x, cell_z), kind)

    def add_settlement(self, settlement: Settlement) -> None:
        for building in settlement.buildings:
            if building.type == "wall_gate":
                spans_x = (building.rotation or 0.0) % 180.0 < 45.0
                size = (GATE_SPAN, 2.0) if spans_x else (2.0, GATE_SPAN)
                self.add("gate", building.x, building.z, size, 0.0)
                continue
            self.add(
                "building",
                building.x,
                building.z,
                BUILDING_SIZES.get(building.type, (2.0, 2.0)),
                BUILDING_GRID_PADDING,
            )
        for wall in settlement.walls:
            for point in wall_run_cells(wall):
                self.add("wall", point[0], point[1], (2.0, 2.0), 0.0)


def wall_run_cells(wall: WallRun) -> list[tuple[int, int]]:
    """The individual wall entities a run spawns, as MapTransformer expands it."""
    start = (snap_wall_coordinate(wall.start[0]), snap_wall_coordinate(wall.start[1]))
    end = (snap_wall_coordinate(wall.end[0]), snap_wall_coordinate(wall.end[1]))
    if abs(end[0] - start[0]) >= abs(end[1] - start[1]):
        step = WALL_SEGMENT_SPACING if end[0] >= start[0] else -WALL_SEGMENT_SPACING
        return [(x, start[1]) for x in range(start[0], end[0] + step, step)]
    step = WALL_SEGMENT_SPACING if end[1] >= start[1] else -WALL_SEGMENT_SPACING
    return [(start[0], z) for z in range(start[1], end[1] + step, step)]


def enclosure_breach(
    settlement: Settlement,
    spec: TierSpec,
    occupancy: OccupancyGrid,
    seal: TerrainMask,
) -> tuple[int, int] | None:
    """Flood the inside of a walled settlement and return where it leaks out.

    A ring is only worth building if an attacker has to come through a gate, so
    this walks out from the centre over ground that is walkable and unoccupied.
    Reaching open country past the ring means there is a hole.
    """
    from collections import deque

    centre_x = int(round(settlement.x))
    centre_z = int(round(settlement.z))
    reach_x = int(spec.outer_half_x * settlement.scale) + 6
    reach_z = int(spec.outer_half_z * settlement.scale) + 6

    start: tuple[int, int] | None = None
    for radius in range(0, 10):
        for offset_z in range(-radius, radius + 1):
            for offset_x in range(-radius, radius + 1):
                probe = (centre_x + offset_x, centre_z + offset_z)
                if probe not in occupancy.blocked and seal.walkable(*probe):
                    start = probe
                    break
            if start is not None:
                break
        if start is not None:
            break
    if start is None:
        return None

    seen = {start}
    pending = deque([start])
    while pending:
        x, z = pending.popleft()
        if abs(x - centre_x) > reach_x or abs(z - centre_z) > reach_z:
            return (x, z)
        for step_x, step_z in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            neighbour = (x + step_x, z + step_z)
            if neighbour in seen or neighbour in occupancy.blocked:
                continue
            if not seal.walkable(*neighbour):
                continue
            seen.add(neighbour)
            pending.append(neighbour)
    return None


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
    check_enclosure: bool = False,
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

    if check_enclosure:
        occupancy = OccupancyGrid(definition)
        for settlement in settlements:
            occupancy.add_settlement(settlement)
        for settlement in settlements:
            if not settlement.palisade or settlement.seal is None:
                continue
            breach = enclosure_breach(
                settlement, TIER_SPECS[settlement.tier], occupancy, settlement.seal
            )
            if breach is not None:
                result.errors.append(
                    f"{settlement.id}: wall ring has a hole - a unit walks from the "
                    f"centre out to {breach[0]},{breach[1]} without passing a gate"
                )

        boxes = [
            ring_box(settlement, TIER_SPECS[settlement.tier])
            for settlement in settlements
            if settlement.palisade
        ]
        for feature in definition.get("terrain") or []:
            if str(feature.get("type", "")).lower() != "hill":
                continue
            for entrance in feature.get("entrances") or []:
                x = float(entrance.get("x", 0.0))
                z = float(entrance.get("z", 0.0))
                if inside_any_ring(boxes, x, z):
                    result.errors.append(
                        f"hill at {feature.get('x')},{feature.get('z')} carves a ramp "
                        f"at {x:.0f},{z:.0f} inside a wall ring, which is a way in "
                        "that never passes a gate"
                    )

    return result


def build_structures(
    settlements: Sequence[Settlement], carried: Sequence[dict] = ()
) -> list[dict]:
    """Serialise every settlement, buildings before walls.

    The map editor canonicalises structures into point buildings followed by
    line walls, so emitting them in that order keeps an editor round-trip
    byte-stable (see MapEditorMapDataTest.RealMapRoundTrips...). Entries this
    tool does not own - landmark pieces and authored settlements - are carried
    through untouched, but they are split the same way: a carried wall run has to
    land in the wall half or the round trip reorders the array.
    """
    carried_points = [e for e in carried if e.get("type") != "wall_segment"]
    carried_walls = [e for e in carried if e.get("type") == "wall_segment"]

    entries: list[dict] = []
    for settlement in settlements:
        entries.extend(building.to_json() for building in settlement.buildings)
    entries.extend(carried_points)
    for settlement in settlements:
        entries.extend(wall.to_json() for wall in settlement.walls)
    entries.extend(carried_walls)
    return entries


def serialise_like(path: Path, definition: dict) -> str:
    """Re-serialise a map in the style it was already written in.

    Maps saved from the editor use four-space indentation with sorted keys;
    hand-authored ones use two spaces in authored order. Writing everything one
    way turns a settlement edit into a whole-file diff, which is what made this
    tool unsafe to run on the maps the editor had touched.
    """
    original = path.read_text()
    sorted_keys = original.startswith('{\n    "')
    indent = 4 if sorted_keys else 2
    return json.dumps(definition, indent=indent, sort_keys=sorted_keys) + "\n"


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

    settlements = [
        Settlement.from_json(entry)
        for entry in raw_settlements
        if not entry.get("authored")
    ]
    authored = [entry for entry in raw_settlements if entry.get("authored")]
    for entry in authored:
        print(f"  authored: {entry.get('id')} is laid out by hand, left alone")
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

        growth = max_hill_growth if settlement.grow_hill else 1.0
        growth_width = min(max_hill_width, float(hill["width"]) * growth)
        growth_depth = min(max_hill_depth, float(hill["depth"]) * growth)

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
        clamp_hill_entrances(hill, definition)
        settlement.hill = hill

    base_mask = TerrainMask(definition, terrain_clearance)
    base_seal = TerrainMask(definition, 0.0, influence=1.0)
    for settlement in settlements:
        spec = TIER_SPECS[settlement.tier]
        mask = base_mask
        settlement.seal = base_seal
        crown: tuple[float, float] | None = None
        if settlement.on_hill:
            mask = TerrainMask(
                definition, terrain_clearance, exclude_hill=settlement.hill
            )

            settlement.seal = TerrainMask(
                definition, 0.0, exclude_hill=settlement.hill, influence=1.0
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
        settlement.gateways = road_gateways(
            definition,
            settlement,
            spec.outer_half_x * settlement.scale,
            spec.outer_half_z * settlement.scale,
        )
        layout_settlement(settlement, spec, mask, crown, settlement.seal)

    ring_boxes = [
        ring_box(settlement, TIER_SPECS[settlement.tier])
        for settlement in settlements
        if settlement.palisade
    ]
    moved_ramps = move_entrances_out_of_rings(terrain, ring_boxes)

    result = validate(
        definition, settlements, max_player_homes, wall_budget, check_enclosure=True
    )

    label = "VALIDATE" if validate_only else "GENERATE"
    status = "PASS" if result.passed() else "FAIL"
    print(
        f"{path} [{label}] {status}: settlements={len(settlements)}, "
        f"buildings={result.buildings}, wall_entities={result.wall_entities}"
    )
    for note in moved_ramps:
        print(f"  moved: {note}")
    for warning in result.warnings:
        print(f"  WARNING: {warning}")
    for error in result.errors:
        print(f"  ERROR: {error}", file=sys.stderr)

    if not result.passed():
        return False

    if write and not validate_only:
        definition["structures"] = build_structures(
            settlements,
            [
                entry
                for entry in definition.get("structures") or []
                if entry.get("landmark") is not None or entry.get("authored")
            ],
        )
        path.write_text(serialise_like(path, definition))
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
