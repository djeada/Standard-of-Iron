#!/usr/bin/env python3
"""Generate clean, connected road geometry for Standard of Iron map JSON.

Existing road endpoints describe the intended network. The generator reroutes
each connection with deterministic A* pathfinding while treating hills,
mountains, lakes, and rivers as hard obstacles. River crossings are opened only
along authored bridge decks, and generated waypoints include the exact bridge
endpoints so the road approaches the bridge at the correct angle.

The command is dry-run by default. Pass --write to replace only the top-level
``roads`` array in each JSON document; all other source formatting is retained.
"""

from __future__ import annotations

import argparse
from collections import deque
import heapq
import json
import math
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Iterator, Sequence


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

NEIGHBORS: tuple[tuple[int, int, float], ...] = (
    (1, 0, 1.0),
    (1, 1, math.sqrt(2.0)),
    (0, 1, 1.0),
    (-1, 1, math.sqrt(2.0)),
    (-1, 0, 1.0),
    (-1, -1, math.sqrt(2.0)),
    (0, -1, 1.0),
    (1, -1, math.sqrt(2.0)),
)


class RoadGenerationError(RuntimeError):
    """Raised when a map cannot be routed without violating its terrain."""


@dataclass(frozen=True)
class Bridge:
    start: Point
    end: Point
    width: float

    @property
    def direction(self) -> Point:
        return normalized(sub(self.end, self.start))


@dataclass
class RouteResult:
    road: dict
    old_length: float
    new_length: float
    bridge_ids: tuple[int, ...]


@dataclass
class ValidationResult:
    road_count: int
    component_count: int
    obstacle_violations: int
    invalid_bridge_crossings: int
    dangling_endpoints: int
    invalid_bridge_spans: int
    orphan_bridge_ends: int
    missing_bridge_approaches: int
    road_style_count: int
    boundary_sides: int

    @property
    def passed(self) -> bool:
        return (
            self.road_count > 0
            and self.component_count <= 1
            and self.obstacle_violations == 0
            and self.invalid_bridge_crossings == 0
            and self.dangling_endpoints == 0
            and self.invalid_bridge_spans == 0
            and self.orphan_bridge_ends == 0
            and self.missing_bridge_approaches == 0
            and self.road_style_count <= 1
            and self.boundary_sides >= 2
        )


def add(a: Point, b: Point) -> Point:
    return a[0] + b[0], a[1] + b[1]


def sub(a: Point, b: Point) -> Point:
    return a[0] - b[0], a[1] - b[1]


def mul(a: Point, amount: float) -> Point:
    return a[0] * amount, a[1] * amount


def length(a: Point) -> float:
    return math.hypot(a[0], a[1])


def distance(a: Point, b: Point) -> float:
    return length(sub(a, b))


def normalized(a: Point) -> Point:
    magnitude = length(a)
    return (0.0, 0.0) if magnitude <= 1.0e-8 else mul(a, 1.0 / magnitude)


def dot(a: Point, b: Point) -> float:
    return a[0] * b[0] + a[1] * b[1]


def point_segment_distance(point: Point, start: Point, end: Point) -> float:
    segment = sub(end, start)
    segment_length_sq = dot(segment, segment)
    if segment_length_sq <= 1.0e-8:
        return distance(point, start)
    t = max(0.0, min(1.0, dot(sub(point, start), segment) / segment_length_sq))
    return distance(point, add(start, mul(segment, t)))


def project_point_to_segment(
    point: Point, start: Point, end: Point
) -> tuple[float, Point]:
    segment = sub(end, start)
    segment_length_sq = dot(segment, segment)
    if segment_length_sq <= 1.0e-8:
        return 0.0, start
    t = max(0.0, min(1.0, dot(sub(point, start), segment) / segment_length_sq))
    return t, add(start, mul(segment, t))


def segment_intersection_point(a: Point, b: Point, c: Point, d: Point) -> Point | None:
    ab = sub(b, a)
    cd = sub(d, c)
    denominator = ab[0] * cd[1] - ab[1] * cd[0]
    if abs(denominator) <= 1.0e-7:
        return None
    offset = sub(c, a)
    t = (offset[0] * cd[1] - offset[1] * cd[0]) / denominator
    u = (offset[0] * ab[1] - offset[1] * ab[0]) / denominator
    if 0.0 <= t <= 1.0 and 0.0 <= u <= 1.0:
        return add(a, mul(ab, t))
    return None


def polyline_length(points: Sequence[Point]) -> float:
    return sum(distance(a, b) for a, b in zip(points, points[1:]))


def road_points(road: dict) -> list[Point]:
    raw = road.get("waypoints") or [road.get("start"), road.get("end")]
    points = [tuple(map(float, point[:2])) for point in raw if point is not None]
    if len(points) < 2:
        raise RoadGenerationError("road must contain start/end or two waypoints")
    return deduplicate(points)


def deduplicate(points: Sequence[Point], epsilon: float = 1.0e-4) -> list[Point]:
    result: list[Point] = []
    for point in points:
        if not result or distance(result[-1], point) > epsilon:
            result.append(point)
    return result


def polyline_location(point: Point, polyline: Sequence[Point]) -> float:
    """Return the distance along a polyline nearest to point."""
    best_distance = math.inf
    best_location = 0.0
    traversed = 0.0
    for start, end in zip(polyline, polyline[1:]):
        t, projected = project_point_to_segment(point, start, end)
        candidate_distance = distance(point, projected)
        if candidate_distance < best_distance:
            best_distance = candidate_distance
            best_location = traversed + distance(start, end) * t
        traversed += distance(start, end)
    return best_location


def network_anchors(
    field: "RoutingField", roads: Sequence[dict], tolerance: float = 1.75
) -> list[list[Point]]:
    """Preserve authored crossings and T-junctions as common routing anchors."""
    polylines = [
        [field.coords.to_grid(point) for point in road_points(road)] for road in roads
    ]
    anchors: list[list[Point]] = [[points[0], points[-1]] for points in polylines]

    def add_common(first: int, second: int, point: Point) -> None:
        if not any(distance(point, anchor) <= 0.25 for anchor in anchors[first]):
            anchors[first].append(point)
        if not any(distance(point, anchor) <= 0.25 for anchor in anchors[second]):
            anchors[second].append(point)

    for first in range(len(polylines)):
        for second in range(first + 1, len(polylines)):
            first_points = polylines[first]
            second_points = polylines[second]
            for first_start, first_end in zip(first_points, first_points[1:]):
                for second_start, second_end in zip(second_points, second_points[1:]):
                    crossing = segment_intersection_point(
                        first_start, first_end, second_start, second_end
                    )
                    if crossing is not None:
                        add_common(first, second, crossing)

            for endpoint in (first_points[0], first_points[-1]):
                candidates = [
                    project_point_to_segment(endpoint, start, end)[1]
                    for start, end in zip(second_points, second_points[1:])
                ]
                projected = min(candidates, key=lambda point: distance(endpoint, point))
                if distance(endpoint, projected) <= tolerance:
                    add_common(first, second, projected)
            for endpoint in (second_points[0], second_points[-1]):
                candidates = [
                    project_point_to_segment(endpoint, start, end)[1]
                    for start, end in zip(first_points, first_points[1:])
                ]
                projected = min(candidates, key=lambda point: distance(endpoint, point))
                if distance(endpoint, projected) <= tolerance:
                    add_common(first, second, projected)

    result: list[list[Point]] = []
    for points, road_anchors in zip(polylines, anchors):
        ordered = sorted(
            road_anchors, key=lambda point: polyline_location(point, points)
        )
        result.append(deduplicate(ordered, epsilon=0.25))
    return result


