"""Plane geometry, the layout frame and the occupancy field the stages share.

Every synthesiser asks the same question - is this ground free of water, high
ground, woods, buildings and roads? - so the answer comes from one raster of
whatever is already in the working map. Locked stages and authored elements
are in that map before any stage synthesises, which is how a rerolled hill
keeps off a locked river.
"""

from __future__ import annotations

import math
import sys
from pathlib import Path
from typing import Iterable, Sequence

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from map_hill_shapes import hill_shape_strokes
from map_water_geometry import river_points

Point = tuple[float, float]

WATER = 1
HILL = 2
MOUNTAIN = 4
FOREST = 8
SETTLEMENT = 16
ROAD = 32
LANDMARK = 64
RESERVED = 128

SOLID = WATER | HILL | MOUNTAIN | FOREST | SETTLEMENT | ROAD | LANDMARK
GROUND = WATER | HILL | MOUNTAIN

TIER_RADIUS = {"town": 46.0, "fortified_camp": 30.0, "marching_camp": 24.0}


def dist(a: Point, b: Point) -> float:
    return math.hypot(a[0] - b[0], a[1] - b[1])


def lerp(a: Point, b: Point, t: float) -> Point:
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)


def point_segment_distance(p: Point, a: Point, b: Point) -> float:
    return dist(p, project_on_segment(p, a, b))


def project_on_segment(p: Point, a: Point, b: Point) -> Point:
    dx, dz = b[0] - a[0], b[1] - a[1]
    length_sq = dx * dx + dz * dz
    if length_sq <= 1e-12:
        return a
    t = ((p[0] - a[0]) * dx + (p[1] - a[1]) * dz) / length_sq
    t = min(max(t, 0.0), 1.0)
    return (a[0] + dx * t, a[1] + dz * t)


def polyline_distance(p: Point, points: Sequence[Point]) -> float:
    if not points:
        return math.inf
    if len(points) == 1:
        return dist(p, points[0])
    return min(
        point_segment_distance(p, a, b)
        for a, b in zip(points, points[1:], strict=False)
    )


def nearest_on_polyline(p: Point, points: Sequence[Point]) -> Point:
    best, best_d = points[0], math.inf
    for a, b in zip(points, points[1:], strict=False):
        q = project_on_segment(p, a, b)
        d = dist(p, q)
        if d < best_d:
            best, best_d = q, d
    return best


def polyline_length(points: Sequence[Point]) -> float:
    return sum(dist(a, b) for a, b in zip(points, points[1:], strict=False))


def sample_polyline(points: Sequence[Point], spacing: float) -> list[Point]:
    out: list[Point] = [points[0]]
    carry = 0.0
    for a, b in zip(points, points[1:], strict=False):
        length = dist(a, b)
        t = spacing - carry
        while t <= length:
            out.append(lerp(a, b, t / length))
            t += spacing
        carry = length - (t - spacing)
    if dist(out[-1], points[-1]) > 1e-6:
        out.append(points[-1])
    return out


def segments_intersect(a: Point, b: Point, c: Point, d: Point) -> Point | None:
    r = (b[0] - a[0], b[1] - a[1])
    s = (d[0] - c[0], d[1] - c[1])
    denom = r[0] * s[1] - r[1] * s[0]
    if abs(denom) < 1e-12:
        return None
    t = ((c[0] - a[0]) * s[1] - (c[1] - a[1]) * s[0]) / denom
    u = ((c[0] - a[0]) * r[1] - (c[1] - a[1]) * r[0]) / denom
    if 0.0 <= t <= 1.0 and 0.0 <= u <= 1.0:
        return (a[0] + r[0] * t, a[1] + r[1] * t)
    return None


def polyline_crossings(a: Sequence[Point], b: Sequence[Point]) -> list[Point]:
    hits = []
    for p, q in zip(a, a[1:], strict=False):
        for r, s in zip(b, b[1:], strict=False):
            hit = segments_intersect(p, q, r, s)
            if hit is not None:
                hits.append(hit)
    return hits


