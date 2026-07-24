#!/usr/bin/env python3
"""Generate and validate campaign rivers and strategically useful lakes.

The tool treats authored water as level-design intent, preserves confluences,
extends loose river ends to a map edge or lake, reroutes around hills and
mountains, and adds deterministic broad meanders. It is dry-run by default.
"""

from __future__ import annotations

import argparse
import heapq
import json
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Sequence

Point = tuple[float, float]

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

STRATEGIC_LAKE_MAPS = {
    "map_crossing_alps.json": (18.0, 11.0),
    "map_campania_campaign.json": (22.0, 14.0),
}

NEIGHBORS = (
    (1, 0, 1.0),
    (1, 1, math.sqrt(2.0)),
    (0, 1, 1.0),
    (-1, 1, math.sqrt(2.0)),
    (-1, 0, 1.0),
    (-1, -1, math.sqrt(2.0)),
    (0, -1, 1.0),
    (1, -1, math.sqrt(2.0)),
)


class WaterGenerationError(RuntimeError):
    pass


@dataclass
class Validation:
    river_count: int
    obstacle_crossings: int
    invalid_endpoints: int
    invalid_lakes: int
    self_crossings: int
    invalid_confluences: int

    @property
    def passed(self) -> bool:
        return (
            self.river_count > 0
            and self.obstacle_crossings == 0
            and self.invalid_endpoints == 0
            and self.invalid_lakes == 0
            and self.self_crossings == 0
            and self.invalid_confluences == 0
        )


def add(a: Point, b: Point) -> Point:
    return a[0] + b[0], a[1] + b[1]


def sub(a: Point, b: Point) -> Point:
    return a[0] - b[0], a[1] - b[1]


def mul(a: Point, amount: float) -> Point:
    return a[0] * amount, a[1] * amount


def dot(a: Point, b: Point) -> float:
    return a[0] * b[0] + a[1] * b[1]


def distance(a: Point, b: Point) -> float:
    return math.hypot(a[0] - b[0], a[1] - b[1])


def normalized(a: Point) -> Point:
    magnitude = math.hypot(a[0], a[1])
    return (0.0, 0.0) if magnitude < 1.0e-8 else (a[0] / magnitude, a[1] / magnitude)


def project_point(point: Point, start: Point, end: Point) -> tuple[float, Point]:
    segment = sub(end, start)
    length_sq = dot(segment, segment)
    if length_sq < 1.0e-8:
        return 0.0, start
    t = max(0.0, min(1.0, dot(sub(point, start), segment) / length_sq))
    return t, add(start, mul(segment, t))


def point_segment_distance(point: Point, start: Point, end: Point) -> float:
    return distance(point, project_point(point, start, end)[1])


def intersection(a: Point, b: Point, c: Point, d: Point) -> Point | None:
    ab, cd = sub(b, a), sub(d, c)
    denominator = ab[0] * cd[1] - ab[1] * cd[0]
    if abs(denominator) < 1.0e-7:
        return None
    offset = sub(c, a)
    t = (offset[0] * cd[1] - offset[1] * cd[0]) / denominator
    u = (offset[0] * ab[1] - offset[1] * ab[0]) / denominator
    return add(a, mul(ab, t)) if 0.0 <= t <= 1.0 and 0.0 <= u <= 1.0 else None


def polyline_length(points: Sequence[Point]) -> float:
    return sum(distance(a, b) for a, b in zip(points, points[1:]))


def deduplicate(points: Sequence[Point], epsilon: float = 0.01) -> list[Point]:
    result: list[Point] = []
    for point in points:
        if not result or distance(result[-1], point) > epsilon:
            result.append(point)
    return result


def feature_points(feature: dict) -> list[Point]:
    raw = feature.get("waypoints") or [feature.get("start"), feature.get("end")]
    result = [tuple(map(float, point[:2])) for point in raw if point is not None]
    if len(result) < 2:
        raise WaterGenerationError("river needs at least two points")
    return deduplicate(result)


class Coordinates:
    def __init__(self, definition: dict):
        grid = definition.get("grid") or {}
        self.width = int(grid.get("width", 0))
        self.height = int(grid.get("height", 0))
        self.tile_size = float(grid.get("tile_size", 1.0))
        self.kind = str(definition.get("coord_system", "grid")).lower()
        if self.width < 3 or self.height < 3 or self.tile_size <= 0.0:
            raise WaterGenerationError("invalid map grid")

    def to_grid(self, point: Sequence[float]) -> Point:
        if self.kind == "grid":
            return float(point[0]), float(point[1])
        return (
            float(point[0]) / self.tile_size + self.width * 0.5 - 0.5,
            float(point[1]) / self.tile_size + self.height * 0.5 - 0.5,
        )

    def from_grid(self, point: Point) -> Point:
        if self.kind == "grid":
            return point
        return (
            (point[0] - (self.width * 0.5 - 0.5)) * self.tile_size,
            (point[1] - (self.height * 0.5 - 0.5)) * self.tile_size,
        )


