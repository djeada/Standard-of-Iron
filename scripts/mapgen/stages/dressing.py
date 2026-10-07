"""Dressing intent: the small things that make a battlefield one place.

Every piece is anchored to something that exists once the earlier stages are
materialised - a bridge, a gate, a hill ramp, a junction, a riverbank, a
town's fields - because ``generate-map-dressing.py`` looks for the nearest
such feature and a piece placed in empty ground has nothing to dress.
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
    dist,
    feature_points,
    polyline_distance,
    polyline_length,
    pt,
    round2,
    sample_polyline,
    settlement_radius,
    spiral,
)

TREES = {
    "temperate": ["pine_tree", "olive_tree", "cypress_tree"],
    "mediterranean": ["olive_tree", "cypress_tree", "olive_tree"],
    "winter": ["pine_tree", "pine_tree", "cypress_tree"],
    "alpine": ["pine_tree", "pine_tree"],
    "pine_forest": ["pine_tree", "pine_tree", "cypress_tree"],
    "canyon": ["olive_tree", "cypress_tree"],
    "iberian": ["olive_tree", "cypress_tree", "pine_tree"],
    "delta": ["cypress_tree", "olive_tree"],
}
ORCHARD_BIOMES = {"temperate", "mediterranean", "iberian", "canyon"}
OPEN = WATER | HILL | MOUNTAIN | FOREST | ROAD | SETTLEMENT | LANDMARK


class Pieces:
    def __init__(self, ctx: Context) -> None:
        self.ctx = ctx
        self.count: dict[str, int] = {}

    def add(self, kind: str, at: Point | None, **extra) -> None:
        index = self.count.get(kind, 0) + 1
        self.count[kind] = index
        entry = {"id": self.ctx.namer.make_id("d", f"{kind} {index}"), "kind": kind}
        if at is not None:
            entry["x"] = round2(at[0])
            entry["z"] = round2(at[1])
        entry.update(extra)
        self.ctx.add("dressing", entry)


def _roll(ctx: Context, chance: float) -> bool:
    return ctx.rng.random() < chance


def _midpoint(entry: dict) -> Point:
    a, b = entry["start"], entry["end"]
    return ((float(a[0]) + float(b[0])) * 0.5, (float(a[1]) + float(b[1])) * 0.5)


def _junctions(ctx: Context) -> list[Point]:
    roads = [feature_points(r) for r in ctx.map.get("roads", [])]
    roads = [r for r in roads if len(r) >= 2]
    found: list[Point] = []
    for i, road in enumerate(roads):
        for end in (road[0], road[-1]):
            for j, other in enumerate(roads):
                if i != j and polyline_distance(end, other[1:-1] or other) < 2.5:
                    if all(dist(end, f) > 20.0 for f in found):
                        found.append(end)
    return found


def _open_spot(field: Field, origin: Point, radius: float) -> Point | None:
    for p in spiral(origin, 6.0, 6):
        if not field.blocked(p[0], p[1], radius, OPEN):
            return p
    return None


def synthesise(ctx: Context) -> None:
    params = ctx.params
    density = float(params["dressing.density"])
    scatter = float(params["dressing.scatter"])
    biome = str(params["general.biome"])
    trees = TREES.get(biome, TREES["temperate"])
    pieces = Pieces(ctx)
    field = ctx.field(reserved=False)

    for bridge in ctx.map.get("bridges", []):
        if _roll(ctx, 0.35 + 0.6 * density):
            pieces.add("bridgehead", _midpoint(bridge), cart=_roll(ctx, 0.5))
    for junction in _junctions(ctx):
        if _roll(ctx, 0.25 + 0.5 * density):
            pieces.add(
                "junction",
                junction,
                style=ctx.rng.choice(("milestone", "shrine", "camp")),
                snap=6,
            )

    rivers = [r for r in ctx.map.get("rivers", []) if r.get("shape") != "ring"]
    bridges = [_midpoint(b) for b in ctx.map.get("bridges", [])]
    for river in rivers:
        points = feature_points(river)
        if len(points) < 2 or polyline_length(points) < 160.0:
            continue
        samples = sample_polyline(points, 12.0)
        clear = [p for p in samples[3:-3] if all(dist(p, b) > 70.0 for b in bridges)]
        if clear and _roll(ctx, 0.3 + 0.5 * density):
            pieces.add(
                "ford_wreck", clear[ctx.rng.randrange(len(clear))], boulders=4, plants=3
            )
        if len(samples) > 12 and _roll(ctx, 0.4 + 0.5 * scatter):
            start = ctx.rng.randrange(2, len(samples) // 2)
            end = min(len(samples) - 3, start + ctx.rng.randrange(6, 14))
            pieces.add(
                "riverbank",
                None,
                **{
                    "from": pt(samples[start]),
                    "to": pt(samples[end]),
                    "spacing": 30,
                    "kinds": ["plant", "boulder", "plant", "dead_tree"],
                },
            )

    for hill in ctx.map.get("terrain", []):
        if (
            hill.get("type") == "hill"
            and hill.get("entrances")
            and _roll(ctx, 0.2 + 0.4 * density)
        ):
            entrance = hill["entrances"][0]
            pieces.add(
                "ramp_mouth",
                (float(entrance["x"]), float(entrance["z"])),
                statue=_roll(ctx, 0.4),
                fire=_roll(ctx, 0.4),
            )
        if hill.get("shape") == "corridor" and _roll(ctx, 0.3 + 0.4 * scatter):
            angle = math.radians(float(hill.get("rotation", 0.0)))
            side = ctx.rng.choice((-1.0, 1.0))
            reach = float(hill.get("thickness", 10.0)) * 0.5 + 12.0
            p = (
                float(hill["x"]) - math.sin(angle) * reach * side,
                float(hill["z"]) + math.cos(angle) * reach * side,
            )
            spot = _open_spot(field, p, 10.0)
            if spot:
                pieces.add(
                    "scree",
                    spot,
                    rotation=round2(float(hill.get("rotation", 0.0))),
                    width=30,
                    depth=14,
                    boulders=12,
                    dead_trees=2,
                )

    for settlement in ctx.map.get("settlements", []):
        centre = (float(settlement["x"]), float(settlement["z"]))
        tier = settlement.get("tier")
        radius = settlement_radius(settlement)
        if tier in ("town", "fortified_camp") and _roll(ctx, 0.4 + 0.6 * density):
            pieces.add("gate_approach", centre, statues=tier == "town")
        if tier in ("town", "fortified_camp") and params["settlements.farmland"]:
            for _ in range(2 if tier == "town" else 1):
                angle = ctx.rng.uniform(0.0, 2.0 * math.pi)
                p = (
                    centre[0] + math.cos(angle) * (radius + 30.0),
                    centre[1] + math.sin(angle) * (radius + 30.0),
                )
                spot = _open_spot(field, p, 16.0)
                if spot:
                    pieces.add(
                        "farmland",
                        spot,
                        cols=3,
                        rows=2,
                        player_id=int(settlement.get("player_id", 0)),
                        nation=settlement.get("nation", ctx.enemy_nation),
                        rotation=0,
                    )
                    field.mark_disc(spot[0], spot[1], 20.0, LANDMARK)
        if (
            tier == "town"
            and biome in ORCHARD_BIOMES
            and _roll(ctx, 0.3 + 0.5 * scatter)
        ):
            angle = ctx.rng.uniform(0.0, 2.0 * math.pi)
            p = (
                centre[0] + math.cos(angle) * (radius + 42.0),
                centre[1] + math.sin(angle) * (radius + 42.0),
            )
            spot = _open_spot(field, p, 20.0)
            if spot:
                pieces.add(
                    "orchard",
                    spot,
                    rows=3,
                    cols=5,
                    tree="olive_tree",
                    rotation=0,
                    spacing=7.0,
                )
                field.mark_disc(spot[0], spot[1], 22.0, LANDMARK)
        if tier == "marching_camp" and settlement.get("player_id") == 1:
            u, v = ctx.layout.frame.to_layout(*centre)
            behind = ctx.world(max(u - 0.06, 0.02), v)
            spot = _open_spot(field, behind, 14.0)
            if spot:
                pieces.add("camp_lines", spot, rows=2, cols=2, rotation=0)
                field.mark_disc(spot[0], spot[1], 16.0, LANDMARK)

    copses = round(scatter * ctx.layout.sectors * 2.5)
    attempts = 0
    while copses > 0 and attempts < 80:
        attempts += 1
        p = (
            ctx.rng.uniform(30.0, ctx.width - 30.0),
            ctx.rng.uniform(30.0, ctx.height - 30.0),
        )
        if field.blocked(p[0], p[1], 22.0, OPEN):
            continue
        pieces.add(
            "copse",
            p,
            radius=ctx.rng.choice((12, 14, 16)),
            count=ctx.rng.randrange(7, 12),
            trees=[ctx.rng.choice(trees) for _ in range(3)],
        )
        field.mark_disc(p[0], p[1], 24.0, LANDMARK)
        copses -= 1

    summary = ", ".join(
        f"{count} {kind}" for kind, count in sorted(pieces.count.items())
    )
    ctx.note(summary or "no dressing")