class CoordinateSystem:
    def __init__(self, definition: dict):
        grid = definition.get("grid") or {}
        self.width = int(grid.get("width", 0))
        self.height = int(grid.get("height", 0))
        self.tile_size = float(grid.get("tile_size", 1.0))
        self.kind = str(definition.get("coord_system", "grid")).lower()
        if self.width < 3 or self.height < 3 or self.tile_size <= 0.0:
            raise RoadGenerationError("map has an invalid grid")
        if self.kind not in {"grid", "world"}:
            raise RoadGenerationError(f"unsupported coord_system: {self.kind}")

    def to_grid(self, point: Sequence[float]) -> Point:
        x, z = float(point[0]), float(point[1])
        if self.kind == "grid":
            return x, z
        return (
            x / self.tile_size + self.width * 0.5 - 0.5,
            z / self.tile_size + self.height * 0.5 - 0.5,
        )

    def from_grid(self, point: Point) -> Point:
        if self.kind == "grid":
            return point
        return (
            (point[0] - (self.width * 0.5 - 0.5)) * self.tile_size,
            (point[1] - (self.height * 0.5 - 0.5)) * self.tile_size,
        )

    def distance_to_grid(self, value: float) -> float:
        return float(value) / self.tile_size


class RoutingField:
    def __init__(self, definition: dict, clearance: float):
        self.definition = definition
        self.coords = CoordinateSystem(definition)
        self.width = self.coords.width
        self.height = self.coords.height
        self.clearance = clearance / self.coords.tile_size
        size = self.width * self.height
        self.terrain_core = bytearray(size)
        self.terrain = bytearray(size)
        self.water = bytearray(size)
        self.bridge_at = [-1] * size
        self.hill_entrances: list[tuple[Point, Point]] = []
        self.river_segments = self._read_river_segments()
        self.bridges = self._read_bridges()
        self._raster_terrain()
        self.bridges = self._move_bridges_off_protected_terrain(self.bridges)
        self._raster_water()
        self._open_bridges()
        self._component_labels: list[int] | None = None

    def index(self, x: int, z: int) -> int:
        return z * self.width + x

    def in_bounds(self, x: int, z: int) -> bool:
        return 1 <= x < self.width - 1 and 1 <= z < self.height - 1

    def passable(self, x: int, z: int) -> bool:
        if not self.in_bounds(x, z):
            return False
        index = self.index(x, z)
        on_bridge = self.bridge_at[index] >= 0
        return (
            self.terrain_core[index] == 0
            and (self.terrain[index] == 0 or on_bridge)
            and (self.water[index] == 0 or on_bridge)
        )

    def bridge_id(self, x: int, z: int) -> int:
        if not (0 <= x < self.width and 0 <= z < self.height):
            return -1
        return self.bridge_at[self.index(x, z)]

    def passable_component(self, point: Point) -> int:
        if self._component_labels is None:
            labels = [-1] * (self.width * self.height)
            component = 0
            for z in range(self.height):
                for x in range(self.width):
                    cell = self.index(x, z)
                    if labels[cell] >= 0 or not self.passable(x, z):
                        continue
                    labels[cell] = component
                    pending = deque([(x, z)])
                    while pending:
                        current_x, current_z = pending.popleft()
                        for dx, dz, _ in NEIGHBORS:
                            nx, nz = current_x + dx, current_z + dz
                            if not self.passable(nx, nz):
                                continue
                            if (
                                dx
                                and dz
                                and (
                                    not self.passable(current_x + dx, current_z)
                                    or not self.passable(current_x, current_z + dz)
                                )
                            ):
                                continue
                            next_cell = self.index(nx, nz)
                            if labels[next_cell] >= 0:
                                continue
                            labels[next_cell] = component
                            pending.append((nx, nz))
                    component += 1
            self._component_labels = labels
        x, z = nearest_passable(self, point, radius=32)
        return self._component_labels[self.index(x, z)]

    def _read_bridges(self) -> list[Bridge]:
        result: list[Bridge] = []
        self.bridge_sources: list[dict] = []
        for item in self.definition.get("bridges") or []:
            if not item.get("start") or not item.get("end"):
                continue
            authored_start = self.coords.to_grid(item["start"])
            authored_end = self.coords.to_grid(item["end"])
            authored_direction = normalized(sub(authored_end, authored_start))
            authored_center = mul(add(authored_start, authored_end), 0.5)
            if not self.river_segments:
                raise RoadGenerationError("map has bridges but no river segments")
            intersecting = [
                segment
                for segment in self.river_segments
                if segment_intersection_point(
                    authored_start, authored_end, segment[0], segment[1]
                )
                is not None
            ]
            river_start, river_end, river_width = min(
                intersecting or self.river_segments,
                key=lambda segment: (
                    abs(
                        dot(authored_direction, normalized(sub(segment[1], segment[0])))
                    )
                    if intersecting
                    else point_segment_distance(authored_center, segment[0], segment[1])
                ),
            )
            river_direction = normalized(sub(river_end, river_start))
            river_length_sq = dot(
                sub(river_end, river_start), sub(river_end, river_start)
            )
            projection_t = max(
                0.0,
                min(
                    1.0,
                    dot(sub(authored_center, river_start), sub(river_end, river_start))
                    / max(river_length_sq, 1.0e-8),
                ),
            )
            crossing_center = add(
                river_start, mul(sub(river_end, river_start), projection_t)
            )
            crossing_direction = (-river_direction[1], river_direction[0])
            if dot(crossing_direction, authored_direction) < 0.0:
                crossing_direction = mul(crossing_direction, -1.0)

            crossing_length = river_width + self.coords.distance_to_grid(1.0)
            candidate = Bridge(
                sub(crossing_center, mul(crossing_direction, crossing_length * 0.5)),
                add(crossing_center, mul(crossing_direction, crossing_length * 0.5)),
                self.coords.distance_to_grid(float(item.get("width", 3.0))),
            )
            duplicate_radius = max(4.0, river_width * 0.90)
            if any(
                distance(
                    crossing_center,
                    mul(add(existing.start, existing.end), 0.5),
                )
                <= duplicate_radius
                for existing in result
            ):
                continue
            result.append(candidate)
            self.bridge_sources.append(dict(item))

        for road in self.definition.get("roads") or []:
            raw_points = road.get("waypoints") or [road.get("start"), road.get("end")]
            points = [self.coords.to_grid(point) for point in raw_points if point]
            road_width = self.coords.distance_to_grid(float(road.get("width", 3.0)))
            for road_start, road_end in zip(points, points[1:]):
                road_direction = normalized(sub(road_end, road_start))
                for river_start, river_end, river_width in self.river_segments:
                    crossing_center = segment_intersection_point(
                        road_start, road_end, river_start, river_end
                    )
                    if crossing_center is None:
                        continue
                    nearby_distance = max(12.0, river_width * 1.5)
                    if any(
                        distance(
                            crossing_center,
                            mul(add(bridge.start, bridge.end), 0.5),
                        )
                        <= nearby_distance
                        for bridge in result
                    ):
                        continue
                    river_direction = normalized(sub(river_end, river_start))
                    crossing_direction = (-river_direction[1], river_direction[0])
                    if dot(crossing_direction, road_direction) < 0.0:
                        crossing_direction = mul(crossing_direction, -1.0)
                    crossing_length = river_width + self.coords.distance_to_grid(1.0)
                    result.append(
                        Bridge(
                            sub(
                                crossing_center,
                                mul(crossing_direction, crossing_length * 0.5),
                            ),
                            add(
                                crossing_center,
                                mul(crossing_direction, crossing_length * 0.5),
                            ),
                            road_width + self.coords.distance_to_grid(1.2),
                        )
                    )
                    self.bridge_sources.append(
                        {
                            "start": [0, 0],
                            "end": [0, 0],
                            "width": json_number(
                                (road_width + self.coords.distance_to_grid(1.2))
                                * self.coords.tile_size
                            ),
                            "height": 0.55,
                        }
                    )
        return result

    def _read_river_segments(self) -> list[tuple[Point, Point, float]]:
        result: list[tuple[Point, Point, float]] = []
        for river in self.definition.get("rivers") or []:
            raw_points = river.get("waypoints") or [
                river.get("start"),
                river.get("end"),
            ]
            points = [self.coords.to_grid(point) for point in raw_points if point]
            width = self.coords.distance_to_grid(float(river.get("width", 3.0)))
            result.extend((start, end, width) for start, end in zip(points, points[1:]))
        return result

    def _move_bridges_off_protected_terrain(
        self, bridges: Sequence[Bridge]
    ) -> list[Bridge]:
        def deck_is_clear(bridge: Bridge) -> bool:
            span = distance(bridge.start, bridge.end)
            samples = max(1, int(math.ceil(span / 0.25)))
            for sample in range(samples + 1):
                point = add(
                    bridge.start,
                    mul(sub(bridge.end, bridge.start), sample / samples),
                )
                x, z = int(round(point[0])), int(round(point[1]))
                if not self.in_bounds(x, z) or self.terrain_core[self.index(x, z)]:
                    return False
            approach_length = self.clearance + 4.0
            direction = bridge.direction
            for endpoint, outward in (
                (bridge.start, mul(direction, -1.0)),
                (bridge.end, direction),
            ):
                approach_samples = max(1, int(math.ceil(approach_length / 0.25)))
                for sample in range(approach_samples + 1):
                    point = add(
                        endpoint,
                        mul(outward, approach_length * sample / approach_samples),
                    )
                    x, z = int(round(point[0])), int(round(point[1]))
                    if not self.in_bounds(x, z) or self.terrain[self.index(x, z)]:
                        return False
            return True

        result: list[Bridge] = []
        for bridge in bridges:
            if deck_is_clear(bridge):
                result.append(bridge)
                continue
            center = mul(add(bridge.start, bridge.end), 0.5)
            candidates: list[tuple[float, Bridge]] = []

            for river_start, river_end, river_width in self.river_segments:
                river_direction = normalized(sub(river_end, river_start))
                crossing_direction = (-river_direction[1], river_direction[0])
                if dot(crossing_direction, bridge.direction) < 0.0:
                    crossing_direction = mul(crossing_direction, -1.0)
                local_bridge_length = river_width + self.coords.distance_to_grid(1.0)
                sample_count = max(1, int(math.ceil(distance(river_start, river_end))))
                for sample in range(sample_count):
                    candidate_center = add(
                        river_start,
                        mul(
                            sub(river_end, river_start),
                            (sample + 0.5) / sample_count,
                        ),
                    )
                    candidate = Bridge(
                        sub(
                            candidate_center,
                            mul(crossing_direction, local_bridge_length * 0.5),
                        ),
                        add(
                            candidate_center,
                            mul(crossing_direction, local_bridge_length * 0.5),
                        ),
                        bridge.width,
                    )
                    duplicate_radius = max(4.0, river_width * 0.90)
                    if deck_is_clear(candidate) and not any(
                        distance(
                            candidate_center,
                            mul(add(existing.start, existing.end), 0.5),
                        )
                        <= duplicate_radius
                        for existing in result
                    ):
                        candidates.append(
                            (distance(center, candidate_center), candidate)
                        )
            replacement = (
                min(candidates, key=lambda item: item[0])[1] if candidates else None
            )
            if replacement is None:
                raise RoadGenerationError(
                    f"bridge near {center} cannot avoid protected terrain"
                )
            result.append(replacement)
        return result

    def generated_bridges(self, bridge_ids: Iterable[int] | None = None) -> list[dict]:
        included = (
            set(range(len(self.bridges))) if bridge_ids is None else set(bridge_ids)
        )
        output: list[dict] = []
        for index, (authored, repaired) in enumerate(
            zip(self.bridge_sources, self.bridges)
        ):
            if index not in included:
                continue
            item = dict(authored)
            start = self.coords.from_grid(repaired.start)
            end = self.coords.from_grid(repaired.end)
            item["start"] = [json_number(start[0]), json_number(start[1])]
            item["end"] = [json_number(end[0]), json_number(end[1])]
            output.append(item)
        return output

    def _raster_ellipse(
        self,
        target: bytearray,
        center: Point,
        half_width: float,
        half_depth: float,
        rotation_degrees: float,
    ) -> None:
        half_width = max(0.5, half_width)
        half_depth = max(0.5, half_depth)
        radius = math.hypot(half_width, half_depth)
        min_x = max(0, int(math.floor(center[0] - radius)))
        max_x = min(self.width - 1, int(math.ceil(center[0] + radius)))
        min_z = max(0, int(math.floor(center[1] - radius)))
        max_z = min(self.height - 1, int(math.ceil(center[1] + radius)))
        angle = math.radians(rotation_degrees)
        cos_angle, sin_angle = math.cos(angle), math.sin(angle)
        for z in range(min_z, max_z + 1):
            for x in range(min_x, max_x + 1):
                dx, dz = x - center[0], z - center[1]
                local_x = dx * cos_angle + dz * sin_angle
                local_z = -dx * sin_angle + dz * cos_angle
                if (local_x / half_width) ** 2 + (local_z / half_depth) ** 2 <= 1.0:
                    target[self.index(x, z)] = 1

    def _raster_capsule(
        self,
        target: bytearray,
        start: Point,
        end: Point,
        radius: float,
        value: int = 1,
    ) -> None:
        radius = max(radius, 0.5)
        min_x = max(0, int(math.floor(min(start[0], end[0]) - radius)))
        max_x = min(self.width - 1, int(math.ceil(max(start[0], end[0]) + radius)))
        min_z = max(0, int(math.floor(min(start[1], end[1]) - radius)))
        max_z = min(self.height - 1, int(math.ceil(max(start[1], end[1]) + radius)))
        for z in range(min_z, max_z + 1):
            for x in range(min_x, max_x + 1):
                if point_segment_distance((x, z), start, end) <= radius:
                    target[self.index(x, z)] = value

    def _raster_terrain(self) -> None:
        for feature in self.definition.get("terrain") or []:
            terrain_type = str(feature.get("type", "")).lower()
            if terrain_type not in {"hill", "mountain", "lake"}:
                continue
            center = self.coords.to_grid((feature.get("x", 0.0), feature.get("z", 0.0)))
            if terrain_type == "hill":
                for entrance in feature.get("entrances") or []:
                    if isinstance(entrance, dict):
                        authored = (entrance.get("x", 0.0), entrance.get("z", 0.0))
                    else:
                        authored = entrance
                    self.hill_entrances.append((center, self.coords.to_grid(authored)))
            radius = self.coords.distance_to_grid(float(feature.get("radius", 5.0)))
            full_width = self.coords.distance_to_grid(
                float(feature.get("width", radius * self.coords.tile_size * 2.0))
            )
            full_depth = self.coords.distance_to_grid(
                float(feature.get("depth", radius * self.coords.tile_size * 2.0))
            )
            if terrain_type == "mountain" and "width" not in feature:
                full_width = radius * 2.68
                full_depth = radius * 1.60
            rotation = float(feature.get("rotation", 0.0))
            self._raster_ellipse(
                self.terrain_core,
                center,
                full_width * 0.5,
                full_depth * 0.5,
                rotation,
            )
            self._raster_ellipse(
                self.terrain,
                center,
                full_width * 0.5 + self.clearance,
                full_depth * 0.5 + self.clearance,
                rotation,
            )

    def _raster_water(self) -> None:
        for river in self.definition.get("rivers") or []:
            raw_points = river.get("waypoints") or [
                river.get("start"),
                river.get("end"),
            ]
            points = [self.coords.to_grid(point) for point in raw_points if point]
            radius = self.coords.distance_to_grid(float(river.get("width", 3.0))) * 0.5
            radius += self.clearance
            for start, end in zip(points, points[1:]):
                self._raster_capsule(self.water, start, end, radius)

        lakes = list(self.definition.get("lakes") or [])
        for feature in self.definition.get("terrain") or []:
            if str(feature.get("type", "")).lower() == "lake":
                lakes.append(feature)
        for lake in lakes:
            if "center" in lake:
                center = self.coords.to_grid(lake["center"])
            else:
                center = self.coords.to_grid((lake.get("x", 0.0), lake.get("z", 0.0)))
            radius = self.coords.distance_to_grid(float(lake.get("radius", 5.0)))
            width = self.coords.distance_to_grid(
                float(lake.get("width", radius * self.coords.tile_size * 2.0))
            )
            depth = self.coords.distance_to_grid(
                float(lake.get("depth", radius * self.coords.tile_size * 2.0))
            )
            self._raster_ellipse(
                self.water,
                center,
                width * 0.5 + self.clearance,
                depth * 0.5 + self.clearance,
                float(lake.get("rotation", lake.get("rotation_deg", 0.0))),
            )

    def _open_bridges(self) -> None:
        for bridge_id, bridge in enumerate(self.bridges):
            self._open_bridge(bridge_id, bridge)

    def _open_bridge(self, bridge_id: int, bridge: Bridge) -> None:
        direction = bridge.direction
        extension = self.clearance + 2.0
        start = sub(bridge.start, mul(direction, extension))
        end = add(bridge.end, mul(direction, extension))
        corridor_radius = max(0.8, min(bridge.width * 0.38, 2.6))
        min_x = max(0, int(math.floor(min(start[0], end[0]) - corridor_radius)))
        max_x = min(
            self.width - 1, int(math.ceil(max(start[0], end[0]) + corridor_radius))
        )
        min_z = max(0, int(math.floor(min(start[1], end[1]) - corridor_radius)))
        max_z = min(
            self.height - 1, int(math.ceil(max(start[1], end[1]) + corridor_radius))
        )
        for z in range(min_z, max_z + 1):
            for x in range(min_x, max_x + 1):
                if point_segment_distance((x, z), start, end) <= corridor_radius:
                    self.bridge_at[self.index(x, z)] = bridge_id

    def ensure_bridge_connection(
        self, start: Point, end: Point, road_width: float
    ) -> bool:
        start_component = self.passable_component(start)
        end_component = self.passable_component(end)
        if start_component == end_component:
            return True

        candidates: list[tuple[float, Bridge]] = []
        for river_start, river_end, river_width in self.river_segments:
            river_direction = normalized(sub(river_end, river_start))
            crossing_direction = (-river_direction[1], river_direction[0])
            segment_span = distance(river_start, river_end)
            samples = max(1, int(math.ceil(segment_span / 2.0)))
            crossing_length = river_width + self.coords.distance_to_grid(1.0)
            for sample in range(samples):
                center = add(
                    river_start,
                    mul(sub(river_end, river_start), (sample + 0.5) / samples),
                )
                candidate = Bridge(
                    sub(center, mul(crossing_direction, crossing_length * 0.5)),
                    add(center, mul(crossing_direction, crossing_length * 0.5)),
                    road_width + self.coords.distance_to_grid(1.2),
                )
                deck_span = distance(candidate.start, candidate.end)
                deck_samples = max(1, int(math.ceil(deck_span / 0.25)))
                if any(
                    not self.in_bounds(int(round(point[0])), int(round(point[1])))
                    or self.terrain_core[
                        self.index(int(round(point[0])), int(round(point[1])))
                    ]
                    for point in (
                        add(
                            candidate.start,
                            mul(
                                sub(candidate.end, candidate.start),
                                deck_sample / deck_samples,
                            ),
                        )
                        for deck_sample in range(deck_samples + 1)
                    )
                ):
                    continue
                first_component = self.passable_component(candidate.start)
                second_component = self.passable_component(candidate.end)
                if {first_component, second_component} != {
                    start_component,
                    end_component,
                }:
                    continue
                score = point_segment_distance(center, start, end)
                score += 0.05 * (distance(start, center) + distance(end, center))
                candidates.append((score, candidate))

        if not candidates:
            return False
        bridge = min(candidates, key=lambda item: item[0])[1]
        self.bridges.append(bridge)
        self.bridge_sources.append(
            {
                "start": [0, 0],
                "end": [0, 0],
                "width": json_number(bridge.width * self.coords.tile_size),
                "height": 0.55,
            }
        )
        self._open_bridge(len(self.bridges) - 1, bridge)
        self._component_labels = None
        return self.passable_component(start) == self.passable_component(end)

    def legal_terminal(self, point: Point) -> Point:
        cell = int(round(point[0])), int(round(point[1]))
        if self.passable(*cell):
            return point

        candidates: list[tuple[float, Point]] = []
        for center, entrance in self.hill_entrances:
            if distance(point, entrance) > 35.0:
                continue
            outward = normalized(sub(entrance, center))
            if outward == (0.0, 0.0):
                continue
            for step in range(0, 24):
                candidate = add(entrance, mul(outward, float(step)))
                candidate_cell = int(round(candidate[0])), int(round(candidate[1]))
                if self.passable(*candidate_cell):
                    candidates.append((distance(point, entrance), candidate))
                    break
        if candidates:
            return min(candidates, key=lambda item: item[0])[1]
        x, z = nearest_passable(self, point, radius=16)
        return float(x), float(z)

    def line_passable(self, start: Point, end: Point, step: float = 0.20) -> bool:
        del step
        x, z = int(math.floor(start[0] + 0.5)), int(math.floor(start[1] + 0.5))
        end_x = int(math.floor(end[0] + 0.5))
        end_z = int(math.floor(end[1] + 0.5))
        dx, dz = end[0] - start[0], end[1] - start[1]
        step_x = 1 if dx > 0.0 else -1 if dx < 0.0 else 0
        step_z = 1 if dz > 0.0 else -1 if dz < 0.0 else 0
        delta_x = math.inf if step_x == 0 else 1.0 / abs(dx)
        delta_z = math.inf if step_z == 0 else 1.0 / abs(dz)
        boundary_x = x + (0.5 if step_x > 0 else -0.5)
        boundary_z = z + (0.5 if step_z > 0 else -0.5)
        next_x = math.inf if step_x == 0 else (boundary_x - start[0]) / dx
        next_z = math.inf if step_z == 0 else (boundary_z - start[1]) / dz

        while True:
            if not self.passable(x, z):
                return False
            if x == end_x and z == end_z:
                return True
            if abs(next_x - next_z) <= 1.0e-10:

                if not self.passable(x + step_x, z) or not self.passable(x, z + step_z):
                    return False
                x += step_x
                z += step_z
                next_x += delta_x
                next_z += delta_z
            elif next_x < next_z:
                x += step_x
                next_x += delta_x
            else:
                z += step_z
                next_z += delta_z


