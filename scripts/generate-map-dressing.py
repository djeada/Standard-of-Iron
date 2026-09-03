#!/usr/bin/env python3
"""Stamp hand-authored dressing into Standard of Iron map JSON.

Settlements come from ``settlements`` and the four stock landmark kinds from
``landmarks``. Everything else that makes a battlefield read as one place - the
raft camp on a river bank, the boulder field under a pass, the cypress avenue
into a town, the fires either side of a gate, the cairn at a ramp mouth - is
authored as intent in a map's ``dressing`` array and stamped into ``world_props``
by this tool. One entry is a handful of lines; the composition, the ground
check and the nudging are here.

Every prop written carries a ``dressing`` key naming the piece it belongs to. A
run strips its own previous output before writing, and leaves everything
without the key alone, so this tool composes with ``generate-map-landmarks.py``
and with hand-placed scatter in either order.

Pieces
  Landmarks - one place worth walking to:
    barrow_field   ring of ruins, fallen statues, dead trees and boulders; the
                   ground a Sepulcher zone rises from. ``shrine`` adds an
                   authored magic shrine at the centre for the zone to adopt.
    hilltop_ruin   a razed citadel: ruins in a rough ring, a statue pair at
                   the gate, a fire still burning in it.
    sacred_grove   a ring of trees around a monument. ``tree`` picks the
                   species (cypress_tree, pine_tree, olive_tree, palm_tree),
                   ``centre`` the monument (statue, magic_shrine, firecamp).
    tree_avenue    paired trees along a road between ``from`` and ``to``.
    oasis          palms, water carts and fires around a well of ruins.
    scree          a slide of boulders with a wrecked cart in it.
    boneyard       dead trees, broken ruins and stones - a field the dead own.
    ford_wreck     a lost crossing: boulders and a cart on both banks.
    raft_camp      an army's landing: a row of carts on the bank, tents,
                   fires and racks behind them. ``facing`` is the bank's
                   outward side.
    cairn          a statue on a ring of stones.
    shore_hamlet   a fishing village: homes along a shore, carts drawn up,
                   reeds and stones at the water. ``facing`` is toward the
                   water.
    sheepfold      a shepherd's steading by a pasture.
  Routes - the small things that say a road is used:
    bridgehead     both ends of the nearest bridge: stones, a fire, a cart.
    junction       the nearest road junction: a wayside statue and a fire.
                   ``style`` is ``milestone`` (statue only), ``shrine``
                   (statue, ruins, fire) or ``camp`` (fire, cart, rack).
    gate_approach  outside the nearest gate: fires either side of the road,
                   racks, a cart; statues too for a town.
    ramp_mouth     the nearest hill entrance: boulders flanking the ramp.
    riverbank      stones, reeds and dead trees along a river between
                   ``from`` and ``to``.

Every piece takes ``id``, ``kind``, ``x``, ``z`` and optionally ``seed``,
``scale`` and ``rotation`` (degrees). Kinds document their own extras.

Placement is checked the way ``fix-map-prop-overlaps.py`` checks it: bodies
keep out of water, roads and bridges, off other bodies and off walls, and -
when ``tools/terrain_probe`` is built - off broken ground and hill ramps. A
prop with nowhere legal to stand within its slack is dropped and reported,
never pushed somewhere it does not belong.

The command is dry-run by default. Pass --write to update the map.
"""

from __future__ import annotations

import argparse
import importlib.util
import json
import math
import random
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Sequence

SCRIPTS = Path(__file__).resolve().parent
sys.path.insert(0, str(SCRIPTS))

_surface_field = importlib.import_module("map_surface_field")
_water_geometry = importlib.import_module("map_water_geometry")
ProbeUnavailable = _surface_field.ProbeUnavailable
SurfaceField = _surface_field.SurfaceField
find_probe = _surface_field.find_probe
load_surface = _surface_field.load_surface
is_ring_river = _water_geometry.is_ring_river
river_points = _water_geometry.river_points


def load_audit_module():
    spec = importlib.util.spec_from_file_location(
        "soi_prop_audit", SCRIPTS / "fix-map-prop-overlaps.py"
    )
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    sys.modules["soi_prop_audit"] = module
    spec.loader.exec_module(module)
    return module


AUDIT = load_audit_module()

DRESSING_KEY = "dressing"

CANOPY_TYPES = {"pine_tree", "olive_tree", "cypress_tree", "palm_tree"}
BLOCKING_TYPES = {"statue", "abandoned_home"}
SOLID_TYPES = {
    "tent",
    "supply_cart",
    "weapon_rack",
    "ruins",
    "dead_tree",
    "boulder",
    "iron_ore",
    "magic_shrine",
    "abandoned_home",
    "statue",
    "cursed_gold_vein",
    "firecamp",
}

DEFAULT_RIVER_WIDTH = 6.0
DEFAULT_BRIDGE_WIDTH = 8.0
DEFAULT_ROAD_WIDTH = 3.0

RIVER_DRAWN_HALF = 0.55
"""The ribbon is drawn wider than the channel is blocked; keep bodies off it."""

ROAD_VERGE = 1.2
WALL_KEEP_OUT = 3.0
SPAWN_KEEP_OUT = 3.0
STRUCTURE_KEEP_OUT = {
    "barracks": 8.0,
    "home": 4.5,
    "marketplace": 6.0,
    "temple": 10.0,
    "farm": 8.5,
    "defense_tower": 3.0,
}
STRUCTURE_KEEP_OUT_DEFAULT = 4.0
PROP_GAP = 0.6
CAPTURABLE_TYPES = {"magic_shrine", "cursed_gold_vein"}
CAPTURABLE_KEEP_OUT = 5.5
"""A shrine or vein raises a barracks body at runtime; nothing built stands on it."""
CANOPY_CLEARANCE = 1.0
GROUND_RELIEF = 0.30
EDGE_MARGIN = 5.0
NUDGE_SLACK = 4.5


@dataclass
class Segment:
    ax: float
    az: float
    bx: float
    bz: float
    half: float

    def distance(self, x: float, z: float) -> float:
        dx = self.bx - self.ax
        dz = self.bz - self.az
        length_sq = dx * dx + dz * dz
        if length_sq <= 1e-9:
            return math.hypot(x - self.ax, z - self.az)
        t = ((x - self.ax) * dx + (z - self.az) * dz) / length_sq
        t = max(0.0, min(1.0, t))
        return math.hypot(x - (self.ax + dx * t), z - (self.az + dz * t))

    def tangent(self) -> tuple[float, float]:
        dx = self.bx - self.ax
        dz = self.bz - self.az
        length = math.hypot(dx, dz)
        if length <= 1e-9:
            return (1.0, 0.0)
        return (dx / length, dz / length)


def polyline(entry: dict) -> list[tuple[float, float]]:
    raw = entry.get("waypoints") or [entry.get("start"), entry.get("end")]
    return [(float(p[0]), float(p[1])) for p in raw if p]