class WaterField:
    def __init__(self, definition: dict, clearance_world: float):
        self.definition = definition
        self.coords = Coordinates(definition)
        self.width, self.height = self.coords.width, self.coords.height
        self.blocked = bytearray(self.width * self.height)
        self.road_influence = bytearray(self.width * self.height)
        self.clearance = clearance_world / self.coords.tile_size
        self.bridges: list[object] = []
        self._raster_obstacles()
        self._raster_roads()

    def index(self, x: int, z: int) -> int:
        return z * self.width + x

    def in_bounds(self, x: int, z: int) -> bool:
        return 0 <= x < self.width and 0 <= z < self.height

    def passable(self, x: int, z: int) -> bool:
        return self.in_bounds(x, z) and self.blocked[self.index(x, z)] == 0

    def bridge_id(self, x: int, z: int) -> int:
        del x, z
        return -1

    def _raster_ellipse(
        self, center: Point, half_width: float, half_depth: float, rotation: float
    ) -> None:
        half_width += self.clearance
        half_depth += self.clearance
        radius = math.hypot(half_width, half_depth)
        angle = math.radians(rotation)
        cosine, sine = math.cos(angle), math.sin(angle)
        for z in range(
            max(0, int(center[1] - radius) - 1),
            min(self.height, int(center[1] + radius) + 2),
        ):
            for x in range(
                max(0, int(center[0] - radius) - 1),
                min(self.width, int(center[0] + radius) + 2),
            ):
                dx, dz = x - center[0], z - center[1]
                local_x = dx * cosine + dz * sine
                local_z = -dx * sine + dz * cosine
                if (local_x / max(half_width, 0.5)) ** 2 + (
                    local_z / max(half_depth, 0.5)
                ) ** 2 <= 1.0:
                    self.blocked[self.index(x, z)] = 1

    def _raster_capsule(
        self, target: bytearray, start: Point, end: Point, radius: float, value: int
    ) -> None:
        for z in range(
            max(0, int(min(start[1], end[1]) - radius) - 1),
            min(self.height, int(max(start[1], end[1]) + radius) + 2),
        ):
            for x in range(
                max(0, int(min(start[0], end[0]) - radius) - 1),
                min(self.width, int(max(start[0], end[0]) + radius) + 2),
            ):
                if point_segment_distance((x, z), start, end) <= radius:
                    target[self.index(x, z)] = value

    def _raster_roads(self) -> None:

        for road in self.definition.get("roads") or []:
            points = [self.coords.to_grid(point) for point in feature_points(road)]
            radius = float(road.get("width", 3.0)) * 0.5 / self.coords.tile_size + 3.0
            for start, end in zip(points, points[1:]):
                self._raster_capsule(self.road_influence, start, end, radius, 1)
        for bridge in self.definition.get("bridges") or []:
            if not bridge.get("start") or not bridge.get("end"):
                continue
            start = self.coords.to_grid(bridge["start"])
            end = self.coords.to_grid(bridge["end"])
            radius = float(bridge.get("width", 3.0)) * 0.75 / self.coords.tile_size
            self._raster_capsule(self.road_influence, start, end, radius, 0)

    def _raster_obstacles(self) -> None:
        for feature in self.definition.get("terrain") or []:
            kind = str(feature.get("type", "")).lower()
            if kind not in {"hill", "mountain"}:
                continue
            center = self.coords.to_grid((feature.get("x", 0), feature.get("z", 0)))
            radius = float(feature.get("radius", 5.0)) / self.coords.tile_size
            width = (
                float(feature.get("width", radius * self.coords.tile_size * 2.0))
                / self.coords.tile_size
            )
            depth = (
                float(feature.get("depth", radius * self.coords.tile_size * 2.0))
                / self.coords.tile_size
            )
            if kind == "mountain" and "width" not in feature:
                width, depth = radius * 2.68, radius * 1.60
            self._raster_ellipse(
                center, width * 0.5, depth * 0.5, float(feature.get("rotation", 0.0))
            )

    def nearest_passable(self, point: Point, radius: int = 24) -> Point:
        origin = round(point[0]), round(point[1])
        if self.passable(*origin):
            return float(origin[0]), float(origin[1])
        candidates: list[tuple[float, int, int]] = []
        for dz in range(-radius, radius + 1):
            for dx in range(-radius, radius + 1):
                x, z = origin[0] + dx, origin[1] + dz
                if self.passable(x, z):
                    candidates.append((dx * dx + dz * dz, x, z))
        if not candidates:
            raise WaterGenerationError(f"no open water route near {point}")
        _, x, z = min(candidates)
        return float(x), float(z)

    def line_passable(self, start: Point, end: Point) -> bool:
        x, z = int(math.floor(start[0] + 0.5)), int(math.floor(start[1] + 0.5))
        end_x, end_z = int(math.floor(end[0] + 0.5)), int(math.floor(end[1] + 0.5))
        dx, dz = end[0] - start[0], end[1] - start[1]
        step_x = 1 if dx > 0 else -1 if dx < 0 else 0
        step_z = 1 if dz > 0 else -1 if dz < 0 else 0
        delta_x = math.inf if step_x == 0 else 1.0 / abs(dx)
        delta_z = math.inf if step_z == 0 else 1.0 / abs(dz)
        next_x = (
            math.inf
            if step_x == 0
            else (x + (0.5 if step_x > 0 else -0.5) - start[0]) / dx
        )
        next_z = (
            math.inf
            if step_z == 0
            else (z + (0.5 if step_z > 0 else -0.5) - start[1]) / dz
        )
        while True:
            if not self.passable(x, z):
                return False
            if x == end_x and z == end_z:
                return True
            if abs(next_x - next_z) <= 1.0e-10:
                if not self.passable(x + step_x, z) or not self.passable(x, z + step_z):
                    return False
                x, z = x + step_x, z + step_z
                next_x, next_z = next_x + delta_x, next_z + delta_z
            elif next_x < next_z:
                x += step_x
                next_x += delta_x
            else:
                z += step_z
                next_z += delta_z


