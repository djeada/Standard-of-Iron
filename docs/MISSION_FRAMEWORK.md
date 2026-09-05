# Mission Framework Documentation

The Mission Framework provides a formal authoring layer for creating structured gameplay experiences in Standard of Iron. It separates playable maps from mission logic, allowing designers to create reusable missions and organize them into campaigns.

## Architecture

### Three-Tier Structure

```
assets/
  ├── maps/          # Playable level geometry + terrain
  │   ├── map_forest.json
  │   ├── map_mountain.json
  │   └── map_rivers.json
  ├── missions/      # Gameplay rules that reference maps
  │   ├── the_timber_levy.json      # standalone: listed under Missions
  │   └── battle_of_cannae.json     # claimed by a campaign
  └── campaigns/     # Ordered mission collections
      └── second_punic_war.json
```

A mission belongs to a campaign or to no campaign; the second kind is a
standalone mission and is what the Missions menu lists. See
[Standalone Missions](#standalone-missions).

### Components

- **Map**: Pure level geometry, terrain, and environment
- **Mission**: Gameplay setup, objectives, and win/loss conditions
- **Campaign**: Ordered collection of missions with narrative metadata

## Mission Configuration

### File Structure

```json
{
  "id": "defend_outpost",
  "title": "Defend the Outpost",
  "summary": "Hold your position against waves of enemy attacks",
  "map_path": ":/assets/maps/map_forest.json",

  "player_setup": { ... },
  "ai_setups": [ ... ],
  "victory_conditions": [ ... ],
  "defeat_conditions": [ ... ],
  "stages": [ ... ],
  "events": [ ... ]
}
```

### Player Setup

Defines the human player's starting configuration:

```json
"player_setup": {
  "nation": "roman_republic",
  "faction": "roman",
  "color": "red",
  "starting_units": [
    {
      "type": "spearman",
      "count": 10,
      "position": {"x": 60, "z": 60}
    }
  ],
  "starting_buildings": [
    {
      "type": "barracks",
      "position": {"x": 60, "z": 60},
      "max_population": 200
    }
  ],
  "starting_resources": {
    "gold": 1000,
    "food": 500
  }
}
```

### Commanders

**Commanders are authored in the map, never in the mission.** A commander reaches
the field as an ordinary entry in the map's `spawns[]` whose `type` is a commander
troop, carrying the owning `player_id` and that force's `nation`:

```json
{
    "type": "carthage_sword_commander",
    "player_id": 1,
    "nation": "carthage",
    "x": 283.85,
    "z": 434.14,
    "max_population": -1
}
```

Every force a mission declares — the player as owner 1, then each entry of
`ai_setups` as owners 2, 3, … in order — needs exactly one such spawn. Mission
setup no longer invents a commander for a force that lacks one; it warns and that
force starts headless, which will lose it the mission to `no_commander`.

This used to be a `commander_troop` field on `player_setup` and each AI setup, which
made two sources of truth: a map that authored a commander silently won, because the
map's units are already in the world when mission setup runs. The field is gone.
`MissionLoader` warns if it finds one and `MissionLoaderTest.ShippedMissionsDoNotAuthorCommanders`
fails the build, so the dead field cannot creep back in.

The map is also what fixes the commander's **position**. Before, position was derived
from the densest cluster of a force's units, and a force with no authored units landed
on a hardcoded fallback far from its camp — `battle_of_ticino`'s reserve, `campania_campaign`'s
siege column and `battle_of_zama`'s rear guard all did. Authoring the spawn puts each
commander exactly where the author wants it.

Two rules are pinned by tests over the shipped campaign:

- **Coverage** — every force has exactly one commander, of its own nation.
  `MissionCommanderSetupTest.EveryCampaignForceHasExactlyOneMapCommander` and
  `CampaignCommandersMatchTheirForcesNation` check the data;
  `CampaignWaveAssaultTest.EveryCampaignForceLoadsWithExactlyOneCommander` loads every
  mission for real and counts the live commanders per owner.
- **Distinctness** — no two forces in one mission share a commander, until a nation
  fields more forces than it has commanders. Each nation has three, so
  `battle_of_zama`'s four Roman forces must repeat exactly one; every other mission
  must be fully distinct.
  `MissionCommanderSetupTest.CampaignCommandersAreUniqueUntilThePoolRunsOut` enforces
  the `min(forces, pool)` rule.

In the map editor, a force whose commander the map has forgotten draws as a red crossed
ring naming the nation-appropriate commander to place — never as a phantom commander badge.

### AI Setup

Defines AI opponents with personality and behavior:

```json
"ai_setups": [
  {
    "id": "enemy_1",
    "nation": "carthage",
    "faction": "carthaginian",
    "color": "blue",
    "difficulty": "hard",
    "strategy": "aggressive",
    "team_id": 1,
    "personality": {
      "aggression": 0.7,
      "defense": 0.3,
      "harassment": 0.5
    },
    "starting_buildings": [
      {
        "type": "barracks",
        "position": {"x": 180, "z": 180},
        "max_population": 140
      }
    ],
    "starting_units": [
      {
        "type": "builder",
        "count": 2,
        "position": {"x": 176, "z": 178}
      },
      {
        "type": "spearman",
        "count": 6,
        "position": {"x": 182, "z": 182}
      }
    ],
    "waves": [
      {
        "timing": 120.0,
        "composition": [
          {"type": "swordsman", "count": 8},
          {"type": "archer", "count": 4}
        ],
        "entry_point": {"x": 190, "z": 190}
      }
    ]
  }
]
```

#### AI fields

| Field                | Required | Meaning                                                             |
| -------------------- | -------- | ------------------------------------------------------------------- |
| `id`                 | Yes      | Mission-local identifier for the AI setup                           |
| `nation`             | Yes      | Nation used for roster resolution                                   |
| `faction`            | Yes      | Faction metadata for mission/UI use                                 |
| `color`              | Yes      | Player color                                                        |
| `difficulty`         | No       | Execution tuning, and a wave-size multiplier; omitted means normal  |
| `strategy`           | No       | Base strategic preset; omitted falls back to `defensive`            |
| `posture`            | No       | `garrison` (default) holds and defends; `field` may attack/expand   |
| `team_id`            | No       | Allies AIs with the same team and prevents them fighting each other |
| `personality`        | No       | Fine-tunes aggression / defense / harassment on top of the strategy |
| `starting_units`     | No       | Spawns the AI with units at mission start                           |
| `starting_buildings` | No       | Spawns the AI with structures at mission start                      |
| `wave_escalation`    | No       | Per-wave growth in wave size for this AI; `0.12` is +12% each wave  |
| `waves`              | No       | Reinforcement waves layered on top of the regular AI                |

### Waves

A wave is a scripted reinforcement drop for one AI setup. Waves are grouped into
**assault phases**; the HUD, the `survive_waves` victory condition and the wave
announcements all count phases, not individual wave entries.

#### Wave fields

| Field             | Required | Meaning                                                                        |
| ----------------- | -------- | ------------------------------------------------------------------------------ |
| `timing`          | Yes\*    | Seconds from mission start. Ignored when `trigger` is `after_previous_cleared` |
| `trigger`         | No       | `time` (default) or `after_previous_cleared`                                   |
| `grace_seconds`   | No       | Breather after the previous wave of this AI clears, before this one lands      |
| `warning_seconds` | No       | How far ahead of the wave the telegraph fires (default 15)                     |
| `phase`           | No       | Explicit assault phase; omitted phases are derived from matching `timing`s     |
| `composition`     | No       | Explicit unit list                                                             |
| `archetype`       | No       | Named template appended to `composition` (see below)                           |
| `strength`        | No       | Multiplies every unit count in this wave (default 1.0)                         |
| `entry_point`     | No       | Where the wave arrives                                                         |
| `entry_points`    | No       | Several arrival points; composition groups are dealt round-robin between them  |
| `label`           | No       | Name used in announcements instead of the AI id                                |
| `clear_reward`    | No       | Resources paid to the player once this wave's phase is broken                  |

\* `timing` is required for `time`-triggered waves, which is the default.

#### Timed versus state-driven waves

A `time` wave lands at `timing`, whatever the player is doing. That suits set-piece
battles where the historical schedule is the point.

An `after_previous_cleared` wave waits until the previous wave _from the same AI_ is
broken, then counts down `grace_seconds`. Its `timing` is not consulted at all. This
is what a defence mission wants: clearing a wave quickly buys the player a real
breather and then pulls the next assault forward, rather than leaving them idle
against a wall clock.

A wave counts as broken when every unit it spawned is dead, has routed, or when at
least 90% of it is dead. Routed survivors are deliberately ignored — a single
fleeing horseman must not be able to stall a `survive_waves` objective forever.

#### What a wave marches on

`entry_point` says where a wave arrives; it never says where it goes. The target is
the player's camp, resolved once at mission setup by
`App::Core::resolve_defense_reference` and carried on each spawned unit as the
`AssaultWaveComponent` march target. The resolution is ordered so the camp, not the
army's current sprawl, wins: the player's barracks, else the centre of the player's
other halls, else the centre of the player's troops. Walls and gates are never
anchors — a rampart's centroid sits on the camp's edge, not in it — and averaging
every owned entity lets a stray scout or a herd of civilians drag the target off the
camp entirely.

The march target is a floor, not a leash. Once moving, `AssaultBehavior` prefers a
live target — the nearest visible enemy troop, then any player building or commander
— and only falls back to the baked march point when it can see nothing at all. See
`docs/AI_ARCHITECTURE.md` for how the wave handles fortifications on the way in.

```json
"waves": [
  {
    "timing": 120.0,
    "phase": 1,
    "label": "The first crossing",
    "composition": [
      {"type": "swordsman", "count": 8},
      {"type": "archer", "count": 4}
    ],
    "entry_point": {"x": 190, "z": 190}
  },
  {
    "trigger": "after_previous_cleared",
    "grace_seconds": 35.0,
    "phase": 2,
    "archetype": "assault",
    "strength": 1.2,
    "clear_reward": {"gold": 150, "food": 100},
    "entry_points": [{"x": 190, "z": 190}, {"x": 120, "z": 210}]
  }
]
```

#### Wave archetypes

`archetype` expands a named template into the wave's composition, on top of anything
in `composition`. The built-in set lives in `game/map/wave_archetype_catalog.cpp`:

- `probe` — a light column that tests the line
- `assault` — a balanced spear/sword/archer push
- `cavalry_flank` — mounted, meant to arrive off-axis
- `skirmish_screen` — ranged and mobile
- `siege_column` — a catapult with an infantry escort
- `elite_guard` — the heaviest set, with elite swordsmen

Dropping a JSON file at `assets/data/waves/archetypes.json` overrides archetypes by
id and adds new ones. There is no such file in the repository: the built-ins are the
only source of truth unless a project deliberately adds one.

#### Wave size scaling

Final unit counts are `count` × `strength` × mission difficulty × AI `difficulty` ×
escalation, rounded, never below 1 per entry. Escalation is
`1 + wave_escalation × wave_index`, so the third wave of an AI with
`"wave_escalation": 0.12` arrives 24% heavier than its first.

A composition entry marked `"elite": true` spawns with 1.6× health, which is how a
named guard or boss unit is authored onto a final wave.

#### What the player sees

- A persistent HUD tracker: current phase, phases cleared, and a countdown to the
  next wave.
- A telegraph `warning_seconds` before the wave lands: an announcement naming the
  direction and rough size, an audio cue, and a pulsing marker on the minimap at the
  entry point.
- An announcement and a cue when a phase is broken, plus any `clear_reward`.

Wave state — the clock, which waves have spawned, and which have been broken — is
written into the save under `mission_waves` and restored with it.

#### Supported strategy values

The current AI strategy parser accepts:

- `balanced`
- `aggressive`
- `defensive`
- `expansionist`
- `economic`
- `harasser` or `harassment`
- `rusher` or `rush`

If the string is omitted, mission AIs fall back to `defensive`; an unrecognized string falls back to `balanced`.

#### Posture values

`posture` decides whether the strategy is allowed to go on the offensive at all:

- `garrison` (default for missions) — the AI gathers at and defends its bases, answers enemies that come close with a small local response, and leaves the offence to scripted `waves`
- `field` — the AI may also initiate attacks, scout, harass, capture neutral barracks and build outposts, like a skirmish opponent

Skirmish AIs always run `field`. See `docs/AI_ARCHITECTURE.md` for how the posture and the local engagement layer interact.

#### Difficulty values

The current difficulty pipeline is execution-only. It affects things like AI update cadence, production speed, and scouting reach without changing the chosen strategic identity.

- `easy`
- `hard`
- `very_hard`
- `medium` currently behaves like normal/default tuning
- omitted or any other value -> normal/default tuning

#### Personality values

`personality` currently exposes three normalized floats, usually in the `0.0` to `1.0` range:

- `aggression`
- `defense`
- `harassment`

If any value is omitted, it defaults to `0.5`.

These values are applied **after** the base strategy preset, so a `defensive` AI with high aggression still feels defensive overall, but attacks sooner and commits more readily than the default preset.

#### Team ID (Optional)

AI opponents can be assigned to teams using the `team_id` field. AI players with the same `team_id` will be allied and won't attack each other. If `team_id` is not specified, each AI player will be on their own team (enemies to all other AI players).

Example with allied AI opponents:

```json
"ai_setups": [
  {
    "id": "roman_legion_1",
    "nation": "roman_republic",
    "strategy": "balanced",
    "difficulty": "hard",
    "team_id": 1,
    ...
  },
  {
    "id": "roman_legion_2",
    "nation": "roman_republic",
    "strategy": "harasser",
    "difficulty": "hard",
    "team_id": 1,
    ...
  }
]
```

#### Authoring examples

Balanced frontline AI:

```json
{
    "id": "roman_line",
    "nation": "roman_republic",
    "faction": "roman",
    "color": "red",
    "difficulty": "hard",
    "strategy": "balanced",
    "personality": {
        "aggression": 0.6,
        "defense": 0.55,
        "harassment": 0.25
    },
    "starting_buildings": [
        {
            "type": "barracks",
            "position": { "x": 132, "z": 82 },
            "max_population": 180
        }
    ],
    "starting_units": [
        {
            "type": "builder",
            "count": 2,
            "position": { "x": 130, "z": 80 }
        },
        {
            "type": "spearman",
            "count": 8,
            "position": { "x": 134, "z": 84 }
        }
    ]
}
```

Forward pressure harasser:

```json
{
    "id": "numidian_raiders",
    "nation": "carthage",
    "faction": "carthaginian",
    "color": "yellow",
    "difficulty": "hard",
    "strategy": "harasser",
    "personality": {
        "aggression": 0.76,
        "defense": 0.3,
        "harassment": 0.85
    },
    "starting_buildings": [
        {
            "type": "barracks",
            "position": { "x": 12, "z": 78 },
            "max_population": 120
        }
    ],
    "starting_units": [
        {
            "type": "builder",
            "count": 1,
            "position": { "x": 14, "z": 80 }
        },
        {
            "type": "horse_swordsman",
            "count": 5,
            "position": { "x": 10, "z": 80 }
        }
    ]
}
```

### Victory Conditions

Supported victory condition types:

- **destroy_all_enemies**: Eliminate all enemy forces
- **survive_duration**: Survive for specified time (in seconds)
- **survive_waves**: Break the specified number of authored assault phases
- **accumulate_resources**: Harvest the specified resource totals
- **control_structures**: Control the specified structure types
- **capture_structures**: Capture the specified structure types from another nation
- **eliminate_commanders**: Kill every enemy commander

By default victory conditions are evaluated as **OR** conditions: if any configured victory
condition is satisfied, the mission ends in victory. Set `"victory_mode": "all"` on the mission
to require **every** condition instead. `victory_mode` defaults to `"any"`.

Beware of leaving multiple conditions under `"any"` — each one becomes an independent shortcut
past the others. The content validator warns about this.

```json
"victory_mode": "all",
"victory_conditions": [
  {
    "type": "capture_structures",
    "structure_types": ["barracks"],
    "min_count": 4,
    "description": "Seize every camp"
  },
  {
    "type": "survive_undead_wave",
    "zone_id": "sepulcher_vanguard",
    "wave_count": 2,
    "description": "Break both risings"
  }
]
```

#### survive_waves

Counts _assault phases_, not individual wave entries. A phase is either an explicit `phase`
number or, when none is authored, the set of waves that share a `timing` across different
`ai_setups`. A phase counts as survived once every wave in it is broken — across all AI setups,
not just one. Authoring `wave_count` higher than the number of phases makes the mission
unwinnable; the validator rejects that.

```json
{
    "type": "survive_waves",
    "wave_count": 3,
    "description": "Break all three Roman assault phases"
}
```

#### eliminate_commanders

A nation stands on its commander. When the last commander of an owner dies,
`Game::Systems::NationCollapse` hands that owner's barracks to the neutral owner, takes
down its other structures and clears its troops from the field. `eliminate_commanders` is
satisfied once no enemy commander is left alive.

The rule **arms** only after an enemy commander has been seen. At mission start the
roster has not spawned and the count is legitimately zero; without arming, the mission
would be won on its first evaluated frame. The consequence for authors is that every AI
force in a mission carrying this condition needs a commander spawn in the map — an AI
without one can never arm the rule, which makes the mission unwinnable.
`MissionAssetRulesTest.DecapitationObjectivesHaveCommandersToKill` fails the build if that
happens. Iron Sepulcher units are excluded from the count: the dead have no commander, so
a Sepulcher zone can never block the objective.

```json
{
    "type": "eliminate_commanders",
    "description": "Kill every enemy commander"
}
```

Pair it with `"victory_mode": "all"`. Under the default `"any"` it becomes an independent
shortcut past whatever else the mission asks for.

#### accumulate_resources

Reads **lifetime harvested** totals, not the current balance, so spending on units and
buildings never rolls progress backwards. Only builder harvesting counts — marketplace trades
and starting resources do not. Yields are 40 wood per tree, 35 stone per boulder, 30 iron per
ore deposit.

A yield is credited when the worker **unloads it at a barracks stockpile**, not when the tree
falls, so a gather objective needs a reachable friendly barracks as well as reachable nodes;
see [RESOURCE_STOCKPILE.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/RESOURCE_STOCKPILE.md).

Harvestable props are scattered procedurally by biome, so a map with low `plant_density` needs
explicit `pine_tree` / `boulder` / `iron_ore` entries in its `world_props` before it can carry
a gather objective. Decorative settlement props (`abandoned_home`, `statue`) never yield
resources but do block the cell they stand on; see
[SETTLEMENT_ASSETS.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/SETTLEMENT_ASSETS.md). Author comfortably more than the target: contested or unreachable nodes
must not be able to soft-lock the mission.

```json
{
    "type": "accumulate_resources",
    "resources": { "wood": 600, "stone": 350, "iron": 300 },
    "description": "Provision the column for the descent"
}
```

For the runtime architecture, default-rule behavior, and extension workflow, see
[VICTORY_SYSTEM.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/VICTORY_SYSTEM.md).

Example:

```json
"victory_conditions": [
  {
    "type": "survive_duration",
    "duration": 600.0,
    "description": "Survive for 10 minutes"
  }
]
```

### Defeat Conditions

Supported defeat condition types:

- **lose_structure**: Specific building type is destroyed
- **lose_all_units**: All units are eliminated
- **lose_commander**: Your commander dies
- **only_commander_remaining**: Your commander is the last surviving force after all troops and barracks are gone
- **time_limit**: The mission deadline (in seconds) expires before victory

`time_limit` is the only way to make an objective time-bound; deadlines mentioned only in
`intro_text` or `summary` have no mechanical effect.

```json
{
    "type": "time_limit",
    "duration": 420.0,
    "description": "Seal the basin before the mist lifts"
}
```

Defeat conditions are evaluated as **OR** conditions: if any configured defeat condition
is satisfied, the mission ends in defeat.

If a mission omits `defeat_conditions`, the engine adds the default commander rules:

- `lose_commander`
- `only_commander_remaining`

Example:

```json
"defeat_conditions": [
  {
    "type": "lose_structure",
    "structure_type": "barracks",
    "description": "Do not lose your barracks"
  },
  {
    "type": "lose_commander",
    "description": "Protect Hannibal"
  }
]
```

### Stages

Victory conditions say when a mission is won. They do not say what to do first.
`stages` is the ordered answer to "what now?" -- the HUD objective line, the
briefing checklist, the minimap pin and the ring drawn on the ground all read
the active stage from the same tracker, so they cannot drift apart.

```json
"stages": [
  {
    "id": "reach_crossing",
    "type": "reach_position",
    "title": "Reach the Rhone crossing",
    "description": "Bring the column to the pontoon bridge on the near bank.",
    "hint": "The nearest crossing is due east of camp.",
    "target": { "x": 240.9, "z": 189.5 },
    "target_radius": 20.0
  },
  {
    "id": "take_hill_fort",
    "type": "capture_structures",
    "structure_types": ["barracks"],
    "required_count": 1,
    "title": "Seize the Roman hill fort",
    "target": { "x": 376, "z": 44 },
    "target_radius": 30.0
  }
]
```

The active stage is the first one that is not complete. A stage that completes
stays complete, so losing a camp again never walks the player back up the list.

| `type`                 | Complete when                                                              | Extra fields                                                              |
| ---------------------- | -------------------------------------------------------------------------- | ------------------------------------------------------------------------- |
| `reach_position`       | any of your non-commander units stands within `target_radius` of `target`  | `target`, `target_radius` (default 6)                                     |
| `capture_structures`   | you hold `required_count` structures that started as an enemy nation's     | `structure_types`, `required_count`                                       |
| `control_structures`   | you own `required_count` structures of those types, however you got them   | `structure_types`, `required_count`                                       |
| `destroy_structures`   | `required_count` of the enemy structures present at mission start are gone | `structure_types`, `required_count`                                       |
| `eliminate_commanders` | every enemy commander alive at mission start is dead                       | none; `required_count` is taken from the opposition unless you author one |
| `accumulate_resources` | every resource kind named has been harvested in full                       | `resources`                                                               |
| `survive_time`         | `duration` seconds of mission clock have passed                            | `duration`                                                                |
| `survive_waves`        | `wave_count` assault phases have been cleared                              | `wave_count`                                                              |

`target` is in the same coordinate space as every other authored mission
position: grid tiles on a grid map, world units otherwise. Counting stages
share one tally, so a run of `capture_structures` stages with `required_count`
1, 2, 3 reads as a sequence of camps and still agrees with a
`capture_structures` victory condition of `min_count: 3`.

`title`, `description` and `hint` are player-visible and go through the
translation catalogues -- `scripts/extract-asset-strings.py` lifts them into the
`Missions` context. Keep `title` short: it has to fit the HUD objective line.

Stages are optional. A mission without them falls back to showing its first
victory condition on the HUD, exactly as before.

### Events

Timed or state-based triggers for dynamic gameplay:

```json
"events": [
  {
    "trigger": {
      "type": "timer",
      "time": 300.0
    },
    "actions": [
      {
        "type": "show_message",
        "text": "Reinforcements approaching!"
      }
    ]
  }
]
```

### Commander messages

An enemy commander can speak to the player mid-battle: a portrait panel below the
minimap, a subtitle, and a herald cue. Missions author the lines and say what
they answer to under a top-level `commander_messages` array.

```json
"commander_messages": [
  {
    "id": "scipio_rhone_open",
    "speaker": "roman_veteran_consul",
    "pose": "dismissive",
    "trigger": { "type": "mission_start", "delay": 2.5 },
    "text": "So. The Barcid crawls down to the Rhone...",
    "voice_cue": "alert.commander_message",
    "duration": 13.0,
    "priority": 100
  }
]
```

| Field       | Meaning                                                                                                                                                            |
| ----------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| `id`        | Stable name. Saves record which lines are spent by id.                                                                                                             |
| `speaker`   | A commander troop type (`game/units/commander_catalog.cpp`). The display name, battlefield role and nation come from the catalogue, so a line never restates them. |
| `text`      | The line. Extracted for translation, so write it in English and never bake it into art.                                                                            |
| `voice_cue` | Cue id played when the panel opens. Optional.                                                                                                                      |
| `duration`  | Seconds the panel holds. Defaults to nine.                                                                                                                         |
| `priority`  | Higher wins when two lines are ready at once; the loser waits its turn.                                                                                            |
| `once`      | Defaults to `true`. Set `false` for a line that may repeat.                                                                                                        |

`trigger.type` is one of:

| Trigger              | Fires when                                                                                    |
| -------------------- | --------------------------------------------------------------------------------------------- |
| `mission_start`      | The mission's forces are placed and the battle begins. `match_start` is an alias.             |
| `mission_victory`    | The victory service resolves the mission as won.                                              |
| `mission_defeat`     | The victory service resolves the mission as lost.                                             |
| `structure_captured` | A barrack changes hands (`BarrackCapturedEvent`). Subject: previous owner; actor: new owner.  |
| `commander_defeated` | A commander dies (`UnitDiedEvent` for a catalogued commander). Subject: owner; actor: killer. |
| `attack_launched`    | An AI commits an attack wave. Subject: the owner it marches on; actor: the AI.                |
| `under_attack`       | An owner's buildings take sustained damage. Subject: building owner; actor: attacker.         |
| `first_contact`      | Two owners trade blows for the first time. Subject: the AI; actor: the other side.            |
| `heavy_losses`       | An owner loses a large share of its troops in a short window. Actor: who did most of it.      |
| `near_defeat`        | An owner is reduced to a handful of units or a last structure.                                |
| `owner_eliminated`   | An owner has nothing left on the field. Actor: who took the last of it.                       |
| `wave_incoming`      | A scripted mission wave is warned. Subject: the wave's owner. `final_wave` filters.           |
| `wave_cleared`       | A scripted wave is broken. Subject: the wave's owner.                                         |

The first five come straight off the event bus. The rest are published by
`Game::Mission::CommanderVoiceObserver` (`game/mission/commander_voice_observer.h`),
which watches building hits, combat hits, deaths and each AI's attack plan on
the main thread and applies the hysteresis that keeps a siege from reading as
sixty separate attacks; the wave beats come from `MissionWaveDirector::Effects`.

Every shipped mission authors at least these three -- `mission_start`,
`mission_victory` and `mission_defeat` -- and
`MissionAssetRulesTest.EveryMissionOpensAndClosesInACommandersVoice` fails the
build if a new one does not.

The two closing lines **hold the outcome banner**. `IronOutcomeOverlay` takes a
`held` flag, `HUDVictory` binds it to `game.commander_message.holds_outcome`, and
the banner -- with the Battle Report button on it, and the report behind that --
stays off screen until the line has finished or the player clicks it away. So the
commander who just lost gets the last word before the summary appears, and a
player in a hurry is one click from the report either way.

Every trigger takes `delay` (seconds of battle time before the line shows, so it
never runs down behind a loading screen or a pause). The two event triggers also
take filters, and a line fires only when all of them match:

| Filter           | Applies to           | Meaning                                                                                                        |
| ---------------- | -------------------- | -------------------------------------------------------------------------------------------------------------- |
| `owner_id`       | both                 | Who owned the camp, or the fallen commander. `"player"` means the local player.                                |
| `by_owner_id`    | both                 | Who took it, or who landed the kill. `"player"` likewise.                                                      |
| `structure_type` | `structure_captured` | Restricts to one building type.                                                                                |
| `unit_type`      | `commander_defeated` | Restricts to one commander troop type.                                                                         |
| `nation`         | `commander_defeated` | The fallen commander's nation, read from the catalogue.                                                        |
| `at` + `radius`  | `structure_captured` | Mission-space position of the camp this line is about. Without it, any camp matching the owner filters counts. |
| `subject`        | both                 | Alias of `owner_id` that also accepts a role token: `self`, `ally_of_speaker`, `enemy_of_speaker`, `any`.      |
| `actor`          | both                 | Alias of `by_owner_id` with the same role tokens.                                                              |
| `cooldown`       | both                 | Seconds before the same line -- or any line of the same speaker on the same trigger -- may fire again.         |
| `final_wave`     | `wave_incoming`      | Restricts to the last authored wave, or to every wave before it.                                               |

So "the village that belongs to owner 3 is taken by the player" is:

```json
"trigger": { "type": "structure_captured", "owner_id": 3, "by_owner_id": "player" }
```

The portrait is not a painting. `ui/commander_portrait_view.h` is a
`QQuickFramebufferObject` that draws the real commander -- the same model the
battle draws -- with the game's own renderer, in a one-entity world of its own.
The body language comes from `taunt_dismissive` and `taunt_cynical`, two moves
authored in `animation/showcase_pose_manifest.cpp` and baked into every humanoid
BPAT profile by `bpat_baker` at build time; the runtime only picks a clip and a
phase, which is why a talking head costs less than one soldier in a battle. The
scene is built on the first frame a line appears and torn down when it closes,
so an idle portrait holds no world, no renderer and no GPU resources.

Adding a taunt costs 2,104 bytes per frame in each of the six humanoid bake
profiles -- the two shipped ones come to about 2 MB across the set. That is the
price of a commander of any profile being able to play them; a portrait-only
species would be cheaper but could not pose a spear or bow commander correctly.

`Game::Mission::CommanderMessageDirector` (`game/mission/commander_message_director.h`)
holds the rules and subscribes to the event bus. It owns no world and no renderer:
the client hands it a position lookup for the `at` filter and calls `update()` on
the simulation tick, which is why the dwell timer stops with a paused battle.
`GameEngine` republishes the active line to `game.commander_message`, which
`ui/qml/CommanderMessagePanel.qml` reads.

The panel sits **below** the minimap rather than over it, because the lines that
fire while a camp is changing hands are exactly the ones a player needs the map
for. End-of-mission lines draw above the outcome overlay, so the loser gets the
last word over the victory banner.

The portrait is framed as a **bust**, and the framing follows the head rather than
sitting at a fixed height: commander archetypes differ by up to eight percent in
proportion scaling, which at this distance is the difference between a portrait
and a cropped chin. Each frame the portrait asks the renderer where the head it
just drew ended up, then eases the camera onto it.

### The face belongs to the portrait

The humanoid rig has a jaw and a nose but no eyes or mouth in its baked body
mesh. Those details are added inside `CommanderPortraitView` instead of in QML.
For the duration of `render_world`, the portrait installs a
`Render::Creature::Pipeline::BoneProbe`
(`render/creature/pipeline/creature_bone_probe.h`). The creature pipeline fills
it with the head bone's world transform from the exact baked frame it submits.
Before `end_frame`, the portrait uses that transform to submit the eye, brow and
mouth meshes into the same renderer queue as the commander.

This is deliberately a 3D attachment, not a screen-space overlay. The face is
depth-tested, lit and graded with the model; a turned head naturally carries the
features around its surface. More importantly, there is no render-thread to GUI-
thread landmark handoff. The old canvas face received its projected anchor
through a queued property update and could therefore trail the animated head by
a frame or more during a taunt. The current face and head always use one bone
matrix from one frame.

The same probe still drives the bust framing. Each completed render updates the
next camera focus from the head position, preserving the smoothing that absorbs
small differences between commander archetypes without re-evaluating the pose on
the UI side.

The panel exposes whether the typewriter is still revealing the line through the
portrait's `talking` property. The render pass turns that into a small procedural
mouth aperture and closes it as soon as the line completes. A deterministic
blink gives the otherwise static features a little life. Reduced motion sends
`talking: false`, so speech animation stops while the 3D attachment remains
locked to the head.

`--screenshot-view commander` opens the panel on its own against a stand-in view
model, which is the only way to look at this without playing a mission to a
trigger. Note that the screenshot harness does not advance QML animations: it is
the surface to review framing and placement on, not motion. The typewriter-to-
portrait handoff is covered by `tests/ui/qml/tst_commander_message.qml`.

### Commander voices

Missions write lines for one battle. Skirmish has no mission file, yet every AI
owner already has a commander (the `commanderTroop` each MapSelect slot carries),
so each of the six commanders also owns a **voice bank**: what they say to the
player as an enemy and as an ally, on the generic trigger vocabulary above.

Banks live in `assets/data/commanders/voices/<commander_id>.json`, one file per
commander, and are registered in `assets.qrc`.

```json
{
    "commander": "roman_veteran_consul",
    "chatter_per_match": 10,
    "lines": [
        {
            "id": "scipio.enemy.attack_launched",
            "relationship": "enemy",
            "pose": "dismissive",
            "priority": 40,
            "once": false,
            "trigger": {
                "type": "attack_launched",
                "actor": "self",
                "subject": "player",
                "cooldown": 90,
                "delay": 1.5
            },
            "variants": [
                "The legion is moving. Do not trouble to form a line...",
                "..."
            ]
        }
    ]
}
```

| Field               | Meaning                                                                                                                                |
| ------------------- | -------------------------------------------------------------------------------------------------------------------------------------- |
| `relationship`      | `enemy` or `ally`: how the speaker stands to the local player. A line is only instantiated for owners in that relationship.            |
| `variants`          | Alternative texts for the same beat. They play in authored order, first unspent; a repeating line cycles. `text` is shorthand for one. |
| `chatter_per_match` | How many chatter lines this speaker may show in one match. Openings, outcomes, commander deaths and eliminations do not count.         |

Role tokens make one bank serve every seat at the table: `self` is the speaker's
own owner, `player` the local player, `ally_of_speaker` / `enemy_of_speaker` are
read from the owner registry's teams. "You took my camp" is therefore
`structure_captured` with `subject: self, actor: player`; the same line as
"my ally took a camp" is `actor: self` with `relationship: ally`.

**Binding.** At match start `GameEngine::configure_commander_messages()` reads the
roster from the world (`game/mission/commander_speaker_roster.h`): every owner
other than the player and the neutral owner who fields a live commander, with
its relationship. Each speaker's bank is expanded into rules whose ids are
`<owner_id>:<line_id>.<variant>` -- so two owners sharing a commander stay
distinct and saves record spent lines by the same mechanism as mission lines.
An AI with no commander on the field is given its nation's default so it still
has a face.

**Missions override banks.** A mission's own `commander_messages` keep working
as before; when an event matches a mission-authored line for a speaker, that
speaker's generic lines for the same trigger are skipped for that event, so a
mission that writes what Scipio says about the river town never hears the stock
capture line as well. A mission can also silence beats it does not want:

```json
"commander_voices": { "generic": true, "muted_triggers": ["first_contact"], "muted_lines": ["scipio.enemy.wave_incoming"] }
```

`"generic": false` turns the banks off for that mission; the tutorial does this.

**Cadence.** The director (`game/mission/commander_message_director.h`) keeps
the banter from becoming noise:

- Chatter -- anything but openings, outcomes, commander deaths and eliminations
  at priority below 80 -- waits at least 12 seconds after the previous line ends.
- A line's `cooldown` also covers every line of the same speaker on the same
  trigger, so variants cannot chain.
- Chatter that has waited 15 seconds unshown is dropped rather than replayed
  after the moment has passed.
- Each speaker has a `chatter_per_match` budget.
- Outcome lines pre-empt whatever is showing and flush pending chatter; nothing
  else pre-empts. After the outcome, only outcome lines play.
- One fact draws one answer: when several speakers could answer the same event,
  the highest priority speaks, ties going to the lower owner id. Openings are
  the exception; every speaker gets one.

The panel marks ally lines with an **ALLY** tag and tints its rule in the
success colour, since the portrait and nation mark alone would not tell a
player which side the speaker is on in a mirror match.

Bank text is extracted for translation under the `CommanderVoices` context;
`tools/content_validator` checks every bank (catalogued commander, unique ids,
baked poses only, cooldowns on repeating lines, chatter short enough to read),
and `tests/map/commander_voice_bank_test.cpp` fails the build if a commander
has no bank or a bank lacks one of the required beats.

#### Writing a commander's lines

- **Voice.** Scipio is clipped and aristocratic and counts things. Fabius is
  patient, agrarian, never boasts; time is his. Marcellus is a blunt soldier:
  tempo and blood, short sentences. Hannibal is sardonic and theatrical and
  speaks of Rome as a stubborn animal. Hasdrubal is the loyal younger brother,
  hunters' imagery, worried about supply. Hanno is the mercenary-master: money
  and ledgers, and no love for the Barcids even as their ally.
- **Length.** Chatter runs 90-180 characters; the panel's cap is 20 seconds, so
  260 is the hard limit the validator enforces. Openings and outcomes may run to
  400 as today.
- **Never restate name, role or nation.** The panel shows them.
- **No numbers the simulation can contradict**, and no assumption about the
  player's nation: a Roman commander may be fighting Rome in a mirror match.
- **Allies never give orders.** They ask, warn and report.
- **Poses.** Only `dismissive` and `cynical` are baked. Enemies use
  `dismissive` for the player's setbacks and their own attacks, `cynical` for
  their own setbacks; allies use `cynical` as their neutral register.

### The tutorial mission

`assets/missions/tutorial.json` is the guided first battle behind the main menu's
**Tutorial** entry. It is an ordinary mission file with one extra flag:

```json
"tutorial": true
```

When a mission carrying that flag finishes loading, the engine attaches
`Game::Mission::TutorialDirector` (`game/mission/tutorial_director.h`), which walks the
player through fifteen fixed steps — selection, movement, attacking a held scouting
party, gathering each material, building a Home, recruiting, assembling an army,
breaking a raid, stances, the commander's aura, camera, speed, objectives and a
final assault. The director is exposed to QML as `game.tutorial`; the HUD card
(`ui/qml/TutorialOverlay.qml`) only reads it, and the Field Manual
(`ui/qml/HelpPanel.qml`) lists the steps and their completion state.

Two things about the mission are load-bearing for the director:

- **The mission clock is held** until the player reaches the _Defend the camp_
  step, so a `waves` entry with a short `timing` still lands only once the player
  has an army to meet it. `GameEngine::update_mission_waves` asks
  `TutorialDirector::holds_mission_clock()` before advancing.
- **The scouting party is authored on the map with `"behavior": "hold"`** for the
  AI owner, so it is never handed to the AI and stays where the _Attack the Roman
  scouts_ step says it is. `MissionAssetRulesTest.TutorialMissionIsFullyAuthored`
  and `TutorialMissionTest` pin both, plus the commanders, props and barracks the
  steps rely on.

The map it uses, `assets/maps/map_tutorial.json`, declares
`"skirmish_hidden": true`; `MapCatalog` skips such maps so a scripted stage never
appears as a free-play choice.

#### Pointing at what a step is talking about

Prose alone leaves a first-time player hunting a dense HUD for the button a step
names, so every step also publishes a _focus_: what to ring, and where.

- `focus_actions` is a list of command-grid ids (`collect`, `build`, `aura`, …).
  `HUDBottom` passes `spotlit` to the matching `IronCommandButton`, which draws a
  `Design.IronSpotlight` ring around it.
- `focus_region` names a HUD zone — `production`, `speed`, `camera`, `objective`,
  `waves` — and the panel or button group in `HUDTop`/`HUDBottom` rings itself.
- `focus_target` names something on the field (`own_troops`, `builders`,
  `enemy_scouts`, `timber`, `stone_and_iron`, `barracks`, `commander`,
  `wave_entry`, `enemy_camp`). The director only names the target; the app layer
  resolves it to world positions in `App::Mission::resolve_tutorial_focus_points`
  (units come from the world, harvest nodes from `TerrainService::world_props()`,
  the wave entry from the live wave alerts) and pushes them back through
  `set_focus_points`, adding minimap coordinates. `TutorialFocusOverlay.qml`
  projects each point through the live camera and rings it, `HUDTop` pins them on
  the minimap, and the card's **Show me** button pans the camera to their centre
  via `CameraViewModel::look_at_world`.

The focus is recomputed from the same observation as the hint, so it follows what
the step still needs: _Fell timber_ rings the builders while none is selected, and
switches to the Collect button and the nearest pines once one is. A completed step
publishes an empty focus, and changing target drops the stale world points so
rings never outlive the step that placed them.

## Standalone Missions

A mission file that no campaign names is a **standalone mission**: one small map,
one authored objective, and nothing to unlock. They are the third mode beside
Skirmish and Campaign, listed on their own screen (`ui/qml/MissionsScreen.qml`)
and started with `MatchSetupViewModel::start_mission_file`.

The split is decided by `Game::Map::MissionCatalog`, which reads
`assets/missions/` and subtracts two sets:

- every `mission_id` any campaign in `assets/campaigns/` claims, and
- the tutorial, which has its own menu entry.

What is left is `MissionCatalog::standalone_missions()` -- the Missions roster.
The catalogue reads no save database, so the record of what has been beaten is
merged in by `MatchSetupViewModel::load_missions()` from
`SaveLoadService::get_mission_progress`.

### Authoring one

Nothing about the mission format changes. A standalone mission is an ordinary
mission file that no campaign lists, with two conventions:

```json
{
    "id": "the_timber_levy",
    "menu_order": 10,
    "title": "The Timber Levy",
    "summary": "Fill the levy out of the Pinewater cut...",
    "map_path": ":/assets/maps/map_pinewater_cut.json"
}
```

- `menu_order` orders the roster; missions without it sort last, then by title.
- `summary` and every condition `description` are what the briefing shows, so
  they have to say what the player is being asked to do, not just set a mood.

The objective vocabulary is the same one campaign missions use --
`accumulate_resources`, `survive_waves`, `capture_structures`,
`control_structures`, `clear_undead_zone`, `purify_shrine`,
`survive_undead_wave`, `survive_duration`, `destroy_all_enemies`,
`eliminate_commanders`.

### The map is not a skirmish field

`MapCatalog::available_maps()` subtracts every map any mission names, campaign
and standalone alike. A map built around one authored objective has nothing to
offer a free-play match: adding a mission that points at a map therefore removes
that map from the skirmish roster, and there is nothing else to update.

Two gates hold the mode together, both in `campaign_tests`:

- `MissionCatalogTest` -- the roster excludes campaign missions and the tutorial,
  carries what the menu needs, and no mission map is offered as a skirmish field.
- `MissionMapReachabilityTest` -- every place a mission's objective sends you
  (harvestables, forests, undead zones, its own wave entry points, its own
  starting-unit positions) is walkable from the camp, and the map is small enough
  to read at once.

### Mission mode versus campaign mode

`MissionContext::mode` is `"mission"` for a standalone mission and `"campaign"`
for a campaign link. Everything that reads a mission definition -- setup, victory
rules, the objectives panel, the battle summary -- asks `has_mission()`, which is
true for both. Only campaign advancement asks `is_campaign()`: a standalone
mission records its own result through `save_mission_result` and unlocks nothing.

## Campaign Configuration

For the shipped Second Punic War campaign specifically -- opponents, wave
pressure, timers, starting bases and what wins each mission -- see
[CAMPAIGN_MISSIONS.md](CAMPAIGN_MISSIONS.md). This section describes the format;
that one describes the content.

### File Structure

```json
{
    "id": "second_punic_war",
    "title": "Second Punic War",
    "description": "Campaign across the Mediterranean",
    "missions": [
        {
            "mission_id": "forest_ambush",
            "order_index": 0,
            "intro_text": "Your first task...",
            "outro_text": "Well done!",
            "difficulty_modifier": 1.0
        }
    ]
}
```

### Mission Ordering

- `order_index` must be contiguous starting from 0 or 1
- Missions are presented in order
- Validator ensures proper sequencing

## Content Validation

### Validator CLI

Run content validation:

```bash
# Via CMake
cmake --build build --target validate-content

# Via Makefile
make validate-content

# Direct execution
./build/bin/content_validator assets
```

### Validation Checks

The validator performs:

1. **JSON Schema Validation**
    - Proper structure and required fields
    - Correct data types

2. **Asset Reference Validation**
    - Referenced maps exist
    - Campaign missions reference valid mission files

3. **Campaign Validation**
    - Mission order indices are contiguous
    - Starts at 0 or 1

4. **Cross-Reference Checks**
    - Units, buildings, and entity types are valid
    - No circular dependencies

### Build Integration

Add to your build process:

```cmake
# Make validation part of the build
add_dependencies(standard_of_iron validate-content)
```

When uncommented, invalid content will fail the build.

## Usage in Code

### Loading Missions

```cpp
#include "game/map/mission_loader.h"

Game::Mission::MissionDefinition mission;
QString error;

if (!Game::Mission::MissionLoader::loadFromJsonFile(
    ":/assets/missions/defend_outpost.json",
    mission,
    &error)) {
  qWarning() << "Failed to load mission:" << error;
  return;
}

// Use mission data
qInfo() << "Loaded mission:" << mission.title;
```

### Loading Campaigns

```cpp
#include "game/map/campaign_loader.h"

Game::Campaign::CampaignDefinition campaign;
QString error;

if (!Game::Campaign::CampaignLoader::loadFromJsonFile(
    ":/assets/campaigns/second_punic_war.json",
    campaign,
    &error)) {
  qWarning() << "Failed to load campaign:" << error;
  return;
}

// Iterate missions
for (const auto& mission : campaign.missions) {
  qInfo() << "Mission" << mission.order_index << ":" << mission.mission_id;
}
```

### Starting a Mission

```cpp
// In GameEngine
void GameEngine::start_campaign_mission(const QString &mission_path) {
  // mission_path format: "campaign_id/mission_id"
  // Example: "second_punic_war/defend_outpost"

  // Mission loader parses JSON and configures:
  // - Player and AI setups
  // - Victory/defeat conditions
  // - Map reference
}
```

## UI Integration

### Mission Selection

The Missions screen lists standalone missions with the field they are fought on,
the orders that win, what is worth doing anyway, the force the player is handed
and what loses. Selecting one and taking the field calls
`game.setup.start_mission_file(file_path)` with the path the catalogue reported.

`--screenshot-view missions` captures the screen offscreen for review.

### Campaign Selection

The campaign screen displays available campaigns and their missions:

1. User selects a campaign
2. Mission list appears with:
    - Order number
    - Mission name
    - Intro text preview
3. User selects specific mission
4. Mission loads with proper configuration

### Victory Conditions

Mission-specific victory conditions override map defaults:

- VictoryService configured from mission JSON
- Win/loss checks match mission requirements
- Campaign progress tracked on completion
- When defeat conditions are omitted, commander-default defeat rules are injected automatically

## Best Practices

### Mission Design

1. **Keep Maps Reusable**: Don't hardcode gameplay in map files
2. **Clear Objectives**: Provide explicit victory/defeat conditions
3. **Balanced Difficulty**: Test with various player skill levels
4. **Progressive Complexity**: Early missions should be simpler

### Campaign Structure

1. **Logical Progression**: Order missions by difficulty
2. **Narrative Flow**: Use intro/outro text for story
3. **Varied Gameplay**: Mix victory condition types
4. **Testing**: Validate entire campaign sequences

### Asset Organization

```
assets/missions/
  ├── 01_tutorial/
  │   ├── basic_combat.json
  │   └── defend_base.json
  ├── 02_main_campaign/
  │   ├── forest_battle.json
  │   └── siege_warfare.json
  └── 03_advanced/
      └── survival_challenge.json
```

## Troubleshooting

### Common Validation Errors

**"Mission not found"**

- Check mission_id matches filename
- Ensure mission file is in assets/missions/

**"Order index not contiguous"**

- Campaign missions must have sequential indices
- No gaps allowed (0, 1, 2, ... or 1, 2, 3, ...)

**"Referenced map not found"**

- Verify map_path is correct
- Check map file exists in assets/maps/

### Debugging

Enable verbose logging:

```bash
QT_LOGGING_RULES="*.debug=true" ./standard_of_iron
```

Check mission loading:

```cpp
qDebug() << "Loading mission:" << mission.id;
qDebug() << "Victory conditions:" << mission.victory_conditions.size();
```

## Future Enhancements

Planned features:

- [ ] Dynamic mission generation
- [ ] User-created campaigns
- [ ] Mission editor UI
- [ ] Achievement tracking
- [ ] Difficulty scaling system
- [ ] Replay system integration

## API Reference

See also:

- `game/map/mission_definition.h` - Mission data structures
- `game/map/campaign_definition.h` - Campaign data structures
- `game/map/mission_loader.h` - Mission loading API
- `game/map/campaign_loader.h` - Campaign loading API
- `tools/content_validator/` - Validation implementation