def segments_of(points: Sequence[tuple[float, float]], half: float) -> list[Segment]:
    return [
        Segment(a[0], a[1], b[0], b[1], half)
        for a, b in zip(points, points[1:], strict=False)
    ]


@dataclass
class Body:
    x: float
    z: float
    radius: float
    kind: str
    canopy: float = 0.0


@dataclass
class Site:
    definition: dict
    surface: SurfaceField | None
    width: float
    height: float
    roads: list[Segment] = field(default_factory=list)
    road_lines: list[tuple[list[tuple[float, float]], float]] = field(
        default_factory=list
    )
    water: list[Segment] = field(default_factory=list)
    river_lines: list[tuple[list[tuple[float, float]], float]] = field(
        default_factory=list
    )
    lakes: list[tuple[float, float, float, float]] = field(default_factory=list)
    bridges: list[Segment] = field(default_factory=list)
    walls: list[Segment] = field(default_factory=list)
    bodies: list[Body] = field(default_factory=list)
    spawns: list[tuple[float, float]] = field(default_factory=list)

    @staticmethod
    def build(definition: dict, surface: SurfaceField | None) -> "Site":
        grid = definition.get("grid") or {}
        site = Site(
            definition=definition,
            surface=surface,
            width=float(grid.get("width", 0)),
            height=float(grid.get("height", 0)),
        )
        for road in definition.get("roads") or []:
            points = polyline(road)
            half = float(road.get("width", DEFAULT_ROAD_WIDTH)) * 0.5
            site.roads.extend(segments_of(points, half))
            site.road_lines.append((points, half))
        for river in definition.get("rivers") or []:
            width = float(river.get("width", DEFAULT_RIVER_WIDTH))
            points = [(float(p[0]), float(p[1])) for p in river_points(river)]
            if is_ring_river(river) and points:
                points = points + [points[0]]
            half = width * RIVER_DRAWN_HALF
            site.water.extend(segments_of(points, half))
            site.river_lines.append((points, width))
        for lake in definition.get("lakes") or []:
            site.lakes.append(
                (
                    float(lake["x"]),
                    float(lake["z"]),
                    float(lake.get("width", lake.get("radius", 20) * 2)) * 0.5,
                    float(lake.get("depth", lake.get("radius", 20) * 2)) * 0.5,
                )
            )
        for bridge in definition.get("bridges") or []:
            half = float(bridge.get("width", DEFAULT_BRIDGE_WIDTH)) * 0.5
            site.bridges.extend(segments_of(polyline(bridge), half + 1.0))
        for entry in definition.get("structures") or []:
            kind = str(entry.get("type", ""))
            if "start" in entry and "end" in entry:
                a = entry["start"]
                b = entry["end"]
                site.walls.append(
                    Segment(float(a[0]), float(a[1]), float(b[0]), float(b[1]), 1.0)
                )
                continue
            if "x" not in entry:
                continue
            site.bodies.append(
                Body(
                    float(entry["x"]),
                    float(entry["z"]),
                    STRUCTURE_KEEP_OUT.get(kind, STRUCTURE_KEEP_OUT_DEFAULT),
                    kind,
                )
            )
        for prop in definition.get("world_props") or []:
            if prop.get(DRESSING_KEY) is not None:
                continue
            site.add_prop_body(prop)
        for spawn in definition.get("spawns") or []:
            site.spawns.append((float(spawn["x"]), float(spawn["z"])))
        return site

    def add_prop_body(self, prop: dict) -> None:
        kind = str(prop["type"])
        scale = float(prop.get("scale", 1.0) or 1.0)
        half_x, half_z = AUDIT.prop_ground_half_extents(kind, scale)
        canopy = 0.0
        if kind in CANOPY_TYPES:
            canopy = AUDIT.WORLD_PROP_RENDER_SCALE.get(kind, 1.0) * scale
        radius = math.hypot(half_x, half_z)
        if kind in CAPTURABLE_TYPES:
            radius = max(radius, CAPTURABLE_KEEP_OUT)
        self.bodies.append(
            Body(float(prop["x"]), float(prop["z"]), radius, kind, canopy)
        )

    def ground_ok(
        self,
        x: float,
        z: float,
        half_x: float,
        half_z: float,
        rotation: float,
        allow_ramp: bool = False,
    ) -> bool:
        if self.surface is None:
            return True
        points = self.surface.footprint_samples(x, z, half_x, half_z, rotation, False)
        if self.surface.relief(points) > GROUND_RELIEF:
            return False
        if not allow_ramp and self.surface.entrance_coverage(points) > 0.0:
            return False
        return True

    def why(
        self,
        kind: str,
        x: float,
        z: float,
        scale: float,
        rotation: float,
        allow_ramp: bool = False,
        road_verge: float = ROAD_VERGE,
    ) -> str | None:
        """The first reason a body of ``kind`` cannot stand at (x, z), or None."""
        if not (
            EDGE_MARGIN <= x <= self.width - EDGE_MARGIN
            and EDGE_MARGIN <= z <= self.height - EDGE_MARGIN
        ):
            return "map edge"
        half_x, half_z = AUDIT.prop_ground_half_extents(kind, scale)
        radius = math.hypot(half_x, half_z)
        for stream in self.water:
            if stream.distance(x, z) < stream.half + radius + 0.5:
                return "water"
        for lx, lz, hx, hz in self.lakes:
            if ((x - lx) / (hx + radius + 2.0)) ** 2 + (
                (z - lz) / (hz + radius + 2.0)
            ) ** 2 < 1.0:
                return "lake"
        for deck in self.bridges:
            if deck.distance(x, z) < deck.half + radius:
                return "bridge deck"
        for road in self.roads:
            if road.distance(x, z) < road.half + road_verge + radius:
                return "road"
        for wall in self.walls:
            if wall.distance(x, z) < WALL_KEEP_OUT + radius:
                return "wall"
        canopy = 0.0
        if kind in CANOPY_TYPES:
            canopy = AUDIT.WORLD_PROP_RENDER_SCALE.get(kind, 1.0) * scale
        for body in self.bodies:
            gap = math.hypot(x - body.x, z - body.z)
            if gap < body.radius + radius + PROP_GAP:
                return f"body ({body.kind})"
            if (
                canopy > 0.0
                and body.kind not in CANOPY_TYPES
                and gap < canopy + body.radius + CANOPY_CLEARANCE
            ):
                return f"canopy over {body.kind}"
            if (
                body.canopy > 0.0
                and kind not in CANOPY_TYPES
                and gap < body.canopy + radius + CANOPY_CLEARANCE
            ):
                return f"under {body.kind} canopy"
        for sx, sz in self.spawns:
            if math.hypot(x - sx, z - sz) < SPAWN_KEEP_OUT + radius:
                return "spawn"
        if self.surface is not None:
            points = self.surface.footprint_samples(
                x, z, half_x, half_z, rotation, False
            )
            if self.surface.relief(points) > GROUND_RELIEF:
                return "broken ground"
            if not allow_ramp and self.surface.entrance_coverage(points) > 0.0:
                return "hill ramp"
        return None

    def legal(
        self,
        kind: str,
        x: float,
        z: float,
        scale: float,
        rotation: float,
        allow_ramp: bool = False,
        road_verge: float = ROAD_VERGE,
    ) -> bool:
        return self.why(kind, x, z, scale, rotation, allow_ramp, road_verge) is None

    def nearest_road(self, x: float, z: float) -> tuple[Segment | None, float]:
        best = None
        best_distance = float("inf")
        for road in self.roads:
            distance = road.distance(x, z)
            if distance < best_distance:
                best = road
                best_distance = distance
        return best, best_distance

    def nearest_river(self, x: float, z: float) -> tuple[Segment | None, float]:
        best = None
        best_distance = float("inf")
        for stream in self.water:
            distance = stream.distance(x, z)
            if distance < best_distance:
                best = stream
                best_distance = distance
        return best, best_distance