def guide_distance(point: Point, guide: Sequence[Point]) -> float:
    return min(point_segment_distance(point, a, b) for a, b in zip(guide, guide[1:]))


def route(
    field: WaterField, start: Point, end: Point, guide: Sequence[Point]
) -> list[Point]:
    start = field.nearest_passable(start)
    end = field.nearest_passable(end)
    start_cell, end_cell = (round(start[0]), round(start[1])), (
        round(end[0]),
        round(end[1]),
    )
    initial = (start_cell[0], start_cell[1], 8)
    queue = [(distance(start_cell, end_cell), 0.0, initial)]
    costs = {initial: 0.0}
    parents: dict[tuple[int, int, int], tuple[int, int, int]] = {}
    guide_cache: dict[tuple[int, int], float] = {}
    final = None
    while queue:
        _, cost, state = heapq.heappop(queue)
        if cost > costs.get(state, math.inf) + 1.0e-6:
            continue
        x, z, previous = state
        if (x, z) == end_cell:
            final = state
            break
        for direction, (dx, dz, step) in enumerate(NEIGHBORS):
            nx, nz = x + dx, z + dz
            if not field.passable(nx, nz):
                continue
            if (
                dx
                and dz
                and (not field.passable(x + dx, z) or not field.passable(x, z + dz))
            ):
                continue
            turn = 0
            if previous < 8:
                turn = min(abs(direction - previous), 8 - abs(direction - previous))
            new_cost = cost + step + (0.0, 0.10, 0.32, 0.85, 1.8)[turn]
            guide_key = (nx, nz)
            if guide_key not in guide_cache:
                guide_cache[guide_key] = guide_distance(guide_key, guide)
            new_cost += min(1.0, guide_cache[guide_key] * 0.012)
            new_cost += 18.0 if field.road_influence[field.index(nx, nz)] else 0.0
            candidate = (nx, nz, direction)
            if new_cost >= costs.get(candidate, math.inf):
                continue
            costs[candidate] = new_cost
            parents[candidate] = state
            heapq.heappush(
                queue, (new_cost + distance((nx, nz), end_cell), new_cost, candidate)
            )
    if final is None:
        raise WaterGenerationError(f"no river route from {start} to {end}")
    cells: list[Point] = []
    state = final
    while True:
        cells.append((float(state[0]), float(state[1])))
        if state == initial:
            break
        state = parents[state]
    cells.reverse()
    cells[0], cells[-1] = start, end
    return cells


def simplify(
    field: WaterField, points: Sequence[Point], mandatory: Sequence[Point]
) -> list[Point]:
    fixed = sorted(
        {0, len(points) - 1}
        | {
            min(range(len(points)), key=lambda index: distance(points[index], anchor))
            for anchor in mandatory
        }
    )
    result: list[Point] = []
    for span_index, (first, last) in enumerate(zip(fixed, fixed[1:])):
        cursor = first
        chunk = [points[cursor]]
        while cursor < last:
            candidate = last
            while candidate > cursor + 1 and not field.line_passable(
                points[cursor], points[candidate]
            ):
                candidate -= 1
            chunk.append(points[candidate])
            cursor = candidate
        result.extend(chunk if span_index == 0 else chunk[1:])
    return deduplicate(result)


