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

Plans (``plan``, with ``plan_options``) set the shape of the ring independently
of its tier: rect, stepped, circle, star, twin, terraced, curtain. See
scripts/RTS_MAP_DESIGN.md and settlement_geometry.py for what each one is.

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
Generated structures carry a ``settlement`` key naming their settlement, which is
how a rerun replaces exactly its own output and nothing else.
"""

from __future__ import annotations

import argparse
import json
import math
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Sequence

sys.path.insert(0, str(Path(__file__).resolve().parent))

from map_hill_shapes import canonical_hill_shape, hill_half_thickness
from map_water_geometry import river_points
from settlement_geometry import (
    WALL_SEGMENT_SPACING,
    Circuit,
    Region,
    bastion_apexes,
    bastioned_polygon,
    chamfered_rectangle_polygon,
    cut_gate,
    ellipse_polygon,
    lattice_range,
    partition_runs,
    path_cells,
    polygon_contains,
    polyline_distance,
    rectangle_polygon,
    region_cells,
    snap_wall_coordinate,
)

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
    "barracks": (8.65, 4.20),
    "home": (3.0, 3.0),
    "marketplace": (3.0, 3.0),
    "defense_tower": (3.0, 3.0),
    "temple": (3.17, 3.0),
    "wall_segment": (2.02, 2.0),
    "wall_gate": (6.0, 2.0),
}
"""The space a building actually occupies, in world units.

These used to be the *navigation* footprints, which are not what a building
looks like: a barracks navigates as 4.0 x 4.0 but renders as 8.65 x 4.20, so
the generator cleared less than half the hall and dropped campfires, towers and
homes inside it.  The rendered bodies are ``s_building_bodies`` in
game/systems/building_collision_registry.cpp; keep these in step with it.

Each entry is the larger of the rendered body and the clearance the generator
already demanded, so fixing the under-measured barracks does not quietly let
everything else pack tighter than it does today.  ``wall_gate`` stays at
``GATE_SPAN`` on purpose -- a gateway needs a clear opening much wider than the
2.02 x 0.76 body of the gate itself.  Every barracks in ``assets/maps`` is
axis-aligned, so the un-rotated body is exact; a rotated one would need the
engine's own axis-aligned expansion.
"""


GATE_SPAN = 6.0


BUILDING_CLEARANCE = 2.0


BUILDING_GRID_PADDING = 1.0


ROAD_VERGE = 0.5
"""How far a building keeps back from the kerb of a road through the ring.

Gates go where roads cross the wall, so the road continues inside; a house
placed on the street grid without looking at it stands in the roadway, which
fix-map-prop-overlaps.py reports and the road generator cannot route around.
"""


TERRAIN_INFLUENCE_MARGIN = 1.22

WALL_WATER_CLEARANCE = 1.5

RAISED_FLAT_MIN_HEIGHT = 0.5
RAISED_FLAT_EDGE_INNER = 0.74
RAISED_FLAT_EDGE_OUTER = 1.08

TERRACE_RAMP_HALF_WIDTH = 6.0
TERRACE_RAMP_REACH = 2.0

TIERS = ("town", "fortified_camp", "marching_camp")

PLAN_KINDS = (
    "rect",
    "stepped",
    "circle",
    "star",
    "twin",
    "terraced",
    "curtain",
)
"""The shape of a settlement's wall circuit, independent of its tier.

