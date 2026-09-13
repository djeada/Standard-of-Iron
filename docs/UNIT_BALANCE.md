# Unit Balance

Unit balance in Standard of Iron is defined by executable simulation inputs: shipped troop data, nation overrides, combat multipliers, formation/stance behavior, production/economy values, and deterministic matchup fixtures run through `tools/balance_sim`.

The repository does not use one prose-only efficiency formula as the balance authority. A relationship such as “spears counter cavalry” is meaningful only when the current simulation, current troop data, and current fixture expectations agree.

## Sources of truth

The main balance inputs are:

- `assets/data/troops/base.json` — base troop production, combat, visual, formation-role, and lore data;
- `assets/data/nations/*.json` — nation-specific troop variants/overrides;
- `game/systems/combat_system/combat_types.h` — combat, terrain, hold-mode, and counter multipliers;
- `assets/balance/*.json` — deterministic matchup fixtures and expectations; and
- `tools/balance_sim/` — production-simulation runner used to execute those fixtures.

`tests/tools/balance_sim_test.cpp` gates the fixture suite.

## What “balanced” means here

Balance is not treated as every unit having equal damage-per-cost in isolation.

The game is built around differentiated battlefield roles. A unit can be intentionally inefficient in one matchup and extremely efficient in another if that relationship produces a clear tactical counter with enough room for formation, terrain, movement, economy, and combined-arms decisions to matter.

The executable balance fixtures therefore encode **relationships**, not a universal ranking.

Examples include:

- mirror stability;
- explicit hard/soft counters;
- line-vs-line faction comparisons;
- elite/commander-vs-line interactions; and
- faction-specific/special-unit matchups.

## Deterministic fixtures

Each JSON file under `assets/balance/` defines a repeatable simulation matchup.

A fixture can specify areas such as:

- participating groups;
- unit types;
- owner/nation context;
- counts;
- stances or setup;
- seed count;
- simulation duration; and
- acceptable outcome bounds.

Because these values live in data, a balance expectation can be reviewed and changed independently from the simulation code while still being checked by an executable test.

## Current fixture relationships

The current fixture set covers relationships including:

- swordsman mirror behavior;
- spearmen against cavalry;
- cavalry against archers;
- infantry against siege;
- archers against elephants;
- spearmen against swordsmen;
- Roman vs Carthaginian line matchups;
- commanders against line infantry;
- Iron Sepulcher forces against Rome; and
- horse archers against spearmen.

These fixtures are not just demonstrations. Their expectation ranges are the maintained regression contract for the represented relationships.

## Example: commander vs line

`assets/balance/08_commander_vs_line.json` places one Roman commander against four legionary swordsman groups.

Its intent is not “the commander should always win because it is elite.” The fixture represents the role distinction that an elite individual can be powerful and tactically valuable without replacing ordinary line infantry as the default cost-efficient army body.

That is a better executable balance statement than a standalone ratio such as health × damage divided by cost, because it tests the production combat system directly.

## Combat multipliers

Current combat constants are defined in `game/systems/combat_system/combat_types.h`.

Important relationships include:

| Rule | Current multiplier |
| --- | ---: |
| Spearmen vs cavalry | 2.5× |
| Infantry melee vs siege | 3.0× |
| Archers vs elephants | 1.68× |
| Archers from high ground | 1.8× damage |
| Spearmen from high ground | 1.8× damage |
| High-ground armour-side factor | 0.85× |
| High-ground health-side factor | 1.15× |

Hold mode also modifies range, damage, and health using constants in the same file. Spearmen have their own hold-range multiplier, and default/archer/spear hold damage tuning is represented separately.

The code is authoritative for the exact numeric values and for where they enter the damage/range calculation.

## Counter relationships

### Spearmen vs cavalry

The 2.5× spear-vs-cavalry relationship makes cavalry pay heavily for entering a prepared spear engagement.

That counter does not imply that spearmen should catch cavalry easily. Movement speed, formation geometry, path choice, flank access, hold/bracing behavior, and terrain still determine whether the favorable combat contact occurs.

### Cavalry vs archers

Cavalry pressure on archers comes from mobility and melee access rather than a single documented “cavalry vs archer” scalar in the table above.

The fixture exercises the full relationship: can cavalry reach the ranged line and convert mobility into combat advantage under the production systems?