class Placer:
    """Lays one piece's props down, nudging each until it stands legally."""

    def __init__(self, site: Site, piece_id: str, seed: int):
        self.site = site
        self.piece_id = piece_id
        self.rng = random.Random(seed)
        self.placed: list[dict] = []
        self.dropped: list[str] = []

    def jitter(self, amount: float) -> float:
        return self.rng.uniform(-amount, amount)

    def put(
        self,
        kind: str,
        x: float,
        z: float,
        rotation: float | None = None,
        scale: float = 1.0,
        slack: float = NUDGE_SLACK,
        allow_ramp: bool = False,
        road_verge: float = ROAD_VERGE,
        extra: dict | None = None,
    ) -> dict | None:
        if rotation is None:
            rotation = self.rng.uniform(0.0, math.tau)
        candidates = [(x, z)]
        ring = 1.2
        while ring <= slack:
            for step in range(12):
                angle = math.tau * step / 12.0 + ring
                candidates.append(
                    (x + math.cos(angle) * ring, z + math.sin(angle) * ring)
                )
            ring += 1.1
        for cx, cz in candidates:
            if self.site.legal(kind, cx, cz, scale, rotation, allow_ramp, road_verge):
                prop = {
                    "type": kind,
                    "x": round(cx, 2),
                    "z": round(cz, 2),
                    "scale": round(scale, 2),
                    "rotation": round(rotation, 3),
                    DRESSING_KEY: self.piece_id,
                }
                if kind == "firecamp":
                    prop["radius"] = 3.2
                    prop["intensity"] = 1.15
                    prop["persistent"] = True
                if extra:
                    prop.update(extra)
                self.site.add_prop_body(prop)
                self.placed.append(prop)
                return prop
        reason = self.site.why(kind, x, z, scale, rotation, allow_ramp, road_verge)
        self.dropped.append(f"{kind} at ({x:.0f}, {z:.0f}): {reason}")
        return None


def rotate(along: float, across: float, heading: float) -> tuple[float, float]:
    """Local (along, across) into a map offset for a piece heading ``heading`` radians."""
    cos_h = math.cos(heading)
    sin_h = math.sin(heading)
    return (along * cos_h - across * sin_h, along * sin_h + across * cos_h)


def heading_of(facing: str | None, rotation_deg: float | None) -> float:
    if rotation_deg is not None:
        return math.radians(rotation_deg)
    return {
        "east": 0.0,
        "south": math.pi * 0.5,
        "west": math.pi,
        "north": -math.pi * 0.5,
    }.get(str(facing or "east").lower(), 0.0)


Composer = Callable[[Placer, dict, float, float], None]


def barrow_field(p: Placer, spec: dict, x: float, z: float) -> None:
    radius = float(spec.get("radius", 11.0))
    ruins = int(spec.get("ruins", 6))
    for i in range(ruins):
        angle = math.tau * i / ruins + p.jitter(0.25)
        r = radius * p.rng.uniform(0.8, 1.05)
        p.put(
            "ruins",
            x + math.cos(angle) * r,
            z + math.sin(angle) * r,
            scale=p.rng.uniform(0.8, 1.2),
        )
    for i in range(int(spec.get("dead_trees", 4))):
        angle = math.tau * (i + 0.5) / 4 + p.jitter(0.4)
        r = radius * p.rng.uniform(1.25, 1.6)
        p.put(
            "dead_tree",
            x + math.cos(angle) * r,
            z + math.sin(angle) * r,
            scale=p.rng.uniform(0.85, 1.15),
        )
    for i in range(int(spec.get("statues", 2))):
        angle = math.tau * i / 2 + p.jitter(0.6)
        r = radius * 0.8
        p.put(
            "statue",
            x + math.cos(angle) * r,
            z + math.sin(angle) * r,
            rotation=p.rng.uniform(0.0, math.tau),
            scale=0.9,
        )
    for _ in range(int(spec.get("boulders", 4))):
        angle = p.rng.uniform(0.0, math.tau)
        r = radius * p.rng.uniform(0.9, 1.3)
        p.put(
            "boulder",
            x + math.cos(angle) * r,
            z + math.sin(angle) * r,
            scale=p.rng.uniform(0.8, 1.3),
        )
    if spec.get("shrine"):
        p.put("magic_shrine", x, z, rotation=0.0, slack=6.0)


def hilltop_ruin(p: Placer, spec: dict, x: float, z: float) -> None:
    heading = heading_of(spec.get("facing"), spec.get("rotation"))
    radius = float(spec.get("radius", 12.0))
    count = int(spec.get("ruins", 7))
    for i in range(count):
        angle = math.tau * i / count + p.jitter(0.2)
        r = radius * p.rng.uniform(0.85, 1.0)
        p.put(
            "ruins",
            x + math.cos(angle) * r,
            z + math.sin(angle) * r,
            rotation=angle + math.pi * 0.5,
            scale=p.rng.uniform(0.9, 1.25),
        )
    for side in (-1.0, 1.0):
        dx, dz = rotate(-radius - 4.0, side * 4.5, heading)
        p.put("statue", x + dx, z + dz, rotation=heading + side * math.pi * 0.5)
    dx, dz = rotate(-2.0, 0.0, heading)
    p.put("firecamp", x + dx, z + dz, rotation=0.0)
    for i in range(int(spec.get("dead_trees", 2))):
        dx, dz = rotate(radius * 0.4, (i - 0.5) * radius * 0.9, heading)
        p.put("dead_tree", x + dx, z + dz)
    for _ in range(int(spec.get("boulders", 3))):
        angle = p.rng.uniform(0.0, math.tau)
        r = radius * p.rng.uniform(1.15, 1.4)
        p.put(
            "boulder",
            x + math.cos(angle) * r,
            z + math.sin(angle) * r,
            scale=p.rng.uniform(0.8, 1.2),
        )


