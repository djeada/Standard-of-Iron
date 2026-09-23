# Mission Framework

Missions add authored gameplay rules to an existing battlefield. A map defines terrain, structures, spawns, environment, camera data, regions, and other world geometry. A mission selects that map and adds player/AI setup, objectives, scripted waves, stages, events, commander messages, and campaign-facing metadata.

Campaigns then order mission IDs and store progression separately from the mission schema.

The three data layers are intentionally distinct:

```text
assets/maps/*.json
  battlefield geometry and world authoring
        │
        ▼
assets/missions/*.json
  mission rules and force setup referencing a map
        │
        ▼
assets/campaigns/*.json
  ordered campaign membership/progression metadata
```

This separation lets the same map infrastructure support campaign missions, standalone missions, tutorial content, skirmish maps, Arena scenarios, and content validation without embedding every gameplay rule into terrain authoring.

The C++ schema is defined in `game/map/mission_definition.h` and parsed by the mission loading/runtime path.

## Mission identity

A mission has its own stable `id`, title, summary, and map reference.

The mission ID is what campaign data and persistence use to identify the scenario. The map path is not the mission identity: different mission definitions can conceptually reference maps independently of campaign ordering, and progression is recorded against mission/campaign state rather than inferred from the filename of the battlefield.

## Mission root

`MissionDefinition` contains these top-level concepts:

- `id`, `title`, `summary`;
- `map_path`;
- optional `teaching_goal`;
- optional `narrative_intent`;
- optional `historical_context`;
- optional `terrain_type`;
- `player_setup`;
- `ai_setups[]`;
- `victory_mode`;
- `victory_conditions[]`;
- `defeat_conditions[]`;
- `optional_objectives[]`;
- `stages[]`;
- `events[]`;
- `commander_messages[]`;
- `commander_voices`;
- `include_ambient_undead`; and
- `tutorial`.

A minimal mission shape is:

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

A minimal file can load, but practical authored missions normally provide explicit force setup, objectives, and the map structures/spawns required by those objectives.

## Map vs mission responsibility

A useful rule is:

- if it describes **where something physically exists**, it usually belongs to the map;
- if it describes **what the scenario asks that thing to do**, it usually belongs to the mission.

Examples:

| Map concern              | Mission concern                 |
| ------------------------ | ------------------------------- |
| terrain height and shape | survive for a duration          |
| rivers, bridges, walls   | capture a structure             |
| authored spawn positions | AI strategy/posture             |
| commander spawn entity   | commander dialogue trigger      |
| camera/environment       | wave timing                     |
| named regions            | objective referring to a region |

This boundary keeps mission logic from duplicating battlefield geometry.

## Player setup

`player_setup` defines mission-level configuration for the human force.

Current fields include:

- `nation`;
- `faction`;
- `color`;
- `starting_units[]`;
- `starting_buildings[]`; and
- `starting_resources`.

### Starting units

A starting-unit entry describes:

- troop type;
- count;
- position;
- behavior;
- guard radius;
- optional patrol waypoints; and
- optional `difficulty_scaling` (default `true`), which holds this spawn group at its authored count on every difficulty preset.

Supported behavior values map to the current runtime behaviors:

- `Strategic`;
- `Guard`;
- `Hold`; and
- `Patrol`.

The mission author therefore chooses an initial behavior without defining a separate combat implementation. Once spawned, the units use the same movement, combat, formation, and order systems as other troops.

### Starting buildings

A starting-building entry contains the structure type, position, and `max_population` value used by the production/reserve system.

Mission setup creates authored starting conditions; ordinary building/economy rules remain owned by the simulation systems after the mission begins.

### Starting resources

`starting_resources` supplies the initial economy state for the local owner. Resource names and behavior come from the economy systems rather than from mission-specific resource semantics.

## Commanders are map spawns

Mission files do not own commander placement.

Commanders are authored in the referenced map's `spawns[]` as troop spawns with the commander troop type, owner/player assignment, nation, and position.

That means commander location is part of battlefield authoring, alongside the rest of the starting world geometry.

The mission can still reference commander outcomes and dialogue through victory/defeat/message rules, but it does not duplicate the physical commander spawn in a second field.

Campaign/mission tests verify expected commander coverage and nation/mission consistency in shipped content.

## AI setup

Each entry in `ai_setups[]` defines one computer-controlled force.

