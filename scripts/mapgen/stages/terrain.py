"""Terrain intent: mountain flanks, ridge boundaries and defended hills.

Hills are positions, not decoration. Each one stands beside a route with its
entrances turned towards it, so holding the hill means watching the road. A
ridge boundary is a wall with the routes' gaps left open in it, and mountains
only ever close the flanks.
"""

from __future__ import annotations

import math

from ..context import Context
from ..geometry import (
    FOREST,
    HILL,
    MOUNTAIN,
    RESERVED,
    ROAD,
    SETTLEMENT,
    WATER,
    Field,
    Point,
    dist,
    footprint_hits,
    mark_terrain,
    nearest_on_polyline,
    round2,
)

BLOCKERS = WATER | HILL | MOUNTAIN | SETTLEMENT | ROAD | RESERVED | FOREST


def _rim_point(entry: dict, towards: Point) -> Point:
    """Where the hill's edge faces ``towards``."""
    cx, cz = float(entry["x"]), float(entry["z"])
    hw = float(entry.get("width", 20.0)) * 0.5
    hd = float(entry.get("depth", 20.0)) * 0.5
    angle = math.radians(float(entry.get("rotation", 0.0)))
    c, s = math.cos(angle), math.sin(angle)
    dx, dz = towards[0] - cx, towards[1] - cz
    lx, lz = dx * c + dz * s, -dx * s + dz * c
    norm = math.hypot(lx, lz) or 1.0
    lx, lz = lx / norm, lz / norm
    t = 1.0 / math.sqrt((lx / hw) ** 2 + (lz / hd) ** 2)
    rx, rz = lx * t, lz * t
    return (cx + rx * c - rz * s, cz + rx * s + rz * c)


def _entrances_towards(
    entry: dict, target: Point, count: int, spread: float
) -> list[dict]:
    cx, cz = float(entry["x"]), float(entry["z"])
    base = math.atan2(target[1] - cz, target[0] - cx)
    offsets = (
        [0.0]
        if count == 1
        else [spread * (i / (count - 1) - 0.5) * 2.0 for i in range(count)]
    )
    entrances = []
    for offset in offsets:
        angle = base + math.radians(offset)
        probe = (cx + math.cos(angle) * 500.0, cz + math.sin(angle) * 500.0)
        x, z = _rim_point(entry, probe)
        entrances.append({"x": round2(x), "z": round2(z), "radius": 9.0})
    return entrances


def _fits(
    field: Field, entry: dict, pad: float, mask: int = BLOCKERS, inside: bool = True
) -> bool:
    if footprint_hits(field, entry, pad, mask):
        return False
    if not inside:
        return True
    hw = (
        max(float(entry.get("width", 0.0)), float(entry.get("radius", 0.0)) * 2.0) * 0.5
    )
    hd = (
        max(float(entry.get("depth", 0.0)), float(entry.get("radius", 0.0)) * 2.0) * 0.5
    )
    reach = max(hw, hd) * 0.6
    x, z = float(entry["x"]), float(entry["z"])
    return reach < x < field.width - reach and reach < z < field.height - reach


def _two_approaches(entry: dict) -> bool:
    from ..validate import entrance_clusters

    return entrance_clusters(entry) >= 2


def _commit(ctx: Context, field: Field, entry: dict) -> None:
    ctx.add("terrain", entry)
    mark_terrain(field, entry, 6.0)


def _lane_target(ctx: Context, p: Point) -> Point:
    best = None
    for lane in range(len(ctx.layout.lanes)):
        q = nearest_on_polyline(p, ctx.layout.lane_world(lane))
        if best is None or dist(p, q) < dist(p, best):
            best = q
    return best or p


def _v_axis_rotation(ctx: Context) -> float:
    return 0.0 if ctx.layout.frame.orientation in (0, 1) else 90.0


