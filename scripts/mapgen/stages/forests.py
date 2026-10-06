"""Forest intent: boundary belts, ambush woods and flank screens.

A wood is closed to horses, siege and elephants, so where it grows decides who
can go where. Belts close the stretches of a forest boundary the routes do not
use, ambush woods crowd the roads where infantry can wait, and screens shut
the outer flanks. Every wood keeps its distance from settlements.
"""

from __future__ import annotations

import math

from ..context import Context
from ..geometry import (
    FOREST,
    HILL,
    LANDMARK,
    MOUNTAIN,
    ROAD,
    SETTLEMENT,
    WATER,
    Field,
    Point,
    feature_points,
    round2,
    sample_polyline,
)

BLOCKERS = WATER | HILL | MOUNTAIN | ROAD | LANDMARK | FOREST


def _fits(field: Field, p: Point, radius: float, clearance: float) -> bool:
    if field.blocked(p[0], p[1], radius + 3.0, BLOCKERS):
        return False
    return not field.blocked(p[0], p[1], radius + clearance, SETTLEMENT)


def _add(ctx: Context, field: Field, p: Point, radius: float) -> None:
    name = ctx.namer.forest(ctx.rng)
    ctx.add(
        "forests",
        {
            "id": ctx.namer.make_id("forest", name),
            "name": name,
            "x": round2(p[0]),
            "z": round2(p[1]),
            "radius": round2(radius),
        },
    )
    field.mark_disc(p[0], p[1], radius, FOREST)


def _radius(ctx: Context) -> float:
    size = float(ctx.params["forests.size"])
    return (16.0 + 24.0 * size) * ctx.rng.uniform(0.8, 1.2)


def _belts(ctx: Context, field: Field, clearance: float) -> int:
    from .terrain import _ridge_segments

    placed = 0
    for boundary in ctx.layout.boundaries:
        if boundary.kind != "forest":
            continue
        for v0, v1 in _ridge_segments(ctx, boundary.u):
            radius = _radius(ctx)
            inset = radius / ctx.layout.frame.v_length
            if v1 - v0 <= inset * 2.0:
                continue
            a = ctx.world(boundary.u, v0 + inset)
            b = ctx.world(boundary.u, v1 - inset)
            for p in sample_polyline([a, b], radius * 1.45):
                u, v = ctx.layout.frame.to_layout(*p)
                q = ctx.world(u + ctx.rng.uniform(-0.012, 0.012), v)
                if _fits(field, q, radius, clearance):
                    _add(ctx, field, q, radius)
                    placed += 1
    return placed


def _ambush(ctx: Context, field: Field, wanted: int, clearance: float) -> int:
    roads = [feature_points(road) for road in ctx.map.get("roads", [])]
    roads = [points for points in roads if len(points) >= 2]
    if not roads:
        return 0
    placed = 0
    attempts = 0
    while placed < wanted and attempts < wanted * 25:
        attempts += 1
        points = roads[ctx.rng.randrange(len(roads))]
        samples = sample_polyline(points, 20.0)
        if len(samples) < 3:
            continue
        i = ctx.rng.randrange(1, len(samples) - 1)
        p, q = samples[i - 1], samples[i + 1]
        tangent = (q[0] - p[0], q[1] - p[1])
        norm = math.hypot(*tangent) or 1.0
        normal = (-tangent[1] / norm, tangent[0] / norm)
        radius = _radius(ctx) * 0.85
        side = ctx.rng.choice((-1.0, 1.0))
        reach = radius + 9.0 + ctx.rng.uniform(0.0, 6.0)
        centre = (
            samples[i][0] + normal[0] * reach * side,
            samples[i][1] + normal[1] * reach * side,
        )
        u = ctx.layout.frame.to_layout(*centre)[0]
        if not 0.15 < u < 0.85:
            continue
        if _fits(field, centre, radius, clearance):
            _add(ctx, field, centre, radius)
            placed += 1
    return placed


def _screens(ctx: Context, field: Field, wanted: int, clearance: float) -> int:
    layout = ctx.layout
    placed = 0
    attempts = 0
    while placed < wanted and attempts < wanted * 25:
        attempts += 1
        radius = _radius(ctx)
        side = ctx.rng.choice([s for s in (0, 1) if s != layout.coast_side] or [0])
        edge = (radius * 0.9) / layout.frame.v_length
        v = layout.v_range[0] + edge + ctx.rng.uniform(0.0, 0.08)
        if side == 1:
            v = layout.v_range[1] - edge - ctx.rng.uniform(0.0, 0.08)
        u = ctx.rng.uniform(0.15, 0.5 if layout.symmetric else 0.85)
        p = ctx.world(u, v)
        if not _fits(field, p, radius, clearance):
            continue
        _add(ctx, field, p, radius)
        placed += 1
        if layout.symmetric:
            twin = ctx.mirror(p)
            if _fits(field, twin, radius, clearance):
                _add(ctx, field, twin, radius)
                placed += 1
    return placed


def synthesise(ctx: Context) -> None:
    params = ctx.params
    field = ctx.field(reserved=False)
    clearance = float(params["forests.clearance"])
    density = float(params["forests.density"]) * (
        1.0 - 0.4 * float(params["tactical.formation_space"])
    )
    wanted = round(density * ctx.layout.sectors * 3.0)
    belts = _belts(ctx, field, clearance)
    ambush_share = float(params["forests.ambush"])
    ambush = _ambush(ctx, field, round(wanted * ambush_share), clearance)
    screens = _screens(ctx, field, wanted - round(wanted * ambush_share), clearance)
    ctx.note(f"{belts} belt woods, {ambush} ambush woods, {screens} flank screens")