### Infantry vs siege

`k_infantry_melee_vs_siege_multiplier` currently gives ordinary infantry melee a 3.0× counter against siege.

The intended role is that siege controls space from range but is vulnerable once unescorted infantry reaches it.

### Archers vs elephants

Archers currently receive the explicit 1.68× elephant counter.

Elephants remain high-impact units, but the counter gives the opposing army a roster answer that can be verified in the fixture suite.

### Spears vs swordsmen

Spears are not intended to be universal heavy infantry. The fixture set checks that their anti-cavalry identity does not also make them automatically superior to swordsmen in an ordinary infantry fight.

## Terrain as balance

High ground is part of the combat balance surface, not merely a map-presentation feature.

The current constants give archers and spearmen a 1.8× damage multiplier from high ground, while high-ground defensive factors also affect survivability through the current armour/health-side multipliers.

This means a matchup fixture run on neutral ground does not represent every tactical situation in a real mission.

Map geometry can amplify or reduce the value of a unit role by controlling:

- approach width;
- line of sight;
- high-ground access;
- cavalry maneuver room;
- siege firing positions; and
- formation frontage.

Unit balance and map balance therefore interact while remaining separate authored layers.

## Formation state as balance

Formation systems also participate in combat outcomes.

Examples include:

- army cohesion modifying incoming damage;
- defensive unit layouts applying directional/combat modifiers;
- hold mode altering effective range/damage/health; and
- constrained terrain changing how many files can make contact.

A unit's base stats should not be interpreted as the entire combat result in isolation from these runtime systems.

See [FORMATION_ARCHITECTURE.md](FORMATION_ARCHITECTURE.md) and [COMBAT_SYSTEM.md](COMBAT_SYSTEM.md).

## Production cost is data-driven

Production values live in troop data and nation overrides.

The economy uses the current reserve/manpower and resource-cost model. Unit profiles can spend resources such as wood, iron, food, stone, and gold where those values are present.

Balance documentation should therefore avoid collapsing every unit into one historical “gold cost” if that is not the value the production system actually charges.

The current balance simulator does not gate fixtures using the old prose formula:

```text
sqrt(health × dps) / gold cost
```

That expression is not the active balance authority.

## Recruit price vs army-cap weight

The economy distinguishes the price a recruiting building spends from the army-cap weight represented by `population_cost()`/the current production data model.

Those quantities have different balance purposes:

- recruit price tunes the economy required to field the unit;
- army-cap weight tunes how much force fits under the scenario/player cap.

Changing one should not automatically be described as changing the other.

See [ECONOMY_GUIDANCE.md](ECONOMY_GUIDANCE.md) for the current production/readout contract.

## Battlefield roles

The current troop catalogue and player-facing lore describe broad roles such as:

### Swordsmen

Heavy line infantry intended to perform well in ordinary infantry contact and beat spears in a direct sword-vs-spear line fight.

### Spearmen

Anti-cavalry line infantry whose value rises when they can hold favorable ground and force mounted units into prepared contact.

### Archers

Ranged pressure that benefits from distance and high ground, but loses value rapidly when mobile/melee troops reach the line.

### Cavalry

Fast force able to choose approaches, exploit flanks, threaten archers, and disengage from unfavorable fights, while being punished by spear counters.

### Siege

Long-range positional pressure against troops/structures, balanced by severe vulnerability once infantry reaches melee.

### Elephants

High-impact faction units with archers as a maintained explicit counter.

### Commanders

Concentrated individual power and utility that supports an army but is not intended to replace ordinary line formations as the default force body.

The exact numbers for each troop remain in current data rather than in this narrative role summary.

## Nation variants

Nation files can override troop data, which means two troops sharing a broad role do not have to be numerically identical across factions.

The balance surface therefore includes both:

- base role identity; and
- nation-specific expression.

Faction line fixtures are useful because they test the resulting combined data rather than assuming all line infantry inherit the base profile unchanged.

## Siege survivability

Siege survivability should be understood as a combined product of:

- current health;
- range;
- damage/cooldown;
- formation/support positioning;
- pathing access for attackers; and
- the 3.0× infantry melee counter.

The durable role rule is:

> Siege should control space at range and lose quickly when unescorted infantry reaches it.

