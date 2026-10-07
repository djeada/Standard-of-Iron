"""Water intent: river barriers, flank tarns, meres, a coast and a moat.

A river is a sector boundary, authored as a guide from one map edge to the
other (or to a flank lake). ``generate-map-water.py`` reroutes it around high
ground and meanders it; the road stage later bridges it wherever a route
crosses. A lake either closes a flank at the map edge or, on a wetland map,
breaks up the interior.
"""

from __future__ import annotations

from ..context import Context
from ..geometry import (
    HILL,
    MOUNTAIN,
    RESERVED,
    ROAD,
    SETTLEMENT,
    WATER,
    Field,
    Point,
    pt,
    round2,
)

LAKE_BLOCKERS = WATER | HILL | MOUNTAIN | SETTLEMENT | ROAD | RESERVED


def _lake_fits(
    field: Field, x: float, z: float, hw: float, hd: float, rotation: float
) -> bool:
    from ..geometry import Collector

    probe = Collector(field.width, field.height)
    probe.mark_ellipse(x, z, hw, hd, rotation, 1, 6.0)
    cells = field.cells
    return not any(cells[i] & LAKE_BLOCKERS for i in probe.touched())


def _extents(ctx: Context, along: float, across: float) -> tuple[float, float]:
    """Width (x) and depth (z) of a lake laid ``along`` the u axis.

    Lakes are never rotated: the water tool samples a lake's extent along the
    map axes, so a rotated ellipse would be validated against ground it does
    not cover.
    """
    if ctx.layout.frame.orientation in (0, 1):
        return across, along
    return along, across


def _add_lake(
    ctx: Context, field: Field, centre: Point, along: float, across: float, label: str
) -> dict | None:
    width, depth = _extents(ctx, along, across)
    if not _lake_fits(field, centre[0], centre[1], width * 0.5, depth * 0.5, 0.0):
        return None
    entry = {
        "id": ctx.namer.make_id("lake", label),
        "x": round2(centre[0]),
        "z": round2(centre[1]),
        "width": round2(width),
        "depth": round2(depth),
    }
    ctx.add("lakes", entry)
    field.mark_ellipse(centre[0], centre[1], width * 0.5, depth * 0.5, 0.0, WATER, 4.0)
    return entry


def _edge_v(ctx: Context, side: int, across: float) -> float:
    """Layout v that puts a lake's centre just inside the map edge.

    The water tool only accepts a lake that feeds a river, touches a road or
    closes against the map edge; a tarn at the edge needs neither.
    """
    inset = (across * 0.3) / ctx.layout.frame.v_length
    return inset if side == 0 else 1.0 - inset


def _coast(ctx: Context, field: Field) -> None:
    side = ctx.layout.coast_side
    if side is None:
        return
    frame = ctx.layout.frame
    across = 0.42 * frame.v_length
    v = 0.0 if side == 0 else 1.0
    for i, u in enumerate((0.17, 0.5, 0.83)):
        centre = ctx.world(u, v)
        along = frame.u_length * ctx.rng.uniform(0.42, 0.5)
        _add_lake(
            ctx,
            field,
            centre,
            along,
            across * ctx.rng.uniform(0.9, 1.05),
            f"coast {i + 1}",
        )
    ctx.note("coast along one flank")


def _river_guide(ctx: Context, u: float, v_start: float, v_end: float) -> list[Point]:
    rng = ctx.rng
    steps = 6
    points = []
    for i in range(steps + 1):
        t = i / steps
        v = v_start + (v_end - v_start) * t
        wobble = 0.0 if i in (0, steps) else rng.uniform(-0.035, 0.035)
        points.append(ctx.world(u + wobble, min(max(v, 0.0), 1.0)))
    return points