Tier says how much a place is - how big its ring, whether it has a market, a
citadel, streets. Plan says what shape that ring is, which is the difference
between a legion's playing-card camp, a native contour fort, a bastioned trace
and a siege line. They are separate axes on purpose: a marching camp can be a
circle and a town can be a star.
"""

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
    settlement: str | None = None

    def to_json(self) -> dict:
        entry: dict = {
            "type": self.type,
            "x": round(self.x, 2),
            "z": round(self.z, 2),
            "player_id": self.player_id,
        }
        if self.settlement:
            entry["authored"] = True
            entry["settlement"] = self.settlement
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
    settlement: str | None = None

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
        if self.settlement:
            entry["authored"] = True
            entry["settlement"] = self.settlement
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
    citadel: bool = True
    outer_towers: int | None = None
    minimum_homes: int = 0
    scale: float = 1.0
    temple: bool | None = None
    grow_hill: bool = True
    gateways: dict[str, float] | None = None
    plan: str = "rect"
    plan_options: dict = field(default_factory=dict)

    buildings: list[Building] = field(default_factory=list)
    walls: list[WallRun] = field(default_factory=list)
    hill: dict | None = None
    terraces: list[dict] = field(default_factory=list)
    seal: "TerrainMask | None" = None
    approaches: list[tuple[float, list[tuple[float, float]]]] = field(
        default_factory=list
    )

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
        plan = str(entry.get("plan", "rect")).lower()
        if plan not in PLAN_KINDS:
            raise SettlementError(f"unknown settlement plan: {plan}")
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
            citadel=bool(entry.get("citadel", True)),
            outer_towers=(
                None if "outer_towers" not in entry else int(entry["outer_towers"])
            ),
            minimum_homes=max(0, int(entry.get("minimum_homes", 0))),
            scale=float(entry.get("scale", 1.0)),
            temple=(None if "temple" not in entry else bool(entry["temple"])),
            grow_hill=bool(entry.get("grow_hill", True)),
            plan=plan,
            plan_options=dict(entry.get("plan_options") or {}),
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
    module.__dict__["__file__"] = str(path)
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
        exclude_hills: Sequence[dict] = (),
        influence: float = TERRAIN_INFLUENCE_MARGIN,
        water_clearance: float = 0.0,
    ):
        roads = _load_road_generator()
        working = json.loads(json.dumps(definition))
        if water_clearance > 0.0:
            for river in working.get("rivers") or []:
                river["width"] = float(river.get("width", 3.0)) + 2.0 * water_clearance
            lakes = list(working.get("lakes") or [])
            lakes.extend(
                feature
                for feature in working.get("terrain") or []
                if str(feature.get("type", "")).lower() == "lake"
            )
            for lake in lakes:
                for key in ("radius", "width", "depth"):
                    if key in lake:
                        lake[key] = float(lake[key]) + (
                            water_clearance
                            if key == "radius"
                            else 2.0 * water_clearance
                        )
        self._flat_edges: list[tuple[float, float, float, float, float]] = []
        for feature in working.get("terrain") or []:
            if str(feature.get("type", "")).lower() != "flat":
                continue
            if abs(float(feature.get("height", 0.0))) < RAISED_FLAT_MIN_HEIGHT:
                continue
            half_width, half_depth = hill_extents(feature)
            if half_width <= 0.0 or half_depth <= 0.0:
                continue
            self._flat_edges.append(
                (
                    float(feature.get("x", 0.0)),
                    float(feature.get("z", 0.0)),
                    half_width,
                    half_depth,
                    math.radians(
                        float(feature.get("rotation", feature.get("rotation_deg", 0.0)))
                    ),
                )
            )
        if exclude_hills:

            def excluded(feature: dict) -> bool:
                if str(feature.get("type", "")).lower() != "hill":
                    return False
                return any(
                    abs(float(feature.get("x", 0.0)) - float(hill["x"])) < 0.01
                    and abs(float(feature.get("z", 0.0)) - float(hill["z"])) < 0.01
                    and abs(
                        float(feature.get("width", 0.0)) - float(hill.get("width", 0.0))
                    )
                    < 0.01
                    for hill in exclude_hills
                )

            working["terrain"] = [
                feature
                for feature in working.get("terrain") or []
                if not excluded(feature)
            ]

        for feature in working.get("terrain") or []:
            kind = str(feature.get("type", "")).lower()
            if kind not in {"hill", "mountain"}:
                continue
            half_width, half_depth = hill_extents(feature)
            if half_width <= 0.0 or half_depth <= 0.0:
                continue
            if canonical_hill_shape(feature.get("shape")):

                feature["thickness"] = hill_half_thickness(feature) * 2.0 * influence
                continue
            if kind == "mountain" and "width" not in feature:
                feature["radius"] = float(feature.get("radius", 5.0)) * influence
                continue
            feature.pop("radius", None)
            feature["width"] = half_width * 2.0 * influence
            feature["depth"] = half_depth * 2.0 * influence

        working["roads"] = []
        self._field = roads.RoutingField(working, clearance)

    def on_raised_flat_edge(self, x: float, z: float) -> bool:
        """Whether the cell lies on the feathered rim of a raised (or sunken) flat.

        The engine blends a flat's height in over the outer fifth of its ellipse,
        so a body there straddles a step of the flat's full height.
        """
        for center_x, center_z, half_width, half_depth, angle in self._flat_edges:
            local_x = x - center_x
            local_z = z - center_z
            cos_a = math.cos(angle)
            sin_a = math.sin(angle)
            rotated_x = local_x * cos_a + local_z * sin_a
            rotated_z = -local_x * sin_a + local_z * cos_a
            normalized = math.hypot(rotated_x / half_width, rotated_z / half_depth)
            if RAISED_FLAT_EDGE_INNER <= normalized <= RAISED_FLAT_EDGE_OUTER:
                return True
        return False

    def walkable(self, x: float, z: float) -> bool:
        """Whether a unit could stand on this cell, ignoring buildings."""
        grid_x = int(round(x))
        grid_z = int(round(z))
        if not self._field.passable(grid_x, grid_z):
            return False
        if self.on_raised_flat_edge(float(grid_x), float(grid_z)):
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
                if self.on_raised_flat_edge(float(grid_x), float(grid_z)):
                    return False
        return True


def hill_height(width: float, depth: float, authored_height: float) -> float:
    """Mirror of the campaign-scale hill height in terrain.cpp."""
    footprint_height = min(width, depth) * 0.18
    return max(authored_height * 2.80, footprint_height)


def hill_crown_extent(
    width: float, depth: float, authored_height: float, crown: float = 0.0
) -> tuple[float, float]:
    """Half-extents of a hill's flat crown, in grid cells.

    Mirrors the plateau computation in terrain.cpp. The crown is the only
    walkable part of a hill, so a settlement has to fit inside it. An authored
    ``crown`` fraction (0-0.9) sets the plateau's share of the footprint
    directly; without it the engine's campaign rule leaves about 38%, which is
    a redoubt on a mound, not a fort on a hill.
    """
    slope_width = max(2.0, width * 0.5)
    slope_depth = max(2.0, depth * 0.5)
    elevation_cells = max(hill_height(width, depth, authored_height), 0.25)
    slope_run = max(7.0, elevation_cells * 4.2)
    min_crown = 0.42
    max_slope = 0.62
    if crown > 0.0:
        min_crown = min(crown, 0.9)
        max_slope = 1.0 - min_crown

    plateau_width = max(
        1.5,
        slope_width * min_crown,
        slope_width - min(slope_width * max_slope, slope_run),
    )
    plateau_depth = max(
        1.5,
        slope_depth * min_crown,
        slope_depth - min(slope_depth * max_slope, slope_run),
    )
    return plateau_width, plateau_depth


def usable_crown_extent(
    width: float,
    depth: float,
    authored_height: float,
    safety: float,
    crown: float = 0.0,
) -> tuple[float, float]:
    """Crown extent reduced for the noise warping applied to the real crown.

    sample_hill() warps the crown ellipse with fbm noise and shifts it off
    centre, so the authored plateau is not reliable right up to its edge. The
    safety factor keeps placement well inside it; the authoritative check is the
    CampaignStructuresStandOnWalkableGround test, which uses the real terrain.
    """
    crown_x, crown_z = hill_crown_extent(width, depth, authored_height, crown)
    return crown_x * safety, crown_z * safety


def crown_fits(
    width: float,
    depth: float,
    authored_height: float,
    half_x: float,
    half_z: float,
    safety: float,
    round_footprint: bool = False,
    crown: float = 0.0,
) -> bool:
    """Whether a footprint sits inside the hill's elliptical crown.

    A rectangle has corners, so it only fits when its corner is inside the
    crown ellipse. A round ring has none: its half-extents are its reach on the
    axes, and it fits whenever those stay inside the crown's - which is why a
    circular fort on a hill is bigger than a square one on the same hill.
    """
    crown_x, crown_z = usable_crown_extent(width, depth, authored_height, safety, crown)
    if round_footprint:
        return half_x <= crown_x and half_z <= crown_z
    return math.hypot(half_x / max(crown_x, 0.001), half_z / max(crown_z, 0.001)) <= 1.0


def fit_hill_to_footprint(
    feature: dict,
    half_x: float,
    half_z: float,
    safety: float,
    max_width: float,
    max_depth: float,
    round_footprint: bool = False,
) -> bool:
    """Widen a hill until its usable crown contains the settlement footprint.

    Growth is capped: a hill wide enough to carry a full town would span half the
    map and stop reading as terrain. Callers shrink the settlement instead when
    this returns False.
    """
    authored_height = float(feature.get("height", 2.0))
    authored_crown = float(feature.get("crown", 0.0))
    changed = False
    for _ in range(96):
        width = float(feature.get("width", 0.0))
        depth = float(feature.get("depth", 0.0))
        if crown_fits(
            width,
            depth,
            authored_height,
            half_x,
            half_z,
            safety,
            round_footprint,
            authored_crown,
        ):
            return changed
        if width >= max_width and depth >= max_depth:
            return changed

        crown_x, crown_z = usable_crown_extent(
            width, depth, authored_height, safety, authored_crown
        )
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
    A radius-only mountain is not a circle, though: the terrain code stretches it
    to 2.68 x 1.60 of its radius, and the road generator's raster knows that.
    ``TerrainMask`` therefore leaves such a mountain's radius alone rather than
    rewriting it as a width and depth, which turned it into a circle that
    blocked ground the engine leaves open - and a wall cell dropped for ground
    that is actually open is a hole in the ring.
    """
    radius = float(feature.get("radius") or 0.0)
    width = float(feature.get("width") or radius * 2.0)
    depth = float(feature.get("depth") or radius * 2.0)
    return width * 0.5, depth * 0.5