def meander(
    field: WaterField, points: Sequence[Point], width: float, seed: float
) -> list[Point]:
    result = [points[0]]
    spacing = max(6.0, width * 1.35)
    for segment_index, (start, end) in enumerate(zip(points, points[1:])):
        span = distance(start, end)
        steps = max(1, int(math.ceil(span / spacing)))
        direction = normalized(sub(end, start))
        perpendicular = (-direction[1], direction[0])
        for sample in range(1, steps + 1):
            t = sample / steps
            base = add(start, mul(sub(end, start), t))
            if sample == steps:
                candidate = end
            else:
                phase = seed * 0.173 + segment_index * 1.91
                wave = math.sin(t * math.pi) * (
                    math.sin(t * math.tau * 1.35 + phase) * 0.72
                    + math.sin(t * math.tau * 2.7 - phase * 0.63) * 0.28
                )
                amplitude = min(width * 0.72, span * 0.075)
                candidate = add(base, mul(perpendicular, amplitude * wave))
                for _ in range(5):
                    if field.line_passable(result[-1], candidate):
                        break
                    candidate = add(base, mul(sub(candidate, base), 0.5))
            if not field.line_passable(result[-1], candidate):
                candidate = base
            result.append(candidate)
    return deduplicate(result)


def polyline_location(point: Point, points: Sequence[Point]) -> float:
    best = (math.inf, 0.0)
    travelled = 0.0
    for start, end in zip(points, points[1:]):
        t, projected = project_point(point, start, end)
        best = min(
            best, (distance(point, projected), travelled + distance(start, end) * t)
        )
        travelled += distance(start, end)
    return best[1]


def network_anchors(
    polylines: Sequence[Sequence[Point]], tolerance: float = 2.0
) -> list[list[Point]]:
    anchors = [[points[0], points[-1]] for points in polylines]
    for first in range(len(polylines)):
        for second in range(first + 1, len(polylines)):
            common: list[Point] = []
            for a, b in zip(polylines[first], polylines[first][1:]):
                for c, d in zip(polylines[second], polylines[second][1:]):
                    crossing = intersection(a, b, c, d)
                    if crossing is not None:
                        common.append(crossing)
            for endpoint in (polylines[first][0], polylines[first][-1]):
                projected = min(
                    (
                        project_point(endpoint, c, d)[1]
                        for c, d in zip(polylines[second], polylines[second][1:])
                    ),
                    key=lambda point: distance(endpoint, point),
                )
                if distance(endpoint, projected) <= tolerance:
                    common.append(projected)
            for endpoint in (polylines[second][0], polylines[second][-1]):
                projected = min(
                    (
                        project_point(endpoint, a, b)[1]
                        for a, b in zip(polylines[first], polylines[first][1:])
                    ),
                    key=lambda point: distance(endpoint, point),
                )
                if distance(endpoint, projected) <= tolerance:
                    common.append(projected)
            for point in common:
                anchors[first].append(point)
                anchors[second].append(point)
    return [
        deduplicate(
            sorted(items, key=lambda point: polyline_location(point, polyline)), 0.25
        )
        for items, polyline in zip(anchors, polylines)
    ]


def lake_contains(
    field: WaterField, lake: dict, point: Point, padding: float = 0.0
) -> bool:
    center = field.coords.to_grid((lake.get("x", 0), lake.get("z", 0)))
    width = (
        float(lake.get("width", float(lake.get("radius", 4)) * 2))
        / field.coords.tile_size
        + padding * 2
    )
    depth = (
        float(lake.get("depth", float(lake.get("radius", 4)) * 2))
        / field.coords.tile_size
        + padding * 2
    )
    angle = math.radians(-float(lake.get("rotation", 0)))
    dx, dz = point[0] - center[0], point[1] - center[1]
    x = dx * math.cos(angle) - dz * math.sin(angle)
    z = dx * math.sin(angle) + dz * math.cos(angle)
    normalized_x = x / max(width * 0.5, 0.5)
    normalized_z = z / max(depth * 0.5, 0.5)
    local_angle = math.atan2(normalized_z, normalized_x)
    if field.coords.kind == "grid":
        center_world = (
            (center[0] - (field.width * 0.5 - 0.5)) * field.coords.tile_size,
            (center[1] - (field.height * 0.5 - 0.5)) * field.coords.tile_size,
        )
    else:
        center_world = (float(lake.get("x", 0)), float(lake.get("z", 0)))
    phase = center_world[0] * 0.071 + center_world[1] * 0.113
    boundary_scale = max(
        0.90,
        min(
            1.10,
            1.0
            + math.sin(local_angle * 3.0 + phase) * 0.055
            + math.sin(local_angle * 7.0 - phase * 1.7) * 0.025,
        ),
    )
    return math.hypot(normalized_x, normalized_z) <= boundary_scale


def on_lake_boundary(
    field: WaterField, lake: dict, point: Point, tolerance: float = 2.0
) -> bool:
    return lake_contains(field, lake, point, tolerance) and not lake_contains(
        field, lake, point, -tolerance
    )


