"""Settlement intent: the player's camp, the enemy's seat and the holdings
between them, each set on a route so the economy is part of the road graph.

``generate-map-settlements.py`` lays the walls, gates and buildings once the
roads exist, so this stage only decides tier, plan, owner and position.
"""

from __future__ import annotations

import math

from ..context import Context
from ..geometry import (
    FOREST,
    HILL,
    MOUNTAIN,
    ROAD,
    SETTLEMENT,
    WATER,
    Field,
    Point,
    dist,
    facing_of,
    polyline_distance,
    round2,
    settlement_radius,
    spiral,
)

PLANS = {
    "town": [
        ("circle", {"size": [30, 28], "towers": 4}),
        ("star", {"bastion": 8, "flank": 5, "points": 4, "size": [26, 22]}),
        ("twin", {"neck": 6, "offset": [30, 26], "size": [22, 16]}),
    ],
    "fortified_camp": [
        ("circle", {"size": [30, 26], "towers": 4}),
        ("star", {"bastion": 8, "flank": 5, "points": 4, "size": [16, 13]}),
        ("stepped", {"chamfer": 9, "size": [30, 25]}),
        ("twin", {"neck": 7, "offset": [46, 0], "size": [17, 15]}),
    ],
    "marching_camp": [
        ("circle", {"gate_count": 3, "size": [16, 14], "towers": 3}),
        ("stepped", {"chamfer": 6, "size": [24, 14]}),
    ],
}
HILLTOP_PLANS = {
    "fortified_camp": [("stepped", {"chamfer": 9, "size": [30, 25]})],
    "marching_camp": [("stepped", {"chamfer": 6, "size": [24, 14]})],
}
POPULATION = {"town": 190, "fortified_camp": 160, "marching_camp": 120}
GROUND = WATER | HILL | MOUNTAIN | FOREST | ROAD


def _existing(ctx: Context) -> list[Point]:
    return [(float(s["x"]), float(s["z"])) for s in ctx.map.get("settlements", [])]


def _pedestal_near(ctx: Context, p: Point, reach: float) -> dict | None:
    if "terrain" in ctx.locked:
        # A hilltop settlement grows its hill to fit; a locked hill must not
        # change, so locked terrain keeps settlements on the flat.
        return None
    for hill in ctx.map.get("terrain", []):
        if hill.get("role") == "pedestal" and dist(p, (hill["x"], hill["z"])) < reach:
            return hill
    return None


def _off_routes(ctx: Context, p: Point, radius: float) -> bool:
    """Whether a camp or fort keeps clear of the planned routes and links."""
    return all(
        polyline_distance(p, points) >= radius + reach + 12.0
        for points, reach in ctx.reserved_lines
    )


def _clear_spot(
    ctx: Context,
    field: Field,
    origin: Point,
    radius: float,
    spacing: float,
    off_routes: bool = False,
) -> Point | None:
    others = _existing(ctx)
    for p in spiral(origin, 8.0, 9):
        if off_routes and not _off_routes(ctx, p, radius):
            continue
        if field.blocked(p[0], p[1], radius + 12.0, GROUND):
            continue
        if any(dist(p, q) < spacing for q in others):
            continue
        if field.blocked(p[0], p[1], radius * 0.7, SETTLEMENT):
            continue
        return p
    return None


def _facing(ctx: Context, p: Point, toward_player: bool) -> str:
    u, _ = ctx.layout.frame.to_layout(*p)
    target = ctx.world(
        max(u - 0.1, 0.0) if toward_player else min(u + 0.1, 1.0),
        ctx.layout.frame.to_layout(*p)[1],
    )
    return facing_of(target[0] - p[0], target[1] - p[1])


