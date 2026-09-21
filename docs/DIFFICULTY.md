# Difficulty Presets

Single-player difficulty is one shared system across the campaign, standalone missions and skirmish. It gives the enemy more or fewer **resources, starting troops and scripted reinforcements**. It does not make the opponent think better, and it does not touch combat statistics.

## The presets

| Preset | Canonical ID | Enemy starting resources | Eligible enemy starting troops | Scripted enemy reinforcements |
| ------ | ------------ | ------------------------ | ------------------------------ | ----------------------------- |
| Easy   | `easy`       | 0.80x                    | 0.80x                          | 0.75x                         |
| Normal | `normal`     | 1.00x                    | 1.00x                          | 1.00x                         |
| Hard   | `hard`       | 1.50x                    | 1.50x                          | 1.50x                         |
| Brutal | `very_hard`  | 2.00x                    | 2.00x                          | 2.00x                         |

`very_hard` is the canonical ID. It is what the save database, the replay header and the settings file store. "Brutal" is only the player-facing name.

`Game::Mission::resolve_difficulty()` in `game/mission/difficulty_profile.h` is the single place those numbers live. Nothing else may hold a copy.

Older authored spellings still resolve: `medium`/`standard` become `normal`, `recruit` becomes `easy`, `veteran` becomes `hard`, and `brutal`/`legendary` become `very_hard`. Anything unrecognised — including an empty string — becomes `normal`, so a mission authored before this system behaves exactly as it did.

## Who it reaches

A preset moves computer opponents and nothing else. Which opponents is decided
once, by `Game::Mission::difficulty_applies_to()` in
`game/mission/difficulty_forces.h`, and every one of the four application points
below asks it rather than deciding for itself:

- a seat that was given its own preset always uses it. That is how the skirmish
  roster works, and it holds in an observed match where every seat is an AI and
  there is no human at all;
- otherwise the match baseline applies, and only to owners who are the local
  human's enemies. An AI on the player's team, a neutral owner and the player's
  own force are left exactly as the scenario authored them.

A mission puts an AI on the player's side by giving `player_setup` a `team_id`
and the friendly `ai_setups` entry the same one. Without a player `team_id` the
player has no team, every authored opponent is an enemy, and a mission written
before this existed behaves exactly as it did.

## What a bonus may not cost

A reinforcement has to be able to stand where it is put. Every troop the preset
adds -- to a map-spawned force, to an authored mission force, or to a scripted
wave -- goes through the one ring search in `game/mission/spawn_placement.h`,
which starts at the position it was copied from and walks outward over ground
the unit can actually occupy, clear of everything already there. A troop with
nowhere to stand is withdrawn, not stacked: the count that arrives is reported
in the log beside the count that was asked for, along with the owner, its team,
the authored baseline and the preset. Authored troops are never withdrawn this
way; a scenario's own force stands where the file puts it.

Reinforcements also respect the population cap the map sets. An owner already at
its cap gets no extra troops, and the shortfall is logged rather than quietly
advertised as a full bonus.

### What is deliberately _not_ scaled

- the player's own units, buildings and treasury;
- damage, hit points, armour, unit cost and movement speed;
- AI decision cadence, vision, scouting range, personality, posture and strategy;
- the number of waves, their timings, their triggers or their rewards;
- commanders, mission stages, scripted messages, objectives, time limits and campaign unlock order.

An opponent's _authored_ difficulty (`ai_setups[].difficulty` in the mission file) is a separate dial that tunes AI execution and authored wave strength. The two are multiplied once each and never applied twice: authored difficulty describes the scenario, the player's preset describes how much material the scenario's opponents are handed.

## Where the number is applied

Each category is scaled exactly once, from an immutable authored baseline:

| Category                      | Applied by                                                  | When                                                 |
| ----------------------------- | ----------------------------------------------------------- | ---------------------------------------------------- |
| Map-spawned enemy troops      | `Game::Mission::apply_starting_force_difficulty()`          | once, after the level loads and before mission setup |
| Mission-authored enemy troops | `MissionSetupCoordinator::apply_mission_setup()`            | once, while spawning `ai_setups[].starting_units`    |
| Enemy resources               | `SkirmishRuntimeCoordinator::initialize_player_resources()` | once, when the economy is endowed                    |
| Wave composition              | `build_pending_mission_waves()`                             | once, while the pending waves are built              |

Rounding is to the nearest whole unit, and a force that exists never rounds to nothing: `scaled_force_count()` keeps at least one unit. A resource stock that is authored empty stays empty, and an authored number too large to scale saturates rather than wrapping.

Waves round **role by role**, so the advertised percentage is the multiplier applied to each role, not a promise about the total. A wave of 1 + 3 + 5 at Hard becomes 2 + 5 + 8 — fifteen troops where an exact +50% would be thirteen and a half. Small waves therefore come out slightly heavier than the headline number; the alternative is a role that rounds away to nothing. `tests/map/wave_archetype_catalog_test.cpp` pins the actual outcomes.

The tutorial is always Normal, whatever the player last chose, and playing it does not change the remembered default.

Reinforcements are placed by a ring search around the unit they were copied from, on ground the unit can actually stand on and clear of anything already there. No separate map or mission file is ever needed. Withdrawn units on Easy are removed from the most numerous troop type first, so an opponent never loses the last unit of a kind.

## Where the player chooses it