def on_boundary(field: WaterField, point: Point, tolerance: float = 1.5) -> bool:
    return (
        min(point[0], point[1], field.width - 1 - point[0], field.height - 1 - point[1])
        <= tolerance
    )


def snap_boundary(field: WaterField, point: Point) -> Point:
    candidates = (
        (0.0, point[1]),
        (field.width - 1.0, point[1]),
        (point[0], 0.0),
        (point[0], field.height - 1.0),
    )
    return min(candidates, key=lambda candidate: distance(point, candidate))


def terminal_joined(
    field: WaterField,
    index: int,
    point: Point,
    polylines: Sequence[Sequence[Point]],
    widths: Sequence[float],
    lakes: Sequence[dict],
    tolerance: float = 1.25,
) -> bool:
    if any(on_lake_boundary(field, lake, point, 2.0) for lake in lakes):
        return True
    return any(
        abs(point_segment_distance(point, start, end) - widths[other] * 0.5)
        <= tolerance
        for other, polyline in enumerate(polylines)
        if other != index
        for start, end in zip(polyline, polyline[1:])
    )


def trim_terminal_to_riverbank(
    index: int,
    endpoint: Point,
    adjacent: Point,
    polylines: Sequence[Sequence[Point]],
    widths: Sequence[float],
) -> Point:
    """Move a centerline confluence back to the receiving river's bank."""
    candidates: list[tuple[float, int]] = []
    for other, polyline in enumerate(polylines):
        if other == index:
            continue
        nearest = min(
            point_segment_distance(endpoint, start, end)
            for start, end in zip(polyline, polyline[1:])
        )
        half_width = widths[other] * 0.5
        if nearest < half_width - 0.25:
            candidates.append((half_width - nearest, other))
    if not candidates:
        return endpoint

    _, receiving = max(candidates)
    receiving_segments = list(zip(polylines[receiving], polylines[receiving][1:]))
    target_distance = widths[receiving] * 0.5
    direction = normalized(sub(adjacent, endpoint))
    if direction == (0.0, 0.0):
        return endpoint

    def bank_distance(point: Point) -> float:
        return min(
            point_segment_distance(point, start, end)
            for start, end in receiving_segments
        )

    low = endpoint
    high = endpoint
    step_size = 0.25
    max_steps = max(16, int(math.ceil((target_distance * 3.0 + 4.0) / step_size)))
    for step in range(1, max_steps + 1):
        high = add(endpoint, mul(direction, step * step_size))
        if bank_distance(high) >= target_distance:
            break
        low = high
    else:
        return endpoint

    for _ in range(20):
        middle = mul(add(low, high), 0.5)
        if bank_distance(middle) < target_distance:
            low = middle
        else:
            high = middle
    return high


def truncate_at_receiving_river(
    index: int,
    points: Sequence[Point],
    polylines: Sequence[Sequence[Point]],
    widths: Sequence[float],
) -> list[Point]:
    """Stop a narrow river at the first wider channel it flows into.

    This also repairs an accidental boundary-to-boundary loop produced when a
    tributary was routed through its receiving river instead of terminating at
    the near bank.
    """
    best: tuple[int, float, Point] | None = None
    for other, receiving in enumerate(polylines):
        if other == index or widths[other] <= widths[index] * 1.20:
            continue
        receiving_segments = list(zip(receiving, receiving[1:]))
        bank_distance = widths[other] * 0.5

        def distance_to_receiving(point: Point) -> float:
            return min(
                point_segment_distance(point, start, end)
                for start, end in receiving_segments
            )

        if distance_to_receiving(points[0]) <= bank_distance + 0.25:
            continue
        for segment_index, (start, end) in enumerate(zip(points, points[1:])):
            start_distance = distance_to_receiving(start)
            end_distance = distance_to_receiving(end)
            if start_distance > bank_distance and end_distance <= bank_distance:
                low, high = start, end
                for _ in range(20):
                    middle = mul(add(low, high), 0.5)
                    if distance_to_receiving(middle) > bank_distance:
                        low = middle
                    else:
                        high = middle
                segment_fraction = distance(start, high) / max(
                    distance(start, end), 1.0e-8
                )
                candidate = (segment_index, segment_fraction, high)
                if best is None or candidate[:2] < best[:2]:
                    best = candidate
                break
    if best is None:
        return list(points)
    segment_index, _, boundary = best
    return deduplicate([*points[: segment_index + 1], boundary])


