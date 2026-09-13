# Victory System Architecture

Victory rules look simple to a player: eliminate an enemy, survive long enough, capture a position, or keep a commander alive. The implementation has to satisfy a harder set of requirements. Rules must be cheap enough to evaluate during simulation, expressive enough for both skirmishes and authored missions, deterministic across machines, and explicit enough that content authors can predict the result of their data.

The current architecture solves that by separating **content translation** from **runtime evaluation**. Map and mission formats are translated once into typed rules, and `VictoryService` evaluates those rules against compact runtime state rather than interpreting content strings every update.

## From authored content to runtime rules

There are two authoring paths and one runtime model.

```text
assets/maps/*.json                      assets/missions/*.json
        │                                        │
        ▼                                        ▼
VictoryConfig                           MissionDefinition
        │                                        │
        ▼                                        ▼
build_rule_set_from_config()            build_victory_rules()
        └───────────────────┬────────────────────┘
                            ▼
                    VictoryRuleSet
                    ├─ victory_rules[]
                    └─ defeat_rules[]
                            │
                            ▼
                     VictoryService
                    ├─ summarize world
                    ├─ evaluate victory
                    ├─ evaluate defeat
                    └─ finalize outcome
```

The boundary is deliberate: content-facing names and normalization stay at the translation layer. Runtime code works with typed payloads that contain only the data each rule needs.

## Default defeat philosophy

When content does **not** provide explicit defeat conditions, both map-driven and mission-driven play receive the same two defaults:

1. **No commander** — losing the commander causes defeat.
2. **Only commander remaining** — if the commander survives but the player has no non-commander troops and no tracked base structures, the match is lost.

The default model is therefore commander-centric rather than structure-centric.

`no_key_structures` remains supported, but content must opt into it explicitly. Losing a barracks can be strategically serious without every mission treating that loss as an automatic defeat.

### `only_commander_remaining` has a startup latch

The rule means **being reduced to only the commander**, not **starting with only the commander**.

It becomes armed only after the local player has previously owned at least one of:

- a non-commander troop; or
- a tracked base structure, currently barracks by default.

That prevents false defeat in commander-only openings, scripted reinforcement starts, and similar mission structures.

## Runtime rule model

The active rules live in one `VictoryRuleSet`:

```cpp
struct VictoryRuleSet {
  std::vector<VictoryRule> victory_rules;
  std::vector<DefeatRule> defeat_rules;
};
```

`VictoryRule` and `DefeatRule` are typed variants. Their payloads are intentionally narrow.

### Victory rules

| Payload                        | Meaning                                      | Data carried             |
| ------------------------------ | -------------------------------------------- | ------------------------ |
| `EliminationVictoryRule`       | Remove all tracked enemy structures          | `structure_types[]`      |
| `SurviveTimeVictoryRule`       | Remain alive until a duration expires        | `duration`               |
| `ControlStructuresVictoryRule` | Own enough tracked structures                | `StructureRequirement`   |
| `CaptureStructuresVictoryRule` | Capture enough formerly foreign structures   | `StructureRequirement`   |

### Defeat rules

| Payload                              | Meaning                                          | Data carried             |
| ------------------------------------ | ------------------------------------------------ | ------------------------ |
| `NoUnitsDefeatRule`                  | Lose all local units                             | none                     |
| `NoKeyStructuresDefeatRule`          | Lose all tracked structures                      | `structure_types[]`      |
| `NoCommanderDefeatRule`              | The commander dies                               | none                     |
| `OnlyCommanderRemainingDefeatRule`   | Only the commander remains after the rule arms   | `structure_types[]`      |

`OnlyCommanderRemainingDefeatRule` is parameterized by structure type even though current content normally tracks barracks. Keeping that dependency in the payload avoids hiding a `"barracks"` literal in evaluator code.

## One world summary per reevaluation

The expensive part of victory logic is not comparing counters; it is discovering the relevant world state.

`VictoryService` therefore builds one `WorldSummary` and reuses it for every active world-based rule:

```cpp
struct WorldSummary {
  bool local_has_units = false;
  int local_commander_count = 0;
  int local_non_commander_troop_count = 0;
  QHash<QString, int> enemy_structure_counts;
  QHash<QString, int> local_owned_structure_counts;
  QHash<QString, int> local_captured_structure_counts;
};
```

The service determines the structure types required by the active rules up front, so the summary does not count unrelated buildings.

This gives the evaluator several useful properties:

- every world-based rule shares the same entity scan;
- structure counting is limited to active rule dependencies;
- capture tracking is enabled only when a capture-based rule needs it; and
- pure timer rules do not require a world scan at all.

## Evaluation semantics

Within each rule list, the semantics are **OR**:

- the first satisfied victory rule wins the match;
- the first satisfied defeat rule loses it.

For world-state reevaluations, non-timer victory rules are checked before defeat rules. If a world-based victory and defeat become true during the same reevaluation, victory currently wins because it is evaluated first.

Time-based victory is handled through the fixed-tick objective clock described below.

## Objective time advances on simulation ticks

`VictoryService::update(world, dt)` runs inside the fixed simulation tick.

`RuntimeFrameOrchestrator::advance_simulation()` invokes it from the per-tick callback passed to `SessionContext::advance()`, immediately after the world step. The `dt` received by the victory service is therefore the session's `tick_seconds`, not presentation-frame time.

This makes timed objectives deterministic. A survive-time objective expires after the same number of simulation ticks whether the renderer is producing 15 FPS, 60 FPS, or an uneven sequence of frames.

