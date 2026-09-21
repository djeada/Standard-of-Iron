# The Second Punic War: Mission Roster

This document explains the purpose of every campaign mission: what the player is asked to do, what the mission is designed to teach, and whether victory comes from attacking, defending, or managing the economy.

It is intentionally not a specification sheet. Troop counts, wave timings, starting purses, enemy strengths, and other tuning values live in `assets/campaigns/second_punic_war.json`, `assets/missions/*.json`, and the maps they reference. Those values change during balancing, so duplicating them here would create documentation that drifts out of date. **Read the JSON for numbers; read this page for design intent.**

For schema details, see [MISSION_FRAMEWORK.md](MISSION_FRAMEWORK.md). Each mission's `teaching_goal` remains the authoritative statement of its lesson; the table below is the concise version.

The roster is also checked by `tests/map/campaign_roster_test.cpp`. The test re-reads the campaign and fails if this page lists missions in the wrong order or region, assigns the wrong goal type, or claims a starting base that the map does not provide. It finds the relevant columns by header name rather than position, so prose can be improved and columns can be added without breaking the test. Only those design claims are pinned; tuning numbers are not.

## Campaign at a glance

| #   | Mission                  | Region           | Player base | Goal      | What it asks of you                                               |
| --- | ------------------------ | ---------------- | ----------- | --------- | ----------------------------------------------------------------- |
| 1   | Crossing the Rhône       | transalpine_gaul | none        | Offensive | Take ground you cannot afford to level, with what you landed with |
| 2   | Crossing the Alps        | alps             | barracks    | Economic  | Win on the length of the haul, not the fight at the far end       |
| 3   | Battle of Ticino         | cisalpine_gaul   | barracks    | Offensive | Spend minutes on high ground instead of on more men               |
| 4   | Battle of Trebia         | cisalpine_gaul   | barracks    | Defensive | Choose where the assault goes by choosing what it can see         |
| 5   | Battle of Lake Trasimene | etruria          | barracks    | Offensive | Behead the army instead of beating it, against a hard clock       |
| 6   | Battle of Cannae         | apulia           | barracks    | Offensive | Run the ridge, the bait and the beheading at once                 |
| 7   | The Campanian Vigil      | campania         | barracks    | Defensive | Defend supply, and notice theirs is raidable too                  |
| 8   | Battle of Zama           | carthage_core    | barracks    | Both      | Fight something the campaign's one reliable rule cannot kill      |

**Mission 1 is the only mission without a player barracks.** That constraint is load-bearing. The player lands with a small force, a stretch of palisade, and no production, so victory comes from occupying two Roman camps rather than grinding them down. It is also the shortest mission and the only one whose pressure consists of a single early wave. Those choices reinforce one another; changing one can undermine the mission's purpose.

## What the goal types mean

The `Goal` column describes the structure of victory rather than the mood of the mission:

- **Offensive** — victory requires taking ground by capturing or controlling structures.
- **Defensive** — victory requires holding ground by surviving waves or a duration.
- **Economic** — victory depends on reaching a stockpile target. Only _Crossing the Alps_ uses this type.
- **Both** — victory combines taking and holding ground. Only _Battle of Zama_ uses this type.

The column is derived, not decorative. `TheGoalColumnAgreesWithTheVictoryConditions` reads each mission's `victory_conditions` and fails the build if the table disagrees with the mission data.

Every mission uses `victory_mode: "all"`, and every mission includes `eliminate_commanders`. A nation effectively dies with the commander who leads it: when that commander is killed, the nation's barracks become neutral, its other works come down, and its troops leave the field.

This makes decapitation the shortest route to a capture objective rather than a way around one. A neutral camp is still a camp the player must occupy. `MissionAssetRulesTest.DecapitationObjectivesHaveCommandersToKill` fails the build if an AI force ships without a commander spawn, because the elimination rule only arms after an enemy commander has been seen. An unarmed rule would make the mission unwinnable. Commanders are authored in the map's `spawns[]`; see [MISSION_FRAMEWORK.md](MISSION_FRAMEWORK.md).

_The Battle of Lake Trasimene is the only mission that can be lost to the clock._ Every other timer is an optional objective or part of a wave schedule.

## How pressure arrives

The campaign uses two wave-scheduling models. The choice is a design decision, not merely a timing value:

- **Timed** — waves arrive according to a wall-clock schedule. Six of the eight missions use only this model, allowing the player to learn the rhythm and plan around it.
- **Phased** (`after_previous_cleared`) — a new wave arrives after the previous one has been defeated, followed by a short breather. Only _Trebia_ and _The Campanian Vigil_ mix phased waves into their schedules. They are also the only two `survive_waves` missions.

For the phased missions, the objective and the schedule are two sides of the same design: pressure advances with the player's own progress, so the mission cannot simply be outrun.

## The role of the economy

Only _Crossing the Alps_ makes the economy itself the win condition. Only _The Campanian Vigil_ gives the player a developed settlement to operate, complete with a marketplace and homes that are also targets for the Roman assault.

Everywhere else, the starting purse is designed to support reinforcement rather than reconstruction. Mission 1 removes production entirely.

## Ground worth taking

Every mission contains at least one dead zone. Clearing its garrison grants a `clear_reward`: spendable resources that are never counted as harvested. This lets a hoard finance the next push without accidentally satisfying the Alps gathering objective. See [IRON_SEPULCHER.md](IRON_SEPULCHER.md).

Every standalone sanctuary has one or two wardens. Two soldiers do not create a battle; they simply ensure that taking a shrine has a cost.

Wildlife is authored per map rather than inherited from a derived default. Each map includes a sheep pasture worth a detour and a wolf range placed in forest terrain. From the Alps onward, maps also introduce scheduled wolf packs. Wolves can pass through forest while cavalry cannot, which creates threats a mounted column cannot simply chase down. See [AMBIENT_WILDLIFE.md](AMBIENT_WILDLIFE.md) and [RTS_MAP_DESIGN.md](../scripts/RTS_MAP_DESIGN.md).

## Difficulty progression

The campaign's `difficulty_modifier` rises monotonically from the Rhône to Zama. The star rating shown in the campaign list is derived from that value:

`ceil((modifier - 1.0) / 0.15)`, clamped to 1–5, with a small epsilon so a
modifier that lands exactly on a step does not round up past it.

Tune the modifier; the stars follow automatically.

That formula lives in one place, `ui/qml/ScenarioChallenge.qml`. The mission
list and the mission detail panel both ask it, so the star row and the Roman
tactical rating always describe the same mission the same way. This is the
scenario's _inherent_ challenge and has nothing to do with the Easy/Normal/Hard/
Brutal preset the player picks before deploying — see
[DIFFICULTY.md](DIFFICULTY.md). Never feed `difficulty_modifier` into a force,
resource or wave multiplier.

## Guidance for mission tuning

Keep these invariants in mind when changing campaign content:

- **Region IDs must exist in the war table.** `world_region_id` controls the objective pin and camera framing in `MediterraneanMapPanel.qml`. A region missing from `mission_regions` receives no pin.
- **Every mission ships with at least three defeat conditions.** `lose_structure` appears on every mission except the first, which has no key structure to lose.
- **The player is Carthage throughout the campaign.** Every mission continues the Carthage-versus-Rome conflict.
- **Design intent and tuning data should remain separate.** If you change what a mission is _for_, update its `teaching_goal` and the final column of the roster together. If you only change quantities, timings, or strengths, neither needs to change.

That separation keeps this page useful as a durable explanation of the campaign while allowing the underlying missions to evolve through balance work.