def snap_terminal_to_nearby_riverbank(
    index: int,
    endpoint: Point,
    adjacent: Point,
    polylines: Sequence[Sequence[Point]],
    widths: Sequence[float],
) -> Point:
    """Re-snap a confluence after the receiving river has been regenerated."""
    candidates: list[tuple[float, Point, int]] = []
    for other, receiving in enumerate(polylines):
        if other == index or widths[other] <= widths[index] * 1.20:
            continue
        for start, end in zip(receiving, receiving[1:]):
            _, projected = project_point(endpoint, start, end)
            candidate_distance = distance(endpoint, projected)
            if candidate_distance <= widths[other] * 0.5 + 6.0:
                candidates.append((candidate_distance, projected, other))
    if not candidates:
        return endpoint
    _, projected, receiving = min(candidates, key=lambda item: item[0])
    bank_distance = widths[receiving] * 0.5
    receiving_segments = list(zip(polylines[receiving], polylines[receiving][1:]))

    def distance_to_receiving(point: Point) -> float:
        return min(
            point_segment_distance(point, start, end)
            for start, end in receiving_segments
        )

    outward = normalized(sub(adjacent, projected))
    if outward == (0.0, 0.0):
        outward = normalized(sub(endpoint, projected))
    if outward == (0.0, 0.0):
        return endpoint
    low = projected
    high = projected
    for step in range(1, max(16, int(math.ceil(bank_distance * 8.0 + 16.0)))):
        high = add(projected, mul(outward, step * 0.25))
        if distance_to_receiving(high) >= bank_distance:
            break
        low = high
    else:
        return endpoint
    for _ in range(20):
        middle = mul(add(low, high), 0.5)
        if distance_to_receiving(middle) < bank_distance:
            low = middle
        else:
            high = middle
    return high


def encode_point(point: Point) -> list[int | float]:
    def number(value: float) -> int | float:
        rounded = round(value, 2)
        return int(rounded) if abs(rounded - round(rounded)) < 1.0e-7 else rounded

    return [number(point[0]), number(point[1])]


def generate_rivers(
    definition: dict, field: WaterField, lakes: Sequence[dict]
) -> list[dict]:
    rivers = definition.get("rivers") or []
    authored = [
        [field.coords.to_grid(point) for point in feature_points(river)]
        for river in rivers
    ]
    widths = [
        float(river.get("width", 3.0)) / field.coords.tile_size for river in rivers
    ]
    authored = [
        truncate_at_receiving_river(index, points, authored, widths)
        for index, points in enumerate(authored)
    ]
    for index, points in enumerate(authored):
        points[0] = trim_terminal_to_riverbank(
            index, points[0], points[1], authored, widths
        )
        points[-1] = trim_terminal_to_riverbank(
            index, points[-1], points[-2], authored, widths
        )
    anchors = network_anchors(authored)
    output: list[dict] = []
    for index, (river, guide, required) in enumerate(zip(rivers, authored, anchors)):
        required = list(required)
        if not terminal_joined(field, index, required[0], authored, widths, lakes):
            required[0] = snap_boundary(field, required[0])
        if not terminal_joined(field, index, required[-1], authored, widths, lakes):
            required[-1] = snap_boundary(field, required[-1])
        required = [field.nearest_passable(point) for point in required]
        routed = [required[0]]
        for start, end in zip(required, required[1:]):
            routed.extend(route(field, start, end, guide)[1:])
        routed = simplify(field, routed, required)
        base_route = routed
        width_grid = float(river.get("width", 3.0)) / field.coords.tile_size
        meandered = meander(
            field, routed, width_grid, seed=(index + 1) * 17.0 + field.width
        )
        if all(field.line_passable(a, b) for a, b in zip(meandered, meandered[1:])):
            routed = meandered
        if not all(field.line_passable(a, b) for a, b in zip(routed, routed[1:])):
            raise WaterGenerationError(
                f"river {index + 1} smoothing left protected terrain"
            )
        coordinates = [encode_point(field.coords.from_grid(point)) for point in routed]
        encoded_grid = [field.coords.to_grid(point) for point in coordinates]
        if not all(
            field.line_passable(a, b) for a, b in zip(encoded_grid, encoded_grid[1:])
        ):
            routed = base_route
            coordinates = [
                encode_point(field.coords.from_grid(point)) for point in routed
            ]
        item = dict(river)
        item["start"], item["end"], item["waypoints"] = (
            coordinates[0],
            coordinates[-1],
            coordinates,
        )
        output.append(item)
        print(
            f"  river {index + 1:02d}: {polyline_length(guide):6.1f} -> {polyline_length(routed):6.1f}, {len(routed):2d} points"
        )

    final_polylines = [
        [field.coords.to_grid(point) for point in feature_points(river)]
        for river in output
    ]
    for index, (item, points) in enumerate(zip(output, final_polylines)):
        if not on_boundary(field, points[0]):
            points[0] = snap_terminal_to_nearby_riverbank(
                index, points[0], points[1], final_polylines, widths
            )
        if not on_boundary(field, points[-1]):
            points[-1] = snap_terminal_to_nearby_riverbank(
                index, points[-1], points[-2], final_polylines, widths
            )
        coordinates = [encode_point(field.coords.from_grid(point)) for point in points]
        item["start"], item["end"], item["waypoints"] = (
            coordinates[0],
            coordinates[-1],
            coordinates,
        )
    return output