def guide_distance(point: Point, guide: Sequence[Point]) -> float:
    return min(point_segment_distance(point, a, b) for a, b in zip(guide, guide[1:]))


def nearest_passable(
    field: RoutingField, point: Point, radius: int = 8
) -> tuple[int, int]:
    origin = int(round(point[0])), int(round(point[1]))
    if field.passable(*origin):
        return origin
    candidates: list[tuple[float, int, int]] = []
    for dz in range(-radius, radius + 1):
        for dx in range(-radius, radius + 1):
            x, z = origin[0] + dx, origin[1] + dz
            if field.passable(x, z):
                candidates.append((dx * dx + dz * dz, x, z))
    if not candidates:
        raise RoadGenerationError(f"no passable road cell near {point}")
    _, x, z = min(candidates)
    return x, z


def route_astar(
    field: RoutingField,
    start: Point,
    end: Point,
    guide: Sequence[Point],
    *,
    directional_state: bool = True,
) -> list[Point]:
    start_cell = nearest_passable(field, start)
    end_cell = nearest_passable(field, end)
    if field.passable_component(start) != field.passable_component(end):
        raise RoadGenerationError(f"no legal road route from {start} to {end}")
    start_state = (start_cell[0], start_cell[1], 8)
    open_heap: list[tuple[float, float, tuple[int, int, int]]] = []
    heapq.heappush(open_heap, (distance(start_cell, end_cell), 0.0, start_state))
    costs = {start_state: 0.0}
    parents: dict[tuple[int, int, int], tuple[int, int, int]] = {}
    end_state: tuple[int, int, int] | None = None
    max_expansions = field.width * field.height * 9
    expansions = 0
    guide_cache: dict[tuple[int, int], float] = {}

    while open_heap:
        _, current_cost, state = heapq.heappop(open_heap)
        if current_cost > costs.get(state, math.inf) + 1.0e-6:
            continue
        x, z, previous_direction = state
        if (x, z) == end_cell:
            end_state = state
            break
        expansions += 1
        if expansions > max_expansions:
            break

        for direction_index, (dx, dz, step_cost) in enumerate(NEIGHBORS):
            nx, nz = x + dx, z + dz
            if not field.passable(nx, nz):
                continue
            if (
                dx
                and dz
                and (not field.passable(x + dx, z) or not field.passable(x, z + dz))
            ):
                continue

            bridge_id = field.bridge_id(nx, nz)
            current_bridge_id = field.bridge_id(x, z)
            if bridge_id >= 0 and bridge_id == current_bridge_id:
                movement = normalized((dx, dz))
                if abs(dot(movement, field.bridges[bridge_id].direction)) < 0.55:
                    continue

            bend_cost = 0.0
            if directional_state and previous_direction < 8:
                turn = abs(direction_index - previous_direction)
                turn = min(turn, 8 - turn)
                bend_cost = (0.10, 0.28, 0.70, 1.35, 2.4)[turn]
            guide_key = (nx, nz)
            if guide_key not in guide_cache:
                guide_cache[guide_key] = min(
                    1.5, guide_distance((nx, nz), guide) * 0.018
                )
            preserve_cost = guide_cache[guide_key]
            edge_distance = min(nx, nz, field.width - 1 - nx, field.height - 1 - nz)
            edge_cost = max(0.0, 5.0 - edge_distance) * 0.35
            new_cost = current_cost + step_cost + bend_cost + preserve_cost + edge_cost

            next_state = (nx, nz, direction_index if directional_state else 8)
            if new_cost + 1.0e-6 >= costs.get(next_state, math.inf):
                continue
            costs[next_state] = new_cost
            parents[next_state] = state
            heuristic = distance((nx, nz), end_cell)
            heapq.heappush(open_heap, (new_cost + heuristic, new_cost, next_state))

    if end_state is None:
        raise RoadGenerationError(f"no legal road route from {start} to {end}")

    cells: list[Point] = []
    state = end_state
    while True:
        cells.append((float(state[0]), float(state[1])))
        if state == start_state:
            break
        state = parents[state]
    cells.reverse()
    cells[0] = start
    cells[-1] = end
    return cells


