#!/usr/bin/env python3
"""The points a map's water feature is made of, in authored coordinates.

A river is either a polyline (``start``/``end``/``waypoints``) or a ring
(``"shape": "ring"`` with ``x``, ``z``, ``radius`` and optionally ``radius_z``
and ``segments``): a moat, an oxbow, a lake outlet that closes on itself. The
engine expands a ring in ``game/map/river_geometry.h``; this is that expansion
for the tools, and the two must agree point for point or a road generated
against the tools' moat lands in the engine's water.
"""

from __future__ import annotations

import math

RING_DEFAULT_SEGMENTS = 48
RING_MIN_SEGMENTS = 12
RING_MAX_SEGMENTS = 256


def is_ring_river(feature: dict) -> bool:
    return str(feature.get("shape", "")).lower() == "ring"


def ring_river_points(feature: dict) -> list[list[float]]:
    """Closed loop, first point repeated last, as the engine lays it."""
    cx = float(feature.get("x", 0.0))
    cz = float(feature.get("z", 0.0))
    radius_x = float(feature.get("radius", 0.0))
    radius_z = float(feature.get("radius_z", radius_x))
    segments = int(feature.get("segments", RING_DEFAULT_SEGMENTS))
    segments = max(RING_MIN_SEGMENTS, min(RING_MAX_SEGMENTS, segments))
    points: list[list[float]] = []
    for index in range(segments + 1):
        angle = 2.0 * math.pi * (index % segments) / segments
        points.append(
            [cx + math.cos(angle) * radius_x, cz + math.sin(angle) * radius_z]
        )
    return points


def river_points(feature: dict) -> list[list[float]]:
    """Every point of a water feature, whichever way it was authored."""
    if is_ring_river(feature):
        return ring_river_points(feature)
    raw = feature.get("waypoints") or [feature.get("start"), feature.get("end")]
    return [[float(point[0]), float(point[1])] for point in raw if point]