def rotate(x: float, z: float, degrees: float) -> Point:
    angle = math.radians(degrees)
    c, s = math.cos(angle), math.sin(angle)
    return (x * c - z * s, x * s + z * c)


def round2(value: float) -> float:
    return round(float(value), 2)


def pt(p: Point) -> list[float]:
    return [round2(p[0]), round2(p[1])]


class Frame:
    """Maps layout coordinates onto the map.

    ``u`` runs from the player's edge (0) to the enemy's (1), ``v`` across the
    field. Synthesis happens in (u, v) so one layout routine serves all four
    orientations.
    """

    def __init__(self, width: float, height: float, orientation: int) -> None:
        self.width = float(width)
        self.height = float(height)
        self.orientation = orientation % 4

    def to_world(self, u: float, v: float) -> Point:
        w, h = self.width - 1.0, self.height - 1.0
        if self.orientation == 0:
            return (v * w, u * h)
        if self.orientation == 1:
            return (v * w, (1.0 - u) * h)
        if self.orientation == 2:
            return (u * w, v * h)
        return ((1.0 - u) * w, v * h)

    def to_layout(self, x: float, z: float) -> Point:
        w, h = self.width - 1.0, self.height - 1.0
        if self.orientation == 0:
            return (z / h, x / w)
        if self.orientation == 1:
            return (1.0 - z / h, x / w)
        if self.orientation == 2:
            return (x / w, z / h)
        return (1.0 - x / w, z / h)

    @property
    def u_length(self) -> float:
        return self.height if self.orientation in (0, 1) else self.width

    @property
    def v_length(self) -> float:
        return self.width if self.orientation in (0, 1) else self.height

    def u_axis_degrees(self) -> float:
        """World yaw (degrees, rotation convention of the map) of the +u axis."""
        a = self.to_world(0.0, 0.5)
        b = self.to_world(1.0, 0.5)
        return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))

    def facing_towards_u(self, sign: float) -> str:
        """Compass facing of +u (sign>0) or -u, in the map's north/south terms."""
        a = self.to_world(0.5, 0.5)
        b = self.to_world(0.5 + 0.1 * sign, 0.5)
        return facing_of(b[0] - a[0], b[1] - a[1])


def facing_of(dx: float, dz: float) -> str:
    """The settlement tools' compass: north is -z, east is +x."""
    if abs(dx) >= abs(dz):
        return "east" if dx > 0 else "west"
    return "south" if dz > 0 else "north"


