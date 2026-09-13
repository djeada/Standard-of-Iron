# Unit Balance

Unit balance is defined by the shipped troop data, combat multipliers, and deterministic matchup fixtures. The repository does not use a prose-only tier formula as the source of truth: combat relationships are exercised by `tools/balance_sim` against the fixtures in `assets/balance/`.

The main sources are:

- `assets/data/troops/base.json` — base troop production, combat, visuals, formation roles, and lore;
- `assets/data/nations/*.json` — nation-specific troop variants;
- `game/systems/combat_system/combat_types.h` — combat and terrain multipliers; and
- `assets/balance/*.json` — deterministic matchup expectations.

`tests/tools/balance_sim_test.cpp` gates the fixture suite.

## What the fixtures assert

The current fixture set covers these relationships:

- mirror swordsman behavior;
- spearmen against cavalry;
- cavalry against archers;
- infantry against siege;
- archers against elephants;
- spearmen against swordsmen;
- Roman/Carthaginian line matchups;
- commanders against line infantry;
- Iron Sepulcher troops against Rome; and
- horse archers against spearmen.

The fixtures are executable balance contracts. Their group composition, stances, seed count, duration, and expected outcome bounds are data rather than prose assumptions.

For example, `08_commander_vs_line.json` describes one Roman commander against four legionary swordsman groups and states the design intent directly: elite units are strong individual bodies but should not replace line infantry on cost efficiency.

## Combat multipliers

The current combat constants are defined in `game/systems/combat_system/combat_types.h`:

| Rule | Multiplier |
| --- | ---: |
| Spearmen vs cavalry | 2.5× |
| Infantry melee vs siege | 3.0× |
| Archers vs elephants | 1.68× |
| Archers from high ground | 1.8× damage |
| Spearmen from high ground | 1.8× damage |
| High-ground armour | 0.85× incoming armour-side factor |
| High-ground health | 1.15× health-side factor |

Hold mode also changes range, damage, and health through the constants in the same file. Spearmen have their own hold-range multiplier, and archer/spearman/default hold damage multipliers are separate values.

Balance documentation should reference these constants rather than duplicate values from an older tuning pass.

## Current battlefield roles

The troop catalogue and lore describe the intended relationships:

- **Swordsmen** are heavy line infantry and beat spears in a straight infantry fight.
- **Spearmen** are the anti-cavalry line, especially when braced or holding favourable ground.
- **Archers** win through range and are vulnerable when cavalry or infantry reaches them.
- **Cavalry** uses speed and flank roles to choose engagements but is punished by spear counters.
- **Siege** provides long-range pressure and is vulnerable to infantry that reaches it.
- **Elephants** are high-impact faction units with archers as a defined counter.
- **Commanders** concentrate power and utility but are not intended to replace line formations as the default army body.

These relationships are represented simultaneously in troop data, combat constants, formation roles, player-facing lore, and balance fixtures.

## Production cost is data-driven

Production data lives in `assets/data/troops/base.json` and nation overrides. The current economy uses manpower/reserve plus resource costs such as wood, iron, food, stone, and gold where specified by the unit profile.

Do not describe the current roster with a single “gold efficiency” formula unless such a formula is implemented by the balance tooling. `balance_sim` runs the production combat simulation from fixture data; it does not derive pass/fail from the old `sqrt(health × dps) / gold cost` prose formula.

## Siege survivability

Siege behavior is defined by current troop health/range data and the `k_infantry_melee_vs_siege_multiplier` counter.

The durable rule is:

> Siege should control space at range and lose quickly when unescorted infantry reaches it.

Exact health, damage, range, and resource costs belong to the troop data and may change independently of that role.

## Changing troop balance

After changing troop or combat data:

```sh
cmake --build build --target balance_sim
./build/bin/balance_sim
```

The command exits non-zero when a selected fixture violates its configured expectations.

Useful options include:

```sh
./build/bin/balance_sim --filter spear
./build/bin/balance_sim --seeds 20
./build/bin/balance_sim --trace --filter commander
./build/bin/balance_sim --json balance.json --csv balance.csv
```

When the intended matchup changes, update the corresponding fixture in `assets/balance/` together with its description and expectation values. When a new troop introduces a new strategic relationship, add a fixture that exercises that relationship instead of documenting an untested counter only in prose.

## Lore is part of the balance surface

Each troop entry can carry a `lore` block with `role`, `strengths`, `weaknesses`, and historical context. The in-game unit details surface presents this text to the player.

A combat-rule change can therefore make player-facing lore wrong even when the simulation itself is correct. A troop or counter change should keep these three representations aligned:

1. troop and combat data;
2. deterministic balance fixtures; and
3. translated player-facing lore.

`TroopCatalogLoader.ShippedTroopsCarryTheLoreTheInspectPanelDraws` checks that shipped troop lore exists. Translation checks require player-facing source text to be represented across the shipped translation catalogues.
