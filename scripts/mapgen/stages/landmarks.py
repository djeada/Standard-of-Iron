"""Landmark intent: places worth walking to, off the main line of march.

Sanctuaries, shrines, burnt hamlets and watch posts sit beside a road but on
its outer side, so visiting one is a detour - a side objective - rather than
something the armies trip over. A share of them are held by an enemy picket.
``generate-map-landmarks.py`` builds the temples, props and guards.
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
    dist,
    facing_of,
    feature_points,
    round2,
    sample_polyline,
)

KINDS = (("sanctuary", 0.3), ("shrine", 0.3), ("hamlet", 0.2), ("watch", 0.2))
BLOCKERS = WATER | HILL | MOUNTAIN | FOREST | ROAD | SETTLEMENT | LANDMARK


def _kind(ctx: Context) -> str:
    roll = ctx.rng.random()
    for kind, weight in KINDS:
        if roll < weight:
            return kind
        roll -= weight
    return KINDS[-1][0]


def synthesise(ctx: Context) -> None:
    params = ctx.params
    wanted = int(params["landmarks.count"])
    if wanted <= 0:
        ctx.note("no landmarks requested")
        return
    spacing = float(params["landmarks.spacing"])
    guarded = float(params["landmarks.guarded"])
    field = ctx.field(reserved=False)
    settlements = [
        (float(s["x"]), float(s["z"])) for s in ctx.map.get("settlements", [])
    ]
    existing = [(float(lm["x"]), float(lm["z"])) for lm in ctx.map.get("landmarks", [])]
    roads = [feature_points(r) for r in ctx.map.get("roads", [])]
    roads = [points for points in roads if len(points) >= 2]
    if not roads:
        ctx.note("no roads to set landmarks beside")
        return
    centre = (ctx.width * 0.5, ctx.height * 0.5)

    placed = 0
    attempts = 0
    while placed < wanted and attempts < wanted * 40:
        attempts += 1
        points = sample_polyline(roads[ctx.rng.randrange(len(roads))], 16.0)
        if len(points) < 3:
            continue
        i = ctx.rng.randrange(1, len(points) - 1)
        p, q = points[i - 1], points[i + 1]
        tangent = (q[0] - p[0], q[1] - p[1])
        norm = math.hypot(*tangent) or 1.0
        normal = (-tangent[1] / norm, tangent[0] / norm)
        # Outer side of the road: the one facing away from the map centre.
        outward = (points[i][0] - centre[0]) * normal[0] + (
            points[i][1] - centre[1]
        ) * normal[1]
        side = 1.0 if outward >= 0.0 else -1.0
        reach = ctx.rng.uniform(28.0, 46.0)
        site = (
            points[i][0] + normal[0] * reach * side,
            points[i][1] + normal[1] * reach * side,
        )
        u = ctx.layout.frame.to_layout(*site)[0]
        if not 0.18 < u < 0.86:
            continue
        if field.blocked(site[0], site[1], 20.0, BLOCKERS):
            continue
        if any(dist(site, s) < 85.0 for s in settlements):
            continue
        if any(dist(site, other) < spacing for other in existing):
            continue
        kind = _kind(ctx)
        name = ctx.namer.landmark(
            ctx.rng, kind, ctx.enemy_nation if u > 0.5 else ctx.player_nation
        )
        entry = {
            "id": ctx.namer.make_id("lm", name),
            "name": name,
            "kind": kind,
            "x": round2(site[0]),
            "z": round2(site[1]),
            "facing": facing_of(points[i][0] - site[0], points[i][1] - site[1]),
        }
        if kind in ("sanctuary", "watch") and ctx.rng.random() < guarded:
            entry["player_id"] = 2
            entry["nation"] = ctx.enemy_nation
            entry["guards"] = [
                {
                    "count": ctx.rng.choice((3, 4)),
                    "type": ctx.rng.choice(("spearman", "archer")),
                }
            ]
        elif kind == "sanctuary":
            entry["nation"] = ctx.enemy_nation if u > 0.5 else ctx.player_nation
        ctx.add("landmarks", entry)
        field.mark_disc(site[0], site[1], 18.0, LANDMARK)
        existing.append(site)
        placed += 1
    ctx.note(f"{placed} landmarks")