def _mountains(ctx: Context, field: Field) -> None:
    density = float(ctx.params["terrain.mountains"])
    if density <= 0.05:
        return
    rng = ctx.rng
    relief = float(ctx.params["terrain.relief"])
    frame = ctx.layout.frame
    sides = [side for side in (0, 1) if side != ctx.layout.coast_side]
    per_side = max(1, round(density * 5))
    rotation = 90.0 - _v_axis_rotation(ctx)
    for side in sides:
        for i in range(per_side):
            u = 0.14 + (0.72 * (i + 0.5) / per_side) + rng.uniform(-0.03, 0.03)
            along = rng.uniform(0.16, 0.24) * frame.u_length
            across = rng.uniform(0.11, 0.16) * frame.v_length * (0.7 + 0.5 * density)
            v = (
                across / frame.v_length * 0.45
                if side == 0
                else 1.0 - across / frame.v_length * 0.45
            )
            x, z = ctx.world(u, v)
            entry = {
                "type": "mountain",
                "x": round2(x),
                "z": round2(z),
                "width": round2(along),
                "depth": round2(across),
                "height": round2(8.0 + 5.0 * relief + rng.uniform(0.0, 2.0)),
                "rotation": round2(rotation + rng.uniform(-8.0, 8.0)),
            }
            probe = dict(entry)
            if _fits(
                field, probe, 4.0, WATER | SETTLEMENT | ROAD | RESERVED, inside=False
            ):
                _commit(ctx, field, entry)


def _ridge_segments(ctx: Context, u: float) -> list[tuple[float, float]]:
    """Spans of a boundary line left after the routes' gaps are cut out."""
    frame = ctx.layout.frame
    v_lo, v_hi = ctx.layout.v_range
    v_lo, v_hi = v_lo + 0.04, v_hi - 0.04
    gap = 34.0 / frame.v_length
    cuts = sorted(ctx.layout.lane_v(lane, u) for lane in range(len(ctx.layout.lanes)))
    spans = []
    cursor = v_lo
    for cut in cuts:
        if cut - gap > cursor:
            spans.append((cursor, cut - gap))
        cursor = max(cursor, cut + gap)
    if cursor < v_hi:
        spans.append((cursor, v_hi))
    return spans


def _ridges(ctx: Context, field: Field) -> None:
    rng = ctx.rng
    relief = float(ctx.params["terrain.relief"])
    choke = float(ctx.params["tactical.choke_intensity"])
    frame = ctx.layout.frame
    rotation = _v_axis_rotation(ctx)
    for boundary in ctx.layout.boundaries:
        if boundary.kind != "ridge":
            continue
        for v0, v1 in _ridge_segments(ctx, boundary.u):
            length = (v1 - v0) * frame.v_length
            closed = length * (0.45 + 0.5 * choke)
            pieces = max(1, round(closed / 150.0))
            piece = closed / pieces
            for i in range(pieces):
                if piece < 30.0:
                    continue
                centre_v = v0 + (v1 - v0) * (i + 0.5) / pieces
                x, z = ctx.world(boundary.u + rng.uniform(-0.01, 0.01), centre_v)
                thickness = 10.0 + 6.0 * relief + rng.uniform(0.0, 3.0)
                entry = {
                    "type": "hill",
                    "shape": "corridor",
                    "x": round2(x),
                    "z": round2(z),
                    "width": round2(piece * rng.uniform(0.8, 0.95)),
                    "depth": round2(thickness),
                    "thickness": round2(thickness),
                    "height": round2(2.2 + 1.2 * relief),
                    "rotation": round2(rotation + rng.uniform(-6.0, 6.0)),
                }
                entry["entrances"] = _ridge_entrances(ctx, entry, choke)
                if _two_approaches(entry) and _fits(field, entry, 3.0):
                    _commit(ctx, field, entry)


def _ridge_entrances(ctx: Context, entry: dict, choke: float) -> list[dict]:
    """Ramps near both ends of a corridor ridge.

    On opposite faces they make a crossing - up one side, along the crown,
    down the other; on the enemy-facing side only, when chokes are high, the
    ridge is a position to hold rather than a way through. Either way the two
    ramps sit at opposite ends, because ramps closer than their own radii are
    one approach to the engine.
    """
    cx, cz = float(entry["x"]), float(entry["z"])
    half_length = float(entry["width"]) * 0.5 - float(entry["thickness"]) * 0.5
    half_thick = float(entry["thickness"]) * 0.5
    angle = math.radians(float(entry["rotation"]))
    along = (math.cos(angle), math.sin(angle))
    normal = (-along[1], along[0])
    reach = half_thick + 0.5
    ends = [
        (cx + along[0] * half_length * 0.8, cz + along[1] * half_length * 0.8),
        (cx - along[0] * half_length * 0.8, cz - along[1] * half_length * 0.8),
    ]
    enemy = ctx.world(1.0, 0.5)
    toward_enemy = (
        1.0 if (enemy[0] - cx) * normal[0] + (enemy[1] - cz) * normal[1] > 0 else -1.0
    )
    faces = (
        (toward_enemy, -toward_enemy) if choke < 0.6 else (toward_enemy, toward_enemy)
    )
    return [
        {
            "x": round2(end[0] + normal[0] * reach * face),
            "z": round2(end[1] + normal[1] * reach * face),
            "radius": 8.0,
        }
        for end, face in zip(ends, faces, strict=True)
    ]