def sacred_grove(p: Placer, spec: dict, x: float, z: float) -> None:
    tree = str(spec.get("tree", "cypress_tree"))
    radius = float(spec.get("radius", 11.0))
    count = int(spec.get("count", 8))
    centre = str(spec.get("centre", "statue"))
    p.put(
        centre,
        x,
        z,
        rotation=heading_of(spec.get("facing"), spec.get("rotation")),
        scale=float(spec.get("centre_scale", 1.1)),
        slack=5.0,
    )
    if spec.get("fire", True) and centre != "firecamp":
        p.put("firecamp", x + 4.0, z + 3.0, rotation=0.0)
    for i in range(count):
        angle = math.tau * i / count + p.jitter(0.15)
        r = radius * p.rng.uniform(0.92, 1.08)
        p.put(
            tree,
            x + math.cos(angle) * r,
            z + math.sin(angle) * r,
            scale=p.rng.uniform(0.9, 1.1),
            slack=5.0,
        )
    for _ in range(int(spec.get("boulders", 2))):
        angle = p.rng.uniform(0.0, math.tau)
        p.put(
            "boulder",
            x + math.cos(angle) * radius * 0.55,
            z + math.sin(angle) * radius * 0.55,
            scale=p.rng.uniform(0.7, 1.0),
        )


def road_path_between(
    site: Site, start: tuple[float, float], end: tuple[float, float]
) -> list[tuple[float, float]]:
    """The stretch of the one road polyline that passes closest to both points."""
    best = None
    best_score = float("inf")
    for points, half in site.road_lines:
        segs = segments_of(points, half)
        if not segs:
            continue
        s_dist = min(s.distance(*start) for s in segs)
        e_dist = min(s.distance(*end) for s in segs)
        score = s_dist + e_dist
        if score < best_score:
            best_score = score
            best = (points, segs)
    if best is None:
        return []
    points, segs = best

    def param(point: tuple[float, float]) -> float:
        best_t = 0.0
        best_d = float("inf")
        run = 0.0
        for seg in segs:
            length = math.hypot(seg.bx - seg.ax, seg.bz - seg.az)
            if length <= 1e-9:
                continue
            t = (
                (point[0] - seg.ax) * (seg.bx - seg.ax)
                + (point[1] - seg.az) * (seg.bz - seg.az)
            ) / (length * length)
            t = max(0.0, min(1.0, t))
            d = math.hypot(
                point[0] - (seg.ax + (seg.bx - seg.ax) * t),
                point[1] - (seg.az + (seg.bz - seg.az) * t),
            )
            if d < best_d:
                best_d = d
                best_t = run + t * length
            run += length
        return best_t

    t0 = param(start)
    t1 = param(end)
    if t1 < t0:
        t0, t1 = t1, t0
    out: list[tuple[float, float]] = []
    run = 0.0
    for seg in segs:
        length = math.hypot(seg.bx - seg.ax, seg.bz - seg.az)
        if length <= 1e-9:
            continue
        a = max(t0, run)
        b = min(t1, run + length)
        if b > a:
            for t in (a, b):
                f = (t - run) / length
                out.append(
                    (seg.ax + (seg.bx - seg.ax) * f, seg.az + (seg.bz - seg.az) * f)
                )
        run += length
    return out


def walk_path(points: Sequence[tuple[float, float]], spacing: float):
    """Points every ``spacing`` along a polyline with the local tangent."""
    if len(points) < 2:
        return
    carry = spacing * 0.5
    for a, b in zip(points, points[1:], strict=False):
        length = math.hypot(b[0] - a[0], b[1] - a[1])
        if length <= 1e-9:
            continue
        tx = (b[0] - a[0]) / length
        tz = (b[1] - a[1]) / length
        d = carry
        while d <= length:
            yield (a[0] + tx * d, a[1] + tz * d, tx, tz)
            d += spacing
        carry = d - length


def tree_avenue(p: Placer, spec: dict, x: float, z: float) -> None:
    tree = str(spec.get("tree", "cypress_tree"))
    start = (float(spec["from"][0]), float(spec["from"][1]))
    end = (float(spec["to"][0]), float(spec["to"][1]))
    path = road_path_between(p.site, start, end)
    if not path:
        p.dropped.append("tree_avenue found no road")
        return
    road, _ = p.site.nearest_road(*path[0])
    half = road.half if road else 1.5
    offset = float(spec.get("offset", half + 4.0))
    spacing = float(spec.get("spacing", 9.0))
    sides = spec.get("sides", "both")
    for px, pz, tx, tz in walk_path(path, spacing):
        nx, nz = -tz, tx
        for side in (-1.0, 1.0):
            if sides == "left" and side > 0 or sides == "right" and side < 0:
                continue
            p.put(
                tree,
                px + nx * offset * side + p.jitter(0.6),
                pz + nz * offset * side + p.jitter(0.6),
                scale=p.rng.uniform(0.9, 1.1),
                slack=3.0,
            )


def oasis(p: Placer, spec: dict, x: float, z: float) -> None:
    radius = float(spec.get("radius", 12.0))
    p.put("ruins", x, z, rotation=0.0, scale=0.8)
    for i in range(int(spec.get("palms", 9))):
        angle = math.tau * i / 9 + p.jitter(0.3)
        r = radius * p.rng.uniform(0.6, 1.1)
        p.put(
            "palm_tree",
            x + math.cos(angle) * r,
            z + math.sin(angle) * r,
            scale=p.rng.uniform(0.85, 1.15),
            slack=5.0,
        )
    heading = heading_of(spec.get("facing"), spec.get("rotation"))
    for along, across in ((radius + 4.0, -3.0), (radius + 5.0, 4.0)):
        dx, dz = rotate(along, across, heading)
        p.put("supply_cart", x + dx, z + dz, rotation=heading + p.jitter(0.3))
    dx, dz = rotate(radius + 9.0, 0.0, heading)
    p.put("firecamp", x + dx, z + dz, rotation=0.0)
    for i in range(int(spec.get("tents", 2))):
        dx, dz = rotate(radius + 12.0, (i - 0.5) * 7.0, heading)
        p.put("tent", x + dx, z + dz, rotation=heading + math.pi)
    for _ in range(int(spec.get("boulders", 3))):
        angle = p.rng.uniform(0.0, math.tau)
        p.put(
            "boulder",
            x + math.cos(angle) * radius * 0.5,
            z + math.sin(angle) * radius * 0.5,
            scale=p.rng.uniform(0.6, 0.9),
        )