def hills_containing(terrain: Sequence[dict], x: float, z: float) -> list[dict]:
    """Every hill a point stands on, largest first.

    Hills stack: a knoll authored on a hill's crown is a second, taller hill at
    the same place. The largest is the ground a settlement is fitted to; the
    ones on top of it are terraces inside the ring, not terrain that blocks it.
    """
    found: list[dict] = []
    for feature in terrain:
        if str(feature.get("type", "")).lower() != "hill":
            continue
        half_width, half_depth = hill_extents(feature)
        if half_width <= 0.0 or half_depth <= 0.0:
            continue
        offset_x = (x - float(feature.get("x", 0.0))) / half_width
        offset_z = (z - float(feature.get("z", 0.0))) / half_depth
        if math.hypot(offset_x, offset_z) <= 1.0:
            found.append(feature)
    found.sort(key=lambda feature: -hill_extents(feature)[0] * hill_extents(feature)[1])
    return found


def hill_containing(terrain: Sequence[dict], x: float, z: float) -> dict | None:
    """The hill a point already stands on, if any."""
    found = hills_containing(terrain, x, z)
    return found[0] if found else None


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
        points = river_points(river)
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


def ensure_terrace_ramp(terrace: dict, settlement: "Settlement") -> None:
    """Give a keep raised on the crown its one ramp, on the settlement's facing.

    A hill without authored entrances gets the engine's default ramp on its
    west side, straight through whatever the lower ring put there. The keep's
    gate is cut on the facing side, so that is where the ramp belongs, and the
    terraced plan keeps that corridor free of buildings.
    """
    if terrace.get("entrances"):
        return
    half_x, half_z = hill_extents(terrace)
    facing_x, facing_z = FACINGS[settlement.facing]
    terrace["entrances"] = [
        {
            "x": round(
                float(terrace["x"]) + facing_x * (half_x + TERRACE_RAMP_REACH), 2
            ),
            "z": round(
                float(terrace["z"]) + facing_z * (half_z + TERRACE_RAMP_REACH), 2
            ),
            "radius": 2.0,
        }
    ]


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


