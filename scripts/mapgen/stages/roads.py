"""Road intent: strategic routes, cross links, spurs and extra crossings.

Every route runs edge to edge through the settlements on it; cross links close
loops between routes; spurs end at a settlement or a hill entrance, never in
empty ground. Each road is authored as endpoints plus a guide polyline.
``generate-map-roads.py`` routes it around high ground and water and builds a
bridge wherever it has to cross a river.

Junctions are exact: every point another road meets a route at is inserted
into that route's guide, which is what lets the road tool join them at a
common anchor instead of leaving two roads that merely pass close.
"""

from __future__ import annotations

from dataclasses import dataclass, field

from ..context import Context
from ..geometry import (
    Point,
    dist,
    feature_points,
    nearest_on_polyline,
    polyline_crossings,
    polyline_distance,
    pt,
    round2,
    settlement_radius,
)

ON_LANE = 75.0


@dataclass
class LanePlan:
    index: int
    nodes: list[tuple[float, Point, str]] = field(
        default_factory=list
    )  # (u, point, kind)
    junctions: list[tuple[float, Point]] = field(default_factory=list)
    settlements: list[tuple[float, Point, str, float]] = field(default_factory=list)
    breaks: list[tuple[float, Point, Point]] = field(default_factory=list)


def _lane_point(ctx: Context, lane: int, u: float) -> Point:
    return ctx.world(u, ctx.layout.lane_v(lane, u))


def _u_of(ctx: Context, p: Point) -> float:
    return ctx.layout.frame.to_layout(*p)[0]


def _guide_between(
    ctx: Context, plan: LanePlan, u0: float, u1: float, a: Point, b: Point
) -> list[Point]:
    inner: list[tuple[float, Point]] = []
    for u, v in ctx.layout.lanes[plan.index]:
        if u0 + 0.01 < u < u1 - 0.01:
            inner.append((u, ctx.world(u, v)))
    for u, p in plan.junctions:
        if u0 < u < u1:
            inner.append((u, p))
    inner.sort(key=lambda item: item[0])
    points = [a] + [p for _, p in inner] + [b]
    deduped: list[Point] = []
    for p in points:
        if not deduped or dist(p, deduped[-1]) > 3.0:
            deduped.append(p)
    if dist(deduped[-1], b) > 0.01:
        deduped[-1] = b
    return deduped


def _road(ctx: Context, guide: list[Point], width: float, role: str) -> dict:
    entry = {
        "start": pt(guide[0]),
        "end": pt(guide[-1]),
        "style": "default",
        "width": round2(width),
        "role": role,
    }
    if len(guide) > 2:
        entry["waypoints"] = [pt(p) for p in guide]
    return ctx.add("roads", entry)


def _junction(ctx: Context, plans: list[LanePlan], lane: int, u: float) -> Point:
    """A point on a lane other roads meet.

    Never a settlement's centre: the route already runs in and out through it,
    and a third road meeting there leaves the settlement tool no ground for a
    barracks off the roads. The junction moves along the lane until it is
    outside the walls.
    """
    u_length = ctx.layout.frame.u_length
    for _ in range(8):
        p = _lane_point(ctx, lane, u)
        crowded = [
            (node_u, q, radius)
            for node_u, q, kind, radius in plans[lane].settlements
            if dist(p, q) < radius + 30.0
        ]
        if not crowded:
            break
        node_u, q, radius = crowded[0]
        step = (radius + 38.0) / u_length
        u = node_u + step if u >= node_u else node_u - step
        u = min(max(u, 0.03), 0.97)
    p = _lane_point(ctx, lane, u)
    plans[lane].junctions.append((u, p))
    return p


def _approaches(
    ctx: Context, plans: list[LanePlan], lane: int, target: Point
) -> list[tuple[Point, Point]]:
    """Two roads from the route to one gate or ramp.

    The road tool joins every endpoint that does not meet another road back
    into the network, so a single spur comes back doubled. Two approaches that
    meet at the gate are what the design rules ask of a camp anyway.
    """
    u = _u_of(ctx, target)
    reach = polyline_distance(target, ctx.layout.lane_world(lane))
    du = min(max(reach * 0.75 / ctx.layout.frame.u_length, 0.035), 0.09)
    a = _junction(ctx, plans, lane, max(u - du, 0.03))
    b = _junction(ctx, plans, lane, min(u + du, 0.97))
    return [(a, target), (b, target)]