def _shaped_hill(ctx: Context, p: Point, target: Point, size: float) -> dict:
    rng = ctx.rng
    relief = float(ctx.params["terrain.relief"])
    away = math.degrees(math.atan2(p[1] - target[1], p[0] - target[0]))
    height = round2(2.0 + 1.4 * relief + rng.uniform(0.0, 0.5))
    roll = rng.random()
    ridge_bias = float(ctx.params["terrain.ridge_bias"])
    if roll > ridge_bias:
        entry = {
            "type": "hill",
            "x": round2(p[0]),
            "z": round2(p[1]),
            "width": round2(size * rng.uniform(0.95, 1.25)),
            "depth": round2(size * rng.uniform(0.6, 0.8)),
            "height": height,
            "rotation": round2(away + 90.0 + rng.uniform(-20.0, 20.0)),
        }
        count = rng.choice((2, 2, 3))
        for spread in (32.0, 45.0, 60.0, 80.0):
            entry["entrances"] = _entrances_towards(entry, target, count, spread)
            if _two_approaches(entry):
                break
        return entry
    shape = rng.choice(("arc", "arc", "elbow", "ring", "corridor"))
    thickness = round2(9.0 + 5.0 * relief)
    entry = {
        "type": "hill",
        "shape": shape,
        "x": round2(p[0]),
        "z": round2(p[1]),
        "width": round2(size * 1.05),
        "depth": round2(size * (0.4 if shape == "corridor" else 0.95)),
        "thickness": thickness,
        "height": height,
        "rotation": round2(away if shape != "corridor" else away + 90.0),
    }
    if shape == "arc":
        entry["arc"] = round2(rng.uniform(110.0, 160.0))
        entry["taper"] = 0.3
    if shape == "elbow":
        entry["rotation"] = round2(away + 135.0)
    entry["entrances"] = _shaped_entrances(entry, target)
    return entry