| Field                | Purpose                                                               |
| -------------------- | --------------------------------------------------------------------- |
| `id`                 | mission-local force identifier                                        |
| `nation`             | nation/roster selection                                               |
| `faction`            | mission/UI faction metadata                                           |
| `color`              | owner colour                                                          |
| `difficulty`         | AI execution/wave-strength tuning                                     |
| `difficulty_scaling` | opt this force out of the player-selected difficulty (default `true`) |
| `team_id`            | alliance grouping between owners                                      |
| `strategy`           | strategic preset                                                      |
| `posture`            | garrison or field behavior                                            |
| `personality`        | aggression/defense/harassment modifiers                               |
| `starting_units`     | mission-level starting troops                                         |
| `starting_buildings` | mission-level starting structures                                     |
| `wave_escalation`    | per-wave strength growth                                              |
| `waves`              | scripted reinforcement waves                                          |

Personality values default to `0.5` when omitted.

`difficulty` is the _authored_ tuning for this opponent and is independent of the difficulty the player selects before a match. The player's preset is documented in [docs/DIFFICULTY.md](DIFFICULTY.md); the two are applied once each and never compounded into one another.

## AI strategy

The mission framework delegates strategic interpretation to the AI strategy parser.

Current accepted strategy names include:

- `balanced`;
- `aggressive`;
- `defensive`;
- `expansionist`;
- `economic`;
- `harasser` / `harassment`;
- `rusher` / `rush`.

The AI system also recognizes its special Sepulcher/undead defensive strategy aliases through the AI strategy layer.

Unknown values are normalized by the AI parser rather than silently creating an author-defined strategy with no runtime implementation.

See [AI_ARCHITECTURE.md](AI_ARCHITECTURE.md) for the complete strategy/posture/behavior model.

## AI posture

Posture controls whether the strategic AI is expected to remain centered on its holdings or operate as a field opponent.

### `garrison`

A garrison AI maintains its economy and local defence but does not use ordinary proactive attack/expansion behavior as a field opponent would.

Authored reinforcement waves can still attack because mission assault waves are handled separately from the strategic posture.

### `field`

A field AI can use ordinary strategic attack and expansion behavior in addition to defence and economy.

This distinction is especially useful for fortress/defence missions: the owner can remain a garrison while mission-authored wave columns attack on a schedule.

## Teams and multiple AI owners

`team_id` groups mission owners into alliances.

Combat hostility uses the shared owner/team rules. Mission authors should therefore use owner/team configuration rather than assuming every different `id` is automatically hostile to every other `id`.

Multiple AI owners can cooperate in one scenario while retaining separate AI state, bases, waves, and color/nation metadata.

## Scripted waves

Mission waves are authored reinforcements attached to an AI setup.

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

Mission waves are distinct from the AI's strategic committed attack waves. A mission wave is content authored by the scenario; strategic waves are assembled by AI reasoning from currently available forces.

## Wave trigger modes

Two trigger modes are defined in `mission_definition.h`.

### `Time`

The wave becomes eligible according to its authored timing.

### `AfterPreviousCleared`

The wave waits until the preceding wave owned by that AI has cleared, then applies its grace period.

The current defaults are:

- grace: `25` seconds;
- warning: `15` seconds.

These defaults come from the schema/runtime rather than from a recommendation in the documentation.

## Entry points

A wave can use either one `entry_point` or several `entry_points[]`.

When the array is present, it takes precedence. `resolved_entry_points()` returns the effective list used by runtime.

The entry point is the spawn location, not necessarily the tactical destination. Spawned assault units receive their objective through the mission wave/AI path after they exist in the world.

This separation allows an authored column to enter from a road, valley, bridge approach, or map edge and still pursue an objective elsewhere.

## Wave composition

Wave composition entries describe the unit mixture to spawn.

Strength scaling clamps the multiplier to the supported range `0.1..8.0`, rounds counts, and never reduces a composition entry below one unit.

This guarantees that an authored role in a wave does not disappear simply because the global multiplier is small.

## Built-in wave archetypes

`game/map/wave_archetype_catalog.cpp` defines these built-in archetypes:

| ID                | Composition role                       |
| ----------------- | -------------------------------------- |
| `probe`           | light swordsman/archer column          |
| `assault`         | mixed spear/sword/archer attack        |
| `cavalry_flank`   | mounted swordsmen and horse archers    |
| `skirmish_screen` | archers, horse archers, and spears     |
| `siege_column`    | catapult with infantry escort          |
| `elite_guard`     | heavy mixed force with elite swordsmen |

If `assets/data/waves/archetypes.json` exists, `WaveArchetypeCatalog` loads it as an overlay:

- a matching ID replaces the built-in definition;
- a new ID extends the catalogue.

The data file therefore customizes the same runtime catalogue instead of creating a second wave implementation.

## Runtime ownership of assault waves

Spawned mission-wave troops receive `AssaultWaveComponent` and are handled by the AI assault behavior.

That ownership excludes them from ordinary reserve/gather/expansion/retreat assignment pools. The AI can therefore remain strategically defensive while the wave follows the mission-authored assault role.

When stalled by a barrier on the route to its objective, the assault path can use the current barrier-breach behavior rather than requiring a separate mission script for every wall encounter.

## Victory rules

`Game::Mission::build_victory_rules()` translates authored mission conditions into the runtime `VictoryRuleSet`.

The current mission condition types are:

| Type                   | Required/important data                               |
| ---------------------- | ----------------------------------------------------- |
| `destroy_all_enemies`  | no extra field; tracks enemy barracks for elimination |
| `survive_duration`     | `duration`                                            |
| `control_structures`   | structure type(s), optional `min_count`               |
| `capture_structures`   | structure type(s), optional `min_count`               |
| `clear_undead_zone`    | `zone_id`                                             |
| `purify_shrine`        | `zone_id`                                             |
| `survive_undead_wave`  | `zone_id`, optional `wave_count`                      |
| `survive_waves`        | optional `wave_count`                                 |
| `accumulate_resources` | positive `resources` map                              |
| `eliminate_commanders` | no extra field                                        |

Mission JSON is therefore declarative: it names a supported rule and supplies its parameters; the victory system owns evaluation.

## Victory mode

`victory_mode` accepts:

- `any`; or
- `all`.

`any` is the default.

`all` sets the runtime rule set to require all translated victory rules before declaring victory.

This makes compound objectives possible without implementing a special one-off mission controller for each scenario.

## Victory-rule fallback

If no supported victory rule is produced from the mission definition, the translator warns and falls back to barracks elimination.

That fallback protects the runtime from a mission with no recognized way to end in victory, but it is not a substitute for valid authored conditions. Content validation/tests should still catch incorrect mission data before release.

## Structure names in objectives

Mission rule translation normalizes structure names to lowercase.

The legacy name:

```text
village
```

maps to:

```text
barracks
```

When a structure condition does not provide a usable structure type, the current fallback is `barracks`.

Authors should prefer current explicit structure names rather than relying on compatibility aliases.

## Defeat rules

Supported defeat condition types are:

| Type                       | Meaning                                                   |
| -------------------------- | --------------------------------------------------------- |
| `lose_all_units`           | no local units remain                                     |
| `lose_commander`           | commander is lost                                         |
| `only_commander_remaining` | commander is the only remaining force after the rule arms |
| `time_limit`               | defeat after `duration`                                   |
| `lose_structure`           | all tracked structure types are lost                      |

If a mission translates no explicit defeat rules, the runtime adds the commander-centered default pair:

```text
lose_commander
only_commander_remaining (tracking barracks)
```

The `only_commander_remaining` rule has startup/arming behavior documented in [VICTORY_SYSTEM.md](VICTORY_SYSTEM.md), which also covers rule evaluation, save state, and timers.

## Optional objectives

`optional_objectives[]` use the same `Condition` structure as mission conditions but feed the objective/UI layer rather than replacing primary victory rules.

This allows a mission to distinguish:

- what ends the battle; and
- what the player can accomplish for an additional goal/reward/narrative result.

The economy guidance layer also reads resource-accumulation objective requirements so an objective resource remains visible even when its current value is zero.

## Mission stages

`stages[]` use `MissionStage`.

A stage can describe goals involving:

- structure counts;
- durations;
- wave counts;
- resource requirements;
- a target position/radius; and
- a route.

Each stage also carries:

- ID;
- title;
- description;
- hint; and
- type.

Stages are useful when the mission should present a sequence of objectives or teaching steps without changing the underlying map.

Tutorial content uses the same mission infrastructure with the `tutorial` flag and tutorial-specific stage/progression behavior layered on top.

## Mission events

`events[]` are `GameEvent` records composed of an `EventTrigger` and one or more `EventAction` records.

The exact accepted trigger/action strings are defined by the mission loader and runtime handlers. A JSON string has no effect merely because it looks plausible; there must be code that recognizes and executes it.

That is an important authoring rule: mission data selects implemented event types, it does not dynamically define new runtime behavior by name.