def _gate_point(settlement: dict, towards: Point) -> Point:
    """Where a spur meets a camp: at its wall, not in its yard.

    The settlement tool cuts a gate where a road reaches the ring. A road run
    on to the centre crosses the whole yard, and a small camp then has no
    ground off the road for its barracks.
    """
    centre = (float(settlement["x"]), float(settlement["z"]))
    size = (settlement.get("plan_options") or {}).get("size")
    if isinstance(size, list) and len(size) >= 2:
        half = max(float(size[0]), float(size[1])) * 0.5
    else:
        half = settlement_radius(settlement) * 0.5
    reach = half + 4.0
    d = dist(centre, towards) or 1.0
    return (
        centre[0] + (towards[0] - centre[0]) / d * reach,
        centre[1] + (towards[1] - centre[1]) / d * reach,
    )


def _rivers(ctx: Context) -> list[list[Point]]:
    return [
        feature_points(river)
        for river in ctx.map.get("rivers", [])
        if river.get("shape") != "ring" and feature_points(river)
    ]


def synthesise(ctx: Context) -> None:
    layout = ctx.layout
    params = ctx.params
    main_width = float(params["roads.width"])
    side_width = max(3.0, round(main_width * 0.8, 1))
    lanes = len(layout.lanes)
    plans = [LanePlan(i) for i in range(lanes)]

    spurs: list[dict] = []
    for settlement in ctx.map.get("settlements", []):
        p = (float(settlement["x"]), float(settlement["z"]))
        u = _u_of(ctx, p)
        best = min(range(lanes), key=lambda i: dist(p, _lane_point(ctx, i, u)))
        if (
            settlement.get("tier") == "town"
            and dist(p, _lane_point(ctx, best, u)) <= ON_LANE
        ):
            plans[best].nodes.append((u, p, "settlement"))
            plans[best].settlements.append(
                (u, p, "settlement", settlement_radius(settlement))
            )
        else:
            spurs.append(settlement)

    # Rivers each route crosses, with where. Too few crossings adds diagonal
    # links over the river; too many funnels the extra routes onto the first.
    wanted_crossings = int(params["water.crossings"])
    rivers = _rivers(ctx)
    extra_links: list[tuple[int, float, int, float, float]] = []
    funnels: list[tuple[int, float]] = []
    for river in rivers:
        crossing_us = []
        for lane in range(lanes):
            hits = polyline_crossings(layout.lane_world(lane), river)
            if hits:
                crossing_us.append((lane, _u_of(ctx, hits[0])))
        if not crossing_us:
            continue
        if wanted_crossings < len(crossing_us):
            keep = {crossing_us[0][0]}
            for lane, u in crossing_us:
                if lane not in keep:
                    funnels.append((lane, u))
        for i in range(max(0, wanted_crossings - len(crossing_us))):
            lane, u = crossing_us[i % len(crossing_us)]
            other = lane + 1 if lane + 1 < lanes else lane - 1
            extra_links.append(
                (lane, u - 0.08, max(other, 0), u + 0.08, 0.16 if lanes == 1 else 0.0)
            )

    for plan in plans:
        plan.nodes.sort(key=lambda node: node[0])

    links = []
    for link in layout.links:
        if link.lane_a == link.lane_b:
            # One route: a loop is a bypass that leaves it and rejoins it.
            a = _junction(ctx, plans, link.lane_a, max(link.u - 0.09, 0.05))
            b = _junction(ctx, plans, link.lane_a, min(link.u + 0.09, 0.95))
            v = layout.lane_v(link.lane_a, link.u)
            bow = 0.17 if v < 0.5 else -0.17
            links.append((a, b, ctx.world(link.u, v + bow)))
            continue
        a = _junction(ctx, plans, link.lane_a, link.u)
        b = _junction(ctx, plans, link.lane_b, link.u)
        links.append((a, b, None))

    crossings = []
    for lane_a, u_a, lane_b, u_b, bow in extra_links:
        a = _junction(ctx, plans, lane_a, max(u_a, 0.03))
        b = _junction(ctx, plans, lane_b, min(u_b, 0.97))
        mid = None
        if bow:
            mu = (u_a + u_b) * 0.5
            v = layout.lane_v(lane_a, mu)
            v = v + bow if v < 0.5 else v - bow
            mid = ctx.world(mu, v)
        crossings.append((a, b, mid))

    funnel_roads = []
    for lane, u in funnels:
        before = _junction(ctx, plans, lane, u - 0.07)
        after = _junction(ctx, plans, lane, u + 0.07)
        main_before = _junction(ctx, plans, 0, u - 0.07)
        main_after = _junction(ctx, plans, 0, u + 0.07)
        plans[lane].breaks.append((u, before, after))
        funnel_roads.append((before, main_before))
        funnel_roads.append((main_after, after))

    spur_roads = []
    for settlement in spurs:
        p = (float(settlement["x"]), float(settlement["z"]))
        u = _u_of(ctx, p)
        lane = min(range(lanes), key=lambda i: dist(p, _lane_point(ctx, i, u)))
        gate = _gate_point(settlement, _lane_point(ctx, lane, u))
        spur_roads.extend(_approaches(ctx, plans, lane, gate))

    secondary = float(params["roads.secondary"])
    for hill in ctx.map.get("terrain", []):
        if hill.get("type") != "hill" or hill.get("role") == "pedestal":
            continue
        if hill.get("shape") == "corridor" or not hill.get("entrances"):
            continue
        if ctx.rng.random() >= secondary:
            continue
        best = None
        for entrance in hill["entrances"]:
            e = (float(entrance["x"]), float(entrance["z"]))
            for lane in range(lanes):
                q = nearest_on_polyline(e, layout.lane_world(lane))
                if best is None or dist(e, q) < best[0]:
                    best = (dist(e, q), lane, e, q)
        if best is None or not 18.0 < best[0] < 140.0:
            continue
        _, lane, entrance_point, q = best
        spur_roads.extend(_approaches(ctx, plans, lane, entrance_point))

    # Routes, edge to edge through their settlements, split at funnels.
    for plan in plans:
        start = _lane_point(ctx, plan.index, 0.0)
        end = _lane_point(ctx, plan.index, 1.0)
        nodes = [(0.0, start, "edge")] + plan.nodes + [(1.0, end, "edge")]
        for (u0, a, _), (u1, b, _) in zip(nodes, nodes[1:], strict=False):
            breaks = [item for item in plan.breaks if u0 < item[0] < u1]
            if breaks:
                cut, before, after = breaks[0]
                _road(
                    ctx,
                    _guide_between(ctx, plan, u0, cut - 0.07, a, before),
                    main_width,
                    "route",
                )
                _road(
                    ctx,
                    _guide_between(ctx, plan, cut + 0.07, u1, after, b),
                    main_width,
                    "route",
                )
                continue
            _road(ctx, _guide_between(ctx, plan, u0, u1, a, b), main_width, "route")

    for a, b, mid in links:
        _road(ctx, [a, mid, b] if mid else [a, b], side_width, "link")
    for a, b, mid in crossings:
        _road(ctx, [a, mid, b] if mid else [a, b], side_width, "crossing")
    for a, b in funnel_roads:
        _road(ctx, [a, b], main_width, "funnel")
    for a, b in spur_roads:
        _road(ctx, [a, b], side_width, "spur")

    roads = [r for r in ctx.map.get("roads", []) if r.get("generated") == "roads"]
    ctx.note(
        f"{len(roads)} road intents: {lanes} routes, {len(links)} cross links, "
        f"{len(crossings)} extra crossings, {len(spur_roads)} spurs"
    )