@dataclass
class Plan:
    """A settlement's wall circuits and the ground they enclose."""

    circuits: list[Circuit]
    region: Region
    half_x: float
    half_z: float
    towers: list[tuple[float, float]] = field(default_factory=list)
    cells: set[tuple[int, int]] = field(default_factory=set)
    keep_clear: list[list[tuple[float, float]]] = field(default_factory=list)
    anchors: dict[str, tuple[float, float]] = field(default_factory=dict)

    def wall_distance(self, x: float, z: float) -> float:
        if not self.cells:
            return float("inf")
        return min(math.hypot(x - cell[0], z - cell[1]) for cell in self.cells)


def plan_extent(settlement: Settlement, spec: TierSpec) -> tuple[float, float]:
    """The half-extents a settlement's circuit reaches, plan included.

    Roads, ramps and the occupancy checks all need to know how far a settlement
    reaches before its walls exist, and a plan is free to be bigger or smaller
    than its tier's template - a bastioned trace throws its points well past the
    curtain, a twin camp is two lobes wide.
    """
    size = settlement.plan_options.get("size")
    if size:
        half_x = float(size[0]) * settlement.scale
        half_z = float(size[1]) * settlement.scale
    else:
        half_x = spec.outer_half_x * settlement.scale
        half_z = spec.outer_half_z * settlement.scale
    if settlement.plan == "star":
        reach = float(settlement.plan_options.get("bastion", 11.0))
        half_x += reach * 0.7
        half_z += reach * 0.7
    elif settlement.plan == "twin":
        offset = settlement.plan_options.get("offset") or [half_x * 1.7, 0.0]
        half_x += abs(float(offset[0]))
        half_z += abs(float(offset[1]))
    elif settlement.plan == "curtain":
        path = settlement.plan_options.get("path") or []
        if path:
            half_x = max(abs(float(point[0]) - settlement.x) for point in path) + 6.0
            half_z = max(abs(float(point[1]) - settlement.z) for point in path) + 6.0
    return (half_x, half_z)