## Commander messages

`commander_messages[]` use `CommanderMessage` records.

A message can specify:

- speaker;
- pose;
- text;
- voice cue;
- trigger;
- subject/actor ownership roles;
- optional subject type;
- optional nation;
- final-wave condition;
- optional location/radius;
- cooldown;
- delay;
- duration;
- priority; and
- one-shot behavior.

The current trigger enum covers mission start/outcome, captures, commander defeat, attack/under-attack, first contact, heavy losses, near defeat, owner elimination, and wave incoming/cleared events.

A line may be spoken by the player's own commander (in the campaign, `carthage_sword_commander` is Hannibal). The player's commander is kept out of the generic voice-bank roster, but `local_commander_speaker` binds authored lines to it, so they show with the ally styling and resolve `owner_id: "player"` roles against the player. A `wave_cleared` phase can span several owners and reports the owner of its last wave, so a Hannibal line on a cleared phase should filter on `final_wave` rather than `owner_id`.

`commander_voices` controls generic commander-line behavior and mission-level trigger/line muting.

## Dialogue vs objective state

Commander messages are presentation/narrative reactions to mission events. They do not replace objective evaluation.

A line can announce an incoming wave or a captured location, but victory/defeat still comes from the authoritative rule set. This keeps the mission playable and testable even when voice/audio presentation is unavailable.

## Ambient undead

`include_ambient_undead` controls whether the mission includes the ambient undead/world-threat path defined by the current mission/runtime implementation.

Objectives such as `clear_undead_zone`, `purify_shrine`, and `survive_undead_wave` use the same mission/victory framework rather than requiring a separate “undead mission” schema.

## Tutorial flag

`tutorial` marks missions that participate in the guided tutorial flow.

The tutorial still uses the ordinary mission/map/simulation systems. Its guided progression, hinting, and UI surfaces are additional orchestration around the same command/economy/combat systems the player uses elsewhere.

### Tutorial progress in saves

`TutorialDirector::serialize()` is written to save metadata under `"tutorial"` (versioned by its own `"version"` key, so the snapshot and database versions do not move). Loading a slot runs `GameEngine::restore_tutorial_state`, which re-activates the director when the restored mission is the tutorial and puts it back on the saved step with its completed steps and counting baselines. Steps are stored by id, not index, so reordering the step list does not shift old saves. A save without the key (made before this existed) restarts the tutorial at step one, except that a raid already broken skips to the step after Defend: otherwise the mission clock would be held for a raid that has already come.

Kill counts survive a load: save metadata carries a versioned `"battle_stats"` key with the top-bar enemy counters and each player's recruited/killed/lost totals (`GlobalStatsRegistry::serialize_counters`), so the scout step's baseline is saved with the tutorial and compared against the restored counter.

A load restores the camera **after** the environment: `Environment::apply` frames the map's authored opening view, and restoring the saved camera before it meant every load opened on that view instead of where the player was looking.

### Tutorial step conditions worth knowing

- The move step accepts a formation order as a move: with soldiers selected, a right-click on the ground goes through formation placement and is issued as `OrderKind::Formation`.
- The scout step counts enemy **units** (squads) killed, not men. Kills are reported in men everywhere else, and one scout squad alone is more men than the step's target.
- The tutorial Roman commander carries a `guard` behaviour in `map_tutorial.json`, which keeps him out of the AI snapshot and at his outpost until the assault step.
- The tutorial barracks reserve (140) covers the three recruits the army step needs even after a squad is lost to the scouts; a Home or civilian selected during the recruit steps gets a hint that walks through Recruit Civilian → Deliver.

## Campaign membership

Campaign JSON lists mission IDs in campaign order.

A mission referenced by a campaign belongs to that campaign's progression path. A mission that is not claimed by a campaign is exposed through the standalone mission flow.

Campaign membership is therefore external to the mission file itself.

This has two practical consequences:

1. the mission definition does not need to know its campaign index; and
2. standalone availability can be determined by cataloguing missions and subtracting campaign-owned IDs.

## Progression persistence

Campaign completion, mission unlock state, and mission results are stored by the persistence layer.

They are not embedded into the mission definition or map file because they are player-specific runtime data, not authored content.

See [SAVE_LOAD_SYSTEM.md](SAVE_LOAD_SYSTEM.md) for the relevant campaign persistence tables.

## Standalone missions and skirmish separation

Maps referenced by missions are not automatically ordinary free-play skirmish boards. The catalog/setup path distinguishes authored mission use from skirmish availability.

