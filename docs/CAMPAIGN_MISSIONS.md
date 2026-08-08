# The Second Punic War — mission roster

A one-page view of what every campaign mission actually is: how many opponents,
how much pressure arrives and when, whether the player has a base to produce
from, and whether the mission is won by attacking or by holding.

`docs/MISSION_FRAMEWORK.md` describes the _schema_ — what a field means and what
values it accepts. This describes the _content_ that ships.

The numbers come from `assets/campaigns/second_punic_war.json`,
`assets/missions/*.json` and the maps they point at.
`tests/map/campaign_roster_test.cpp` re-reads those files and fails if this
table drifts away from them.

## At a glance

| #   | Mission                  | Region           | CPUs | Waves | Player base | Player troops | Goal      |
| --- | ------------------------ | ---------------- | ---: | ----: | ----------- | ------------: | --------- |
| 1   | Crossing the Rhône       | transalpine_gaul |    2 |     1 | none        |             9 | Offensive |
| 2   | Crossing the Alps        | alps             |    3 |     6 | barracks    |            14 | Economic  |
| 3   | Battle of Ticino         | cisalpine_gaul   |    2 |     4 | barracks    |            17 | Offensive |
| 4   | Battle of Trebia         | cisalpine_gaul   |    2 |     6 | barracks    |            33 | Defensive |
| 5   | Battle of Lake Trasimene | etruria          |    2 |     3 | barracks    |            24 | Offensive |
| 6   | Battle of Cannae         | apulia           |    3 |     6 | barracks    |            35 | Offensive |
| 7   | The Campanian Vigil      | campania         |    3 |     9 | barracks    |            26 | Defensive |
| 8   | Battle of Zama           | carthage_core    |    4 |     7 | barracks    |            39 | Both      |

"Player troops" counts the units the map spawns for player 1 at mission start.
"Player base" is what the map gives player 1 to produce from — every mission but
the first opens with a barracks.

## Objectives and timers

| #   | Mission                  | Victory                                                          | Defeat                                                              | Optional             | Hard timer |
| --- | ------------------------ | ---------------------------------------------------------------- | ------------------------------------------------------------------- | -------------------- | ---------- |
| 1   | Crossing the Rhône       | Capture 2 barracks + kill every commander                        | units / commander / commander-alone                                 | Finish inside 10 min | —          |
| 2   | Crossing the Alps        | Gather 600 wood, 350 stone, 300 iron + kill every commander      | structures / units / commander / commander-alone                    | Survive 15 min       | —          |
| 3   | Battle of Ticino         | Capture 2 barracks + kill every commander                        | structures / units / commander / commander-alone                    | Wave count           | —          |
| 4   | Battle of Trebia         | Survive 3 wave phases + kill every commander                     | structures / units / commander / commander-alone                    | Wave count           | —          |
| 5   | Battle of Lake Trasimene | Capture 2 barracks + kill every commander                        | **20 min limit** + structures / units / commander / commander-alone | Wave count           | 20 min     |
| 6   | Battle of Cannae         | Capture 3 barracks + kill every commander                        | structures / units / commander / commander-alone                    | Control structures   | —          |
| 7   | The Campanian Vigil      | Survive 3 wave phases + kill every commander                     | structures / units / commander / commander-alone                    | Control + capture    | —          |
| 8   | Battle of Zama           | Capture 4 barracks, survive 2 undead waves, kill every commander | structures / units / commander / commander-alone                    | Capture structures   | —          |

Trasimene is the only mission that can be lost to the clock. Every other timer
is either an optional objective or a wave schedule.

Every mission now runs `victory_mode: "all"` and every mission carries
`eliminate_commanders`. A nation dies with the man who leads it: on a commander's
death its barracks fall to the neutral owner, its other works come down and its
troops leave the field. That makes decapitation the shortest route to a capture
objective rather than a way around one — a neutral camp is still a camp you have
to walk into. `MissionAssetRulesTest.DecapitationObjectivesHaveCommandersToKill`
fails the build if any AI setup ships without a `commander_troop`, because the
rule only arms once an enemy commander has been seen and an unarmed rule would
make the mission unwinnable.

## Economy

| #   | Mission                  | Starting gold / food | Other resources               | Economic pressure                                         |
| --- | ------------------------ | -------------------: | ----------------------------- | --------------------------------------------------------- |
| 1   | Crossing the Rhône       |            300 / 200 | —                             | None — no base, fight with what you land with             |
| 2   | Crossing the Alps        |            450 / 650 | —                             | **The objective.** 4 builders, gather 600/350/300         |
| 3   | Battle of Ticino         |            550 / 450 | —                             | Light                                                     |
| 4   | Battle of Trebia         |            500 / 380 | —                             | Light                                                     |
| 5   | Battle of Lake Trasimene |            360 / 280 | —                             | Tightest purse in the campaign                            |
| 6   | Battle of Cannae         |            750 / 600 | —                             | Moderate                                                  |
| 7   | The Campanian Vigil      |          1800 / 1500 | 900 wood, 700 stone, 450 iron | **Full economy.** Opens with a marketplace and four homes |
| 8   | Battle of Zama           |            650 / 520 | —                             | Moderate                                                  |