class Field:
    """A coarse occupancy raster with one bit per kind of obstacle."""

    CELL = 2.0

    def __init__(self, width: float, height: float) -> None:
        self.width = float(width)
        self.height = float(height)
        self.cols = int(math.ceil(self.width / self.CELL)) + 1
        self.rows = int(math.ceil(self.height / self.CELL)) + 1
        self.cells = bytearray(self.cols * self.rows)

    def _bbox(self, x0: float, z0: float, x1: float, z1: float):
        c0 = max(0, int(math.floor(x0 / self.CELL)))
        c1 = min(self.cols - 1, int(math.ceil(x1 / self.CELL)))
        r0 = max(0, int(math.floor(z0 / self.CELL)))
        r1 = min(self.rows - 1, int(math.ceil(z1 / self.CELL)))
        return c0, c1, r0, r1

    def mark_disc(self, x: float, z: float, r: float, bit: int) -> None:
        c0, c1, r0, r1 = self._bbox(x - r, z - r, x + r, z + r)
        r_sq = r * r
        for row in range(r0, r1 + 1):
            dz = row * self.CELL - z
            base = row * self.cols
            for col in range(c0, c1 + 1):
                dx = col * self.CELL - x
                if dx * dx + dz * dz <= r_sq:
                    self.cells[base + col] |= bit

    def mark_ellipse(
        self,
        x: float,
        z: float,
        hw: float,
        hd: float,
        rotation: float,
        bit: int,
        pad: float = 0.0,
    ) -> None:
        hw, hd = hw + pad, hd + pad
        reach = max(hw, hd)
        c0, c1, r0, r1 = self._bbox(x - reach, z - reach, x + reach, z + reach)
        angle = math.radians(rotation)
        c, s = math.cos(angle), math.sin(angle)
        for row in range(r0, r1 + 1):
            dz = row * self.CELL - z
            base = row * self.cols
            for col in range(c0, c1 + 1):
                dx = col * self.CELL - x
                lx = dx * c + dz * s
                lz = -dx * s + dz * c
                if (lx / hw) ** 2 + (lz / hd) ** 2 <= 1.0:
                    self.cells[base + col] |= bit

    def mark_capsule(self, a: Point, b: Point, r: float, bit: int) -> None:
        c0, c1, r0, r1 = self._bbox(
            min(a[0], b[0]) - r,
            min(a[1], b[1]) - r,
            max(a[0], b[0]) + r,
            max(a[1], b[1]) + r,
        )
        for row in range(r0, r1 + 1):
            base = row * self.cols
            for col in range(c0, c1 + 1):
                if (
                    point_segment_distance((col * self.CELL, row * self.CELL), a, b)
                    <= r
                ):
                    self.cells[base + col] |= bit

    def mark_polyline(self, points: Sequence[Point], r: float, bit: int) -> None:
        for a, b in zip(points, points[1:], strict=False):
            self.mark_capsule(a, b, r, bit)
        if len(points) == 1:
            self.mark_disc(points[0][0], points[0][1], r, bit)

    def mark_box(self, x: float, z: float, hw: float, hd: float, bit: int) -> None:
        c0, c1, r0, r1 = self._bbox(x - hw, z - hd, x + hw, z + hd)
        for row in range(r0, r1 + 1):
            base = row * self.cols
            for col in range(c0, c1 + 1):
                self.cells[base + col] |= bit

    def blocked(self, x: float, z: float, r: float, mask: int) -> bool:
        if x - r < 0 or z - r < 0 or x + r > self.width - 1 or z + r > self.height - 1:
            return True
        c0, c1, r0, r1 = self._bbox(x - r, z - r, x + r, z + r)
        r_sq = r * r
        cells = self.cells
        for row in range(r0, r1 + 1):
            dz = row * self.CELL - z
            base = row * self.cols
            for col in range(c0, c1 + 1):
                if cells[base + col] & mask:
                    dx = col * self.CELL - x
                    if dx * dx + dz * dz <= r_sq:
                        return True
        return False

    def at(self, x: float, z: float) -> int:
        col = min(max(int(round(x / self.CELL)), 0), self.cols - 1)
        row = min(max(int(round(z / self.CELL)), 0), self.rows - 1)
        return self.cells[row * self.cols + col]

    def fraction(self, mask: int) -> float:
        hit = sum(1 for value in self.cells if value & mask)
        return hit / len(self.cells)


class Collector(Field):
    """A field that records which cells a shape covers instead of marking them."""

    def __init__(self, width: float, height: float) -> None:
        super().__init__(width, height)
        from collections import defaultdict

        self.cells = defaultdict(int)

    def touched(self) -> list[int]:
        return [index for index, value in self.cells.items() if value]


def footprint_hits(field: Field, entry: dict, pad: float, mask: int) -> bool:
    """Whether a terrain entry, padded, overlaps anything in ``mask``."""
    probe = Collector(field.width, field.height)
    mark_terrain(probe, entry, pad)
    cells = field.cells
    return any(cells[index] & mask for index in probe.touched())


def feature_points(entry: dict) -> list[Point]:
    """Polyline of a river, road or bridge entry (ring rivers expanded)."""
    if entry.get("shape") == "ring":
        return [(float(x), float(z)) for x, z in river_points(entry)]
    points = entry.get("waypoints")
    if points:
        return [(float(p[0]), float(p[1])) for p in points]
    start, end = entry.get("start"), entry.get("end")
    if start and end:
        return [(float(start[0]), float(start[1])), (float(end[0]), float(end[1]))]
    return []