def inject_bridge_endpoints(
    field: RoutingField,
    points: Sequence[Point],
    required_points: Sequence[Point] = (),
) -> tuple[list[Point], set[int], tuple[int, ...]]:
    def water_bridge_id(point: Point) -> int:
        x, z = int(round(point[0])), int(round(point[1]))
        if not (0 <= x < field.width and 0 <= z < field.height):
            return -1
        return field.bridge_id(x, z) if field.water[field.index(x, z)] else -1

    touched = {
        field.bridge_id(int(round(point[0])), int(round(point[1])))
        for point in points
        if field.bridge_id(int(round(point[0])), int(round(point[1]))) >= 0
    }
    crossed: list[tuple[int, int]] = []
    for bridge_id in touched:
        bridge = field.bridges[bridge_id]
        center = mul(add(bridge.start, bridge.end), 0.5)
        signed_positions = [
            dot(sub(point, center), bridge.direction) for point in points
        ]
        half_span = distance(bridge.start, bridge.end) * 0.5
        if (
            min(signed_positions) < -half_span * 0.35
            and max(signed_positions) > half_span * 0.35
        ):
            first_touch = min(
                range(len(points)),
                key=lambda index: point_segment_distance(
                    points[index], bridge.start, bridge.end
                ),
            )
            crossed.append((first_touch, bridge_id))
    used = [bridge_id for _, bridge_id in sorted(crossed)]

    result = list(points)
    fixed_points: list[Point] = [result[0], result[-1], *required_points]
    for bridge_id in dict.fromkeys(used):
        bridge = field.bridges[bridge_id]
        indices = [
            index
            for index, point in enumerate(result)
            if water_bridge_id(point) == bridge_id
        ]
        if not indices:
            continue
        first, last = min(indices), max(indices)
        before = result[max(0, first - 1)]
        if distance(before, bridge.start) <= distance(before, bridge.end):
            entry, exit_point = bridge.start, bridge.end
        else:
            entry, exit_point = bridge.end, bridge.start
        crossing_direction = normalized(sub(exit_point, entry))
        approach_distance = field.clearance + 2.0
        entry_approach = sub(entry, mul(crossing_direction, approach_distance))
        exit_approach = add(exit_point, mul(crossing_direction, approach_distance))

        result = (
            result[:first]
            + [entry_approach, entry, exit_point, exit_approach]
            + result[last + 1 :]
        )
        fixed_points.extend((entry_approach, entry, exit_point, exit_approach))

    result = deduplicate(result)
    fixed = {
        index
        for index, point in enumerate(result)
        if any(distance(point, required) < 1.0e-4 for required in fixed_points)
    }
    return result, fixed, tuple(sorted(used))