def _place(
    ctx: Context,
    field: Field,
    origin: Point,
    tier: str,
    player_id: int,
    spacing: float,
    exact: bool = False,
) -> dict | None:
    rng = ctx.rng
    entry: dict = {"tier": tier}
    hill = _pedestal_near(ctx, origin, 70.0) if tier != "town" else None
    variety = float(ctx.params["settlements.plan_variety"])
    plans = HILLTOP_PLANS.get(tier) if hill else PLANS[tier]
    if plans and (hill or rng.random() < variety):
        plan, options = plans[rng.randrange(len(plans))]
        entry["plan"] = plan
        entry["plan_options"] = {
            k: (list(v) if isinstance(v, list) else v) for k, v in options.items()
        }
    radius = settlement_radius(entry)
    if tier != "town" and not exact:
        # Only a town is big enough to straddle a route: a camp or fort with
        # the road through its middle has no ground left for a barracks. It
        # stands beside the route instead and a spur leads in.
        origin = _beside_route(ctx, origin, radius)
    if hill:
        spot: Point | None = (float(hill["x"]), float(hill["z"]))
        if any(dist(spot, q) < spacing for q in _existing(ctx)):
            spot = None
        entry["on_hill"] = True
    elif exact:
        spot = origin
        entry["on_hill"] = False
    else:
        spot = _clear_spot(
            ctx, field, origin, radius, spacing, off_routes=tier != "town"
        )
        entry["on_hill"] = False
    if spot is None:
        return None
    nation = ctx.nation_of(player_id)
    name = ctx.namer.settlement(rng, nation)
    entry.update(
        {
            "id": ctx.namer.make_id("s", name),
            "name": name,
            "x": round2(spot[0]),
            "z": round2(spot[1]),
            "player_id": player_id,
            "nation": nation,
            "facing": _facing(ctx, spot, toward_player=player_id != 1),
            "max_population": POPULATION[tier],
        }
    )
    if tier == "marching_camp":
        entry["homes"] = 6
    ctx.add("settlements", entry)
    field.mark_disc(spot[0], spot[1], settlement_radius(entry), SETTLEMENT)
    return entry


def _beside_route(ctx: Context, origin: Point, radius: float) -> Point:
    u, v = ctx.layout.frame.to_layout(*origin)
    offset = (radius + 34.0) / ctx.layout.frame.v_length
    lo, hi = ctx.layout.v_range
    side = -1.0 if v > (lo + hi) * 0.5 else 1.0
    if ctx.rng.random() < 0.35:
        side = -side
    v = min(max(v + side * offset, lo + 0.06), hi - 0.06)
    return ctx.world(u, v)


def _moat_centre(ctx: Context) -> Point | None:
    for river in ctx.map.get("rivers", []):
        if river.get("role") == "moat" and river.get("shape") == "ring":
            return (float(river["x"]), float(river["z"]))
    return None


def synthesise(ctx: Context) -> None:
    params = ctx.params
    field = ctx.field(reserved=False)
    # Spacing is authored for a standard 650 map; a smaller field scales it
    # down rather than running out of room for the settlements it asked for.
    spacing = float(params["settlements.spacing"]) * min(
        1.0, min(ctx.width, ctx.height) / 650.0
    )
    spacing = max(spacing, 70.0)
    towns = int(params["settlements.towns"])
    forts = int(params["settlements.forts"])
    camps = int(params["settlements.camps"])

    start = _place(
        ctx, field, ctx.world(*ctx.layout.start), "marching_camp", 1, spacing
    )
    if start is not None and not params["settlements.walled_start"]:
        start["palisade"] = False
    if start is None:
        ctx.note("no clear ground for the player's start camp")

    main_tier = "town" if towns else ("fortified_camp" if forts else "marching_camp")
    if main_tier == "town":
        towns -= 1
    elif main_tier == "fortified_camp":
        forts -= 1
    moat = _moat_centre(ctx)
    main = _place(
        ctx,
        field,
        moat or ctx.world(*ctx.layout.enemy_main),
        main_tier,
        2,
        spacing,
        exact=moat is not None,
    )
    if main is None:
        ctx.note("no clear ground for the enemy's main settlement")

    wanted = [("town", towns), ("fortified_camp", forts), ("marching_camp", camps)]
    roles = {"town": "town", "fortified_camp": "fort", "marching_camp": "camp"}
    for tier, count in wanted:
        sites = sorted(
            ctx.layout.sites,
            key=lambda site: (site.role != roles[tier], ctx.rng.random()),
        )
        placed = 0
        for site in sites:
            if placed >= count:
                break
            if not 0.2 < site.u < 0.95:
                continue
            # The player starts from one camp; every other holding is there to
            # be taken, and the settlement tool caps the homes a player owns.
            owner = 2
            if (
                _place(ctx, field, ctx.world(site.u, site.v), tier, owner, spacing)
                is not None
            ):
                placed += 1
        if placed < count:
            ctx.note(f"placed {placed} of {count} {tier.replace('_', ' ')}s")

    generated = [
        s for s in ctx.map.get("settlements", []) if s.get("generated") == "settlements"
    ]
    ctx.note(
        ", ".join(
            f"{s['name']} ({s['tier'].replace('_', ' ')}, player {s['player_id']})"
            for s in generated
        )
        or "no settlements"
    )


def angle_between(a: Point, b: Point) -> float:
    return math.degrees(math.atan2(b[1] - a[1], b[0] - a[0]))
