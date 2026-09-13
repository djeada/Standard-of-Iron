# Mission Framework

Missions add gameplay rules to authored maps. A map owns terrain, structures, spawns, camera/environment data, and other world geometry; a mission selects a map and adds player/AI setup, objectives, scripted waves, stages, events, commander dialogue, and campaign-facing metadata.

Campaigns order mission IDs. Missions that are not claimed by a campaign are available as standalone missions.

```text
assets/maps/*.json       battlefield/world authoring
        │
        ▼
assets/missions/*.json   mission rules referencing a map
        │
        ▼
assets/campaigns/*.json  ordered campaign mission lists
```

The C++ mission schema is defined in `game/map/mission_definition.h` and parsed by `MissionLoader`.

## Mission root

`MissionDefinition` contains these top-level concepts:

- `id`, `title`, `summary`;
- `map_path`;
- optional `teaching_goal`, `narrative_intent`, `historical_context`, and `terrain_type`;
- `player_setup`;
- `ai_setups[]`;
- `victory_mode`;
- `victory_conditions[]`;
- `defeat_conditions[]`;
- `optional_objectives[]`;
- `stages[]`;
- `events[]`;
- `commander_messages[]` and `commander_voices`;
- `include_ambient_undead`; and
- `tutorial`.

A minimal shape is:

```json
{
  "id": "defend_outpost",
  "title": "Defend the Outpost",
  "summary": "Hold the position against repeated attacks.",
  "map_path": ":/assets/maps/map_forest.json",
  "player_setup": {},
  "ai_setups": [],
  "victory_conditions": [],
  "defeat_conditions": []
}
```

## Player setup

`player_setup` defines the human force's mission-level configuration:

- `nation`;
- `faction`;
- `color`;
- `starting_units[]`;
- `starting_buildings[]`; and
- `starting_resources`.

A unit setup contains a troop type, count, position, behavior, guard radius, and optional patrol waypoints. Supported behavior values map to `Strategic`, `Guard`, `Hold`, and `Patrol`.

A building setup contains its type, position, and `max_population` value used by the production/reserve system.

## Commanders are map spawns

Mission files do not own commander placement. Commanders are authored in the referenced map's `spawns[]` as ordinary troop spawns with a commander troop type, `player_id`, nation, and position.

Each mission force therefore receives its commander from map data rather than from a second mission-level commander field.

The shipped campaign is checked by commander-setup tests that verify force coverage, nation matching, and the expected commander distinctness constraints.

## AI setups

Each entry in `ai_setups[]` defines one AI force.

| Field | Purpose |
| --- | --- |
| `id` | mission-local force identifier |
| `nation` | roster/nation selection |
| `faction` | mission/UI faction metadata |
| `color` | player colour |
| `difficulty` | execution/wave-strength tuning |
| `team_id` | alliances between mission AIs |
| `strategy` | strategic preset |
| `posture` | garrison or field behavior |
| `personality` | aggression, defense, harassment modifiers |
| `starting_units` | mission-level starting troops |
| `starting_buildings` | mission-level starting structures |
| `wave_escalation` | per-wave strength growth |
| `waves` | scripted reinforcement waves |

Personality values default to `0.5` when omitted.

### Strategy and posture

The AI strategy parser accepts:

- `balanced`;
- `aggressive`;
- `defensive`;
- `expansionist`;
- `economic`;
- `harasser` / `harassment`; and
- `rusher` / `rush`.

Mission strategy omission falls back to the mission/default strategy path; unknown values are normalized by the AI parser rather than creating a new strategy implicitly.

`posture` controls whether the strategic AI is allowed to leave its local defensive role:

- `garrison` keeps the force centered on its holdings and uses scripted waves for authored offensives;
- `field` allows the ordinary expansion/attack behavior used by field opponents.

## Waves

A `Wave` contains:

- `timing`;
- `composition[]`;
- `entry_point` or `entry_points[]`;
- `trigger`;
- `grace_seconds`;
- `warning_seconds`;
- optional `phase`;
- optional `archetype`;
- `strength`;
- `label`; and
- `clear_reward`.

### Trigger modes

Two trigger modes exist in `mission_definition.h`:

- `Time` — spawn according to `timing`;
- `AfterPreviousCleared` — wait for the previous wave from that AI to clear, then use the grace period.

The default grace is 25 seconds and the default warning is 15 seconds.

### Entry points

`entry_points[]` takes precedence when present. Otherwise `resolved_entry_points()` returns the single `entry_point`.