def nearest_road_distance(definition: dict, point: Point) -> float:
    return min(
        point_segment_distance(point, a, b)
        for road in definition.get("roads") or []
        for a, b in zip(feature_points(road), feature_points(road)[1:])
    )


def add_strategic_lake(
    path: Path,
    definition: dict,
    field: WaterField,
    rivers: Sequence[dict],
    lakes: list[dict],
) -> None:
    if path.name not in STRATEGIC_LAKE_MAPS:
        return
    width, depth = STRATEGIC_LAKE_MAPS[path.name]

    lakes[:] = [
        lake
        for lake in lakes
        if abs(float(lake.get("width", 0.0)) - width) > 0.01
        or abs(float(lake.get("depth", 0.0)) - depth) > 0.01
    ]
    if lakes:
        return
    candidates: list[tuple[float, Point, float]] = []
    bridge_centers = [
        mul(
            add(
                field.coords.to_grid(bridge["start"]),
                field.coords.to_grid(bridge["end"]),
            ),
            0.5,
        )
        for bridge in definition.get("bridges") or []
        if bridge.get("start") and bridge.get("end")
    ]
    for river in rivers:
        points = [field.coords.to_grid(point) for point in feature_points(river)]
        for start, end in zip(points, points[1:]):
            span = distance(start, end)
            for sample in range(1, max(2, int(span // 8))):
                point = add(
                    start, mul(sub(end, start), sample / max(2, int(span // 8)))
                )
                if on_boundary(field, point, 18.0) or any(
                    distance(point, bridge) < 20.0 for bridge in bridge_centers
                ):
                    continue
                road_distance = nearest_road_distance(definition, point)
                safe_road_distance = max(width, depth) * 0.5 + 5.0
                if not safe_road_distance <= road_distance <= safe_road_distance + 25.0:
                    continue
                radius = math.hypot(width, depth) * 0.5 / field.coords.tile_size
                if any(
                    not field.passable(
                        round(point[0] + math.cos(angle) * radius),
                        round(point[1] + math.sin(angle) * radius),
                    )
                    for angle in (0, math.pi * 0.5, math.pi, math.pi * 1.5)
                ):
                    continue
                direction = normalized(sub(end, start))
                rotation = math.degrees(math.atan2(direction[1], direction[0]))
                candidates.append(
                    (abs(road_distance - (safe_road_distance + 7.0)), point, rotation)
                )
    if not candidates:
        print("  strategic lake: no safe tactical site found")
        return
    _, center, rotation = min(candidates)
    world = field.coords.from_grid(center)
    lakes.append(
        {
            "x": encode_point(world)[0],
            "z": encode_point(world)[1],
            "width": width,
            "depth": depth,
            "rotation": round(rotation, 1),
        }
    )
    print(f"  strategic lake: added at {encode_point(world)}")


def validate(
    definition: dict, field: WaterField, rivers: Sequence[dict], lakes: Sequence[dict]
) -> Validation:
    polylines = [
        [field.coords.to_grid(point) for point in feature_points(river)]
        for river in rivers
    ]
    widths = [
        float(river.get("width", 3.0)) / field.coords.tile_size for river in rivers
    ]
    obstacle_crossings = sum(
        not field.line_passable(start, end)
        for points in polylines
        for start, end in zip(points, points[1:])
    )
    invalid_endpoints = 0
    invalid_confluences = 0
    for index, points in enumerate(polylines):
        for endpoint in (points[0], points[-1]):
            embedded = any(
                point_segment_distance(endpoint, start, end)
                < widths[other] * 0.5 - 0.25
                for other, polyline in enumerate(polylines)
                if other != index
                for start, end in zip(polyline, polyline[1:])
            )
            if embedded:
                invalid_confluences += 1
            if not on_boundary(field, endpoint) and not terminal_joined(
                field, index, endpoint, polylines, widths, lakes
            ):
                invalid_endpoints += 1
    self_crossings = 0
    for points in polylines:
        segments = list(zip(points, points[1:]))
        for first in range(len(segments)):
            for second in range(first + 2, len(segments)):
                if second == first + 1:
                    continue
                if intersection(*segments[first], *segments[second]) is not None:
                    self_crossings += 1
    invalid_lakes = 0
    roads = definition.get("roads") or []
    for lake in lakes:
        center = field.coords.to_grid((lake.get("x", 0), lake.get("z", 0)))
        width = (
            float(lake.get("width", float(lake.get("radius", 4)) * 2))
            / field.coords.tile_size
        )
        depth = (
            float(lake.get("depth", float(lake.get("radius", 4)) * 2))
            / field.coords.tile_size
        )
        lake_samples = [
            (
                round(center[0] + math.cos(angle) * width * 0.5),
                round(center[1] + math.sin(angle) * depth * 0.5),
            )
            for angle in (0, math.pi * 0.5, math.pi, math.pi * 1.5)
        ]
        if any(
            field.in_bounds(x, z) and not field.passable(x, z) for x, z in lake_samples
        ):
            invalid_lakes += 1
            continue
        river_link = any(
            point_segment_distance(center, start, end) <= min(width, depth) * 0.45
            for points in polylines
            for start, end in zip(points, points[1:])
        )
        road_link = any(
            point_segment_distance(
                center, field.coords.to_grid(a), field.coords.to_grid(b)
            )
            <= max(width, depth) * 0.5 + 4.0
            for road in roads
            for a, b in zip(feature_points(road), feature_points(road)[1:])
        )
        if (
            not river_link
            and not road_link
            and not on_boundary(field, center, max(width, depth) * 0.5)
        ):
            invalid_lakes += 1
    return Validation(
        len(rivers),
        obstacle_crossings,
        invalid_endpoints,
        invalid_lakes,
        self_crossings,
        invalid_confluences,
    )


def replace_array(source: str, key: str, value: list[dict]) -> str:
    match = re.search(rf'(?m)^  "{re.escape(key)}"\s*:\s*', source)
    rendered = json.dumps(value, indent=2, ensure_ascii=False).replace("\n", "\n  ")
    if not match:
        closing = source.rfind("}")
        before = source[:closing].rstrip()
        return (
            before
            + ("\n" if before.endswith("{") else ",\n")
            + f'  "{key}": {rendered}\n'
            + "}"
            + source[closing + 1 :]
        )
    start = source.find("[", match.end())
    depth, in_string, escaped = 0, False, False
    for index in range(start, len(source)):
        char = source[index]
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
        elif char == '"':
            in_string = True
        elif char == "[":
            depth += 1
        elif char == "]":
            depth -= 1
            if depth == 0:
                return source[:start] + rendered + source[index + 1 :]
    raise WaterGenerationError(f"unterminated {key} array")


def process(
    path: Path,
    write: bool,
    validate_only: bool,
    strategic_lakes: bool,
    clearance: float,
) -> bool:
    source = path.read_text(encoding="utf-8")
    definition = json.loads(source)
    rivers = definition.get("rivers") or []
    if not rivers:
        raise WaterGenerationError("map has no authored river intent")
    max_half_width = max(float(river.get("width", 3.0)) * 0.5 for river in rivers)
    field = WaterField(definition, clearance + max_half_width)
    lakes = list(definition.get("lakes") or [])
    if validate_only:
        result = validate(definition, field, rivers, lakes)
    else:
        rivers = generate_rivers(definition, field, lakes)
        if strategic_lakes:
            add_strategic_lake(path, definition, field, rivers, lakes)
        result = validate(definition, field, rivers, lakes)
    status = "PASS" if result.passed else "FAIL"
    print(
        f"{path} [{status}]: rivers={result.river_count}, "
        f"obstacles={result.obstacle_crossings}, "
        f"endpoints={result.invalid_endpoints}, "
        f"lake_issues={result.invalid_lakes}, "
        f"self_crossings={result.self_crossings}, "
        f"embedded_confluences={result.invalid_confluences}"
    )
    if not result.passed:
        return False
    if write and not validate_only:
        updated = replace_array(source, "rivers", rivers)
        updated = replace_array(updated, "lakes", lakes)
        path.write_text(updated, encoding="utf-8")
        print(f"  wrote {path}")
    return True


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("maps", nargs="*", type=Path)
    parser.add_argument("--campaign", action="store_true")
    parser.add_argument("--write", action="store_true")
    parser.add_argument("--validate-only", action="store_true")
    parser.add_argument(
        "--strategic-lakes",
        action="store_true",
        help="add lakes only on campaign maps with an explicit historical/geographic policy",
    )
    parser.add_argument("--clearance", type=float, default=1.25)
    args = parser.parse_args(argv)
    if not args.campaign and not args.maps:
        parser.error("provide maps or --campaign")
    if args.write and args.validate_only:
        parser.error("--write and --validate-only cannot be combined")
    return args


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(sys.argv[1:] if argv is None else argv)
    root = Path(__file__).resolve().parents[1]
    paths = list(args.maps)
    if args.campaign:
        paths.extend(root / path for path in CAMPAIGN_MAPS)
    failures = 0
    for path in dict.fromkeys(path.resolve() for path in paths):
        try:
            if not process(
                path,
                args.write,
                args.validate_only,
                args.strategic_lakes,
                args.clearance,
            ):
                failures += 1
        except (OSError, json.JSONDecodeError, WaterGenerationError) as error:
            print(f"{path}: ERROR: {error}", file=sys.stderr)
            failures += 1
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
