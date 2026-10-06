"""Starting armies: the player's column outside its camp, enemy garrisons.

Every owner gets exactly one commander - the victory rules end a side that
loses its commander - and the player's troops form up on the enemy side of
the start camp so the first order is a march, not an untangling.
"""

from __future__ import annotations

from ..context import COMMANDERS, Context
from ..geometry import (
    FOREST,
    HILL,
    MOUNTAIN,
    SETTLEMENT,
    WATER,
    Field,
    Point,
    round2,
    settlement_radius,
    spiral,
)

BLOCKERS = WATER | HILL | MOUNTAIN | FOREST | SETTLEMENT
PLAYER_MIX = (
    ("swordsman", 3),
    ("spearman", 3),
    ("archer", 3),
    ("horse_swordsman", 1),
    ("horse_archer", 1),
    ("healer", 1),
)
GARRISON_MIX = (("spearman", 3), ("archer", 2), ("swordsman", 2))
GARRISON_SIZE = {"town": 6, "fortified_camp": 4, "marching_camp": 3}


def _pick(ctx: Context, mix) -> str:
    total = sum(weight for _, weight in mix)
    roll = ctx.rng.uniform(0.0, total)
    for kind, weight in mix:
        if roll < weight:
            return kind
        roll -= weight
    return mix[-1][0]


def _spot(field: Field, origin: Point) -> Point | None:
    for p in spiral(origin, 3.5, 10):
        if not field.blocked(p[0], p[1], 3.0, BLOCKERS):
            return p
    return None


def _add(ctx: Context, field: Field, kind: str, origin: Point, player_id: int) -> bool:
    p = _spot(field, origin)
    if p is None:
        return False
    ctx.add(
        "spawns",
        {
            "type": kind,
            "x": round2(p[0]),
            "z": round2(p[1]),
            "player_id": player_id,
            "nation": ctx.nation_of(player_id),
            "max_population": -1,
        },
    )
    field.mark_disc(p[0], p[1], 3.0, SETTLEMENT)
    return True


def _front_of(
    ctx: Context, settlement: dict, toward_enemy: bool, extra: float
) -> tuple[Point, Point, Point]:
    """Centre of a formation block in front of a settlement, plus its axes."""
    x, z = float(settlement["x"]), float(settlement["z"])
    u, v = ctx.layout.frame.to_layout(x, z)
    du = 0.02 if toward_enemy else -0.02
    ahead = ctx.world(u + du, v)
    forward = (ahead[0] - x, ahead[1] - z)
    norm = (forward[0] ** 2 + forward[1] ** 2) ** 0.5 or 1.0
    forward = (forward[0] / norm, forward[1] / norm)
    across = (-forward[1], forward[0])
    reach = settlement_radius(settlement) + extra
    return (x + forward[0] * reach, z + forward[1] * reach), forward, across


def _block(
    ctx: Context,
    field: Field,
    kinds: list[str],
    centre: Point,
    forward: Point,
    across: Point,
    player_id: int,
) -> int:
    columns = 4
    placed = 0
    for i, kind in enumerate(kinds):
        row, col = divmod(i, columns)
        offset_across = (col - (columns - 1) / 2.0) * 7.0
        offset_forward = -row * 7.0
        p = (
            centre[0] + across[0] * offset_across + forward[0] * offset_forward,
            centre[1] + across[1] * offset_across + forward[1] * offset_forward,
        )
        if _add(ctx, field, kind, p, player_id):
            placed += 1
    return placed


def _has_commander(ctx: Context, player_id: int) -> bool:
    commanders = {kind for kinds in COMMANDERS.values() for kind in kinds}
    return any(
        s.get("player_id") == player_id and s.get("type") in commanders
        for s in ctx.map.get("spawns", [])
    )


def synthesise(ctx: Context) -> None:
    field = ctx.field(reserved=False)
    settlements = ctx.map.get("settlements", [])
    player = [s for s in settlements if s.get("player_id") == 1]
    enemy = [s for s in settlements if s.get("player_id") == 2]

    army = int(ctx.params["spawns.army"])
    placed_player = 0
    if player:
        start = min(
            player,
            key=lambda s: ctx.layout.frame.to_layout(float(s["x"]), float(s["z"]))[0],
        )
        centre, forward, across = _front_of(ctx, start, True, 22.0)
        kinds = (
            []
            if _has_commander(ctx, 1)
            else [ctx.rng.choice(COMMANDERS[ctx.nation_of(1)])]
        )
        kinds += [_pick(ctx, PLAYER_MIX) for _ in range(army)]
        placed_player = _block(ctx, field, kinds, centre, forward, across, 1)

    garrison = float(ctx.params["spawns.garrison"])
    placed_enemy = 0
    if enemy:
        main = max(
            enemy,
            key=lambda s: ctx.layout.frame.to_layout(float(s["x"]), float(s["z"]))[0],
        )
        for settlement in enemy:
            size = round(
                GARRISON_SIZE.get(settlement.get("tier"), 3) * (0.4 + 1.2 * garrison)
            )
            kinds = [_pick(ctx, GARRISON_MIX) for _ in range(size)]
            if settlement is main and not _has_commander(ctx, 2):
                kinds.insert(0, ctx.rng.choice(COMMANDERS[ctx.nation_of(2)]))
            if not kinds:
                continue
            centre, forward, across = _front_of(ctx, settlement, False, 16.0)
            placed_enemy += _block(ctx, field, kinds, centre, forward, across, 2)
    ctx.note(f"player army {placed_player}, enemy garrisons {placed_enemy}")
