#!/usr/bin/env python3
"""Find and repair authored map objects that sit inside each other or the terrain.

Every object on a map carries the *rectangle* it plants on the ground -- the
same body the engine blocks navigation with, mirrored from
``world_prop_ground_half_extents`` in game/map/map_definition.h and the building
bodies in game/systems/building_collision_registry.cpp.  Rectangles matter: a
ruin is a 5.5 x 4.1 m plinth and a fallen dead tree is 4.2 m of timber one metre
wide, and a disc big enough to hold either of them reserves open ground off the
short flank while leaving the long corners free for something else to stand in.
Keep the tables here in step with the header; the C++ guard
tests/render/prop_model_footprint_test.cpp measures the models and fails if the
header drifts from them.

Five kinds of defect are reported, and all five must be at zero:

``overlap``   two solid bodies intersect.
``canopy``    a tree's crown has swallowed something built.  A pine blocks only
              its trunk so that a wood stays walkable, but it *draws* nine
              metres of crown, and a crown centred on a tent is the same
              immersion break as a tent inside a tent even though nothing
              blocks anything.
``road``      a body stands in a road or bridge corridor.  Roads are laid by
              scripts/generate-map-roads.py, which routes around water and slope
              but has never known that props exist, so it paves over statues.
``water``     a body stands in a river or a lake.
``slope``     the ground breaks under the body.  A prop wholly on the flat or
              wholly on the shoulder settles; one lying across a break cannot,
              and the high corner of its model floats.
``ramp``      a body stands in a hill entrance.  The engine cuts a ramp corridor
              from the crown out past the foot of every hill, and it is both the
              only way up and re-graded ground the map JSON never mentions.

``slope`` and ``ramp`` are measured against the heightfield the engine actually
builds, read back from ``tools/terrain_probe``.  They cannot be derived from the
map JSON: a hill is authored as a centre and a radius, but `Landform::sample_hill`
warps that boundary with fbm, roughens it by up to +-34% of the radius, unions in
an off-centre lobe, widens and hash-rotates the footprint at campaign scale, and
then cuts the ramps.  The audit used to model the authored ellipse instead and
reported zero slope defects on maps carrying 300 of them.

Repairs push the *lower priority* object out along the separation axis: a tent
that overlaps a wall moves, the wall does not.  A push is only accepted when it
lands the object inside the map, clear of water, roads and bridges, and no
deeper into anything else than it already was.

Structures are not all equally fixed.  A wall ring is geometry -- its segments
are *meant* to abut and must never move -- and a settlement is laid out around
its barracks, temple and marketplace, so those are anchors too.  The homes and
towers dropped into the space between them are fill, and a tower standing inside
the barracks hall is a defect that moving the tower fixes.

An anchor building standing in a road is the one defect fixed from the other
end.  Roads are generated to *reach* these buildings and are authored with the
building's centre as an endpoint, so the last few metres of the route run
through the hall.  The building is where the designer put it; the road is what
overshoots, so the road is trimmed back to the doorway.
"""

from __future__ import annotations

import argparse
import itertools
import json
import math
import sys
from collections.abc import Callable
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from map_hill_shapes import hill_half_extents, hill_shape_strokes
from map_surface_field import ProbeUnavailable, SurfaceField, find_probe, load_surface

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

WORLD_PROP_MODEL_HALF_EXTENTS = {
    "ruins": (0.94, 0.70),
    "abandoned_home": (1.16, 0.82),
    "supply_cart": (0.90, 1.42),
    "weapon_rack": (0.88, 0.54),
    "magic_shrine": (0.86, 0.86),
    "statue": (0.55, 0.55),
    "tent": (0.69, 0.90),
    "firecamp": (0.90, 0.90),
    "boulder": (0.60, 0.55),
    "iron_ore": (0.82, 0.56),
    "dead_tree": (1.34, 0.35),
    "pine_tree": (1.0, 1.0),
    "olive_tree": (1.0, 1.0),
    "cypress_tree": (1.0, 1.0),
    "palm_tree": (1.0, 1.0),
}
WORLD_PROP_MODEL_HALF_EXTENTS_DEFAULT = (0.55, 0.55)

CANOPY_TYPES = {"pine_tree", "olive_tree", "cypress_tree", "palm_tree"}
"""Trees that block only their trunk.

Grass and stones belong *under* a canopy, and blocking the full crown of a pine
would shave a bald ring 9 m across around every trunk.  ``dead_tree`` is
deliberately not here: it is a log lying on the ground, so all of it is
something you walk around."""

STEM_FRACTION = 0.22
MIN_GROUND_HALF_EXTENT = 0.5

CANOPY_OVERHANG_FRACTION = 0.35
"""How far a crown may lean over a built body, as a share of its own reach.

A tree leaning over the corner of a ruin is scenery and a tree standing in the
middle of a tent is a defect, and the line between them scales with the tree:
a cypress may clip what a pine would swallow whole."""

CANOPY_BLIND_TO = frozenset({"plant"})
"""What a crown is allowed to stand over outright.

Grass belongs under a canopy, and so does another tree -- a wood is crowns
touching by definition -- so canopies are only ever measured against the
built things a crown should not have grown through."""


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

Troops muster on a road as often as beside one and are spread by the formation
pass on the first tick, so a spawn is checked against solid bodies and water but
never against roads or hill rims.

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

A corner tower or a gatehouse is ring geometry even though its type says fill,
and sliding it off the ring opens a hole no wall segment covers.  Measured over
every map in ``assets/maps``, the nearest a free-standing home or tower sits to
a wall body is 0.81 m and all but one clear 2 m, so this locks a gatehouse or a
corner tower without freezing ordinary settlement fill."""

DEFAULT_ROAD_WIDTH = 3.0
DEFAULT_BRIDGE_WIDTH = 8.0
DEFAULT_RIVER_WIDTH = 4.0

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

ROAD_MARGIN = 0.2
"""How far clear of the kerb a body has to stand.

Small on purpose: props are meant to line a road, and demanding a metre of
verge would march every camp back off its own street."""

SLOPE_STRADDLE_MARGIN = 0.35
"""How far a body may hang over the rim of a hill before the ground breaks.

Prop renderers settle a model on the *lowest* ground its footprint spans, so a
body wholly on the flat or wholly on the shoulder sits flush either way.  What
cannot be settled is a body lying across the rim, where the ground under one
half is metres below the other."""

RIM_TOLERANCE_FRACTION = 0.5
"""The share of itself a body may hang over a rim, over and above the margin.

Camps are pitched against banks on purpose and a ruin is often built into a
shoulder, so clipping an edge is not a defect; a body whose *centre* is nearer
the rim than half its own reach is genuinely lying across the break.

Only used when there is no terrain probe to measure the real ground with."""

GROUND_RELIEF_TOLERANCE = 0.35
"""How far the ground may break under one body, in metres.