def build_plan(settlement: Settlement, spec: TierSpec) -> Plan:
    """Turn a settlement's plan into circuits, an enclosed region and towers."""
    cx, cz = settlement.x, settlement.z
    options = settlement.plan_options
    size = options.get("size")
    if size:
        half_x = float(size[0]) * settlement.scale
        half_z = float(size[1]) * settlement.scale
    else:
        half_x = spec.outer_half_x * settlement.scale
        half_z = spec.outer_half_z * settlement.scale

    extent_x, extent_z = plan_extent(settlement, spec)

    if settlement.plan == "stepped":
        chamfer = float(options.get("chamfer", min(half_x, half_z) * 0.34))
        outline = chamfered_rectangle_polygon(cx, cz, half_x, half_z, chamfer)
        region = Region([outline])
        towers = [
            (cx + sx * (half_x - chamfer * 0.5), cz + sz * (half_z - chamfer * 0.5))
            for sx, sz in ((-1, -1), (1, -1), (1, 1), (-1, 1))
        ]
        return Plan([Circuit(region=region)], region, extent_x, extent_z, towers)

    if settlement.plan == "circle":
        outline = ellipse_polygon(cx, cz, half_x, half_z)
        region = Region([outline])
        count = max(1, int(options.get("towers", 4)))
        towers = []
        for index in range(count):
            angle = math.pi * 0.25 + 2.0 * math.pi * index / count
            towers.append(
                (
                    cx + math.cos(angle) * (half_x - 5.0),
                    cz + math.sin(angle) * (half_z - 5.0),
                )
            )
        return Plan([Circuit(region=region)], region, extent_x, extent_z, towers)

    if settlement.plan == "star":
        sides = int(options.get("points", 4))
        bastion = float(options.get("bastion", 11.0))
        flank = float(options.get("flank", min(half_x, half_z) * 0.32))
        stretch = 1.0 / math.cos(math.pi / sides)
        outline = bastioned_polygon(
            cx, cz, half_x * stretch, half_z * stretch, sides, bastion, flank
        )
        region = Region([outline])
        towers = bastion_apexes(cx, cz, half_x * stretch, half_z * stretch, sides, -6.0)
        return Plan([Circuit(region=region)], region, extent_x, extent_z, towers)

    if settlement.plan == "twin":
        offset = options.get("offset") or [half_x * 1.7, 0.0]
        shift_x, shift_z = float(offset[0]), float(offset[1])
        lobe = str(options.get("lobe", "rect"))
        neck = float(options.get("neck", 7.0))
        polygons = []
        for sign in (-1.0, 1.0):
            lobe_x = cx + sign * shift_x * 0.5
            lobe_z = cz + sign * shift_z * 0.5
            if lobe == "circle":
                polygons.append(ellipse_polygon(lobe_x, lobe_z, half_x, half_z))
            else:
                polygons.append(rectangle_polygon(lobe_x, lobe_z, half_x, half_z))
        if abs(shift_x) >= abs(shift_z):
            polygons.append(rectangle_polygon(cx, cz, abs(shift_x) * 0.5 + 1.0, neck))
        else:
            polygons.append(rectangle_polygon(cx, cz, neck, abs(shift_z) * 0.5 + 1.0))
        region = Region(polygons)
        keep_clear = [
            rectangle_polygon(
                cx,
                cz,
                abs(shift_x) * 0.5 + 2.0 if abs(shift_x) >= abs(shift_z) else neck,
                neck if abs(shift_x) >= abs(shift_z) else abs(shift_z) * 0.5 + 2.0,
            )
        ]
        towers = []
        for sign in (-1.0, 1.0):
            lobe_x = cx + sign * shift_x * 0.5
            lobe_z = cz + sign * shift_z * 0.5
            towers.extend(
                [
                    (lobe_x + sign * (half_x - 4.0), lobe_z - (half_z - 4.0)),
                    (lobe_x + sign * (half_x - 4.0), lobe_z + (half_z - 4.0)),
                ]
            )
        lobe_a = (cx - shift_x * 0.5, cz - shift_z * 0.5)
        lobe_b = (cx + shift_x * 0.5, cz + shift_z * 0.5)
        return Plan(
            [Circuit(region=region)],
            region,
            extent_x,
            extent_z,
            towers,
            keep_clear=keep_clear,
            anchors={
                "barracks": lobe_a,
                "market": lobe_b,
                "temple": (lobe_b[0], lobe_b[1] - half_z * 0.4),
            },
        )

    if settlement.plan == "terraced":
        inner = options.get("inner") or [half_x * 0.42, half_z * 0.42]
        inner_x, inner_z = float(inner[0]), float(inner[1])
        outer_outline = ellipse_polygon(cx, cz, half_x, half_z)
        inner_outline = ellipse_polygon(cx, cz, inner_x, inner_z)
        outer_region = Region([outer_outline])
        inner_region = Region([inner_outline])
        towers = [
            (
                cx + math.cos(math.pi * 0.25 + math.pi * 0.5 * index) * (half_x - 5.0),
                cz + math.sin(math.pi * 0.25 + math.pi * 0.5 * index) * (half_z - 5.0),
            )
            for index in range(4)
        ]
        upper_gate = FACINGS[settlement.facing]
        facing_x, facing_z = upper_gate
        side_x, side_z = (facing_z, facing_x)
        if abs(facing_x) > 0.0:
            inner_along, inner_across = inner_x, inner_z
            outer_along = half_x
        else:
            inner_along, inner_across = inner_z, inner_x
            outer_along = half_z
        towers.extend(
            [
                (
                    cx + side_x * (inner_across - 3.0),
                    cz + side_z * (inner_across - 3.0),
                ),
                (
                    cx - side_x * (inner_across - 3.0),
                    cz - side_z * (inner_across - 3.0),
                ),
            ]
        )
        corridor_mid = (inner_along + outer_along) * 0.5
        corridor_half = (outer_along - inner_along) * 0.5
        keep_clear = [
            rectangle_polygon(
                cx + facing_x * corridor_mid,
                cz + facing_z * corridor_mid,
                corridor_half if abs(facing_x) > 0.0 else TERRACE_RAMP_HALF_WIDTH,
                TERRACE_RAMP_HALF_WIDTH if abs(facing_x) > 0.0 else corridor_half,
            )
        ]
        circuits = [
            Circuit(region=outer_region),
            Circuit(
                region=inner_region,
                gate_targets=[
                    (cx + upper_gate[0] * inner_x, cz + upper_gate[1] * inner_z)
                ],
            ),
        ]
        market = (
            cx + side_x * (inner_across + 9.0) + facing_x * inner_along * 0.6,
            cz + side_z * (inner_across + 9.0) + facing_z * inner_along * 0.6,
        )
        temple = (
            cx - side_x * (inner_across + 9.0) - facing_x * inner_along * 0.6,
            cz - side_z * (inner_across + 9.0) - facing_z * inner_along * 0.6,
        )
        return Plan(
            circuits,
            outer_region,
            extent_x,
            extent_z,
            towers,
            keep_clear=keep_clear,
            anchors={
                "barracks": (cx, cz),
                "market": market,
                "temple": temple,
            },
        )

    if settlement.plan == "curtain":
        raw_path = options.get("path")
        if not raw_path:
            raise SettlementError(
                f"{settlement.id}: a curtain plan needs a plan_options.path"
            )
        path = [(float(point[0]), float(point[1])) for point in raw_path]
        depth = float(options.get("depth", 24.0))
        inward = []
        for point in path:
            toward = _towards(point, (cx, cz))
            inward.append((point[0] + toward[0] * depth, point[1] + toward[1] * depth))
        camp = options.get("camp") or [half_x * 0.55, half_z * 0.55]
        region = Region(
            [
                path + list(reversed(inward)),
                rectangle_polygon(cx, cz, float(camp[0]), float(camp[1])),
            ]
        )
        towers = []
        count = max(1, int(options.get("towers", 4)))
        total = sum(
            math.hypot(b[0] - a[0], b[1] - a[1])
            for a, b in zip(path, path[1:], strict=False)
        )
        for index in range(count):
            along = total * (index + 0.5) / count
            for a, b in zip(path, path[1:], strict=False):
                leg = math.hypot(b[0] - a[0], b[1] - a[1])
                if along > leg and leg > 0.0:
                    along -= leg
                    continue
                fraction = along / leg if leg > 0.0 else 0.0
                point = (
                    a[0] + (b[0] - a[0]) * fraction,
                    a[1] + (b[1] - a[1]) * fraction,
                )
                toward = _towards(point, (cx, cz))
                towers.append((point[0] + toward[0] * 4.0, point[1] + toward[1] * 4.0))
                break
        return Plan(
            [Circuit(path=path)],
            region,
            extent_x,
            extent_z,
            towers,
            anchors={"barracks": (cx, cz), "market": (cx, cz + 10.0)},
        )

    outline = rectangle_polygon(cx, cz, half_x, half_z)
    region = Region([outline])
    return Plan([Circuit(region=region)], region, extent_x, extent_z, [])