def simplify_polyline(
    field: RoutingField, points: Sequence[Point], fixed: set[int]
) -> tuple[list[Point], set[int]]:
    mandatory = sorted(fixed | {0, len(points) - 1})
    result: list[Point] = []
    result_fixed: set[int] = set()
    for span_index, (span_start, span_end) in enumerate(zip(mandatory, mandatory[1:])):
        cursor = span_start
        chunk = [points[cursor]]
        while cursor < span_end:
            candidate = span_end
            while candidate > cursor + 1 and not field.line_passable(
                points[cursor], points[candidate]
            ):
                candidate -= 1
            chunk.append(points[candidate])
            cursor = candidate
        if span_index > 0:
            chunk = chunk[1:]
        result.extend(chunk)
        result_fixed.add(len(result) - 1)
    fixed_points = [result[index] for index in result_fixed | {0}]
    result = deduplicate(result)
    result_fixed = {
        index
        for index, point in enumerate(result)
        if any(distance(point, required) < 1.0e-4 for required in fixed_points)
    }
    return result, result_fixed


def rounded_polyline(
    field: RoutingField,
    points: Sequence[Point],
    fixed: set[int],
    road_width: float,
) -> list[Point]:
    if len(points) < 3:
        return list(points)
    result = [points[0]]
    for index in range(1, len(points) - 1):
        corner = points[index]
        if index in fixed:
            result.append(corner)
            continue
        incoming_length = distance(points[index - 1], corner)
        outgoing_length = distance(corner, points[index + 1])
        radius = min(
            max(1.4, road_width * 0.75), incoming_length * 0.28, outgoing_length * 0.28
        )
        if radius < 0.6:
            result.append(corner)
            continue
        before = add(corner, mul(normalized(sub(points[index - 1], corner)), radius))
        after = add(corner, mul(normalized(sub(points[index + 1], corner)), radius))
        if field.line_passable(result[-1], before) and field.line_passable(
            before, after
        ):
            result.extend((before, after))
        else:
            result.append(corner)
    result.append(points[-1])
    return deduplicate(result)


def resample_polyline(
    points: Sequence[Point], max_segment_length: float
) -> list[Point]:
    result = [points[0]]
    for start, end in zip(points, points[1:]):
        span = distance(start, end)
        steps = max(1, int(math.ceil(span / max_segment_length)))
        for index in range(1, steps + 1):
            result.append(add(start, mul(sub(end, start), index / steps)))
    return deduplicate(result)


def json_number(value: float) -> int | float:
    rounded = round(value, 2)
    return int(round(rounded)) if abs(rounded - round(rounded)) < 1.0e-7 else rounded


