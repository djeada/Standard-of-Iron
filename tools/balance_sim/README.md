# Headless battle-balance simulator

`balance_sim` fights army-vs-army matchups using the production `World`, unit
factories, troop/nation catalogs, command service and combat/movement systems —
with no QML, no window and no rendering. It runs at a fixed timestep from a
seeded start, so a given fixture and seed always produce the same battle, and it
is safe to run in CI on a machine with no display.

Unlike the Arena harness, which exists to inspect how a fight _looks_, this tool
only measures how a fight _resolves_.

## Running

```bash
cmake --build build --target balance_sim
./build/bin/balance_sim                     # whole matrix, human-readable report
./build/bin/balance_sim --filter cavalry    # only fixtures whose id contains "cavalry"
./build/bin/balance_sim --seeds 32          # override every fixture's seed count
./build/bin/balance_sim --json out.json --csv out.csv --quiet
./build/bin/balance_sim --filter mirror --trace   # per-second state of one battle
```

Run it from the repository root, or from anywhere the `assets/` tree is
reachable — fixtures default to `assets/balance` and the melee weapon trace needs
the baked creature poses in `assets/creatures`.

The exit status is `0` when every fixture met its declared expectations, `1` when
a balance expectation failed, and `2`/`3` for fixture-loading and output errors.
That makes the no-argument invocation usable directly as a CI gate.

## Fixtures

One JSON file per matchup in `assets/balance`. A side is a nation, a stance and a
list of troop groups:

```json
{
    "id": "spear_vs_cavalry_frontal",
    "label": "Braced spears vs frontal cavalry charge",
    "duration_seconds": 120,
    "seeds": 8,
    "separation": 26.0,
    "side_a": {
        "label": "Spear line",
        "nation": "roman_republic",
        "stance": "hold",
        "groups": [{ "troop": "spearman", "count": 4 }]
    },
    "side_b": {
        "label": "Cavalry",
        "nation": "roman_republic",
        "stance": "charge",
        "groups": [{ "troop": "horse_swordsman", "count": 2 }]
    },
    "expect": { "a_win_rate_min": 0.6, "max_timeout_rate": 0.25 }
}
```

Stances map onto orders a player can actually give: `attack` (engage and chase),
`charge` (mounted charge intent plus attack), `hold` (hold-mode, let them come)
and `stand` (no orders, auto-engagement only). Attack and charge orders are
re-issued as targets die, which is what an attack-move models.

Every seed is run twice with the sides swapped, so results are keyed to the
fixture's own side A/B while positional effects cancel out. Report those fixtures
on gold, not headcount: several matchups exist specifically to compare what equal
purses buy.

Optional `expect` keys: `a_win_rate_min`, `a_win_rate_max`, `max_timeout_rate`,
`max_spawn_side_bias`.

## What it reports

Per fixture: win/draw/timeout rates, median survivor counts and survivor
fraction, median time to victory, mean centroid distance at first contact,
damage split into ranged and melee per side, and mean formation cohesion radius
(the RMS spread of a side's squads around their own centre — rising spread means
the line is coming apart).

It also counts behaviours that should never happen whatever the numbers say:
friendly-fire hits, ranged attacks fired by a unit that is melee-locked and could
have swung instead, and unit-seconds spent idle with no target while a live enemy
stands in vision.

`--csv` writes one row per battle (seed, swap flag, outcome, timings, survivors,
damage, cohesion, invalid-behaviour counts); `--json` writes the same plus the
aggregate summary and any expectation failures.

## Reading the spawn-side bias

`spawn_side_bias` is the win rate of whichever side spawned on the left,
normalised so `0.0` is fair and `1.0` means the left side always won. It is
meaningful for asymmetric fixtures.

For an _exactly_ symmetric mirror it always reads `1.0`, and that is not a
balance defect: two identical armies produce a tie that the deterministic
simulation has to break somehow, and it breaks on entity processing order. The
mirror fixture therefore asserts a fair 50/50 win rate after the side swap and
deliberately does not assert spawn-side bias.

## Regression coverage

`tests/tools/balance_sim_test.cpp` runs a subset of the matrix inside the normal
GTest suite: determinism, that an even infantry fight resolves rather than
stalling, and the designed counters (spears beat a frontal charge, cavalry
overruns exposed archers, infantry overruns unescorted siege, an elite commander
loses to equal-cost line infantry, and the two playable factions are even at
equal cost).