def _rivers(ctx: Context, field: Field) -> None:
    rng = ctx.rng
    params = ctx.params
    width = float(params["water.river_width"])
    layout = ctx.layout
    rivers = [b for b in layout.boundaries if b.kind == "river"]
    if layout.coast_side is not None:

        wanted = int(params["water.rivers"])
        centres = layout.sector_centres()
        inner = [b.u for b in layout.boundaries] or centres
        for u in inner[:wanted]:
            inland = 1.0 if layout.coast_side == 0 else 0.0
            sea = 0.12 if layout.coast_side == 0 else 0.88
            _emit_river(
                ctx,
                field,
                _river_guide(ctx, u, inland, sea),
                width * rng.uniform(0.8, 1.15),
            )
        return
    for boundary in rivers:
        edge_to_edge = rng.random() < float(params["water.edge_to_edge"])
        if edge_to_edge:
            ends = (0.0, 1.0) if rng.random() < 0.5 else (1.0, 0.0)
            _emit_river(
                ctx,
                field,
                _river_guide(ctx, boundary.u, *ends),
                width * rng.uniform(0.8, 1.2),
            )
            continue

        side = rng.choice((0, 1))
        along = rng.uniform(52.0, 72.0)
        across = rng.uniform(56.0, 76.0)
        lake_v = _edge_v(ctx, side, across)
        centre = ctx.world(boundary.u, lake_v)
        lake = _add_lake(ctx, field, centre, along, across, f"source {boundary.index}")
        if lake is None:
            ends = (0.0, 1.0)
        else:
            ends = (1.0 - side, lake_v)
        _emit_river(
            ctx,
            field,
            _river_guide(ctx, boundary.u, *ends),
            width * rng.uniform(0.8, 1.2),
        )


def _emit_river(ctx: Context, field: Field, guide: list[Point], width: float) -> None:
    name = ctx.namer.river(ctx.rng)
    entry = {
        "name": name,
        "start": pt(guide[0]),
        "end": pt(guide[-1]),
        "waypoints": [pt(p) for p in guide],
        "width": round2(width),
    }
    ctx.add("rivers", entry)
    field.mark_polyline(guide, width * 0.5 + 8.0, WATER)


def _tarns(ctx: Context, field: Field) -> None:
    rng = ctx.rng
    layout = ctx.layout
    wanted = int(ctx.params["water.lakes"])
    if layout.coast_side is not None:
        wanted = max(0, wanted - 1)
    sides = [s for s in (0, 1) if s != layout.coast_side]
    centres = layout.sector_centres()
    placed = 0
    attempts = 0
    while placed < wanted and attempts < wanted * 20:
        attempts += 1
        side = rng.choice(sides)
        u = centres[rng.randrange(len(centres))] + rng.uniform(-0.06, 0.06)
        along = rng.uniform(40.0, 72.0)
        across = rng.uniform(32.0, 54.0)
        centre = ctx.world(u, _edge_v(ctx, side, across))
        if _add_lake(ctx, field, centre, along, across, f"tarn {placed + 1}") is None:
            continue
        placed += 1
        if layout.symmetric and placed < wanted:
            if (
                _add_lake(
                    ctx, field, ctx.mirror(centre), along, across, f"tarn {placed + 1}"
                )
                is not None
            ):
                placed += 1


def _moat(ctx: Context, field: Field) -> None:
    if not ctx.params["water.ring_moat"]:
        return
    centre = ctx.world(*ctx.layout.enemy_main)
    radius = 74.0 if int(ctx.params["settlements.towns"]) > 0 else 52.0
    from ..geometry import Collector

    probe = Collector(field.width, field.height)
    probe.mark_disc(centre[0], centre[1], radius + 8.0, 1)
    if any(field.cells[i] & (WATER | HILL | MOUNTAIN) for i in probe.touched()):
        ctx.note("moat skipped: no clear ground round the main settlement")
        return
    entry = {
        "name": ctx.namer.river(ctx.rng),
        "shape": "ring",
        "x": round2(centre[0]),
        "z": round2(centre[1]),
        "radius": round2(radius),
        "width": 7.0,
        "segments": 48,
        "role": "moat",
    }
    ctx.add("rivers", entry)
    from ..geometry import feature_points

    field.mark_polyline(feature_points(entry), 6.0, WATER)


def synthesise(ctx: Context) -> None:
    field = ctx.field()
    _coast(ctx, field)
    _moat(ctx, field)
    _rivers(ctx, field)
    _tarns(ctx, field)
    rivers = [r for r in ctx.map.get("rivers", []) if r.get("generated") == "water"]
    lakes = [r for r in ctx.map.get("lakes", []) if r.get("generated") == "water"]
    ctx.note(f"{len(rivers)} rivers, {len(lakes)} lakes")