def generate_road(
    field: RoutingField,
    road: dict,
    max_segment_length: float,
    anchors: Sequence[Point],
    *,
    directional_state: bool = True,
) -> RouteResult:
    authored_points = [field.coords.to_grid(point) for point in road_points(road)]
    legal_anchors = [field.legal_terminal(point) for point in anchors]
    for index in range(1, len(legal_anchors)):
        if (
            distance(legal_anchors[index - 1], legal_anchors[index]) <= 0.25
            and distance(anchors[index - 1], anchors[index]) > 0.25
        ):
            x, z = nearest_passable(field, anchors[index], radius=32)
            legal_anchors[index] = float(x), float(z)
    legal_anchors = deduplicate(legal_anchors, epsilon=0.25)
    road_width = field.coords.distance_to_grid(float(road.get("width", 3.0)))
    for leg_start, leg_end in zip(legal_anchors, legal_anchors[1:]):
        field.ensure_bridge_connection(leg_start, leg_end, road_width)
    guide = [legal_anchors[0], *authored_points[1:-1], legal_anchors[-1]]
    routed: list[Point] = [legal_anchors[0]]
    for leg_start, leg_end in zip(legal_anchors, legal_anchors[1:]):
        leg = route_astar(
            field,
            leg_start,
            leg_end,
            guide,
            directional_state=directional_state,
        )
        routed.extend(leg[1:])
    with_bridges, fixed, bridge_ids = inject_bridge_endpoints(
        field, routed, required_points=legal_anchors
    )
    simplified, simplified_fixed = simplify_polyline(field, with_bridges, fixed)
    rounded = rounded_polyline(
        field,
        simplified,
        simplified_fixed,
        road_width,
    )
    if not all(field.line_passable(a, b) for a, b in zip(rounded, rounded[1:])):
        rounded = simplified
    sampled = resample_polyline(rounded, max_segment_length / field.coords.tile_size)
    authored_output = [field.coords.from_grid(point) for point in sampled]
    encoded = [[json_number(x), json_number(z)] for x, z in authored_output]

    output = dict(road)
    output["start"] = encoded[0]
    output["end"] = encoded[-1]
    output["waypoints"] = encoded
    return RouteResult(
        output,
        polyline_length(authored_points),
        polyline_length(sampled),
        bridge_ids,
    )


class DisjointSet:
    def __init__(self, count: int):
        self.parent = list(range(count))

    def find(self, value: int) -> int:
        while self.parent[value] != value:
            self.parent[value] = self.parent[self.parent[value]]
            value = self.parent[value]
        return value

    def join(self, first: int, second: int) -> None:
        first, second = self.find(first), self.find(second)
        if first != second:
            self.parent[second] = first


def orientation(a: Point, b: Point, c: Point) -> float:
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])


def segments_intersect(
    a: Point, b: Point, c: Point, d: Point, tolerance: float = 0.35
) -> bool:
    if (
        point_segment_distance(a, c, d) <= tolerance
        or point_segment_distance(b, c, d) <= tolerance
        or point_segment_distance(c, a, b) <= tolerance
        or point_segment_distance(d, a, b) <= tolerance
    ):
        return True
    return (
        orientation(a, b, c) * orientation(a, b, d) <= 0.0
        and orientation(c, d, a) * orientation(c, d, b) <= 0.0
    )


def dangling_road_endpoints(
    field: RoutingField,
    roads: Sequence[dict],
    tolerance: float = 0.35,
) -> list[tuple[int, Point]]:
    """Return road terminals that do not have a visible network purpose."""
    polylines = [
        [field.coords.to_grid(point) for point in road_points(road)] for road in roads
    ]
    bridge_ends = [
        endpoint for bridge in field.bridges for endpoint in (bridge.start, bridge.end)
    ]
    dangling: list[tuple[int, Point]] = []
    for index, points in enumerate(polylines):
        for endpoint in (points[0], points[-1]):
            on_edge = (
                min(
                    endpoint[0],
                    endpoint[1],
                    field.width - 1 - endpoint[0],
                    field.height - 1 - endpoint[1],
                )
                <= 1.5
            )
            joins_road = any(
                point_segment_distance(endpoint, start, end) <= tolerance
                for other, polyline in enumerate(polylines)
                if other != index
                for start, end in zip(polyline, polyline[1:])
            )
            joins_bridge = any(
                distance(endpoint, bridge_end) <= tolerance
                for bridge_end in bridge_ends
            )
            if not on_edge and not joins_road and not joins_bridge:
                dangling.append((index, endpoint))
    return dangling


def missing_bridge_approaches(
    field: RoutingField,
    roads: Sequence[dict],
    tolerance: float = 0.35,
) -> list[tuple[Point, Point]]:
    """Return bridge ends without a road continuing away from the deck."""
    polylines = [
        [field.coords.to_grid(point) for point in road_points(road)] for road in roads
    ]
    missing: list[tuple[Point, Point]] = []
    for bridge in field.bridges:
        direction = bridge.direction
        for endpoint, outward in (
            (bridge.start, mul(direction, -1.0)),
            (bridge.end, direction),
        ):
            supported = False
            for points in polylines:
                for start, end in zip(points, points[1:]):
                    _, contact = project_point_to_segment(endpoint, start, end)
                    if distance(endpoint, contact) > tolerance:
                        continue
                    for candidate in (start, end):
                        extension = sub(candidate, endpoint)
                        if (
                            length(extension) >= 1.5
                            and dot(normalized(extension), outward) >= 0.65
                        ):
                            supported = True
                            break
                    if supported:
                        break
                if supported:
                    break
            if not supported:
                missing.append((endpoint, outward))
    return missing


def invalid_bridge_geometry(
    field: RoutingField,
    bridges: Sequence[Bridge] | None = None,
) -> int:
    """Count decks that are not a short, perpendicular local river span."""
    invalid = 0
    bearing = field.coords.distance_to_grid(1.0)
    for bridge in field.bridges if bridges is None else bridges:
        center = mul(add(bridge.start, bridge.end), 0.5)
        intersecting = [
            segment
            for segment in field.river_segments
            if segment_intersection_point(
                bridge.start, bridge.end, segment[0], segment[1]
            )
            is not None
        ]
        if not intersecting:
            invalid += 1
            continue
        river = min(
            intersecting,
            key=lambda segment: point_segment_distance(center, segment[0], segment[1]),
        )
        river_direction = normalized(sub(river[1], river[0]))
        bridge_length = distance(bridge.start, bridge.end)
        maximum_length = river[2] + bearing + 0.25
        minimum_length = max(river[2] * 0.80, river[2] - bearing)
        if (
            bridge_length < minimum_length
            or bridge_length > maximum_length
            or abs(dot(bridge.direction, river_direction)) > 0.25
        ):
            invalid += 1
    return invalid


def validate_roads(field: RoutingField, roads: Sequence[dict]) -> ValidationResult:
    polylines = [
        [field.coords.to_grid(point) for point in road_points(road)] for road in roads
    ]
    obstacle_violations = 0
    invalid_bridge_crossings = 0

    for points in polylines:
        for start, end in zip(points, points[1:]):
            span = distance(start, end)
            samples = max(1, int(math.ceil(span / 0.25)))
            water_bridge_samples: dict[int, int] = {}
            if not field.line_passable(start, end):
                obstacle_violations += 1
            for sample in range(samples + 1):
                point = add(start, mul(sub(end, start), sample / samples))
                x, z = int(round(point[0])), int(round(point[1]))
                index = field.index(x, z)
                if field.water[index]:
                    bridge_id = field.bridge_id(x, z)
                    if bridge_id < 0:
                        invalid_bridge_crossings += 1
                    else:
                        water_bridge_samples[bridge_id] = (
                            water_bridge_samples.get(bridge_id, 0) + 1
                        )
            road_direction = normalized(sub(end, start))
            for bridge_id, sample_count in water_bridge_samples.items():

                water_distance = sample_count * span / samples
                if (
                    water_distance >= 2.0
                    and abs(dot(road_direction, field.bridges[bridge_id].direction))
                    < 0.90
                ):
                    invalid_bridge_crossings += 1

    components = len(road_components(field, roads))
    sides: set[str] = set()
    for points in polylines:
        for x, z in points:
            if x <= 1.5:
                sides.add("west")
            if x >= field.width - 2.5:
                sides.add("east")
            if z <= 1.5:
                sides.add("north")
            if z >= field.height - 2.5:
                sides.add("south")
    orphan_bridge_ends = sum(
        not any(
            point_segment_distance(endpoint, start, end) <= 0.35
            for points in polylines
            for start, end in zip(points, points[1:])
        )
        for bridge in field.bridges
        for endpoint in (bridge.start, bridge.end)
    )
    return ValidationResult(
        len(roads),
        components,
        obstacle_violations,
        invalid_bridge_crossings,
        len(dangling_road_endpoints(field, roads)),
        invalid_bridge_geometry(field),
        orphan_bridge_ends,
        len(missing_bridge_approaches(field, roads)),
        len({str(road.get("style", "default")) for road in roads}),
        len(sides),
    )


