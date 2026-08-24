#!/usr/bin/env python3
"""Shared hill-shape geometry for the map generators.

`game/map/hill_shape.h` lets a hill be a ridge rather than a mound: a corridor,
an arc (boomerang), an elbow, a ring or a hand-drawn path. The engine builds a
spine for those shapes and raises ground only within `thickness` of it, so the
concave pocket of an arc is open ground a road may cross and a camp may stand
in.

The road, water and settlement generators used to raster every hill as its
bounding ellipse, which blocks that pocket and makes a boomerang ridge unusable
as a corridor wall. This module returns the spine instead, in the map's own
authoring units, so each generator can lay capsules along it.

`hill_shape_strokes` returns an empty list for a blob or a mask, which is the
signal to fall back to the ellipse the caller already knows how to raster.
"""

from __future__ import annotations

import math
from typing import Sequence

Point = tuple[float, float]

MIN_HALF_THICKNESS = 1.25
DEFAULT_THICKNESS_RATIO = 0.34
ARC_SEGMENT_DEG = 9.0
DEFAULT_SWEEP_DEG = 120.0
DEFAULT_ELBOW_SWEEP_DEG = 90.0

_SPINE_ALIASES = {
    "corridor": "corridor",
    "ridge": "corridor",
    "wall": "corridor",
    "capsule": "corridor",
    "arc": "arc",
    "boomerang": "arc",
    "crescent": "arc",
    "banana": "arc",
    "horseshoe": "arc",
    "elbow": "elbow",
    "corner": "elbow",
    "l": "elbow",
    "chevron": "elbow",
    "ring": "ring",
    "crater": "ring",
    "atoll": "ring",
    "path": "path",
    "polyline": "path",
    "spline": "path",
    "custom": "path",
}


def canonical_hill_shape(name: object) -> str:
    """Map an authored shape name onto the spine kind it builds, or ``""``."""

    if not isinstance(name, str):
        return ""
    return _SPINE_ALIASES.get(name.strip().lower(), "")


def hill_half_extents(feature: dict) -> tuple[float, float]:
    """Authored half width and half depth in map units."""

    radius = float(feature.get("radius", 5.0) or 0.0)
    width = float(feature.get("width", 0.0) or 0.0) or radius * 2.0
    depth = float(feature.get("depth", 0.0) or 0.0) or radius * 2.0
    return max(width * 0.5, 0.5), max(depth * 0.5, 0.5)


def hill_half_thickness(feature: dict) -> float:
    """Half thickness of the ridge, matching `build_hill_shape`."""

    kind = canonical_hill_shape(feature.get("shape"))
    half_width, half_depth = hill_half_extents(feature)
    if kind == "corridor":
        default = min(half_width, half_depth)
    else:
        default = max(
            MIN_HALF_THICKNESS, min(half_width, half_depth) * DEFAULT_THICKNESS_RATIO
        )
    authored = float(feature.get("thickness", 0.0) or 0.0) * 0.5
    thickness = authored if authored > 0.0 else default
    if kind != "path":
        thickness = min(thickness, min(half_width, half_depth))
    return max(thickness, MIN_HALF_THICKNESS)


def _arc_points(
    radius_x: float, radius_z: float, start_deg: float, sweep_deg: float
) -> list[Point]:
    steps = max(6, int(math.ceil(abs(sweep_deg) / ARC_SEGMENT_DEG)))
    points: list[Point] = []
    for step in range(steps + 1):
        angle = math.radians(start_deg + sweep_deg * (step / steps))
        points.append((radius_x * math.cos(angle), radius_z * math.sin(angle)))
    return points


def _local_spine(feature: dict, kind: str) -> list[Point]:
    half_width, half_depth = hill_half_extents(feature)
    half_thickness = hill_half_thickness(feature)
    inset_x = max(half_width - half_thickness, 0.0)
    inset_z = max(half_depth - half_thickness, 0.0)

    if kind == "corridor":
        if half_width >= half_depth:
            return [(-inset_x, 0.0), (inset_x, 0.0)]
        return [(0.0, -inset_z), (0.0, inset_z)]

    if kind in {"arc", "ring"}:
        default_sweep = 360.0 if kind == "ring" else DEFAULT_SWEEP_DEG
        sweep = float(feature.get("arc", default_sweep))
        sweep = min(max(sweep, 5.0), 360.0)
        start = float(feature.get("arc_start", -sweep * 0.5))
        points = _arc_points(inset_x, inset_z, start, sweep)
        if sweep >= 359.0 and len(points) >= 2:
            points[-1] = points[0]
        return points

    if kind == "elbow":
        sweep = min(
            max(float(feature.get("arc", DEFAULT_ELBOW_SWEEP_DEG)), 15.0), 345.0
        )
        corner = (-inset_x, -inset_z)
        angle = math.radians(sweep)
        return [
            (corner[0] + inset_x * 2.0, corner[1]),
            corner,
            (
                corner[0] + math.cos(angle) * inset_z * 2.0,
                corner[1] + math.sin(angle) * inset_z * 2.0,
            ),
        ]

    return []


def hill_shape_strokes(feature: dict) -> tuple[list[tuple[Point, Point]], float]:
    """Spine segments in map units plus the ridge half thickness.

    An empty segment list means the feature has no spine and the caller should
    raster it as an ellipse, which is what a blob or a painted mask is.
    """

    if str(feature.get("type", "")).lower() != "hill":
        return [], 0.0
    kind = canonical_hill_shape(feature.get("shape"))
    if not kind:
        return [], 0.0

    center_x = float(feature.get("x", 0.0))
    center_z = float(feature.get("z", 0.0))
    half_thickness = hill_half_thickness(feature)

    if kind == "path":
        authored: Sequence[object] = feature.get("points") or []
        points: list[Point] = []
        for entry in authored:
            if isinstance(entry, dict):
                points.append((float(entry.get("x", 0.0)), float(entry.get("z", 0.0))))
            elif isinstance(entry, (list, tuple)) and len(entry) >= 2:
                points.append((float(entry[0]), float(entry[1])))
        if len(points) < 2:
            return [], 0.0
        return list(zip(points, points[1:], strict=False)), half_thickness

    local = _local_spine(feature, kind)
    if len(local) < 2:
        return [], 0.0

    angle = math.radians(float(feature.get("rotation", 0.0)))
    cosine, sine = math.cos(angle), math.sin(angle)
    world = [
        (
            center_x + local_x * cosine - local_z * sine,
            center_z + local_x * sine + local_z * cosine,
        )
        for local_x, local_z in local
    ]
    return list(zip(world, world[1:], strict=False)), half_thickness