def _towards(
    point: tuple[float, float], target: tuple[float, float]
) -> tuple[float, float]:
    delta_x = target[0] - point[0]
    delta_z = target[1] - point[1]
    length = math.hypot(delta_x, delta_z)
    if length <= 1e-9:
        return (0.0, 0.0)
    return (delta_x / length, delta_z / length)


def gateway_targets(
    settlement: Settlement, half_x: float, half_z: float
) -> list[tuple[float, float]]:
    """Where the ring's gates want to be, in world coordinates.

    ``road_gateways`` answers in sides and offsets because a rectangle has
    sides. A circuit that is not a rectangle has no sides, so the same answer is
    read as a point on the bounding box and the gate goes to the straight run of
    wall nearest it.
    """
    gateways = settlement.gateways or {}
    targets: list[tuple[float, float]] = []
    for side, along in gateways.items():
        if side == "north":
            targets.append((along, settlement.z - half_z))
        elif side == "south":
            targets.append((along, settlement.z + half_z))
        elif side == "west":
            targets.append((settlement.x - half_x, along))
        else:
            targets.append((settlement.x + half_x, along))
    return targets


def approach_targets(
    plan: Plan, settlement: Settlement, wanted: int = 2
) -> list[tuple[float, float]]:
    """Where roads actually cross this circuit, widest first, at most two.

    The rectangular ring reads its gates off its own sides. A circle or a star
    has no sides, and a gate aimed at where a road crosses the bounding box can
    land a wall's length from where that road meets the curve. So the crossing
    is taken against the enclosed region itself: the last waypoint outside and
    the first inside bracket the wall, and the gate goes between them.
    """
    if plan.circuits[0].region is None:
        return []
    region = plan.circuits[0].region
    crossings: list[tuple[float, tuple[float, float]]] = []
    for width, points in settlement.approaches:
        for a, b in zip(points, points[1:], strict=False):
            if region.contains(*a) == region.contains(*b):
                continue
            outside, inside = (a, b) if region.contains(*b) else (b, a)
            for _ in range(12):
                mid = ((outside[0] + inside[0]) * 0.5, (outside[1] + inside[1]) * 0.5)
                if region.contains(*mid):
                    inside = mid
                else:
                    outside = mid
            crossings.append((width, inside))
    crossings.sort(key=lambda item: -item[0])
    chosen: list[tuple[float, float]] = []
    for _width, point in crossings:
        if any(math.hypot(point[0] - c[0], point[1] - c[1]) < 24.0 for c in chosen):
            continue
        chosen.append(point)
        if len(chosen) == wanted:
            break
    return chosen


def emit_plan_walls(
    plan: Plan,
    settlement: Settlement,
    seal: "TerrainMask | None",
) -> tuple[list[WallRun], list[Building]]:
    """Rasterise every circuit, cut its gates, and group the rest into runs."""
    walls: list[WallRun] = []
    gates: list[Building] = []
    walkable = None if seal is None else (lambda x, z: seal.walkable(x, z))
    authored_gates = settlement.plan_options.get("gates")
    if authored_gates:
        default_targets = [
            (float(point[0]), float(point[1])) for point in authored_gates
        ]
    else:
        wanted = int(settlement.plan_options.get("gate_count", 2))
        default_targets = approach_targets(plan, settlement, wanted)
        for target in gateway_targets(settlement, plan.half_x, plan.half_z):
            if len(default_targets) >= wanted:
                break
            if all(
                math.hypot(target[0] - c[0], target[1] - c[1]) >= 24.0
                for c in default_targets
            ):
                default_targets.append(target)

    for index, circuit in enumerate(plan.circuits):
        if circuit.path is not None:
            cells = path_cells(circuit.path, walkable)
        elif circuit.region is not None:
            cells = region_cells(circuit.region, walkable)
        else:
            continue
        plan.cells |= cells
        runs = partition_runs(cells)
        targets = circuit.gate_targets or (default_targets if index == 0 else [])
        for target in targets:
            runs, gate_cell, horizontal = cut_gate(runs, target, GATE_CELLS)
            if gate_cell is None:
                continue
            gates.append(
                Building(
                    "wall_gate",
                    float(gate_cell[0]),
                    float(gate_cell[1]),
                    settlement.player_id,
                    settlement.nation,
                    rotation=0.0 if horizontal else 90.0,
                )
            )
            plan.cells.discard(gate_cell)
        for run in runs:
            ordered = sorted(run, key=lambda cell: (cell[1], cell[0]))
            walls.append(
                WallRun(
                    (float(ordered[0][0]), float(ordered[0][1])),
                    (float(ordered[-1][0]), float(ordered[-1][1])),
                    settlement.player_id,
                    settlement.nation,
                )
            )
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
    extent_x, extent_z = plan_extent(settlement, spec)
    half_x = extent_x + WALL_SEGMENT_SPACING
    half_z = extent_z + WALL_SEGMENT_SPACING
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