A paused match advances no simulation ticks, so objective time also stops.

`RuntimeFrameOrchestratorTest.TheObjectiveClockRunsOnTicksNotOnFrames` verifies that the same survive-time rule resolves on the same tick under 60 FPS, 15 FPS, and a deliberately stuttering presentation schedule.

Because the victory update now runs on the simulation path, its completion callback also runs on the simulation thread. `GameEngine` serializes simulation and render access through the frame mutex, preserving the same world consistency expected by callers.

## Victory state survives save and load

`VictoryService::serialize_state()` and `restore_state()` persist the state that would otherwise change the meaning of a resumed match:

- elapsed objective time;
- startup delay;
- spectator polling timer;
- rule-arming flags;
- per-objective completion flags; and
- the decided outcome, if one already exists.

`GameEngine` registers that state as a contributor to `Game::Session::SessionSnapshot`.

This is especially important for timed objectives. Reloading a survive-time mission near its deadline must resume the existing clock rather than forcing the player to survive the entire duration again.

## Event-driven world reevaluation

World-based rules do not scan the world on every simulation tick. The service marks its summary dirty when events relevant to the current rule catalog occur:

- `UnitSpawnedEvent`;
- `UnitDiedEvent`; and
- `BarrackCapturedEvent`.

Those events are sufficient for the current world-based rules because their inputs are troop presence, commander presence, and tracked structure ownership.

A new rule family may require more than another variant alternative. It can also require:

- new fields in `WorldSummary`;
- additional event subscriptions or dirty triggers;
- map and/or mission translation support; and
- regression coverage for both runtime semantics and authoring defaults.

For example, a destination objective needs positional or region state, a resource objective needs resource-change notifications, and a defeat timer belongs on the time path rather than being forced through the entity summary.

The architecture is extension-friendly because those dependencies are explicit, not because new rule types are automatically free.

## Map and skirmish translation

Maps author victory through `VictoryConfig` in `game/map/map_definition.h`. `build_rule_set_from_config()` in `game/systems/victory_service.cpp` translates that data into a runtime `VictoryRuleSet`.

Supported map victory types include:

- `elimination`;
- `survive_time`;
- `control_structures`;
- `capture_structures`; and
- `undead_zones`.

### Undead-zone objectives

`undead_zones` lets a skirmish map use authored Iron Sepulcher content as its victory target rather than forcing the match into a barracks-elimination or timer model.

A map can define:

```json
"victory": {
  "type": "undead_zones",
  "undead_objectives": [
    { "type": "clear_undead_zone", "zone_id": "ruins_guard" },
    { "type": "purify_shrine", "zone_id": "shrine_sentinels" },
    { "type": "survive_undead_wave", "zone_id": "ruins_guard", "wave_count": 2 }
  ]
}
```

Each entry becomes the same runtime rule produced by the mission path. `undead_objectives` may also accompany another map victory type, in which case the undead objectives are appended to that rule set.

An `undead_zones` map with no objectives falls back to `elimination` and emits a warning rather than creating an unwinnable match.

Supported map defeat strings are:

- `no_units`;
- `no_key_structures`;
- `no_commander`; and
- `only_commander_remaining`.

When `defeat_conditions` is empty, translation injects:

```json
["no_commander", "only_commander_remaining"]
```

## Mission translation

Mission definitions use structured `Condition` entries. `Game::Mission::build_victory_rules()` translates them into the same runtime rule model as maps.

Supported mission victory conditions include:

- `destroy_all_enemies`;
- `survive_duration`;
- `control_structures`; and
- `capture_structures`.

Supported mission defeat conditions include:

- `lose_all_units`;
- `lose_structure`;
- `lose_commander`; and
- `only_commander_remaining`.

A mission with no explicit defeat conditions receives the same commander-centric default pair as a skirmish map.

## Normalization and compatibility

The translation layer normalizes a small amount of legacy content vocabulary before creating typed rules. The most important current alias is:

```text
village → barracks
```

Keeping compatibility at the edge lets older or partially migrated content continue to load without spreading legacy terminology into the runtime evaluator.

## Adding a new rule kind

Treat a new objective or defeat condition as a vertical slice through authoring, runtime state, invalidation, and tests.

1. Add the typed runtime payload.
2. Decide whether the rule belongs to victory, defeat, or both.
3. Extend `refresh_rule_metadata()` when new world data must be tracked.
4. Extend `summarize_world()` or add adjacent subsystem state for new inputs.
5. Add every event or timer trigger required to reevaluate the rule correctly.
6. Add translation support to maps, missions, or both.
7. Cover runtime semantics and translation defaults with tests.
8. Update the relevant authoring documentation.

If a rule needs region progress, per-entity state, resource totals, or another subsystem's data, represent that dependency explicitly. Do not encode additional semantics into generic string fields merely to avoid adding a proper payload.

## Implementation map

The core implementation and tests live in:

- `game/systems/victory_service.h`;
- `game/systems/victory_service.cpp`;
- `game/map/mission_victory_rules.h`;
- `game/map/mission_victory_rules.cpp`;
- `game/map/map_definition.h`;
- [MISSION_FRAMEWORK.md](MISSION_FRAMEWORK.md);
- `tests/systems/victory_service_test.cpp`;
- `tests/map/mission_victory_rules_test.cpp`; and
- `tests/map/mission_asset_rules_test.cpp`.

The practical model is one typed rule set, one shared world summary, deterministic objective time, and a commander-centric default defeat philosophy. Content can opt into other rules explicitly without requiring the runtime to reinterpret loosely typed mission data every frame.