Only _Crossing the Alps_ makes the economy the win condition; only _The
Campanian Vigil_ hands the player a developed settlement to run.

## Wave pressure

| #   | Mission                  | Waves | Schedule                               | First wave |      Last wave |
| --- | ------------------------ | ----: | -------------------------------------- | ---------: | -------------: |
| 1   | Crossing the Rhône       |     1 | timed                                  |       0:15 |           0:15 |
| 2   | Crossing the Alps        |     6 | timed                                  |       2:30 |          13:00 |
| 3   | Battle of Ticino         |     4 | timed                                  |       3:00 |           8:00 |
| 4   | Battle of Trebia         |     6 | 3 phases, state-driven after the first |       2:00 | on clear + 40s |
| 5   | Battle of Lake Trasimene |     3 | timed                                  |       2:30 |           5:20 |
| 6   | Battle of Cannae         |     6 | timed                                  |       3:30 |           9:00 |
| 7   | The Campanian Vigil      |     9 | 3 phases, state-driven after the first |       3:00 | on clear + 45s |
| 8   | Battle of Zama           |     7 | timed                                  |       3:00 |           8:40 |

Trebia and Campania are the two missions built on `after_previous_cleared`:
their waves arrive in phases keyed to the player breaking the previous one, with
a 35–45 second breather, rather than on a wall clock. Both are also the two
`survive_waves` missions — the survive-three-phases objective and the
phase-driven schedule are the same design.

## Difficulty curve

| #   | Mission                  | Modifier | Stars | CPUs | Waves | Enemy troops on the map |
| --- | ------------------------ | -------: | ----- | ---: | ----: | ----------------------: |
| 1   | Crossing the Rhône       |     1.10 | ★☆☆☆☆ |    2 |     1 |                      38 |
| 2   | Crossing the Alps        |     1.15 | ★☆☆☆☆ |    3 |     6 |                      20 |
| 3   | Battle of Ticino         |     1.18 | ★★☆☆☆ |    2 |     4 |                      28 |
| 4   | Battle of Trebia         |     1.28 | ★★☆☆☆ |    2 |     6 |                      32 |
| 5   | Battle of Lake Trasimene |     1.38 | ★★★☆☆ |    2 |     3 |                      26 |
| 6   | Battle of Cannae         |     1.55 | ★★★★☆ |    3 |     6 |                      44 |
| 7   | The Campanian Vigil      |     1.58 | ★★★★☆ |    3 |     9 |                      32 |
| 8   | Battle of Zama           |     1.70 | ★★★★★ |    4 |     7 |                      40 |

The modifier rises monotonically. The star column is what the campaign list
draws: `ceil((modifier - 1.0) / 0.15)`, clamped to 1–5.

"Enemy troops on the map" counts line troops only: spawns owned by player 2 or
above, minus builders and civilians, and **minus anything tagged with a
`landmark`**. Sanctuary wardens and watch pickets are deliberately outside the
number because they are not part of the army the mission is balanced against.

## Ground worth walking to

Every mission ships at least one dead zone, and every dead zone pays a
`clear_reward` when its garrison breaks — spendable resources, never counted as
harvested, so a hoard funds the next push without shortcutting the Alps gather
objective. See [IRON_SEPULCHER.md](IRON_SEPULCHER.md).

Every standalone sanctuary now has one or two wardens on it. Two men do not make
a battle; they make the shrine cost something, which is the point.

Every map authors its wildlife rather than inheriting the derived default: a
sheep pasture somewhere worth the detour, a wolf range denned in a wood, and —
from the Alps onward — wolf packs on a schedule. Wolves are forest-passable and
cavalry is not, so a pack in a grove is a threat a mounted column cannot chase.
See [AMBIENT_WILDLIFE.md](AMBIENT_WILDLIFE.md) and
[RTS_MAP_DESIGN.md](../scripts/RTS_MAP_DESIGN.md).

| #   | Mission                  | Dead zones | Wolf packs |
| --- | ------------------------ | ---------: | ---------: |
| 1   | Crossing the Rhône       |          1 |          0 |
| 2   | Crossing the Alps        |          1 |          1 |
| 3   | Battle of Ticino         |          2 |          1 |
| 4   | Battle of Trebia         |          1 |          2 |
| 5   | Battle of Lake Trasimene |          1 |          1 |
| 6   | Battle of Cannae         |          1 |          2 |
| 7   | The Campanian Vigil      |          1 |          3 |
| 8   | Battle of Zama           |          2 |          2 |

## Notes for anyone tuning this

- **Mission 1 is the only one without a player barracks.** The player lands with
  nine soldiers, four wall segments and no production, and has to take two
  Roman camps with them. It is the shortest mission and the only one whose
  pressure is a single 15-second wave — those two facts are load-bearing
  together.
- **Region ids must exist in the war table.** `world_region_id` drives the
  objective pin and the camera framing in `MediterraneanMapPanel.qml`. A region
  that is not in `mission_regions` there gets no pin.
- **Every mission ships three or more defeat conditions.** `lose_structure` is
  present on all but the first, which has no key structure to lose.
- The campaign runs Carthage against Rome throughout; the player is Carthage in
  every mission.