def road_components(field: RoutingField, roads: Sequence[dict]) -> list[list[int]]:
    polylines = [
        [field.coords.to_grid(point) for point in road_points(road)] for road in roads
    ]
    sets = DisjointSet(len(polylines))
    for first in range(len(polylines)):
        for second in range(first + 1, len(polylines)):
            if any(
                segments_intersect(a, b, c, d)
                for a, b in zip(polylines[first], polylines[first][1:])
                for c, d in zip(polylines[second], polylines[second][1:])
            ):
                sets.join(first, second)

    for bridge in field.bridges:
        start_roads = [
            index
            for index, points in enumerate(polylines)
            if any(
                point_segment_distance(bridge.start, start, end) <= 0.35
                for start, end in zip(points, points[1:])
            )
        ]
        end_roads = [
            index
            for index, points in enumerate(polylines)
            if any(
                point_segment_distance(bridge.end, start, end) <= 0.35
                for start, end in zip(points, points[1:])
            )
        ]
        for start_road in start_roads:
            for end_road in end_roads:
                sets.join(start_road, end_road)
    grouped: dict[int, list[int]] = {}
    for index in range(len(polylines)):
        grouped.setdefault(sets.find(index), []).append(index)
    return list(grouped.values())


def connect_road_components(
    field: RoutingField,
    roads: list[dict],
    max_segment_length: float,
) -> list[RouteResult]:
    """Add the shortest legal links needed to make one useful road network."""
    connectors: list[RouteResult] = []
    while len(components := road_components(field, roads)) > 1:
        polylines = [
            [field.coords.to_grid(point) for point in road_points(road)]
            for road in roads
        ]
        candidates: list[tuple[float, Point, Point]] = []
        for first_index in range(len(components)):
            first_points = [
                point
                for road_index in components[first_index]
                for point in polylines[road_index]
            ]
            for second_index in range(first_index + 1, len(components)):
                second_points = [
                    point
                    for road_index in components[second_index]
                    for point in polylines[road_index]
                ]
                for first in first_points:
                    for second in second_points:
                        candidates.append((distance(first, second), first, second))

        connector: RouteResult | None = None

        for _, start, end in sorted(candidates, key=lambda item: item[0])[:96]:
            authored_start = field.coords.from_grid(start)
            authored_end = field.coords.from_grid(end)
            road = {
                "start": list(authored_start),
                "end": list(authored_end),
                "width": 3.2,
                "style": "rough",
            }
            try:
                candidate = generate_road(
                    field,
                    road,
                    max_segment_length,
                    (start, end),
                    directional_state=False,
                )
            except RoadGenerationError:
                continue
            validation = validate_roads(field, [candidate.road])
            if validation.obstacle_violations or validation.invalid_bridge_crossings:
                continue
            connector = candidate
            break
        if connector is None:
            raise RoadGenerationError(
                f"cannot legally connect {len(components)} remaining road components"
            )
        roads.append(connector.road)
        connectors.append(connector)
    return connectors


def connect_incomplete_terminals(
    field: RoutingField,
    roads: list[dict],
    max_segment_length: float,
) -> list[RouteResult]:
    """Complete dangling roads and both approaches of every retained bridge."""
    connectors: list[RouteResult] = []
    for _ in range(64):
        polylines = [
            [field.coords.to_grid(point) for point in road_points(road)]
            for road in roads
        ]
        pending: list[tuple[Point, int | None, Point | None]] = [
            (point, road_index, None)
            for road_index, point in dangling_road_endpoints(field, roads)
        ]
        pending.extend(
            (endpoint, None, outward)
            for endpoint, outward in missing_bridge_approaches(field, roads)
        )
        if not pending:
            return connectors

        terminal, excluded_road, required_outward = pending[0]
        targets: list[tuple[float, Point]] = []
        for road_index, points in enumerate(polylines):
            if road_index == excluded_road:
                continue
            for start, end in zip(points, points[1:]):
                _, projected = project_point_to_segment(terminal, start, end)
                target_distance = distance(terminal, projected)
                if target_distance <= 0.35:
                    continue
                if (
                    required_outward is not None
                    and dot(normalized(sub(projected, terminal)), required_outward)
                    < 0.35
                ):
                    continue
                targets.append((distance(terminal, projected), projected))

        if required_outward is not None:
            edge_targets = [
                *((1.0, float(z)) for z in range(2, field.height - 2, 8)),
                *((field.width - 2.0, float(z)) for z in range(2, field.height - 2, 8)),
                *((float(x), 1.0) for x in range(2, field.width - 2, 8)),
                *((float(x), field.height - 2.0) for x in range(2, field.width - 2, 8)),
            ]
            for target in edge_targets:
                if (
                    not field.passable(round(target[0]), round(target[1]))
                    or dot(normalized(sub(target, terminal)), required_outward) < 0.35
                ):
                    continue
                targets.append((distance(terminal, target), target))

        connector: RouteResult | None = None
        for _, target in sorted(targets, key=lambda item: item[0])[:64]:
            road = {
                "start": list(field.coords.from_grid(terminal)),
                "end": list(field.coords.from_grid(target)),
                "width": 3.2,
                "style": "rough",
            }
            connector_anchors: tuple[Point, ...] = (terminal, target)
            if required_outward is not None:
                aligned_approach = add(
                    terminal,
                    mul(required_outward, field.clearance + 2.0),
                )
                connector_anchors = (terminal, aligned_approach, target)
            try:
                candidate = generate_road(
                    field,
                    road,
                    max_segment_length,
                    connector_anchors,
                    directional_state=False,
                )
            except RoadGenerationError:
                continue
            check = validate_roads(field, [candidate.road])
            if check.obstacle_violations or check.invalid_bridge_crossings:
                continue
            if required_outward is not None and any(
                distance(endpoint, terminal) <= 0.10
                for endpoint, _ in missing_bridge_approaches(
                    field, [*roads, candidate.road]
                )
            ):
                continue
            connector = candidate
            break
        if connector is None:
            raise RoadGenerationError(
                f"cannot complete road/bridge terminal near {terminal}"
            )
        roads.append(connector.road)
        connectors.append(connector)
    raise RoadGenerationError("road terminal repair exceeded its safety limit")


def add_boundary_connections(
    field: RoutingField,
    roads: list[dict],
    max_segment_length: float,
) -> list[RouteResult]:
    """Ensure the connected road network enters from two distinct map sides."""

    def touched_sides() -> set[str]:
        result: set[str] = set()
        for road in roads:
            for x, z in (field.coords.to_grid(point) for point in road_points(road)):
                if x <= 1.5:
                    result.add("west")
                if x >= field.width - 2.5:
                    result.add("east")
                if z <= 1.5:
                    result.add("north")
                if z >= field.height - 2.5:
                    result.add("south")
        return result

    connectors: list[RouteResult] = []
    while len(touched_sides()) < 2:
        existing_sides = touched_sides()
        network_points = [
            field.coords.to_grid(point) for road in roads for point in road_points(road)
        ]
        side_targets: dict[str, list[Point]] = {
            "west": [(1.0, float(z)) for z in range(2, field.height - 2, 4)],
            "east": [
                (field.width - 2.0, float(z)) for z in range(2, field.height - 2, 4)
            ],
            "north": [(float(x), 1.0) for x in range(2, field.width - 2, 4)],
            "south": [
                (float(x), field.height - 2.0) for x in range(2, field.width - 2, 4)
            ],
        }
        candidates: list[tuple[float, Point, Point]] = []
        for side, targets in side_targets.items():
            if side in existing_sides:
                continue
            for target in targets:
                if not field.passable(round(target[0]), round(target[1])):
                    continue
                start = min(network_points, key=lambda point: distance(point, target))
                candidates.append((distance(start, target), start, target))

        connector: RouteResult | None = None
        for _, start, end in sorted(candidates, key=lambda item: item[0])[:48]:
            road = {
                "start": list(field.coords.from_grid(start)),
                "end": list(field.coords.from_grid(end)),
                "width": 3.2,
                "style": "rough",
            }
            try:
                candidate = generate_road(
                    field,
                    road,
                    max_segment_length,
                    (start, end),
                    directional_state=False,
                )
            except RoadGenerationError:
                continue
            check = validate_roads(field, [candidate.road])
            if check.obstacle_violations or check.invalid_bridge_crossings:
                continue
            connector = candidate
            break
        if connector is None:
            raise RoadGenerationError("cannot connect road network to two map edges")
        roads.append(connector.road)
        connectors.append(connector)
    return connectors