A model is settled on the lowest ground its footprint spans, so this is also how
far its high corner floats.  Measured over every shipped map, relief is a
continuum up to about a quarter of a metre -- ordinary gentle ground, 599 bodies
above 0.10 m thinning to 357 above 0.25 -- and then flattens into a separate
population that barely moves between 0.30 m and 0.50 m.  Those are the bodies
lying across a real break, and 0.35 sits in the gap between the two."""

RAMP_COVERAGE_TOLERANCE = 0.15
"""The share of a footprint that may stand in a hill entrance.

Not zero: the corridor is wide -- `k_campaign_hill_entry_ramp_cells` alone is
7.25 cells of half width before the mouth flare -- and a tent pitched beside a
ramp with one corner over the edge of it is scenery.  A body with a sixth of
itself on the ramp is in the gateway."""


def axis_aligned_body(
    body: tuple[float, float], facing_degrees: float
) -> tuple[float, float]:
    """The axis-aligned box a rotated building body fills, as the engine does it.

    Buildings carry *degrees* and are registered axis-aligned by
    ``BuildingCollisionRegistry``; props carry radians and are drawn rotated.
    The two conventions are not interchangeable -- keep them apart."""
    radians = math.radians(facing_degrees)
    cosine = abs(math.cos(radians))
    sine = abs(math.sin(radians))
    return (
        body[0] * cosine + body[1] * sine,
        body[0] * sine + body[1] * cosine,
    )


def prop_ground_half_extents(prop_type: str, scale: float) -> tuple[float, float]:
    render_scale = WORLD_PROP_RENDER_SCALE.get(prop_type, 1.0) * scale
    if prop_type in CANOPY_TYPES:
        model = (STEM_FRACTION, STEM_FRACTION)
    else:
        model = WORLD_PROP_MODEL_HALF_EXTENTS.get(
            prop_type, WORLD_PROP_MODEL_HALF_EXTENTS_DEFAULT
        )
    return (
        max(MIN_GROUND_HALF_EXTENT, model[0] * render_scale),
        max(MIN_GROUND_HALF_EXTENT, model[1] * render_scale),
    )


def prop_canopy_half_extents(prop_type: str, scale: float) -> tuple[float, float]:
    """The crown a tree draws, as opposed to the trunk it blocks.

    ``prop_ground_half_extents`` shrinks a canopy to its stem so that scatter
    and soldiers may pass under it.  Nothing may be *built* under it, so the
    drawn crown is measured separately."""
    render_scale = WORLD_PROP_RENDER_SCALE.get(prop_type, 1.0) * scale
    model = WORLD_PROP_MODEL_HALF_EXTENTS.get(
        prop_type, WORLD_PROP_MODEL_HALF_EXTENTS_DEFAULT
    )
    return (model[0] * render_scale, model[1] * render_scale)


@dataclass
class Placeable:
    """One solid body on the map, as an oriented rectangle or a disc."""

    key: str
    kind: str
    name: str
    x: float
    z: float
    half_x: float
    half_z: float
    priority: int
    payload: dict = field(repr=False, default_factory=dict)

    rotation: float = 0.0
    """Radians, matching the prop shaders.  Buildings are pre-inflated instead."""

    is_disc: bool = False
    body_type: str = ""
    x_field: str = "x"
    z_field: str = "z"
    moved: float = 0.0
    travel_budget: float = 0.0
    avoids_roads: bool = True
    avoids_slope_rims: bool = True

    is_ring: bool = False
    """Part of a defensive ring: a wall run, or a tower locked onto one."""

    canopy_half: tuple[float, float] | None = None
    """The crown this body draws, when it draws one bigger than it blocks."""

    def canopy_body(self) -> "Placeable | None":
        """This tree as the crown it draws, ready to be measured like any body."""
        if self.canopy_half is None:
            return None
        return Placeable(
            key=self.key,
            kind=self.kind,
            name=self.name,
            x=self.x,
            z=self.z,
            half_x=self.canopy_half[0],
            half_z=self.canopy_half[1],
            rotation=self.rotation,
            priority=self.priority,
            body_type=self.body_type,
        )

    @property
    def radius(self) -> float:
        """The disc that contains the whole body, corners included."""
        return math.hypot(self.half_x, self.half_z)

    def ground_footprint(self, surface: SurfaceField) -> list[tuple[float, float]]:
        """The patch of ground this body has to stand on, as sample points.

        A canopy tree is measured across its trunk.  Its crown hangs over
        whatever it likes -- that is what the ``canopy`` check is for -- but only
        the stem is planted, and judging a pine by the nine metres of ground
        under its crown would call every tree on a hillside a defect."""
        half_x, half_z = self.half_x, self.half_z
        if self.canopy_half is not None:
            half_x = half_z = max(half_x, half_z) * STEM_FRACTION
        return surface.footprint_samples(
            self.x, self.z, half_x, half_z, self.rotation, self.is_disc
        )

    def support_radius(self, unit_x: float, unit_z: float) -> float:
        """How far the body reaches from its centre along one bearing.

        A dead tree reaches 2 m along its length and 0.5 m across it; judging
        either against the disc that bounds both would call half the map a
        defect."""
        if self.is_disc:
            return self.half_x
        own_axes = self.axes()
        return self.half_x * abs(
            unit_x * own_axes[0][0] + unit_z * own_axes[0][1]
        ) + self.half_z * abs(unit_x * own_axes[1][0] + unit_z * own_axes[1][1])

    def axes(self) -> tuple[tuple[float, float], tuple[float, float]]:
        cosine = math.cos(self.rotation)
        sine = math.sin(self.rotation)
        return (cosine, sine), (-sine, cosine)

    def contains_depth(self, x: float, z: float, radius: float) -> float:
        """How deep a disc at (x, z) sits inside this body; <= 0 when clear."""
        cosine = math.cos(self.rotation)
        sine = math.sin(self.rotation)
        dx = x - self.x
        dz = z - self.z
        local_x = cosine * dx + sine * dz
        local_z = -sine * dx + cosine * dz
        gap_x = abs(local_x) - self.half_x
        gap_z = abs(local_z) - self.half_z
        if gap_x <= 0.0 and gap_z <= 0.0:
            return radius - max(gap_x, gap_z)
        return radius - math.hypot(max(gap_x, 0.0), max(gap_z, 0.0))

    def separation_from(
        self, other: "Placeable", clearance: float
    ) -> tuple[float, float, float]:
        """How deep ``self`` sits in ``other``, and the shortest way out.

        Depth and direction are derived together so they can never disagree.
        Two rectangles are separated by the axis theorem, which reports the real
        minimum translation even when one body is entirely inside the other --
        the case that used to degenerate, when an archer standing in the middle
        of a barracks hall looked 0.65 m deep though it was 2.75 m from the
        nearest wall, and got pushed out on a bearing taken from its own name.
        """
        if self.is_disc and other.is_disc:
            dx = self.x - other.x
            dz = self.z - other.z
            distance = math.hypot(dx, dz)
            depth = (self.half_x + other.half_x + clearance) - distance
            if distance < 1e-4:
                return depth, 0.0, 0.0
            return depth, dx / distance, dz / distance

        if self.is_disc or other.is_disc:
            box, disc = (other, self) if self.is_disc else (self, other)
            depth = box.contains_depth(disc.x, disc.z, disc.half_x + clearance)
            cosine = math.cos(box.rotation)
            sine = math.sin(box.rotation)
            dx = disc.x - box.x
            dz = disc.z - box.z
            local_x = cosine * dx + sine * dz
            local_z = -sine * dx + cosine * dz
            inside_x = box.half_x - abs(local_x)
            inside_z = box.half_z - abs(local_z)
            if inside_x > 0.0 and inside_z > 0.0:
                if inside_x <= inside_z:
                    exit_local = (1.0 if local_x >= 0.0 else -1.0, 0.0)
                else:
                    exit_local = (0.0, 1.0 if local_z >= 0.0 else -1.0)
            else:
                offset_x = local_x - min(max(local_x, -box.half_x), box.half_x)
                offset_z = local_z - min(max(local_z, -box.half_z), box.half_z)
                length = math.hypot(offset_x, offset_z)
                exit_local = (
                    (offset_x / length, offset_z / length)
                    if length > 1e-4
                    else (0.0, 0.0)
                )
            exit_x = cosine * exit_local[0] - sine * exit_local[1]
            exit_z = sine * exit_local[0] + cosine * exit_local[1]
            if self.is_disc:
                return depth, exit_x, exit_z
            return depth, -exit_x, -exit_z

        return self._rectangle_separation(other, clearance)

    def _rectangle_separation(
        self, other: "Placeable", clearance: float
    ) -> tuple[float, float, float]:
        offset_x = self.x - other.x
        offset_z = self.z - other.z
        own_axes = self.axes()
        other_axes = other.axes()

        best_depth = math.inf
        best_axis = (0.0, 0.0)
        for axis in (*own_axes, *other_axes):
            reach_self = self.half_x * abs(
                axis[0] * own_axes[0][0] + axis[1] * own_axes[0][1]
            ) + self.half_z * abs(axis[0] * own_axes[1][0] + axis[1] * own_axes[1][1])
            reach_other = other.half_x * abs(
                axis[0] * other_axes[0][0] + axis[1] * other_axes[0][1]
            ) + other.half_z * abs(
                axis[0] * other_axes[1][0] + axis[1] * other_axes[1][1]
            )
            distance = offset_x * axis[0] + offset_z * axis[1]
            depth = reach_self + reach_other + clearance - abs(distance)
            if depth < best_depth:
                best_depth = depth
                sign = 1.0 if distance >= 0.0 else -1.0
                best_axis = (axis[0] * sign, axis[1] * sign)
        return best_depth, best_axis[0], best_axis[1]

    def overlap_with(self, other: "Placeable", clearance: float) -> float:
        """How deep the two solid bodies intersect; <= 0 when they are clear."""
        return self.separation_from(other, clearance)[0]

    def exempt_from(self, other: "Placeable") -> bool:
        """Pairs that are meant to touch.

        Wall panels abut by definition, and a gatehouse or corner tower is built
        *into* the run rather than beside it -- 214 of those towers, more than
        half of every overlapping pair on the campaign maps, are the ring doing
        its job.  Two towers overlapping each other is still a defect, so the
        exemption is only ever between a wall and something locked onto it.
        """
        if self.kind == "spawn" and other.kind == "spawn":
            return True
        if self.body_type in WALL_TYPES and other.body_type in WALL_TYPES:
            return True
        wall, ring = (self, other) if self.body_type in WALL_TYPES else (other, self)
        return wall.body_type in WALL_TYPES and ring.is_ring


def make_prop(key: str, kind: str, prop: dict, prop_type: str, default_priority: int):
    """One world prop as the rectangle its model plants on the ground.

    ``rotation`` is copied across raw.  Props store it unconverted and the prop
    shaders read it as radians, so the ``180`` authored on a ruin is 180
    radians, not half a turn; mirror that oddity or the body will not sit where
    the model is drawn.
    """
    scale = float(prop.get("scale", 1.0) or 1.0)
    half_x, half_z = prop_ground_half_extents(prop_type, scale)
    canopy = (
        prop_canopy_half_extents(prop_type, scale)
        if prop_type in CANOPY_TYPES
        else None
    )
    return Placeable(
        key=key,
        kind=kind,
        name=str(prop.get("id") or prop_type),
        x=float(prop.get("x", 0.0)),
        z=float(prop.get("z", 0.0)),
        half_x=half_x,
        half_z=half_z,
        rotation=float(prop.get("rotation", 0.0) or 0.0),
        priority=PROP_PRIORITY.get(prop_type, default_priority),
        payload=prop,
        body_type=prop_type,
        canopy_half=canopy,
    )


def collect(map_data: dict, path_name: str) -> list[Placeable]:
    items: list[Placeable] = []

    for index, prop in enumerate(map_data.get("world_props") or []):
        prop_type = str(prop.get("type", ""))
        items.append(
            make_prop(
                f"world_props[{index}]",
                "world_prop",
                prop,
                prop_type,
                PROP_PRIORITY_DEFAULT,
            )
        )

    for index, camp in enumerate(map_data.get("firecamps") or []):
        items.append(
            make_prop(
                f"firecamps[{index}]", "firecamp", camp, "firecamp", PRIORITY_FIRECAMP
            )
        )

    for index, structure in enumerate(map_data.get("structures") or []):
        items.append(make_structure(f"structures[{index}]", structure, path_name))

    for index, spawn in enumerate(map_data.get("spawns") or []):
        items.append(
            Placeable(
                key=f"spawns[{index}]",
                kind="spawn",
                name=str(spawn.get("id") or spawn.get("type") or "spawn"),
                x=float(spawn.get("x", 0.0)),
                z=float(spawn.get("z", 0.0)),
                half_x=SPAWN_BODY_RADIUS,
                half_z=SPAWN_BODY_RADIUS,
                is_disc=True,
                priority=PRIORITY_SPAWN,
                payload=spawn,
                body_type=str(spawn.get("type", "")),
                avoids_roads=False,
                avoids_slope_rims=False,
            )
        )

    lock_ring_structures(items)
    return items


class MapContentError(ValueError):
    """A map that cannot be checked, rather than a map with a defect in it."""


def make_structure(key: str, structure: dict, path_name: str) -> Placeable:
    """One building, or one wall run held as the rectangle it actually fills.

    A wall segment is *line* geometry -- ``start`` and ``end``, expanded by
    ``read_structures`` in game/map/map_loader.cpp into a run of panels -- and
    reading it as a point body silently parked 212 wall runs at the map origin,
    where they guarded nothing and reported nothing.  There is no safe default
    for a missing position, so a structure with neither shape is an error.

    A ring is laid across whatever ground it has to hold and a gateway is a hole
    in it for a road to run through, so walls are checked against other bodies
    but never against roads or hill rims.
    """
    body_type = str(structure.get("type", ""))
    priority = STRUCTURE_PRIORITY.get(body_type, STRUCTURE_PRIORITY_DEFAULT)
    name = str(structure.get("id") or body_type or "structure")

    start = structure.get("start")
    end = structure.get("end")
    if start and end:
        start_x, start_z = float(start[0]), float(start[1])
        end_x, end_z = float(end[0]), float(end[1])
        run_x = end_x - start_x
        run_z = end_z - start_z
        length = math.hypot(run_x, run_z)
        half_width = float(structure.get("width", 2.0)) * 0.5
        panel = BUILDING_BODIES.get(body_type, BUILDING_BODY_DEFAULT)
        return Placeable(
            key=key,
            kind="structure",
            name=name,
            x=0.5 * (start_x + end_x),
            z=0.5 * (start_z + end_z),
            half_x=max(0.5 * length, 0.5 * panel[0]),
            half_z=max(half_width, 0.5 * panel[1]),
            rotation=math.atan2(run_z, run_x) if length > 1e-6 else 0.0,
            priority=PRIORITY_IMMOVABLE,
            payload=structure,
            body_type=body_type,
            avoids_roads=False,
            avoids_slope_rims=False,
            is_ring=True,
        )

    if "x" not in structure or "z" not in structure:
        raise MapContentError(
            f"{path_name}: {name} ({body_type or 'no type'}) has neither x/z nor "
            "start/end, so it has no position to check"
        )

    width, depth = axis_aligned_body(
        BUILDING_BODIES.get(body_type, BUILDING_BODY_DEFAULT),
        float(structure.get("facing", structure.get("rotation", 0.0)) or 0.0),
    )
    return Placeable(
        key=key,
        kind="structure",
        name=name,
        x=float(structure["x"]),
        z=float(structure["z"]),
        half_x=width * 0.5,
        half_z=depth * 0.5,
        priority=priority,
        payload=structure,
        body_type=body_type,
        avoids_roads=body_type not in WALL_TYPES,
        avoids_slope_rims=body_type not in WALL_TYPES,
        is_ring=body_type in WALL_TYPES,
    )


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
            item.is_ring = True

            item.avoids_slope_rims = False
            item.avoids_roads = False


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

    def length(self) -> float:
        return math.hypot(self.x2 - self.x1, self.z2 - self.z1)

    def depth_into(self, item: Placeable, margin: float) -> float:
        """How deep this corridor cuts into a body; <= 0 when they are clear.

        The corridor is walked in half-metre steps rather than solved: a road is
        a chain of capsules and a body is a rotated rectangle, and stepping the
        centre line is both exact enough at these tolerances and impossible to
        get subtly wrong."""
        length = self.length()
        if length <= 1e-6:
            return item.contains_depth(self.x1, self.z1, self.half_width + margin)
        steps = max(2, int(length / 0.5) + 1)
        deepest = -math.inf
        for step in range(steps + 1):
            t = step / steps
            depth = item.contains_depth(
                self.x1 + (self.x2 - self.x1) * t,
                self.z1 + (self.z2 - self.z1) * t,
                self.half_width + margin,
            )
            deepest = max(deepest, depth)
        return deepest


def polyline_segments(entries, default_width: float) -> list[Segment]:
    segments: list[Segment] = []
    for entry in entries or []:
        points = entry.get("waypoints") or [entry.get("start"), entry.get("end")]
        points = [point for point in points if point]
        half_width = float(entry.get("width", default_width)) * 0.5
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


def collect_water(map_data: dict) -> list[Segment]:
    """Rivers and bridges a body must not stand in."""
    return polyline_segments(
        map_data.get("rivers"), DEFAULT_RIVER_WIDTH
    ) + polyline_segments(map_data.get("bridges"), DEFAULT_BRIDGE_WIDTH)


def collect_roads(map_data: dict) -> list[Segment]:
    return polyline_segments(map_data.get("roads"), DEFAULT_ROAD_WIDTH)


def collect_lakes(map_data: dict) -> list[tuple[float, float, float]]:
    lakes = []
    for lake in map_data.get("lakes") or []:
        center = lake.get("center") or [lake.get("x"), lake.get("z")]
        if not center or center[0] is None:
            continue
        radius = float(lake.get("radius", lake.get("radius_x", 4.0)) or 4.0)
        lakes.append((float(center[0]), float(center[1]), radius))
    return lakes


def collect_hill_rims(map_data: dict) -> list[Segment]:
    """Every hill body, as the capsules the engine raises ground along.

    ``map_hill_shapes`` is the same module the road, water and settlement
    generators use, so a ridge authored as an arc is read here as the arc it is
    and not as the ellipse that bounds it -- the concave pocket of a boomerang
    is open ground a camp may stand in.  A blob has no spine at all, so its
    ellipse is walked as a closed ring of hairline capsules that stand in for
    its own boundary."""
    rims: list[Segment] = []
    for feature in map_data.get("terrain") or []:
        if str(feature.get("type", "")).lower() not in ("hill", "mountain"):
            continue
        strokes, half_thickness = hill_shape_strokes(feature)
        if strokes:
            for (start_x, start_z), (end_x, end_z) in strokes:
                rims.append(Segment(start_x, start_z, end_x, end_z, half_thickness))
            continue
        half_x, half_z = hill_half_extents(feature)
        if half_x <= 0.0 or half_z <= 0.0:
            continue

        center_x = float(feature.get("x", 0.0))
        center_z = float(feature.get("z", 0.0))
        steps = 24
        ring = [
            (
                center_x + half_x * math.cos(2.0 * math.pi * step / steps),
                center_z + half_z * math.sin(2.0 * math.pi * step / steps),
            )
            for step in range(steps + 1)
        ]
        for first, second in zip(ring, ring[1:], strict=False):
            rims.append(Segment(first[0], first[1], second[0], second[1], 0.0))
    return rims


def rim_bearing(item: Placeable, rim: Segment) -> tuple[float, float, float]:
    """Distance from a body's centre to a hill boundary, and the way off it.

    The raised region is everything within ``half_width`` of the spine, so its
    boundary is the offset curve at exactly that distance -- and inside the
    ridge the way off the rim points inward, not outward.
    """
    run_x = rim.x2 - rim.x1
    run_z = rim.z2 - rim.z1
    length_sq = run_x * run_x + run_z * run_z
    if length_sq <= 1e-9:
        near_x, near_z = rim.x1, rim.z1
    else:
        t = ((item.x - rim.x1) * run_x + (item.z - rim.z1) * run_z) / length_sq
        t = max(0.0, min(1.0, t))
        near_x = rim.x1 + run_x * t
        near_z = rim.z1 + run_z * t
    offset_x = item.x - near_x
    offset_z = item.z - near_z
    spine_distance = math.hypot(offset_x, offset_z)
    if spine_distance < 1e-4:
        unit_x, unit_z = 1.0, 0.0
    else:
        unit_x, unit_z = offset_x / spine_distance, offset_z / spine_distance

    signed = spine_distance - rim.half_width
    if signed < 0.0:
        unit_x, unit_z = -unit_x, -unit_z
    return abs(signed), unit_x, unit_z


def straddles_rim(item: Placeable, rims: list[Segment], margin: float) -> float:
    """How far a body hangs over the edge of a hill; <= 0 when it does not.

    Distance from the *boundary*, not from the hill: a prop deep inside a ridge
    and a prop out on the flat both stand on ground that is locally consistent,
    and only the rim breaks under them.  The body is measured along the bearing
    to that rim rather than by the disc that bounds it, and the tolerance grows
    with the body, so clipping the edge of a shoulder with one corner of a
    plinth is not read as the same defect as a tent pitched across it.
    """
    worst = -math.inf
    for rim in rims:
        distance, unit_x, unit_z = rim_bearing(item, rim)
        reach = item.support_radius(unit_x, unit_z)
        tolerance = max(margin, reach * RIM_TOLERANCE_FRACTION)
        worst = max(worst, reach - tolerance - distance)
    return worst


@dataclass
class Violation:
    kind: str
    item: Placeable
    depth: float
    other: Placeable | None = None
    detail: str = ""

    def describe(self) -> str:
        if self.kind == "overlap" and self.other is not None:
            return (
                f"{self.item.name} ({self.item.kind}) x {self.other.name} "
                f"({self.other.kind}) overlap {self.depth:.2f}"
            )
        if self.kind == "canopy" and self.other is not None:
            return (
                f"{self.item.name} ({self.item.kind}) crown over "
                f"{self.other.name} ({self.other.kind}) by {self.depth:.2f}"
            )
        if self.kind in ("slope", "ramp"):
            return (
                f"{self.item.name} ({self.item.kind}) on "
                f"{'broken ground' if self.kind == 'slope' else 'a hill entrance'}"
                f"{', ' + self.detail if self.detail else ''}"
            )
        return (
            f"{self.item.name} ({self.item.kind}) in {self.kind}"
            f"{' ' + self.detail if self.detail else ''} by {self.depth:.2f}"
        )


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
        min_x = int((item.x - item.radius) // cell)
        max_x = int((item.x + item.radius) // cell)
        min_z = int((item.z - item.radius) // cell)
        max_z = int((item.z + item.radius) // cell)
        for bucket_x in range(min_x, max_x + 1):
            for bucket_z in range(min_z, max_z + 1):
                buckets.setdefault((bucket_x, bucket_z), []).append(item)

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


def canopy_intrusion(
    tree: Placeable, other: Placeable, fraction: float
) -> tuple[float, float, float]:
    """How far a crown has grown *through* a body, and the way back out.

    Depth alone would call every tree beside a wall a defect, so what is
    reported is the depth beyond the lean the crown is allowed: a pine may
    overhang a ruin by a third of its own reach along that bearing and still
    read as a tree standing next to a ruin."""
    crown = tree.canopy_body()
    if crown is None:
        return -math.inf, 0.0, 0.0
    depth, unit_x, unit_z = crown.separation_from(other, 0.0)
    allowance = crown.support_radius(unit_x, unit_z) * fraction
    return depth - allowance, unit_x, unit_z


def find_canopy_intrusions(items: list[Placeable], fraction: float) -> list[Violation]:
    """Every crown that has swallowed something built, worst first."""
    trees = [item for item in items if item.canopy_half is not None]
    if not trees:
        return []
    built = [
        item
        for item in items
        if item.kind in ("world_prop", "firecamp", "structure")
        and item.canopy_half is None
        and item.body_type not in CANOPY_BLIND_TO
    ]

    found: list[Violation] = []
    for tree in trees:
        for other in built:
            if other.key == tree.key or tree.exempt_from(other):
                continue
            depth, _, _ = canopy_intrusion(tree, other, fraction)
            if depth > 1e-3:
                found.append(Violation("canopy", tree, depth, other))
    found.sort(key=lambda entry: entry.depth, reverse=True)
    return found


@dataclass
class Terrain:
    """Everything on a map that is not a body but still occupies ground."""

    width: float = 0.0
    height: float = 0.0
    bounded: bool = False
    roads: list[Segment] = field(default_factory=list)
    water: list[Segment] = field(default_factory=list)
    lakes: list[tuple[float, float, float]] = field(default_factory=list)
    rims: list[Segment] = field(default_factory=list)

    surface: SurfaceField | None = None
    """The heightfield the engine builds, when there is a probe to build it.

    Present or absent, this decides how ``slope`` is measured: against the real
    ground, or against the authored ellipses in ``rims`` as a fallback."""

    relief_tolerance: float = GROUND_RELIEF_TOLERANCE
    ramp_coverage: float = RAMP_COVERAGE_TOLERANCE


def read_terrain(map_data: dict, surface: SurfaceField | None = None) -> Terrain:
    grid = map_data.get("grid") or {}
    width = float(grid.get("width", 0) or 0)
    height = float(grid.get("height", 0) or 0)
    return Terrain(
        width=width,
        height=height,
        bounded=width > 0.0 and height > 0.0,
        roads=collect_roads(map_data),
        water=collect_water(map_data),
        lakes=collect_lakes(map_data),
        rims=collect_hill_rims(map_data),
        surface=surface,
    )


def ground_defects(item: Placeable, terrain: Terrain) -> tuple[float, float]:
    """How far the ground breaks under a body, and how much of it is on a ramp.

    Both are measured on the surface the engine builds, so a hill that grew,
    rotated or opened a gateway somewhere the JSON never said is measured where
    it actually landed.  Returns ``(0.0, 0.0)`` for a body that is not judged
    against the ground at all -- a wall run is laid across whatever it has to
    hold, and a spawn is spread by the formation pass on the first tick."""
    if terrain.surface is None or not item.avoids_slope_rims:
        return 0.0, 0.0
    points = item.ground_footprint(terrain.surface)
    return (
        terrain.surface.relief(points),
        terrain.surface.entrance_coverage(points),
    )


def terrain_violations(
    items: list[Placeable], terrain: Terrain, road_margin: float, rim_margin: float
) -> list[Violation]:
    found: list[Violation] = []
    for item in items:
        if item.avoids_roads:
            deepest = -math.inf
            for road in terrain.roads:
                deepest = max(deepest, road.depth_into(item, road_margin))
            if deepest > 1e-3:
                found.append(Violation("road", item, deepest))
        deepest = -math.inf
        for stream in terrain.water:
            deepest = max(deepest, stream.depth_into(item, 0.0))
        for lake_x, lake_z, lake_radius in terrain.lakes:
            deepest = max(
                deepest,
                item.contains_depth(lake_x, lake_z, lake_radius),
            )
        if deepest > 1e-3:
            found.append(Violation("water", item, deepest))
        if terrain.surface is not None:
            relief, on_ramp = ground_defects(item, terrain)
            if relief > terrain.relief_tolerance:
                found.append(
                    Violation(
                        "slope",
                        item,
                        relief - terrain.relief_tolerance,
                        detail=f"relief {relief:.2f} m",
                    )
                )
            if on_ramp > terrain.ramp_coverage:
                found.append(
                    Violation(
                        "ramp",
                        item,
                        on_ramp - terrain.ramp_coverage,
                        detail=f"{on_ramp * 100:.0f}% of footprint",
                    )
                )
        elif item.avoids_slope_rims and terrain.rims:
            straddle = straddles_rim(item, terrain.rims, rim_margin)
            if straddle > 1e-3:
                found.append(Violation("slope", item, straddle))
    found.sort(key=lambda entry: entry.depth, reverse=True)
    return found


def is_placeable_spot(
    item: Placeable,
    x: float,
    z: float,
    terrain: Terrain,
    road_margin: float,
    rim_margin: float,
    margin: float,
) -> bool:
    if terrain.bounded and not (
        margin <= x <= terrain.width - margin and margin <= z <= terrain.height - margin
    ):
        return False
    probe = Placeable(
        key=item.key,
        kind=item.kind,
        name=item.name,
        x=x,
        z=z,
        half_x=item.half_x,
        half_z=item.half_z,
        rotation=item.rotation,
        is_disc=item.is_disc,
        priority=item.priority,
        body_type=item.body_type,
        avoids_slope_rims=item.avoids_slope_rims,
        canopy_half=item.canopy_half,
    )
    for stream in terrain.water:
        if stream.depth_into(probe, 0.0) > 0.0:
            return False
    for lake_x, lake_z, lake_radius in terrain.lakes:
        if probe.contains_depth(lake_x, lake_z, lake_radius) > 0.0:
            return False
    if item.avoids_roads:
        for road in terrain.roads:
            if road.depth_into(probe, road_margin) > 0.0:
                return False
    if terrain.surface is not None:
        relief, on_ramp = ground_defects(probe, terrain)
        if relief > terrain.relief_tolerance or on_ramp > terrain.ramp_coverage:
            return False
    elif item.avoids_slope_rims and terrain.rims:
        if straddles_rim(probe, terrain.rims, rim_margin) > 0.0:
            return False
    return True


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
ESCAPE_MARGIN = 0.02
ESCAPE_STEP_GROWTH = 1.35

POSITION_PRECISION = 2
"""Decimals a repaired position is written with, and checked at."""


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
    if item.kind == "structure" and item.priority >= PRIORITY_ANCHOR:
        return budgets["anchor"]
    return budgets.get(item.kind, budgets["world_prop"])


def try_move(
    mover: Placeable,
    items: list[Placeable],
    terrain: Terrain,
    clearance: float,
    budget: float,
    overlap: float,
    unit_x: float,
    unit_z: float,
    road_margin: float,
    rim_margin: float,
    must_clear: Placeable | None,
    accept: Callable[[], bool] | None = None,
) -> bool:
    """Walk the escape ladder and keep the first push that actually works.

    Candidate positions are rounded to the precision the JSON is written at
    *before* they are checked.  A push that clears by a hair is otherwise
    rounded straight back inside on the way to disk, and the next run reports a
    defect of 0.00 that no further push can shift.
    """
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

        new_x = round(previous_x + (unit_dx * reach), POSITION_PRECISION)
        new_z = round(previous_z + (unit_dz * reach), POSITION_PRECISION)
        mover.x, mover.z = new_x, new_z
        if not is_placeable_spot(
            mover, new_x, new_z, terrain, road_margin, rim_margin, 1.0
        ):
            continue
        if must_clear is not None and mover.overlap_with(must_clear, clearance) > 1e-3:
            continue
        if accept is not None and not accept():
            continue
        if any(
            mover.overlap_with(candidate, clearance)
            > max(depth_before.get(candidate.key, 0.0), 1e-3)
            for candidate in items
            if candidate.key != mover.key
            and (must_clear is None or candidate.key != must_clear.key)
            and candidate.priority >= mover.priority
            and not mover.exempt_from(candidate)
        ):
            continue
        mover.moved += reach
        return True
    mover.x, mover.z = previous_x, previous_z
    return False


def repair(
    items: list[Placeable],
    terrain: Terrain,
    clearance: float,
    max_passes: int,
    budgets: dict[str, float],
    road_margin: float,
    rim_margin: float,
    canopy_fraction: float,
) -> int:
    resolved = 0
    for _ in range(max_passes):
        progressed = False

        for item, other, _ in find_overlaps(items, clearance):
            if item.overlap_with(other, clearance) <= 1e-3:
                continue
            mover, anchor = (
                (item, other) if item.priority <= other.priority else (other, item)
            )
            if mover.priority >= PRIORITY_ANCHOR:
                continue
            overlap, unit_x, unit_z = mover.separation_from(anchor, clearance)
            if try_move(
                mover,
                items,
                terrain,
                clearance,
                travel_budget_for(mover, budgets),
                overlap,
                unit_x,
                unit_z,
                road_margin,
                rim_margin,
                anchor,
            ):
                progressed = True
                resolved += 1

        for violation in find_canopy_intrusions(items, canopy_fraction):
            mover = violation.item
            other = violation.other
            if other is None or mover.priority >= PRIORITY_ANCHOR:
                continue
            depth, unit_x, unit_z = canopy_intrusion(mover, other, canopy_fraction)
            if depth <= 1e-3:
                continue
            if try_move(
                mover,
                items,
                terrain,
                clearance,
                travel_budget_for(mover, budgets),
                depth,
                unit_x,
                unit_z,
                road_margin,
                rim_margin,
                None,
                accept=lambda mover=mover, other=other: canopy_intrusion(
                    mover, other, canopy_fraction
                )[0]
                <= 1e-3,
            ):
                progressed = True
                resolved += 1

        for violation in terrain_violations(items, terrain, road_margin, rim_margin):
            mover = violation.item
            if not may_step_aside(mover, violation.kind):
                continue
            unit_x, unit_z = escape_bearing(mover, terrain, violation.kind)
            if try_move(
                mover,
                items,
                terrain,
                clearance,
                travel_budget_for(mover, budgets),
                violation.depth,
                unit_x,
                unit_z,
                road_margin,
                rim_margin,
                None,
            ):
                progressed = True
                resolved += 1

        if not progressed:
            break
    return resolved


def may_step_aside(mover: Placeable, kind: str) -> bool:
    """Whether this body is allowed to move to get out of the terrain.

    Ring geometry never moves.  An anchor building normally never moves either,
    but a road running *through* a hall is a defect the road cannot be trimmed
    out of -- the route has to carry on past the building -- and a hall standing
    in a river is worse still.  Stepping a ferry house 4.5 m off the highway it
    serves keeps it exactly where it was placed along that road, so anchors give
    way for a road or for water and for nothing else; a temple built into a
    hillside is a decision, not a defect.
    """
    if mover.priority >= PRIORITY_IMMOVABLE:
        return False
    if mover.priority >= PRIORITY_ANCHOR:
        return kind in ("road", "water")
    return True


def escape_bearing(item: Placeable, terrain: Terrain, kind: str) -> tuple[float, float]:
    """Straight out from whatever the body is standing in.

    For a road or a river that is the perpendicular; for a hill rim it is
    whichever way carries the body clear of the boundary soonest, which the fan
    then explores around."""
    if kind in ("slope", "ramp") and terrain.surface is not None:
        return terrain.surface.settled_bearing(
            item.x, item.z, item.radius + 2.0, prefer_off_ramp=kind == "ramp"
        )

    if kind == "slope":
        best = None
        best_distance = math.inf
        for rim in terrain.rims:
            distance, unit_x, unit_z = rim_bearing(item, rim)
            if distance < best_distance:
                best_distance = distance
                best = (unit_x, unit_z)
        return best if best is not None else (1.0, 0.0)

    sources = terrain.roads if kind == "road" else terrain.water
    nearest = None
    nearest_distance = math.inf
    for segment in sources:
        distance = segment.distance(item.x, item.z)
        if distance < nearest_distance:
            nearest_distance = distance
            nearest = segment
    if nearest is None:
        return 1.0, 0.0

    run_x = nearest.x2 - nearest.x1
    run_z = nearest.z2 - nearest.z1
    length_sq = run_x * run_x + run_z * run_z
    if length_sq <= 1e-9:
        offset_x = item.x - nearest.x1
        offset_z = item.z - nearest.z1
    else:
        t = ((item.x - nearest.x1) * run_x + (item.z - nearest.z1) * run_z) / length_sq
        t = max(0.0, min(1.0, t))
        offset_x = item.x - (nearest.x1 + run_x * t)
        offset_z = item.z - (nearest.z1 + run_z * t)
    length = math.hypot(offset_x, offset_z)
    if length < 1e-4:
        if length_sq <= 1e-9:
            return 1.0, 0.0
        return -run_z / math.sqrt(length_sq), run_x / math.sqrt(length_sq)
    return offset_x / length, offset_z / length


def trim_roads_out_of_anchors(
    map_data: dict, items: list[Placeable], road_margin: float
) -> int:
    """Pull a road's endpoint back out of the building it was routed to.

    Roads are generated to *reach* a settlement and are authored with the
    building's own centre as an endpoint, so the last few metres of every
    approach run through the hall it is meant to arrive at.  The building is
    where the designer put it, so the road is what gives way: the head or tail
    of the polyline is walked outward until it clears the body, and the vertex
    that was inside is replaced by the crossing point.  A road that cuts through
    an anchor *mid-route* is not trimmed -- that needs a reroute, not a trim --
    and is left to be reported.
    """
    anchors = [
        item
        for item in items
        if item.kind == "structure"
        and item.priority >= PRIORITY_ANCHOR
        and item.body_type not in WALL_TYPES
    ]
    if not anchors:
        return 0

    trimmed = 0
    for road in map_data.get("roads") or []:
        points = road.get("waypoints")
        if not points:
            start, end = road.get("start"), road.get("end")
            if not start or not end:
                continue
            points = [list(start), list(end)]
            explicit_pair = True
        else:
            points = [list(point) for point in points]
            explicit_pair = False
        half_width = float(road.get("width", DEFAULT_ROAD_WIDTH)) * 0.5

        changed = False
        for from_head in (True, False):
            order = range(len(points)) if from_head else range(len(points) - 1, -1, -1)
            inside_count = 0
            for index in order:
                if not any(
                    anchor.contains_depth(
                        points[index][0], points[index][1], half_width + road_margin
                    )
                    > 0.0
                    for anchor in anchors
                ):
                    break
                inside_count += 1
            if inside_count == 0 or inside_count >= len(points) - 1:
                continue

            keep = (
                points[inside_count]
                if from_head
                else points[len(points) - 1 - inside_count]
            )
            drop = (
                points[inside_count - 1]
                if from_head
                else points[len(points) - inside_count]
            )
            crossing = walk_out_of_anchors(
                keep, drop, anchors, half_width + road_margin
            )
            if crossing is None:
                continue
            if from_head:
                points = [crossing] + points[inside_count:]
            else:
                points = points[: len(points) - inside_count] + [crossing]
            changed = True

        if not changed:
            continue
        rounded = [[tidy(point[0]), tidy(point[1])] for point in points]
        if explicit_pair:
            road["start"] = rounded[0]
            road["end"] = rounded[-1]
        else:
            road["waypoints"] = rounded
        trimmed += 1
    return trimmed


def tidy(value: float) -> float | int:
    """A coordinate written the way the maps author them: whole where it can be."""
    rounded = round(value, POSITION_PRECISION)
    return int(rounded) if rounded == int(rounded) else rounded


def walk_out_of_anchors(
    outside: list[float],
    inside: list[float],
    anchors: list[Placeable],
    reach: float,
) -> list[float] | None:
    """The first point on ``outside``->``inside`` that is still clear of them."""
    steps = 64
    for step in range(steps + 1):
        t = step / steps
        x = outside[0] + (inside[0] - outside[0]) * t
        z = outside[1] + (inside[1] - outside[1]) * t
        if any(anchor.contains_depth(x, z, reach) > 0.0 for anchor in anchors):
            if step == 0:
                return None
            previous = (step - 1) / steps
            return [
                outside[0] + (inside[0] - outside[0]) * previous,
                outside[1] + (inside[1] - outside[1]) * previous,
            ]
    return [inside[0], inside[1]]


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
        item.payload[item.x_field] = tidy(item.x)
        item.payload[item.z_field] = tidy(item.z)
        changed += 1
    return changed


def audit(
    map_data: dict,
    path_name: str,
    clearance: float,
    road_margin: float,
    rim_margin: float,
    canopy_fraction: float,
    surface: SurfaceField | None = None,
    relief_tolerance: float = GROUND_RELIEF_TOLERANCE,
    ramp_coverage: float = RAMP_COVERAGE_TOLERANCE,
) -> tuple[list[Placeable], Terrain, list[Violation]]:
    items = collect(map_data, path_name)
    terrain = read_terrain(map_data, surface)
    terrain.relief_tolerance = relief_tolerance
    terrain.ramp_coverage = ramp_coverage
    found = [
        Violation("overlap", item, depth, other)
        for item, other, depth in find_overlaps(items, clearance)
    ]
    found.extend(find_canopy_intrusions(items, canopy_fraction))
    found.extend(terrain_violations(items, terrain, road_margin, rim_margin))
    found.sort(key=lambda entry: entry.depth, reverse=True)
    return items, terrain, found


def audit_map(
    args: argparse.Namespace,
    path_name: str,
    map_data: dict,
    surface: SurfaceField | None,
) -> tuple[list[Placeable], Terrain, list[Violation]]:
    """One map audited with the tolerances the command line asked for."""
    return audit(
        map_data,
        path_name,
        args.clearance,
        args.road_margin,
        args.rim_margin,
        args.canopy_overhang,
        surface,
        args.ground_relief,
        args.ramp_coverage,
    )


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
        "--road-margin",
        type=float,
        default=ROAD_MARGIN,
        help="Verge a body must leave beyond the kerb.",
    )
    parser.add_argument(
        "--rim-margin",
        type=float,
        default=SLOPE_STRADDLE_MARGIN,
        help="How far a body may hang over the edge of a hill.",
    )
    parser.add_argument(
        "--canopy-overhang",
        type=float,
        default=CANOPY_OVERHANG_FRACTION,
        help="How far a tree's crown may lean over something built, as a share "
        "of the crown's own reach. Raise it to let trees crowd a camp; lower "
        "it to keep them off the tents entirely.",
    )
    parser.add_argument(
        "--max-travel",
        type=float,
        default=10.0,
        help="Furthest a prop may be nudged from where it was authored. The "
        "ladder always takes the shortest push that works, so raising this "
        "only changes the handful of props wedged in a dense camp; at 6 m "
        "eleven of them across assets/maps had nowhere legal to go.",
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
        default=4.0,
        help="Furthest a unit spawn may be nudged, so it stays with its camp. "
        "Wide enough to step a front rank out of a river, which is the only "
        "thing that ever needs more than a metre.",
    )
    parser.add_argument(
        "--max-travel-anchor",
        type=float,
        default=6.0,
        help="Furthest an anchor building may be nudged to clear a road or a "
        "river. Anchors never move for each other; see may_step_aside.",
    )
    parser.add_argument("--max-passes", type=int, default=12)
    parser.add_argument(
        "--ground-relief",
        type=float,
        default=GROUND_RELIEF_TOLERANCE,
        help="How far the ground may break under one body, in metres, before "
        "the model is read as floating.",
    )
    parser.add_argument(
        "--ramp-coverage",
        type=float,
        default=RAMP_COVERAGE_TOLERANCE,
        help="The share of a footprint that may stand in a hill entrance.",
    )
    parser.add_argument(
        "--terrain-probe",
        help="Path to the terrain_probe binary. Found in build*/bin by default.",
    )
    parser.add_argument(
        "--surface",
        choices=("require", "auto", "off"),
        default="auto",
        help="Whether to measure the ground the engine actually builds. "
        "'require' fails if terrain_probe is missing, 'auto' warns and falls "
        "back to the authored hill ellipses, 'off' never runs the probe. Use "
        "'require' in a gate: the fallback cannot see a grown, rotated or "
        "gateway-cut hill and reports zero defects on maps that have hundreds.",
    )
    parser.add_argument(
        "--surface-cache",
        help="Directory for probe dumps (default: a terrain-probe directory "
        "under the system temporary directory).",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Report defects and exit non-zero without writing anything.",
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
        "anchor": args.max_travel_anchor,
    }

    probe = None
    if args.surface != "off":
        try:
            probe = find_probe(args.terrain_probe)
        except ProbeUnavailable as error:
            if args.surface == "require":
                print(f"{error}", file=sys.stderr)
                return 1
            print(
                f"warning: {error}; slope is measured against the authored hill "
                f"ellipses, which cannot see a grown, rotated or gateway-cut "
                f"hill",
                file=sys.stderr,
            )
    cache_dir = Path(args.surface_cache) if args.surface_cache else None

    total_before = 0
    total_remaining = 0
    total_moved = 0
    failed = False
    for path in paths:
        try:
            source = path.read_text()
            map_data = json.loads(source)
        except (OSError, json.JSONDecodeError) as error:
            print(f"{path}: cannot read ({error})", file=sys.stderr)
            failed = True
            continue
        if not isinstance(map_data, dict) or "grid" not in map_data:
            continue

        surface = None
        if probe is not None:
            try:
                surface = load_surface(path, probe, cache_dir)
            except ProbeUnavailable as error:
                print(f"{path.name}: {error}", file=sys.stderr)
                failed = True
                continue

        try:
            items, terrain, before = audit_map(args, path.name, map_data, surface)
        except MapContentError as error:
            print(error, file=sys.stderr)
            failed = True
            continue

        total_before += len(before)

        if args.check:
            if before:
                print(f"{path.name}: {len(before)} defect(s)")
                if not args.quiet:
                    for violation in before[:12]:
                        print(f"    {violation.describe()}")
                    if len(before) > 12:
                        print(f"    ... {len(before) - 12} more")
                total_remaining += len(before)
            continue

        if not before:
            continue

        style = detect_format(source, map_data)
        if style is None:
            print(
                f"{path.name}: unrecognised formatting, refusing to rewrite it",
                file=sys.stderr,
            )
            total_remaining += len(before)
            failed = True
            continue

        trimmed = trim_roads_out_of_anchors(map_data, items, args.road_margin)
        if trimmed:
            terrain = read_terrain(map_data, surface)
            terrain.relief_tolerance = args.ground_relief
            terrain.ramp_coverage = args.ramp_coverage

        repair(
            items,
            terrain,
            args.clearance,
            args.max_passes,
            budgets,
            args.road_margin,
            args.rim_margin,
            args.canopy_overhang,
        )
        moved = apply(items)

        remaining = len(audit_map(args, path.name, map_data, surface)[2])
        total_remaining += remaining
        total_moved += moved
        indent, sort_keys, trailing_newline = style
        path.write_text(
            json.dumps(map_data, indent=indent, sort_keys=sort_keys)
            + ("\n" if trailing_newline else "")
        )
        print(
            f"{path.name}: {len(before)} defect(s) -> {remaining} left, "
            f"{moved} object(s) nudged, {trimmed} road(s) trimmed"
        )

    if args.check:
        print(f"total: {total_before} defect(s)")
        return 1 if (total_before or failed) else 0

    print(
        f"total: {total_before} defect(s) -> {total_remaining} left, "
        f"{total_moved} object(s) nudged"
    )
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