The wave entry position is the spawn location, not the final tactical target. Spawned assault-wave units receive their mission assault target through the runtime wave/AI path.

### Built-in wave archetypes

`game/map/wave_archetype_catalog.cpp` defines these built-ins:

| ID | Composition role |
| --- | --- |
| `probe` | light swordsman/archer column |
| `assault` | mixed spear/sword/archer attack |
| `cavalry_flank` | mounted swordsmen and horse archers |
| `skirmish_screen` | archers, horse archers, and spears |
| `siege_column` | catapult with infantry escort |
| `elite_guard` | heavy mixed force with elite swordsmen |

If `assets/data/waves/archetypes.json` exists, `WaveArchetypeCatalog` loads it as an overlay: matching IDs replace built-ins and new IDs are added.

Wave composition scaling clamps the multiplier to `0.1..8.0`, rounds counts, and never produces fewer than one unit for a composition entry.

## Victory conditions

`Game::Mission::build_victory_rules()` translates mission conditions into the runtime `VictoryRuleSet`.

Supported mission victory condition types are:

| Type | Required data |
| --- | --- |
| `destroy_all_enemies` | none; tracks enemy barracks for elimination |
| `survive_duration` | `duration` |
| `control_structures` | structure type(s), optional `min_count` |
| `capture_structures` | structure type(s), optional `min_count` |
| `clear_undead_zone` | `zone_id` |
| `purify_shrine` | `zone_id` |
| `survive_undead_wave` | `zone_id`, optional `wave_count` |
| `survive_waves` | optional `wave_count` |
| `accumulate_resources` | positive `resources` map |
| `eliminate_commanders` | none |

`victory_mode` accepts `any` or `all`. `any` is the default. `all` sets `require_all_victory_rules` in the runtime rule set.

If no supported victory rule is produced, the translator warns and falls back to barracks elimination.

### Structure names

Mission rule translation normalizes structure names to lowercase and maps the legacy name `village` to `barracks`. When a structure condition does not provide a usable type, the current fallback is `barracks`.

## Defeat conditions

Supported mission defeat condition types are:

| Type | Meaning |
| --- | --- |
| `lose_all_units` | no local units remain |
| `lose_commander` | commander is lost |
| `only_commander_remaining` | commander is the only remaining force after the rule arms |
| `time_limit` | defeat after `duration` |
| `lose_structure` | all tracked structure types are lost |

If a mission produces no explicit defeat rules, the runtime adds the commander-centric default pair:

```text
lose_commander
only_commander_remaining (tracking barracks)
```

See [VICTORY_SYSTEM.md](VICTORY_SYSTEM.md) for evaluation order, save state, timer behavior, and the startup latch on `only_commander_remaining`.

## Optional objectives and stages

`optional_objectives[]` use the same `Condition` structure as mission conditions and feed the mission/objective UI without replacing the primary victory rule set.

`stages[]` use `MissionStage`, which can describe:

- structure-count goals;
- durations;
- wave counts;
- resource requirements;
- a target position/radius; and
- a route.

Each stage also carries an ID, title, description, hint, and type.

## Mission events

`events[]` are `GameEvent` entries composed of an `EventTrigger` and one or more `EventAction` records.

The exact accepted trigger/action strings are defined by the mission loader/runtime handlers. Content should use values already recognized by those handlers; adding a JSON string alone does not create a new runtime event type.

## Commander messages

`commander_messages[]` use `CommanderMessage` records. A message can specify:

- speaker;
- pose;
- text and voice cue;
- trigger;
- subject/actor ownership roles;
- optional subject type, nation, final-wave condition, location/radius, and cooldown;
- delay and duration;
- priority; and
- whether it is one-shot.

The current trigger enum covers mission start/outcome, captures, commander defeat, attack/under-attack, first contact, heavy losses, near defeat, owner elimination, and wave incoming/cleared events.

`commander_voices` controls generic commander lines and trigger/line muting for a mission.

## Campaign membership and standalone missions

Campaign JSON lists mission IDs in campaign order. A mission that is referenced by a campaign belongs to that campaign's progression path. A mission that is not claimed by a campaign is exposed through the standalone mission flow.

Campaign progression, completion, and mission unlock state are stored by the persistence system rather than embedded into the mission definition itself.

## Validation

Mission content is exercised by loader tests, campaign asset rules, victory-rule translation tests, commander setup tests, wave tests, and `content_validator`.

The authoring contract is the schema and runtime translation implemented by the repository. Speculative features such as dynamic mission generation or user-authored campaign tooling are not part of this format unless code and validation for them exist.