def replace_top_level_array(source: str, key: str, value: list[dict]) -> str:
    match = re.search(rf'(?m)^  "{re.escape(key)}"\s*:\s*', source)
    if not match:
        closing = source.rfind("}")
        if closing < 0:
            raise RoadGenerationError("map has no top-level JSON object")
        before = source[:closing].rstrip()
        rendered = json.dumps(value, indent=2, ensure_ascii=False).replace("\n", "\n  ")
        separator = "\n" if before.endswith("{") else ",\n"
        suffix = source[closing + 1 :]
        return before + separator + f'  "{key}": ' + rendered + "\n}" + suffix
    start = source.find("[", match.end())
    if start < 0:
        raise RoadGenerationError(f'"{key}" is not an array')
    depth = 0
    in_string = False
    escaped = False
    end = -1
    for index in range(start, len(source)):
        char = source[index]
        if in_string:
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            continue
        if char == '"':
            in_string = True
        elif char == "[":
            depth += 1
        elif char == "]":
            depth -= 1
            if depth == 0:
                end = index + 1
                break
    if end < 0:
        raise RoadGenerationError(f'could not find the end of "{key}"')
    rendered = json.dumps(value, indent=2, ensure_ascii=False)
    rendered = rendered.replace("\n", "\n  ")
    return source[:start] + rendered + source[end:]


def process_map(
    path: Path,
    *,
    clearance: float,
    max_segment_length: float,
    write: bool,
    validate_only: bool,
) -> bool:
    source = path.read_text(encoding="utf-8")
    definition = json.loads(source)
    roads = definition.get("roads") or []
    if not roads:
        raise RoadGenerationError("map has no roads")
    max_half_width = max(float(road.get("width", 3.0)) * 0.5 for road in roads)
    style_counts: dict[str, int] = {}
    for road in roads:
        style = str(road.get("style", "default"))
        style_counts[style] = style_counts.get(style, 0) + 1
    map_road_style = max(style_counts, key=style_counts.get)
    field = RoutingField(definition, clearance + max_half_width)

    if validate_only:
        validation = validate_roads(field, roads)
        authored_bridges = [
            Bridge(
                field.coords.to_grid(item["start"]),
                field.coords.to_grid(item["end"]),
                field.coords.distance_to_grid(float(item.get("width", 3.0))),
            )
            for item in definition.get("bridges") or []
            if item.get("start") and item.get("end")
        ]
        validation.invalid_bridge_spans = invalid_bridge_geometry(
            field, authored_bridges
        )
        print_validation(path, validation, "existing")
        return validation.passed

    generated: list[dict] = []
    old_total = 0.0
    new_total = 0.0
    used_bridges: set[int] = set()
    anchors = network_anchors(field, roads)
    for index, road in enumerate(roads):
        try:
            result = generate_road(field, road, max_segment_length, anchors[index])
        except RoadGenerationError as error:
            print(f"  road {index + 1:02d}: skipped ({error})")
            continue
        if result.new_length > max(result.old_length * 2.25, result.old_length + 80.0):
            print(
                f"  road {index + 1:02d}: skipped "
                f"({result.old_length:.1f} -> {result.new_length:.1f} detour)"
            )
            continue
        road_validation = validate_roads(field, [result.road])
        if (
            road_validation.obstacle_violations
            or road_validation.invalid_bridge_crossings
        ):
            print(f"  road {index + 1:02d}: skipped (unsafe generated geometry)")
            continue
        generated.append(result.road)
        old_total += result.old_length
        new_total += result.new_length
        used_bridges.update(result.bridge_ids)
        print(
            f"  road {index + 1:02d}: {result.old_length:6.1f} -> "
            f"{result.new_length:6.1f}, {len(result.road['waypoints']):2d} points"
        )

    connectors = connect_road_components(field, generated, max_segment_length)
    for connector_index, connector in enumerate(connectors, 1):
        old_total += connector.old_length
        new_total += connector.new_length
        used_bridges.update(connector.bridge_ids)
        print(
            f"  connector {connector_index:02d}: {connector.new_length:6.1f}, "
            f"{len(connector.road['waypoints']):2d} points"
        )

    edge_connectors = add_boundary_connections(field, generated, max_segment_length)
    for connector_index, connector in enumerate(edge_connectors, 1):
        old_total += connector.old_length
        new_total += connector.new_length
        used_bridges.update(connector.bridge_ids)
        print(
            f"  edge connector {connector_index:02d}: "
            f"{connector.new_length:6.1f}, "
            f"{len(connector.road['waypoints']):2d} points"
        )

    terminal_connectors = connect_incomplete_terminals(
        field, generated, max_segment_length
    )
    for connector_index, connector in enumerate(terminal_connectors, 1):
        old_total += connector.old_length
        new_total += connector.new_length
        used_bridges.update(connector.bridge_ids)
        print(
            f"  terminal connector {connector_index:02d}: "
            f"{connector.new_length:6.1f}, "
            f"{len(connector.road['waypoints']):2d} points"
        )

    for road in generated:
        road["style"] = map_road_style

    validation = validate_roads(field, generated)
    print_validation(path, validation, "generated")
    print(
        f"  length: {old_total:.1f} -> {new_total:.1f}; "
        f"bridges crossed by a road: {len(used_bridges)}/{len(field.bridges)}; "
        f"bridges retained with two approaches: {len(field.bridges)}"
    )
    if not validation.passed:
        return False
    if write:
        updated = replace_top_level_array(source, "roads", generated)
        if "bridges" in definition or used_bridges:
            updated = replace_top_level_array(
                updated, "bridges", field.generated_bridges()
            )
        temporary = path.with_suffix(path.suffix + ".roadgen.tmp")
        temporary.write_text(updated, encoding="utf-8")
        temporary.replace(path)
        print(f"  wrote {path}")
    else:
        print("  dry-run: pass --write to update the roads array")
    return True


def print_validation(path: Path, validation: ValidationResult, label: str) -> None:
    status = "PASS" if validation.passed else "FAIL"
    print(
        f"{path} [{label}] {status}: roads={validation.road_count}, "
        f"components={validation.component_count}, "
        f"obstacle_violations={validation.obstacle_violations}, "
        f"invalid_bridge_crossings={validation.invalid_bridge_crossings}, "
        f"dangling_endpoints={validation.dangling_endpoints}, "
        f"invalid_bridge_spans={validation.invalid_bridge_spans}, "
        f"orphan_bridge_ends={validation.orphan_bridge_ends}, "
        f"missing_bridge_approaches={validation.missing_bridge_approaches}, "
        f"road_styles={validation.road_style_count}, "
        f"boundary_sides={validation.boundary_sides}"
    )


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
        help="replace the roads array after successful generation and validation",
    )
    parser.add_argument(
        "--validate-only",
        action="store_true",
        help="validate existing roads without generating replacements",
    )
    parser.add_argument(
        "--clearance",
        type=float,
        default=1.25,
        help="extra world-space obstacle clearance beyond half the road width",
    )
    parser.add_argument(
        "--max-segment-length",
        type=float,
        default=18.0,
        help="maximum world-space distance between emitted waypoints",
    )
    args = parser.parse_args(argv)
    if args.write and args.validate_only:
        parser.error("--write and --validate-only cannot be combined")
    if args.clearance < 0.0 or args.max_segment_length <= 1.0:
        parser.error("clearance must be non-negative and max segment length > 1")
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
                clearance=args.clearance,
                max_segment_length=args.max_segment_length,
                write=args.write,
                validate_only=args.validate_only,
            ):
                failures += 1
        except (OSError, json.JSONDecodeError, RoadGenerationError) as error:
            print(f"{path}: ERROR: {error}", file=sys.stderr)
            failures += 1
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