def scree(p: Placer, spec: dict, x: float, z: float) -> None:
    half_w = float(spec.get("width", 30.0)) * 0.5
    half_d = float(spec.get("depth", 14.0)) * 0.5
    heading = heading_of(spec.get("facing"), spec.get("rotation"))
    for _ in range(int(spec.get("boulders", 14))):
        along = p.rng.uniform(-half_w, half_w)
        across = p.rng.uniform(-half_d, half_d) * (1.0 - abs(along) / half_w * 0.5)
        dx, dz = rotate(along, across, heading)
        p.put("boulder", x + dx, z + dz, scale=p.rng.uniform(0.9, 2.1), slack=3.0)
    for i in range(int(spec.get("dead_trees", 3))):
        dx, dz = rotate(
            p.rng.uniform(-half_w, half_w),
            (half_d + 3.0) * (1 if i % 2 else -1),
            heading,
        )
        p.put("dead_tree", x + dx, z + dz, scale=p.rng.uniform(0.8, 1.1))
    if spec.get("cart", True):
        dx, dz = rotate(
            p.rng.uniform(-half_w * 0.5, half_w * 0.5), half_d * 0.6, heading
        )
        p.put("supply_cart", x + dx, z + dz, rotation=heading + p.rng.uniform(0.8, 1.4))


def boneyard(p: Placer, spec: dict, x: float, z: float) -> None:
    radius = float(spec.get("radius", 14.0))
    for _ in range(int(spec.get("dead_trees", 7))):
        angle = p.rng.uniform(0.0, math.tau)
        r = radius * math.sqrt(p.rng.uniform(0.1, 1.0))
        p.put(
            "dead_tree",
            x + math.cos(angle) * r,
            z + math.sin(angle) * r,
            scale=p.rng.uniform(0.8, 1.2),
        )
    for _ in range(int(spec.get("ruins", 3))):
        angle = p.rng.uniform(0.0, math.tau)
        r = radius * p.rng.uniform(0.3, 0.8)
        p.put(
            "ruins",
            x + math.cos(angle) * r,
            z + math.sin(angle) * r,
            scale=p.rng.uniform(0.7, 1.0),
        )
    for _ in range(int(spec.get("boulders", 5))):
        angle = p.rng.uniform(0.0, math.tau)
        r = radius * p.rng.uniform(0.2, 1.1)
        p.put(
            "boulder",
            x + math.cos(angle) * r,
            z + math.sin(angle) * r,
            scale=p.rng.uniform(0.6, 1.2),
        )
    for _i in range(int(spec.get("statues", 1))):
        p.put("statue", x + p.jitter(3.0), z + p.jitter(3.0), scale=0.9)


def bank_frame(
    site: Site, x: float, z: float
) -> tuple[Segment, float, float, float, float] | None:
    stream, _ = site.nearest_river(x, z)
    if stream is None:
        return None
    tx, tz = stream.tangent()
    nx, nz = -tz, tx
    return stream, tx, tz, nx, nz


def ford_wreck(p: Placer, spec: dict, x: float, z: float) -> None:
    frame = bank_frame(p.site, x, z)
    if frame is None:
        p.dropped.append("ford_wreck found no river")
        return
    stream, tx, tz, nx, nz = frame
    offset = stream.half + 3.0
    for side in (-1.0, 1.0):
        for i in range(int(spec.get("boulders", 3))):
            along = (i - 1) * 5.0 + p.jitter(1.5)
            out = offset + p.rng.uniform(0.0, 2.5)
            p.put(
                "boulder",
                x + tx * along + nx * out * side,
                z + tz * along + nz * out * side,
                scale=p.rng.uniform(0.8, 1.4),
            )
        for i in range(int(spec.get("plants", 3))):
            along = (i - 1) * 4.0 + p.jitter(2.0)
            out = offset - 0.5 + p.rng.uniform(0.0, 1.5)
            p.put(
                "plant",
                x + tx * along + nx * out * side,
                z + tz * along + nz * out * side,
                scale=p.rng.uniform(0.9, 1.3),
                slack=2.0,
            )
    out = offset + 5.0
    p.put("dead_tree", x + tx * 6.0 + nx * out, z + tz * 6.0 + nz * out)
    for i in range(int(spec.get("carts", 1))):
        side = -1.0 if i % 2 == 0 else 1.0
        p.put(
            "supply_cart",
            x - tx * (4.0 + i * 5.0) + nx * (offset + 4.0) * side,
            z - tz * (4.0 + i * 5.0) + nz * (offset + 4.0) * side,
            rotation=math.atan2(nz, nx) + p.rng.uniform(0.5, 1.2),
        )
    if spec.get("ruins", True):
        p.put(
            "ruins",
            x + tx * 12.0 - nx * (offset + 7.0),
            z + tz * 12.0 - nz * (offset + 7.0),
            scale=0.8,
        )


def raft_camp(p: Placer, spec: dict, x: float, z: float) -> None:
    heading = heading_of(spec.get("facing"), spec.get("rotation"))
    carts = int(spec.get("carts", 5))
    for i in range(carts):
        across = (i - (carts - 1) * 0.5) * 6.0 + p.jitter(0.8)
        dx, dz = rotate(0.0 + p.jitter(1.0), across, heading)
        p.put(
            "supply_cart",
            x + dx,
            z + dz,
            rotation=heading + math.pi * 0.5 + p.jitter(0.2),
        )
    tents = int(spec.get("tents", 6))
    for i in range(tents):
        row = i // 3
        across = ((i % 3) - 1) * 8.0 + p.jitter(1.0)
        dx, dz = rotate(9.0 + row * 8.0 + p.jitter(1.0), across, heading)
        p.put(
            "tent",
            x + dx,
            z + dz,
            rotation=heading + math.pi,
            scale=p.rng.uniform(0.95, 1.05),
        )
    for i in range(int(spec.get("fires", 3))):
        dx, dz = rotate(13.0 + p.jitter(2.0), (i - 1) * 11.0, heading)
        p.put("firecamp", x + dx, z + dz, rotation=0.0)
    for i in range(int(spec.get("racks", 2))):
        dx, dz = rotate(6.0, (i - 0.5) * 18.0, heading)
        p.put("weapon_rack", x + dx, z + dz, rotation=heading)
    for _ in range(int(spec.get("boulders", 3))):
        dx, dz = rotate(-3.0 + p.jitter(1.0), p.jitter(carts * 4.0), heading)
        p.put("boulder", x + dx, z + dz, scale=p.rng.uniform(0.7, 1.1))