This keeps a battlefield designed around one mission objective from appearing as a generic match simply because it is technically a valid map.

## Mission startup

Loading a mission involves more than parsing its JSON.

The application resolves/reuses map context, builds the world, initializes AI, prepares terrain/render resources, and waits on startup readiness gates before presenting the battle as ready.

The current loading readiness includes terrain-scatter GPU readiness and initial AI decision readiness, with bounded wait behavior documented in [MISSION_STARTUP.md](MISSION_STARTUP.md).

Mission schema correctness and mission startup readiness are therefore separate concerns: a valid mission file can still reveal runtime startup work if its map/assets/AI need preparation.

## Relationship to the AI system

Mission AI setup configures the same strategic AI described in [AI_ARCHITECTURE.md](AI_ARCHITECTURE.md).

Mission data supplies:

- owner identity/team/nation;
- strategy/posture/personality/difficulty;
- starting force/buildings; and
- authored reinforcement waves.

The AI system supplies:

- strategic reasoning;
- economy/production decisions;
- base management;
- formation/station planning;
- committed strategic attacks; and
- assault behavior for mission waves.

The mission framework configures the AI; it does not duplicate it.

## Relationship to victory/defeat runtime

Mission condition translation produces the runtime victory/defeat rule set.

The runtime then owns:

- evaluation cadence;
- timers;
- arming/latching behavior;
- save/restoration of rule state; and
- final outcome transition.

This keeps mission JSON declarative while allowing the victory system to be tested independently from any one asset file.

## Content validation

Mission content is exercised by several layers:

- mission loader tests;
- campaign asset rules;
- victory-rule translation tests;
- commander setup tests;
- wave/archetype tests;
- scenario/runtime tests; and
- `content_validator`.

Validation checks both structural correctness and cross-file references where supported.

A mission author should treat a validator failure as a schema/runtime contract failure rather than something to silence with undocumented JSON.

## Authoring checklist

A mission is easier to review when these questions have explicit answers in data:

### Identity and battlefield

- Does the mission have a stable ID?
- Does `map_path` resolve to the intended battlefield?
- Are commander/critical spawns present in the map rather than duplicated in mission JSON?

### Player force

- Is the nation/faction/colour correct?
- Are starting units/buildings/resources sufficient for the opening objective?
- Are starting behaviors intentional?

### AI force

- Are team relationships correct?
- Is posture `garrison` or `field` appropriate for the scenario?
- Are strategy/personality/difficulty values supported by the AI parser?

### Waves

- Are entry points valid map positions?
- Is the trigger mode correct?
- Does every composition/archetype resolve to supported content?
- Does wave escalation preserve the intended role mix?

### Objectives

- Does at least one supported victory rule translate?
- Are defeat rules explicit when the default commander-centric pair is not desired?
- Are optional objectives truly optional rather than hidden win conditions?

### Presentation

- Do stage text and commander messages describe actual runtime events?
- Are voice/message triggers supported by code?

### Campaign

- If campaign-owned, is the ID listed in the correct campaign order?
- If standalone, is it intentionally not claimed by a campaign?

## Common authoring boundaries

Several mistakes are avoided by keeping responsibilities in the correct file/system:

- do not put terrain geometry in mission logic when the map should own it;
- do not add a mission-level commander position when the map spawn is authoritative;
- do not invent an event/action string without a runtime handler;
- do not use mission waves as a replacement for ordinary strategic AI when `field` posture is the intended behavior;
- do not rely on dialogue to implement win/loss state; and
- do not store player progression inside the authored mission definition.

## Source map

| Concern              | Source                                   |
| -------------------- | ---------------------------------------- |
| Mission schema       | `game/map/mission_definition.h`          |
| Mission loading      | mission loader/runtime under `game/map/` |
| Victory translation  | `game/map/mission_victory_rules.cpp`     |
| Wave archetypes      | `game/map/wave_archetype_catalog.cpp`    |
| Wave data overlay    | `assets/data/waves/archetypes.json`      |
| Mission assets       | `assets/missions/`                       |
| Map assets           | `assets/maps/`                           |
| Campaign membership  | `assets/campaigns/`                      |
| Strategic/assault AI | `game/systems/ai_system/`                |
| Progress persistence | save storage/persistence layer           |

The mission framework documented here is the schema and runtime behavior implemented by the repository. Dynamic mission generation, arbitrary user-defined runtime event types, or other speculative authoring features are not part of the format unless the corresponding code and validation exist.