Exact values belong to the troop/combat data and can change while that role remains stable.

## Running the balance suite

Build and run:

```sh
cmake --build build --target balance_sim
./build/bin/balance_sim
```

The tool exits non-zero when a selected fixture violates its configured expectations.

Useful options include:

```sh
./build/bin/balance_sim --filter spear
./build/bin/balance_sim --seeds 20
./build/bin/balance_sim --trace --filter commander
./build/bin/balance_sim --json balance.json --csv balance.csv
```

These modes support focused iteration, larger seed sets, trace inspection, and machine-readable reports.

## How to change balance safely

A balance change should identify which layer is actually being tuned.

### Troop stat change

Edit the current troop/nation data, then run the fixtures affected by that role.

### Counter/terrain/hold multiplier change

Edit `combat_types.h`, then run every fixture that depends on that relationship plus the general suite.

### Economy/cost change

Edit production/resource data and verify the economy/UI still advertise the value the production system spends. Combat fixtures alone do not prove the economy side is correct.

### Intended matchup change

If the design expectation truly changes, update the corresponding fixture description and bounds together with the data/code change.

### New strategic relationship

Add a new fixture rather than relying only on a prose statement such as “X should counter Y.”

The fixture is what makes the relationship executable and regression-tested.

## Diagnosing a fixture failure

A failed matchup does not automatically mean “increase damage.”

Check the layers in order:

1. did the expected units/data load?
2. did formation/stance setup match the fixture intent?
3. did pathing/contact occur as expected?
4. did target/range rules create the intended engagement?
5. did combat multipliers apply?
6. did the fight finish inside the configured duration?
7. is the expectation range still the desired design relationship?

A counter fixture can fail because the units never made appropriate contact, not because the counter scalar is wrong.

## Seed count and variance

Fixtures use deterministic seeds so individual runs are reproducible while the suite can still sample several seeded outcomes.

Increasing `--seeds` is useful when a change may affect variance across deterministic starting/random sequences.

The goal is not to hide variance but to keep it bounded and inspectable.

## Lore is part of the balance surface

Troop entries can carry a `lore` block with fields such as:

- `role`;
- `strengths`;
- `weaknesses`; and
- historical/context text.

The unit-details UI exposes this information to players.

A combat or economy change can therefore make the game mechanically correct while making the explanation wrong.

A balance change should keep three representations aligned:

1. troop/combat/economy data;
2. deterministic fixture expectations; and
3. translated player-facing lore.

`TroopCatalogLoader.ShippedTroopsCarryTheLoreTheInspectPanelDraws` checks that shipped troop lore is present. Translation checks ensure the player-facing source text is represented across the shipped translation catalogues.

## Translation surface

The repository currently ships translation catalogues for English, German, Spanish, Brazilian Portuguese, Arabic, Turkish, Polish, and Russian.

When role/strength/weakness text changes, the source translation entries need to remain synchronized with the player-facing lore surface.

Balance documentation should not claim a mechanical relationship that the inspect panel describes differently.

## Balance invariants

The current balance workflow depends on these invariants:

- executable fixture data is the regression contract;
- combat constants come from current source, not old tuning notes;
- production cost and army-cap weight remain separate concepts;
- terrain/formation/stance can influence real matchup outcomes;
- faction variants are tested as loaded data;
- fixture failures are diagnosed through the production simulation path; and
- player-facing lore stays aligned with current mechanics.

## Source map

| Concern | Source |
| --- | --- |
| Base troop data | `assets/data/troops/base.json` |
| Nation overrides | `assets/data/nations/*.json` |
| Combat/counter constants | `game/systems/combat_system/combat_types.h` |
| Fixture data | `assets/balance/*.json` |
| Simulator | `tools/balance_sim/` |
| Test gate | `tests/tools/balance_sim_test.cpp` |
| Economy explanation | [ECONOMY_GUIDANCE.md](ECONOMY_GUIDANCE.md) |
| Combat pipeline | [COMBAT_SYSTEM.md](COMBAT_SYSTEM.md) |
| Formation/stance effects | [FORMATION_ARCHITECTURE.md](FORMATION_ARCHITECTURE.md) |

The active unit-balance contract is the combination of current production data, current combat rules, and deterministic fixtures. Historical one-number efficiency formulas are not a substitute for that executable system.