def hill_half_extents(entry: dict) -> Point:
    radius = float(entry.get("radius", 0.0) or 0.0)
    width = float(entry.get("width", 0.0) or 0.0) or radius * 2.0
    depth = float(entry.get("depth", 0.0) or 0.0) or radius * 2.0
    return max(width * 0.5, 1.0), max(depth * 0.5, 1.0)


def mark_terrain(field: Field, entry: dict, pad: float = 0.0) -> None:
    kind = str(entry.get("type", "hill")).lower()
    bit = MOUNTAIN if kind == "mountain" else HILL
    if kind == "lake":
        bit = WATER
    strokes, half_thickness = hill_shape_strokes(entry)
    if strokes:
        for a, b in strokes:
            field.mark_capsule(a, b, half_thickness + pad, bit)
        return
    hw, hd = hill_half_extents(entry)
    field.mark_ellipse(
        float(entry.get("x", 0.0)),
        float(entry.get("z", 0.0)),
        hw,
        hd,
        float(entry.get("rotation", 0.0)),
        bit,
        pad,
    )


def settlement_radius(entry: dict) -> float:
    options = entry.get("plan_options") or {}
    size = options.get("size")
    base = TIER_RADIUS.get(str(entry.get("tier")), 30.0)
    if isinstance(size, list) and len(size) >= 2:
        base = max(base, max(float(size[0]), float(size[1])) * 0.9)
    return base


def build_field(definition: dict, width: float, height: float) -> Field:
    field = Field(width, height)
    for entry in definition.get("terrain") or []:
        mark_terrain(field, entry)
    for entry in definition.get("rivers") or []:
        points = feature_points(entry)
        if points:
            field.mark_polyline(
                points, float(entry.get("width", 8.0)) * 0.5 + 2.0, WATER
            )
    for entry in definition.get("lakes") or []:
        field.mark_ellipse(
            float(entry.get("x", 0.0)),
            float(entry.get("z", 0.0)),
            float(entry.get("width", 20.0)) * 0.5,
            float(entry.get("depth", 20.0)) * 0.5,
            float(entry.get("rotation", 0.0)),
            WATER,
            2.0,
        )
    for entry in definition.get("forests") or []:
        field.mark_disc(
            float(entry.get("x", 0.0)),
            float(entry.get("z", 0.0)),
            float(entry.get("radius", 12.0)),
            FOREST,
        )
    for entry in definition.get("roads") or []:
        points = feature_points(entry)
        if points:
            field.mark_polyline(
                points, float(entry.get("width", 3.0)) * 0.5 + 1.5, ROAD
            )
    for entry in definition.get("bridges") or []:
        points = feature_points(entry)
        if points:
            field.mark_polyline(
                points, float(entry.get("width", 4.0)) * 0.5 + 1.5, ROAD
            )
    for entry in definition.get("settlements") or []:
        field.mark_disc(
            float(entry.get("x", 0.0)),
            float(entry.get("z", 0.0)),
            settlement_radius(entry),
            SETTLEMENT,
        )
    for entry in definition.get("structures") or []:
        field.mark_disc(
            float(entry.get("x", 0.0)), float(entry.get("z", 0.0)), 5.0, SETTLEMENT
        )
    for entry in definition.get("landmarks") or []:
        field.mark_disc(
            float(entry.get("x", 0.0)),
            float(entry.get("z", 0.0)),
            16.0 * float(entry.get("scale", 1.0)),
            LANDMARK,
        )
    for entry in definition.get("world_props") or []:
        field.mark_disc(
            float(entry.get("x", 0.0)), float(entry.get("z", 0.0)), 2.0, LANDMARK
        )
    return field


def spiral(origin: Point, step: float, rings: int) -> Iterable[Point]:
    """Candidate points around ``origin``, nearest first."""
    yield origin
    for ring in range(1, rings + 1):
        radius = ring * step
        count = max(6, int(2.0 * math.pi * radius / step))
        for i in range(count):
            angle = 2.0 * math.pi * i / count + ring * 0.37
            yield (
                origin[0] + math.cos(angle) * radius,
                origin[1] + math.sin(angle) * radius,
            )