def _shaped_entrances(entry: dict, target: Point) -> list[dict]:
    """Two ramps on a ridge's spine, apart, on the side facing the road.

    The engine counts a hill's approaches by clustering its entrances, so two
    ramps at one spot are one approach. A ring's spine closes on itself - its
    first and last points are the same place - and is walked as a loop; a
    two-point corridor uses both of its ends.
    """
    from map_hill_shapes import hill_shape_strokes

    strokes, half_thickness = hill_shape_strokes(entry)
    if not strokes:
        return _entrances_towards(entry, target, 2, 30.0)
    points = [strokes[0][0]] + [b for _, b in strokes]
    closed = len(points) > 2 and dist(points[0], points[-1]) < 0.5
    if closed:
        points = points[:-1]
    count = len(points)

    def gap(i: int, j: int) -> int:
        span = abs(i - j)
        return min(span, count - span) if closed else span

    ranked = sorted(range(count), key=lambda i: dist(points[i], target))
    chosen = [ranked[0]]
    wanted_gap = max(1, count // 3) if count > 2 else 1
    for index in ranked[1:]:
        if all(gap(index, other) >= wanted_gap for other in chosen):
            chosen.append(index)
        if len(chosen) == 2:
            break

    entrances = []
    for index in chosen:
        if closed:
            a, b = points[index - 1], points[(index + 1) % count]
        else:
            a = points[max(index - 1, 0)]
            b = points[min(index + 1, count - 1)]
        tangent = (b[0] - a[0], b[1] - a[1])
        norm = math.hypot(*tangent) or 1.0
        normal = (-tangent[1] / norm, tangent[0] / norm)
        p = points[index]
        side_a = (p[0] + normal[0] * half_thickness, p[1] + normal[1] * half_thickness)
        side_b = (p[0] - normal[0] * half_thickness, p[1] - normal[1] * half_thickness)
        rim = min((side_a, side_b), key=lambda q: dist(q, target))
        entrances.append({"x": round2(rim[0]), "z": round2(rim[1]), "radius": 8.0})
    return entrances


def _defended_hills(ctx: Context, field: Field) -> None:
    rng = ctx.rng
    params = ctx.params
    layout = ctx.layout
    density = float(params["terrain.hills"]) * (
        1.0 - 0.45 * float(params["tactical.formation_space"])
    )
    wanted = round(density * layout.sectors * 2.4)
    if wanted <= 0:
        return
    relief = float(params["terrain.relief"])
    frame = layout.frame
    half = layout.symmetric
    placed = 0
    attempts = 0
    while placed < wanted and attempts < wanted * 12:
        attempts += 1
        lane = rng.randrange(len(layout.lanes))
        u = rng.uniform(0.12, 0.5 if half else 0.88)
        side = rng.choice((-1.0, 1.0))
        size = (52.0 + 50.0 * relief) * rng.uniform(0.85, 1.15)
        offset = (size * 0.55 + 26.0 + rng.uniform(0.0, 30.0)) / frame.v_length
        v = layout.lane_v(lane, u) + side * offset
        if not layout.v_range[0] + 0.05 < v < layout.v_range[1] - 0.05:
            continue
        p = ctx.world(u, v)
        target = _lane_target(ctx, p)
        entry = _shaped_hill(ctx, p, target, size)
        if not _two_approaches(entry) or not _fits(field, entry, 6.0):
            continue
        _commit(ctx, field, entry)
        placed += 1
        if half:
            twin = _mirrored(ctx, entry)
            if _fits(field, twin, 6.0):
                _commit(ctx, field, twin)


def _mirrored(ctx: Context, entry: dict) -> dict:
    twin = dict(entry)
    x, z = ctx.mirror((float(entry["x"]), float(entry["z"])))
    twin["x"], twin["z"] = round2(x), round2(z)
    twin["rotation"] = round2(float(entry.get("rotation", 0.0)) + 180.0)
    twin["entrances"] = [
        {
            **e,
            "x": round2(ctx.mirror((e["x"], e["z"]))[0]),
            "z": round2(ctx.mirror((e["x"], e["z"]))[1]),
        }
        for e in entry.get("entrances", [])
    ]
    twin.pop("generated", None)
    return twin


def _pedestals(ctx: Context, field: Field) -> None:
    """Hills raised under some fort sites, for hilltop settlements."""
    chance = float(ctx.params["settlements.hilltop"])
    forts = int(ctx.params["settlements.forts"]) + int(ctx.params["settlements.camps"])
    if chance <= 0.0 or forts == 0:
        return
    rng = ctx.rng
    wanted = sum(1 for _ in range(forts) if rng.random() < chance)
    sites = [site for site in ctx.layout.sites if site.role == "fort"]
    placed = 0
    for site in sites:
        if placed >= wanted:
            break
        p = ctx.world(site.u, site.v + (0.12 if site.v < 0.5 else -0.12))
        size = rng.uniform(78.0, 96.0)
        entry = {
            "type": "hill",
            "role": "pedestal",
            "x": round2(p[0]),
            "z": round2(p[1]),
            "width": round2(size),
            "depth": round2(size * 0.85),
            "height": round2(rng.uniform(2.2, 2.8)),
            "rotation": round2(rng.uniform(0.0, 180.0)),
        }
        entry["entrances"] = _entrances_towards(entry, _lane_target(ctx, p), 2, 40.0)
        if _two_approaches(entry) and _fits(field, entry, 8.0):
            _commit(ctx, field, entry)
            placed += 1


def synthesise(ctx: Context) -> None:
    field = ctx.field()
    _mountains(ctx, field)
    _pedestals(ctx, field)
    _ridges(ctx, field)
    _defended_hills(ctx, field)
    hills = [t for t in ctx.map.get("terrain", []) if t.get("generated") == "terrain"]
    ctx.note(
        f"{sum(1 for t in hills if t['type'] == 'hill')} hills, "
        f"{sum(1 for t in hills if t['type'] == 'mountain')} mountains"
    )