def terrace_features(settlements: Sequence["Settlement"]) -> list[dict]:
    """Hills raised on a settlement's own crown: their ramps stay inside its ring."""
    return [terrace for settlement in settlements for terrace in settlement.terraces]


def move_entrances_out_of_rings(
    terrain: Sequence[dict],
    boxes: Sequence[tuple[float, float, float, float]],
    terraces: Sequence[dict] = (),
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
        if any(feature is terrace for terrace in terraces):
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
    nation = settlement.nation
    owner = settlement.player_id

    plan: Plan | None = None
    if settlement.plan != "rect":
        plan = build_plan(settlement, spec)
        half_x, half_z = plan.half_x, plan.half_z
    else:
        half_x, half_z = plan_extent(settlement, spec)

    buildings: list[Building] = []
    walls: list[WallRun] = []

    if plan is not None and settlement.palisade:
        plan_walls, plan_gates = emit_plan_walls(plan, settlement, seal)
        walls.extend(plan_walls)
        buildings.extend(plan_gates)

    def ground_is_clear(x: float, z: float, building_type: str) -> bool:
        size = BUILDING_SIZES.get(building_type, (3.0, 3.0))
        if crown is not None:

            reach_x = (abs(x - cx) + size[0] * 0.5) / max(crown[0], 0.001)
            reach_z = (abs(z - cz) + size[1] * 0.5) / max(crown[1], 0.001)
            if math.hypot(reach_x, reach_z) > 1.0:
                return False
        reach = max(size) * 0.5 + ROAD_VERGE
        for road_width, points in settlement.approaches:
            if len(points) < 2:
                continue
            if polyline_distance(points, x, z) < road_width * 0.5 + reach:
                return False
        for terrace in settlement.terraces:
            terrace_x, terrace_z = hill_extents(terrace)
            offset_x = (x - float(terrace["x"])) / max(terrace_x, 0.001)
            offset_z = (z - float(terrace["z"])) / max(terrace_z, 0.001)
            if math.hypot(offset_x, offset_z) > 1.05:
                continue
            crown_x, crown_z = usable_crown_extent(
                terrace_x * 2.0,
                terrace_z * 2.0,
                float(terrace.get("height", 2.0)),
                0.8,
                float(terrace.get("crown", 0.0)),
            )
            reach_x = (abs(x - float(terrace["x"])) + size[0] * 0.5) / max(
                crown_x, 0.001
            )
            reach_z = (abs(z - float(terrace["z"])) + size[1] * 0.5) / max(
                crown_z, 0.001
            )
            if math.hypot(reach_x, reach_z) > 1.0:
                return False
        if plan is None:
            if (
                abs(x - cx) + size[0] * 0.5 > half_x
                or abs(z - cz) + size[1] * 0.5 > half_z
            ):
                return False
        else:
            if not plan.region.contains(x, z):
                return False
            if any(polygon_contains(polygon, x, z) for polygon in plan.keep_clear):
                return False
            wall_gap = 0.5 if building_type == "defense_tower" else BUILDING_CLEARANCE
            if plan.wall_distance(x, z) < max(size) * 0.5 + wall_gap:
                return False
        if mask is None:
            return True
        return mask.clear_for(x, z, size)

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
        if plan is None:
            walls, buildings = build_ring(
                cx, cz, half_x, half_z, owner, nation, gateways, seal
            )
        for wall in walls:
            wall.settlement = settlement.id
        for building in buildings:
            building.settlement = settlement.id
        settlement.walls, settlement.buildings = walls, buildings
        return

    anchors = dict(plan.anchors) if plan is not None else {}
    for name in ("barracks", "market", "temple"):
        authored = settlement.plan_options.get(name)
        if authored:
            anchors[name] = (float(authored[0]), float(authored[1]))
    barracks_anchor = anchors.get("barracks", (cx, cz - 6.0))
    barracks_site = nearest_clear(barracks_anchor[0], barracks_anchor[1], "barracks")
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
        market_anchor = anchors.get("market", (cx, cz + 6.0))
        market_site = nearest_clear(market_anchor[0], market_anchor[1], "marketplace")
        if market_site is not None:
            buildings.append(
                Building("marketplace", market_site[0], market_site[1], owner, nation)
            )

    wants_temple = spec.temple if settlement.temple is None else settlement.temple
    if wants_temple:

        temple_anchor = anchors.get("temple", (cx - 9.0, cz))
        temple_site = nearest_clear(temple_anchor[0], temple_anchor[1], "temple")
        if temple_site is not None:
            buildings.append(
                Building("temple", temple_site[0], temple_site[1], owner, nation)
            )

    if spec.inner_half_x is not None and plan is None and settlement.citadel:
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
        if spec.inner_half_x and plan is None and settlement.citadel
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

            if plan is None:
                if (
                    settlement.facing in ("north", "south")
                    and abs(offset_x) < gate_corridor
                ):
                    continue
                if (
                    settlement.facing in ("east", "west")
                    and abs(offset_z) < gate_corridor
                ):
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
    if settlement.homes is not None and placed_homes < settlement.homes:
        print(
            f"  short: {settlement.id} has room for {placed_homes} of the "
            f"{settlement.homes} homes it asked for"
        )

    if settlement.palisade and plan is None:
        outer_walls, outer_gates = build_ring(
            cx, cz, half_x, half_z, owner, nation, gateways, seal
        )
        walls.extend(outer_walls)
        buildings.extend(outer_gates)

    corner_inset = 3.0
    corners = (
        plan.towers
        if plan is not None
        else [
            (cx - half_x + corner_inset, cz - half_z + corner_inset),
            (cx + half_x - corner_inset, cz - half_z + corner_inset),
            (cx - half_x + corner_inset, cz + half_z - corner_inset),
            (cx + half_x - corner_inset, cz + half_z - corner_inset),
        ]
    )
    outer_tower_count = (
        (spec.outer_towers if plan is None else len(corners))
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

    for wall in walls:
        wall.settlement = settlement.id
    for building in buildings:
        building.settlement = settlement.id
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
    extent_x, extent_z = plan_extent(settlement, spec)
    reach_x = int(extent_x) + 6
    reach_z = int(extent_z) + 6

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
            if settlement.plan == "curtain":
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
            if settlement.palisade and settlement.plan != "curtain"
        ]
        terraces = terrace_features(settlements)
        for feature in definition.get("terrain") or []:
            if str(feature.get("type", "")).lower() != "hill":
                continue
            if any(feature is terrace for terrace in terraces):
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

        if settlement.on_hill is not True and (
            settlement.tier == "town" or settlement.on_hill is False
        ):
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
        stacked = hills_containing(terrain, settlement.x, settlement.z)
        hill = (
            stacked[0] if stacked else find_hill_at(terrain, settlement.x, settlement.z)
        )
        if hill is None:
            raise SettlementError(
                f"{settlement.id} is marked on_hill but no hill is near it"
            )
        normalise_hill_dimensions(hill)
        settlement.terraces = [feature for feature in stacked if feature is not hill]
        for terrace in settlement.terraces:
            ensure_terrace_ramp(terrace, settlement)

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
        round_footprint = settlement.plan in ("circle", "terraced")
        for _ in range(24):
            layout_settlement(settlement, spec)
            half_x, half_z = settlement_footprint(settlement)
            fit_hill_to_footprint(
                hill,
                half_x,
                half_z,
                crown_safety,
                growth_width,
                growth_depth,
                round_footprint,
            )
            if crown_fits(
                float(hill["width"]),
                float(hill["depth"]),
                authored_height,
                half_x,
                half_z,
                crown_safety,
                round_footprint,
                float(hill.get("crown", 0.0)),
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
        if settlement.scale < 0.999:
            print(
                f"  fitted: {settlement.id} shrunk to {settlement.scale:.2f} of its "
                f"authored size to stay on the crown of its {hill['width']:.0f}x"
                f"{hill['depth']:.0f} hill"
            )

    base_mask = TerrainMask(definition, terrain_clearance)
    base_seal = TerrainMask(
        definition, 0.0, influence=1.0, water_clearance=WALL_WATER_CLEARANCE
    )
    for settlement in settlements:
        spec = TIER_SPECS[settlement.tier]
        mask = base_mask
        settlement.seal = base_seal
        crown: tuple[float, float] | None = None
        if settlement.on_hill:
            excluded = [settlement.hill, *settlement.terraces]
            mask = TerrainMask(definition, terrain_clearance, exclude_hills=excluded)

            settlement.seal = TerrainMask(
                definition,
                0.0,
                exclude_hills=excluded,
                influence=1.0,
                water_clearance=WALL_WATER_CLEARANCE,
            )
            crown = usable_crown_extent(
                float(settlement.hill["width"]),
                float(settlement.hill["depth"]),
                float(settlement.hill.get("height", 2.0)),
                crown_safety,
                float(settlement.hill.get("crown", 0.0)),
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
        gate_half_x, gate_half_z = plan_extent(settlement, spec)
        settlement.gateways = road_gateways(
            definition, settlement, gate_half_x, gate_half_z
        )
        settlement.approaches = [
            (
                float(road.get("width", 3.0)),
                [
                    (float(point[0]), float(point[1]))
                    for point in (
                        road.get("waypoints") or [road.get("start"), road.get("end")]
                    )
                    if point
                ],
            )
            for road in definition.get("roads") or []
        ]
        layout_settlement(settlement, spec, mask, crown, settlement.seal)

    ring_boxes = [
        ring_box(settlement, TIER_SPECS[settlement.tier])
        for settlement in settlements
        if settlement.palisade and settlement.plan != "curtain"
    ]
    moved_ramps = move_entrances_out_of_rings(
        terrain, ring_boxes, terrace_features(settlements)
    )

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
        owned = {settlement.id for settlement in settlements}
        definition["structures"] = build_structures(
            settlements,
            [
                entry
                for entry in definition.get("structures") or []
                if entry.get("landmark") is not None
                or (entry.get("authored") and entry.get("settlement") not in owned)
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