def shore_hamlet(p: Placer, spec: dict, x: float, z: float) -> None:
    heading = heading_of(spec.get("facing"), spec.get("rotation"))
    homes = int(spec.get("homes", 3))
    for i in range(homes):
        dx, dz = rotate(p.jitter(2.0), (i - (homes - 1) * 0.5) * 11.0, heading)
        p.put(
            "abandoned_home",
            x + dx,
            z + dz,
            rotation=heading + p.jitter(0.25),
            scale=p.rng.uniform(0.9, 1.05),
            slack=6.0,
        )
    for i in range(int(spec.get("carts", 2))):
        dx, dz = rotate(8.0 + p.jitter(1.5), (i - 0.5) * 9.0 + p.jitter(1.0), heading)
        p.put(
            "supply_cart",
            x + dx,
            z + dz,
            rotation=heading + math.pi * 0.5 + p.jitter(0.4),
        )
    dx, dz = rotate(7.0, homes * 5.5 + 3.0, heading)
    p.put("ruins", x + dx, z + dz, scale=0.85)
    dx, dz = rotate(-7.0, -homes * 5.5, heading)
    p.put("dead_tree", x + dx, z + dz)
    dx, dz = rotate(5.0, 0.0, heading)
    p.put("firecamp", x + dx, z + dz, rotation=0.0)
    for _ in range(int(spec.get("plants", 5))):
        dx, dz = rotate(
            12.0 + p.rng.uniform(0.0, 3.0),
            p.rng.uniform(-homes * 6.0, homes * 6.0),
            heading,
        )
        p.put("plant", x + dx, z + dz, scale=p.rng.uniform(0.9, 1.4), slack=2.5)
    for _ in range(int(spec.get("boulders", 3))):
        dx, dz = rotate(
            11.0 + p.rng.uniform(0.0, 4.0),
            p.rng.uniform(-homes * 6.0, homes * 6.0),
            heading,
        )
        p.put("boulder", x + dx, z + dz, scale=p.rng.uniform(0.6, 1.0), slack=2.5)


def cairn(p: Placer, spec: dict, x: float, z: float) -> None:
    p.put(
        "statue",
        x,
        z,
        rotation=heading_of(spec.get("facing"), spec.get("rotation")),
        scale=float(spec.get("scale", 1.15)),
        slack=6.0,
    )
    count = int(spec.get("boulders", 6))
    radius = float(spec.get("radius", 4.5))
    for i in range(count):
        angle = math.tau * i / count + p.jitter(0.2)
        p.put(
            "boulder",
            x + math.cos(angle) * radius,
            z + math.sin(angle) * radius,
            scale=p.rng.uniform(1.1, 1.7),
            slack=3.0,
        )
    if spec.get("fire", True):
        p.put("firecamp", x + radius + 4.0, z + 1.0, rotation=0.0)


def sheepfold(p: Placer, spec: dict, x: float, z: float) -> None:
    heading = heading_of(spec.get("facing"), spec.get("rotation"))
    p.put("abandoned_home", x, z, rotation=heading, slack=6.0)
    for i in range(int(spec.get("boulders", 6))):
        angle = heading + math.pi * 0.5 + math.pi * (i / 5.0) + p.jitter(0.15)
        p.put(
            "boulder",
            x + math.cos(angle) * 8.0,
            z + math.sin(angle) * 8.0,
            scale=p.rng.uniform(0.7, 1.0),
        )
    dx, dz = rotate(-6.0, 5.0, heading)
    p.put("dead_tree", x + dx, z + dz)
    dx, dz = rotate(5.0, -4.0, heading)
    p.put("supply_cart", x + dx, z + dz, rotation=heading + 0.4)


def nearest_bridge(site: Site, x: float, z: float) -> Segment | None:
    best = None
    best_distance = float("inf")
    for deck in site.bridges:
        d = deck.distance(x, z)
        if d < best_distance:
            best_distance = d
            best = deck
    return best


def bridgehead(p: Placer, spec: dict, x: float, z: float) -> None:
    deck = nearest_bridge(p.site, x, z)
    if deck is None:
        p.dropped.append("bridgehead found no bridge")
        return
    tx, tz = deck.tangent()
    nx, nz = -tz, tx
    ends = ((deck.ax, deck.az, -1.0), (deck.bx, deck.bz, 1.0))
    for i, (ex, ez, direction) in enumerate(ends):
        ax, az = tx * direction, tz * direction
        for side in (-1.0, 1.0):
            reach = deck.half + 1.5 + p.rng.uniform(0.0, 1.0)
            p.put(
                "boulder",
                ex + ax * 4.0 + nx * reach * side,
                ez + az * 4.0 + nz * reach * side,
                scale=p.rng.uniform(0.8, 1.3),
                slack=3.0,
            )
        fire_side = 1.0 if i == 0 else -1.0
        reach = deck.half + 4.5
        p.put(
            "firecamp",
            ex + ax * 8.0 + nx * reach * fire_side,
            ez + az * 8.0 + nz * reach * fire_side,
            rotation=0.0,
            slack=3.5,
        )
        p.put(
            "dead_tree" if spec.get("dead_trees", True) else "boulder",
            ex + ax * 11.0 - nx * (deck.half + 4.0) * fire_side,
            ez + az * 11.0 - nz * (deck.half + 4.0) * fire_side,
            slack=3.5,
        )
        if spec.get("cart", True) and i == 0:
            p.put(
                "supply_cart",
                ex + ax * 13.0 + nx * (deck.half + 5.5) * fire_side,
                ez + az * 13.0 + nz * (deck.half + 5.5) * fire_side,
                rotation=math.atan2(az, ax) + p.jitter(0.3),
                slack=3.5,
            )


def road_junctions(site: Site) -> list[tuple[float, float]]:
    """Where one road's end meets another road, or two roads share an endpoint."""
    points: list[tuple[float, float]] = []
    for index, (line, _half) in enumerate(site.road_lines):
        if len(line) < 2:
            continue
        for end in (line[0], line[-1]):
            for other_index, (other, other_half) in enumerate(site.road_lines):
                if other_index == index:
                    continue
                if min(s.distance(*end) for s in segments_of(other, other_half)) <= 4.0:
                    if all(
                        math.hypot(end[0] - q[0], end[1] - q[1]) > 6.0 for q in points
                    ):
                        points.append(end)
                    break
    return points


def junction(p: Placer, spec: dict, x: float, z: float) -> None:
    candidates = road_junctions(p.site)
    if not candidates:
        p.dropped.append("junction found no road junction")
        return
    jx, jz = min(candidates, key=lambda q: math.hypot(q[0] - x, q[1] - z))
    if math.hypot(jx - x, jz - z) > float(spec.get("snap", 25.0)):
        jx, jz = x, z
    road, _ = p.site.nearest_road(jx, jz)
    tx, tz = road.tangent() if road else (1.0, 0.0)
    nx, nz = -tz, tx
    style = str(spec.get("style", "shrine"))
    side = 1.0 if spec.get("side", "left") == "left" else -1.0
    reach = (road.half if road else 1.5) + 4.0
    sx, sz = jx + nx * reach * side, jz + nz * reach * side
    if style in ("milestone", "shrine"):
        p.put(
            "statue",
            sx,
            sz,
            rotation=math.atan2(-nz * side, -nx * side),
            scale=1.0,
            slack=6.0,
        )
    if style == "shrine":
        p.put(
            "firecamp",
            sx + tx * 5.0 + nx * 2.0 * side,
            sz + tz * 5.0 + nz * 2.0 * side,
            rotation=0.0,
        )
        p.put(
            "ruins",
            sx - tx * 6.0 + nx * 3.0 * side,
            sz - tz * 6.0 + nz * 3.0 * side,
            scale=0.8,
        )
    if style == "camp":
        p.put("firecamp", sx, sz, rotation=0.0, slack=6.0)
        p.put(
            "supply_cart",
            sx + tx * 5.0 + nx * 2.0 * side,
            sz + tz * 5.0 + nz * 2.0 * side,
            rotation=math.atan2(tz, tx),
        )
        p.put(
            "weapon_rack",
            sx - tx * 4.0 + nx * 2.0 * side,
            sz - tz * 4.0 + nz * 2.0 * side,
            rotation=math.atan2(tz, tx),
        )
    if spec.get("tree"):
        p.put(str(spec["tree"]), sx + nx * 6.0 * side, sz + nz * 6.0 * side, slack=5.0)