- **Campaign** — the selector sits on the mission detail panel of the war table, and the choice is remembered between sessions. Replaying a mission re-offers it.
- **Standalone mission** — the same selector sits above the deploy button on the missions screen.
- **Skirmish** — difficulty belongs to each AI seat, not to the match. Click the Difficulty chip on an opponent's roster row to cycle it. Changing the map, colour, team, nation or commander leaves each seat's preset alone.

All three routes go through `DifficultySelector.qml` and `DifficultyCatalog.qml` so the wording, the icons and the numbers cannot drift apart. Every card shows the preset's icon, its name in words and its exact numeric effect — the icon and the joke never carry the meaning on their own.

The preset is persisted in three places, and each one survives a relaunch:

- `match/difficulty` in the user settings is the remembered default for the next campaign or mission launch;
- the mission context is saved with the match and restored with it, so loading a save resumes at the difficulty it was played at;
- the replay header carries it, so a replay reproduces the run it recorded.

Loading a save restores units and stockpiles as they were; it never re-applies a bonus.

A mixed skirmish cannot be described by one string, so the save carries a
`match_launch` block beside the mission context: the baseline, the per-seat
presets by owner id, and the file the mission definition was read from. A
mission that has not spawned its later waves yet also carries those waves'
resolved composition, so loading a Brutal save does not rebuild the coming
assault at its authored size -- or at today's size, if the mission file has been
edited since. A save written before any of this normalises to Normal.

The mission file reference matters for a mission started from an arbitrary path:
its id alone cannot find it again. A bundled mission is remembered by its
resource path, which resolves from any working directory; anything else is
remembered verbatim, and if it has gone missing the load says so rather than
quietly substituting a bundled mission that happens to share the id.

A replay reads its difficulty from its own header and only from there. An old
header that names none is Normal, not whatever the menus are set to now, and
watching a replay never changes the remembered default.

## Scenario challenge is a different number

`difficulty_modifier` in the campaign file says how hard a mission is _as
written_. It is not a preset, it never reaches a force multiplier, and the war
table shows it through `ui/qml/ScenarioChallenge.qml` -- one formatter for the
star row on the list and the Roman rating on the detail panel, so the two cannot
disagree about the same mission.

## Author markup

Scenario authors can hold part of a mission out of the scaling.

```json
{
    "ai_setups": [
        {
            "id": "roman_outpost",
            "difficulty_scaling": false,
            "starting_units": [
                {
                    "type": "spearman",
                    "count": 4,
                    "position": { "x": 30, "z": 12 }
                },
                {
                    "type": "archer",
                    "count": 2,
                    "position": { "x": 32, "z": 12 },
                    "difficulty_scaling": false
                }
            ]
        }
    ]
}
```

- `ai_setups[].difficulty_scaling: false` — this opponent is a fixed set piece. Its authored troops and its waves are used exactly as written at every preset.
- `starting_units[].difficulty_scaling: false` — this one spawn group is fixed while the rest of the opponent still scales. Use it for a garrison whose size is an objective, or for a unit a stage counts.

Both default to `true`. Commanders are never duplicated or withdrawn, whether or not the markup is present, and neither are buildings: only ordinary troops move with the preset.

## Tests

- `tests/map/difficulty_profile_test.cpp` — the multiplier contract, ID normalisation, rounding, clamping, idempotence and per-owner resolution.
- `tests/core/difficulty_presets_test.cpp` — a whole mission loaded at each preset, checking that every enemy category moves together, that the player's force and treasury never move, that commanders are never duplicated, and that Normal reproduces the authored match exactly.

    The mission it loads is written by the test itself onto `assets/maps/map_tutorial.json`: two enemy opponents on one team, each with a wave to scale, on a 96x96 map that spawns the opposing line and its commander. The enemy troops on the field come from the map alone, so each opponent's force is one group and the sum of the scaled parts is the scaled sum -- the arithmetic the assertions spell out. `TwoSourcesOfStartingTroopsAreEachScaledOnce` is the test that authors troops in the mission as well, and it is the one that proves the two sources are each scaled exactly once. It used to load `assets/missions/battle_of_ticino.json`, and that is the whole reason for the change: a 650x650 campaign map costs about twelve seconds to load in a Debug build on a CI runner, the suite loads a match fourteen times, and five of its tests ran past the 10 s per-test budget that `scripts/check-test-speed.py` enforces on the pull-request profile. Nothing about the assertions is campaign content: the presets are arithmetic over whatever a mission authored, so a small authored fixture proves the same thing and pins the counts the assertions expect instead of inheriting them from content someone may retune. Shipped campaign content is loaded at difficulty in the extended profile by `CampaignWaveAssaultTest` and `MissionWaveAssaultTest`.

- `tests/map/difficulty_profile_test.cpp` (again) — `difficulty_applies_to`: the mission baseline reaching only the human's enemies, a skirmish seat carrying its own preset whatever team it is on, and an observed match with no human at all.
- `tests/core/campaign_manager_test.cpp` — where a mission is remembered from, and that a custom mission is never swapped for a bundled one with the same id.
- `tests/core/mission_wave_director_test.cpp` — an unspawned wave keeping the composition it was saved with.
- `tests/ui/qml/tst_difficulty_selector.qml` — the catalogue, the numeric summaries, keyboard navigation and the selected state.
- `tests/ui/qml/tst_scenario_challenge.qml` — the list and the detail panel agreeing on a mission's inherent rating.
- `tests/ui/qml/tst_map_select.qml` — per-seat difficulty in the skirmish roster and its trip into the launch configuration.