def nearest_gate(site: Site, x: float, z: float) -> dict | None:
    best = None
    best_distance = float("inf")
    for entry in site.definition.get("structures") or []:
        if entry.get("type") != "wall_gate" or "x" not in entry:
            continue
        d = math.hypot(float(entry["x"]) - x, float(entry["z"]) - z)
        if d < best_distance:
            best_distance = d
            best = entry
    return best


def gate_approach(p: Placer, spec: dict, x: float, z: float) -> None:
    gate = nearest_gate(p.site, x, z)
    if gate is None:
        p.dropped.append("gate_approach found no gate")
        return
    gx, gz = float(gate["x"]), float(gate["z"])
    settlement_id = gate.get("settlement")
    centre = None
    for s in p.site.definition.get("settlements") or []:
        if s.get("id") == settlement_id:
            centre = (float(s["x"]), float(s["z"]))
    if centre is None:
        centre = min(
            (
                (float(s["x"]), float(s["z"]))
                for s in p.site.definition.get("settlements") or []
            ),
            key=lambda c: math.hypot(c[0] - gx, c[1] - gz),
            default=(x, z),
        )
    ox, oz = gx - centre[0], gz - centre[1]
    if abs(ox) > abs(oz):
        ox, oz = (1.0 if ox > 0 else -1.0), 0.0
    else:
        ox, oz = 0.0, (1.0 if oz > 0 else -1.0)
    nx, nz = -oz, ox
    road, _ = p.site.nearest_road(gx + ox * 8.0, gz + oz * 8.0)
    half = road.half if road else 1.6
    reach = half + 3.5
    for side in (-1.0, 1.0):
        p.put(
            "firecamp",
            gx + ox * 9.0 + nx * reach * side,
            gz + oz * 9.0 + nz * reach * side,
            rotation=0.0,
            slack=4.0,
        )
    p.put(
        "weapon_rack",
        gx + ox * 13.0 + nx * (reach + 1.0),
        gz + oz * 13.0 + nz * (reach + 1.0),
        rotation=math.atan2(oz, ox),
    )
    p.put(
        "supply_cart",
        gx + ox * 15.0 - nx * (reach + 1.5),
        gz + oz * 15.0 - nz * (reach + 1.5),
        rotation=math.atan2(oz, ox) + 0.3,
    )
    if spec.get("statues", False):
        for side in (-1.0, 1.0):
            p.put(
                "statue",
                gx + ox * 20.0 + nx * (reach + 1.0) * side,
                gz + oz * 20.0 + nz * (reach + 1.0) * side,
                rotation=math.atan2(-nz * side, -nx * side),
                slack=4.0,
            )
    if spec.get("tents", 0):
        for i in range(int(spec["tents"])):
            p.put(
                "tent",
                gx + ox * (18.0 + 6.0 * (i // 2)) - nx * (reach + 6.0 + 5.0 * (i % 2)),
                gz + oz * (18.0 + 6.0 * (i // 2)) - nz * (reach + 6.0 + 5.0 * (i % 2)),
                rotation=math.atan2(-oz, -ox),
            )


def hill_entrances(site: Site) -> list[tuple[float, float, float, float]]:
    out = []
    for feature in site.definition.get("terrain") or []:
        if feature.get("type") != "hill":
            continue
        hx, hz = float(feature["x"]), float(feature["z"])
        for entrance in feature.get("entrances") or []:
            ex, ez = float(entrance["x"]), float(entrance["z"])
            out.append((ex, ez, hx, hz))
    return out


def ramp_mouth(p: Placer, spec: dict, x: float, z: float) -> None:
    """Frame the mouth of the nearest hill ramp where its corridor meets flat ground.

    The engine's ramp corridor runs from the crown out well past the authored
    entrance and flares as it goes, so the mouth is found on the built surface:
    walk out along the ramp axis until the entrance mask ends, then out to each
    side until it ends there too, and stand the stones just beyond that edge."""
    entrances = hill_entrances(p.site)
    if not entrances:
        p.dropped.append("ramp_mouth found no hill entrance")
        return
    ex, ez, hx, hz = min(entrances, key=lambda e: math.hypot(e[0] - x, e[1] - z))
    ax, az = ex - hx, ez - hz
    length = math.hypot(ax, az) or 1.0
    ax, az = ax / length, az / length
    nx, nz = -az, ax
    surface = p.site.surface

    def on_ramp(px: float, pz: float) -> bool:
        return surface is not None and surface.is_hill_entrance(px, pz)

    foot = None
    for along in range(0, 70, 2):
        if not on_ramp(ex + ax * along, ez + az * along):
            foot = float(along)
            break
    if foot is None:
        p.dropped.append(
            f"ramp_mouth at ({ex:.0f}, {ez:.0f}): corridor never reaches flat ground"
        )
        return
    for side in (-1.0, 1.0):
        along = max(foot - 6.0, 2.0)
        reach = None
        for candidate in range(4, 40, 2):
            if not on_ramp(
                ex + ax * along + nx * candidate * side,
                ez + az * along + nz * candidate * side,
            ):
                reach = float(candidate) + 2.5
                break
        if reach is None:
            continue
        for step in range(int(spec.get("pairs", 2))):
            p.put(
                "boulder",
                ex + ax * (along + step * 6.0) + nx * (reach + step * 2.0) * side,
                ez + az * (along + step * 6.0) + nz * (reach + step * 2.0) * side,
                scale=p.rng.uniform(0.9, 1.5),
                slack=5.0,
            )
        if side < 0 and spec.get("dead_tree", True):
            p.put(
                "dead_tree",
                ex + ax * (along - 8.0) + nx * (reach + 4.0) * side,
                ez + az * (along - 8.0) + nz * (reach + 4.0) * side,
                slack=5.0,
            )
        if side > 0 and spec.get("statue", False):
            p.put(
                "statue",
                ex + ax * (foot + 4.0) + nx * (reach + 1.0) * side,
                ez + az * (foot + 4.0) + nz * (reach + 1.0) * side,
                rotation=math.atan2(-az, -ax),
                slack=5.0,
            )
        if side > 0 and spec.get("fire", False):
            p.put(
                "firecamp",
                ex + ax * (foot + 8.0) + nx * (reach + 3.0) * side,
                ez + az * (foot + 8.0) + nz * (reach + 3.0) * side,
                rotation=0.0,
                slack=5.0,
            )


def river_path_between(
    site: Site, start: tuple[float, float], end: tuple[float, float]
) -> tuple[list[tuple[float, float]], float]:
    best = None
    best_score = float("inf")
    for points, width in site.river_lines:
        segs = segments_of(points, width * 0.5)
        if not segs:
            continue
        score = min(s.distance(*start) for s in segs) + min(
            s.distance(*end) for s in segs
        )
        if score < best_score:
            best_score = score
            best = (points, width)
    if best is None:
        return [], 0.0
    points, width = best
    fake = Site(site.definition, None, site.width, site.height)
    fake.road_lines = [(points, width * 0.5)]
    fake.roads = segments_of(points, width * 0.5)
    return road_path_between(fake, start, end), width


def riverbank(p: Placer, spec: dict, x: float, z: float) -> None:
    start = (float(spec["from"][0]), float(spec["from"][1]))
    end = (float(spec["to"][0]), float(spec["to"][1]))
    path, width = river_path_between(p.site, start, end)
    if not path:
        p.dropped.append("riverbank found no river")
        return
    spacing = float(spec.get("spacing", 26.0))
    offset = width * RIVER_DRAWN_HALF + 2.5
    kinds = spec.get("kinds") or ["boulder", "plant", "dead_tree", "plant", "boulder"]
    index = 0
    for px, pz, tx, tz in walk_path(path, spacing):
        nx, nz = -tz, tx
        side = 1.0 if index % 2 == 0 else -1.0
        kind = str(kinds[index % len(kinds)])
        out = offset + p.rng.uniform(0.0, 3.0)
        p.put(
            kind,
            px + nx * out * side + p.jitter(1.5),
            pz + nz * out * side + p.jitter(1.5),
            scale=p.rng.uniform(0.8, 1.3),
            slack=3.5,
        )
        if kind == "plant":
            p.put(
                "plant",
                px + nx * (out + 2.0) * side + tx * 3.0,
                pz + nz * (out + 2.0) * side + tz * 3.0,
                scale=p.rng.uniform(0.9, 1.4),
                slack=2.0,
            )
        index += 1


COMPOSERS: dict[str, Composer] = {
    "barrow_field": barrow_field,
    "hilltop_ruin": hilltop_ruin,
    "sacred_grove": sacred_grove,
    "tree_avenue": tree_avenue,
    "oasis": oasis,
    "scree": scree,
    "boneyard": boneyard,
    "ford_wreck": ford_wreck,
    "raft_camp": raft_camp,
    "cairn": cairn,
    "shore_hamlet": shore_hamlet,
    "sheepfold": sheepfold,
    "bridgehead": bridgehead,
    "junction": junction,
    "gate_approach": gate_approach,
    "ramp_mouth": ramp_mouth,
    "riverbank": riverbank,
}


def strip_generated(entries: Sequence[dict] | None) -> list[dict]:
    return [entry for entry in entries or [] if entry.get(DRESSING_KEY) is None]


def detect_format(source: str, data: dict) -> tuple[int, bool, bool]:
    found = AUDIT.detect_format(source, data)
    return found if found is not None else (4, True, True)


def run(
    map_path: Path,
    write: bool,
    surface_mode: str,
    probe: str | None,
    cache_dir: str | None,
    verbose: bool,
) -> int:
    source = map_path.read_text()
    definition = json.loads(source)
    indent, sort_keys, newline = detect_format(source, definition)
    definition["world_props"] = strip_generated(definition.get("world_props"))
    pieces = definition.get(DRESSING_KEY) or []
    surface = None
    if surface_mode != "off":
        try:
            surface = load_surface(
                map_path, find_probe(probe), Path(cache_dir) if cache_dir else None
            )
        except ProbeUnavailable as error:
            if surface_mode == "require":
                print(f"{map_path.name}: {error}", file=sys.stderr)
                return 2
            print(
                f"{map_path.name}: warning: {error}; slope and ramp checks skipped",
                file=sys.stderr,
            )
    site = Site.build(definition, surface)
    placed_total = 0
    dropped_total = 0
    seen: set[str] = set()
    for spec in pieces:
        piece_id = str(spec.get("id", ""))
        kind = str(spec.get("kind", ""))
        if not piece_id or kind not in COMPOSERS:
            print(
                f"{map_path.name}: piece {piece_id or '?'} has unknown kind {kind!r}",
                file=sys.stderr,
            )
            return 2
        if piece_id in seen:
            print(f"{map_path.name}: duplicate piece id {piece_id}", file=sys.stderr)
            return 2
        seen.add(piece_id)
        seed = int(spec.get("seed", sum(ord(c) for c in piece_id) * 7919))
        placer = Placer(site, piece_id, seed)
        anchor = spec.get("from") or [spec.get("x", 0.0), spec.get("z", 0.0)]
        COMPOSERS[kind](
            placer,
            spec,
            float(spec.get("x", anchor[0])),
            float(spec.get("z", anchor[1])),
        )
        placed_total += len(placer.placed)
        dropped_total += len(placer.dropped)
        definition["world_props"].extend(placer.placed)
        if verbose or placer.dropped:
            print(
                f"  {piece_id} ({kind}): {len(placer.placed)} placed, {len(placer.dropped)} dropped"
            )
            for line in placer.dropped:
                print(f"      dropped {line}")
    print(
        f"{map_path.name}: {len(pieces)} pieces, {placed_total} props placed, {dropped_total} dropped"
    )
    if write:
        text = json.dumps(definition, indent=indent, sort_keys=sort_keys) + (
            "\n" if newline else ""
        )
        map_path.write_text(text)
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("maps", nargs="+", type=Path)
    parser.add_argument(
        "--write", action="store_true", help="Update the map files in place."
    )
    parser.add_argument(
        "--surface",
        choices=("auto", "require", "off"),
        default="auto",
        help="Use tools/terrain_probe for slope and ramp checks.",
    )
    parser.add_argument("--probe", help="Path to the terrain_probe binary.")
    parser.add_argument("--surface-cache", help="Directory for probe dumps.")
    parser.add_argument("--verbose", action="store_true")
    args = parser.parse_args()
    status = 0
    for map_path in args.maps:
        status = max(
            status,
            run(
                map_path,
                args.write,
                args.surface,
                args.probe,
                args.surface_cache,
                args.verbose,
            ),
        )
    return status


if __name__ == "__main__":
    sys.exit(main())
