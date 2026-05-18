# How the Victory System Actually Works

Victory rules look simple on the surface: destroy the enemy, hold out for a timer, protect your commander. The tricky part is making those rules fast enough to check every frame, flexible enough for missions and skirmishes, and explicit enough that content authors can tell what will actually happen.

This document walks through the current victory architecture: how map and mission content become runtime rules, how the engine evaluates those rules cheaply, why the commander is now the default defeat anchor, and what needs to change when we add new objective families later.

## What we'll cover

1. The two translation paths: maps and missions
2. The runtime rule model in `VictoryService`
3. The default commander-centric defeat rules
4. The single-pass world summary that keeps evaluation cheap
5. The currently supported rule catalog
6. How event-driven reevaluation works
7. How to extend the system with new rule kinds safely


## The core idea: translate content once, evaluate typed rules cheaply

The runtime no longer interprets JSON-like strings on every update. Instead, content is translated into typed rule payloads once, then the service evaluates those payloads against a compact summary of the world.

```text
┌──────────────────────────────────────────────────────────────────────────────┐
│                             CONTENT LAYER                                   │
│                                                                              │
│  assets/maps/*.json                  assets/missions/*.json                  │
│  ┌───────────────────────┐           ┌────────────────────────────────────┐  │
│  │ VictoryConfig         │           │ MissionDefinition                  │  │
│  │ - type                │           │ - victory_conditions[]             │  │
│  │ - key_structures[]    │           │ - defeat_conditions[]              │  │
│  │ - defeat_conditions[] │           │                                    │  │
│  └──────────┬────────────┘           └──────────────────┬─────────────────┘  │
│             │                                           │                    │
└─────────────┼───────────────────────────────────────────┼────────────────────┘
              │                                           │
              ▼                                           ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                           TRANSLATION LAYER                                 │
│                                                                              │
│  victory_service.cpp                mission_victory_rules.cpp               │
│  - build_rule_set_from_config()     - build_victory_rules()                 │
│  - map/skirmish defaults            - mission defaults                      │
│  - string normalization             - condition normalization               │
└───────────────────────────────┬──────────────────────────────────────────────┘
                                │
                                ▼
┌──────────────────────────────────────────────────────────────────────────────┐
│                             RUNTIME LAYER                                   │
│                                                                              │
│  VictoryRuleSet                                                              │
│  ├── victory_rules[]   # OR semantics                                        │
│  └── defeat_rules[]    # OR semantics                                        │
│                                                                              │
│  VictoryService                                                              │
│  ├── summarise_world() once when dirty                                       │
│  ├── evaluate victory rules                                                  │
│  ├── evaluate defeat rules                                                   │
│  └── finalize_game(\"victory\" | \"defeat\")                                 │
└──────────────────────────────────────────────────────────────────────────────┘
```

The important design choice is that translation and evaluation are separate concerns. Content-facing strings stay at the edge. The service itself works with typed rule payloads.


## The two default defeat rules

If content does **not** declare explicit defeat conditions, the engine now applies exactly these two defaults:

1. **No commander** — if your commander dies, you lose.
2. **Only commander remaining** — if your commander is still alive but you have no non-commander troops and no tracked base structures left, you lose.

This is the baseline defeat model for both map-driven and mission-driven play.

### Why structure loss is no longer a default

`no_key_structures` is still supported, but it is now an explicit opt-in rule. The default defeat model is commander-centric, not structure-centric. That matters because many missions want barracks pressure without making every lost building an automatic failure state.

### The startup safety latch on `only_commander_remaining`

`only_commander_remaining` is meant to express **being reduced to only the commander**, not **starting with only the commander**.

To enforce that, the rule only becomes armed after the local player has previously owned at least one of the following:

- a non-commander troop
- a tracked base structure for the rule (currently barracks by default)

That prevents false defeats on commander-only openings, scripted reinforcement starts, and similar setups.


## The runtime rule model

The runtime stores rules in a `VictoryRuleSet`:

```cpp
struct VictoryRuleSet {
  std::vector<VictoryRule> victory_rules;
  std::vector<DefeatRule> defeat_rules;
};
```

`VictoryRule` and `DefeatRule` are typed variants. Each payload only stores the data that rule actually needs.

### Supported victory payloads

| Rule | Meaning | Payload |
| --- | --- | --- |
| `EliminationVictoryRule` | Remove all tracked enemy structures | `structure_types[]` |
| `SurviveTimeVictoryRule` | Stay alive until timer expires | `duration` |
| `ControlStructuresVictoryRule` | Own enough tracked structures | `StructureRequirement` |
| `CaptureStructuresVictoryRule` | Capture enough foreign structures | `StructureRequirement` |

### Supported defeat payloads

| Rule | Meaning | Payload |
| --- | --- | --- |
| `NoUnitsDefeatRule` | Lose all local units | none |
| `NoKeyStructuresDefeatRule` | Lose all tracked structures | `structure_types[]` |
| `NoCommanderDefeatRule` | Commander is dead | none |
| `OnlyCommanderRemainingDefeatRule` | Commander is isolated and rule is armed | `structure_types[]` |

`OnlyCommanderRemainingDefeatRule` is parameterised over structure types even though current content uses barracks. That keeps the rule explicit instead of hiding a `"barracks"` literal deep in evaluation logic.


## One summary per reevaluation

The expensive part of victory logic used to be repeated entity scans. The service now builds one `WorldSummary` and reuses it for every active rule:

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

That summary is built only when the world is dirty for world-based rules. The service tracks which structure types matter up front, so it does not need to count unrelated buildings.

### Why this scales better

- All world-based rules share the same scan.
- Structure tracking is filtered to the structure types actually referenced by active rules.
- Capture tracking is only enabled if a capture-based victory rule is present.
- Timer victories do not need a world scan at all once configured.


## Evaluation order and semantics

### Within each list

- Victory rules use **OR** semantics: the first satisfied victory rule wins.
- Defeat rules use **OR** semantics: the first satisfied defeat rule loses the game.

### Between victory and defeat

The service evaluates non-timer victory rules first, then defeat rules. Time-based victory is checked earlier in the update loop using elapsed time. In other words, if both a world-based victory and defeat become true in the same reevaluation, victory currently wins because it is checked first.


## Event-driven reevaluation

World-based rules do not need a full scan every tick. The service marks itself dirty and reevaluates on the events that matter to current rules:

- `UnitSpawnedEvent`
- `UnitDiedEvent`
- `BarrackCapturedEvent`

That is enough for the current rule catalog because all current world-based rules depend on force counts, commander presence, and structure ownership.

### Why future rule types may need more than a new payload

Adding a new variant alternative is not the whole job. A new rule may also need:

1. New data in `WorldSummary`
2. New dirty triggers or subscriptions
3. Translation support for maps and missions
4. Regression tests for both runtime behavior and content defaults

For example:

- a **reach destination** rule will probably need position or region state and movement-related dirtying
- a **gather resource** rule will need resource totals plus resource-change dirtying
- a **fail if timer expires** rule may need a defeat-side time path instead of a world summary path

That is why the current design is described as **extension-friendly**, not magically plug-in.


## Current translation rules

### Map / skirmish path

`VictoryConfig` in `game/map/map_definition.h` is the map-facing format. `build_rule_set_from_config()` in `game/systems/victory_service.cpp` translates it into runtime rules.

Current supported map victory types:

- `elimination`
- `survive_time`
- `control_structures`
- `capture_structures`

Current supported map defeat condition strings:

- `no_units`
- `no_key_structures`
- `no_commander`
- `only_commander_remaining`

If `defeat_conditions` is empty, the translator injects:

```json
["no_commander", "only_commander_remaining"]
```

### Mission path

Mission definitions use structured `Condition` entries. `Game::Mission::build_victory_rules()` translates them into the same runtime rule set.

Current supported mission victory condition types:

- `destroy_all_enemies`
- `survive_duration`
- `control_structures`
- `capture_structures`

Current supported mission defeat condition types:

- `lose_all_units`
- `lose_structure`
- `lose_commander`
- `only_commander_remaining`

If a mission omits defeat conditions entirely, the same commander-default pair is added automatically.


## Normalisation and legacy compatibility

The translators still normalize some legacy structure names. Most notably:

- `village` → `barracks`

That keeps older content and partially migrated missions/maps working while the asset vocabulary remains in transition.


## How to add a new rule kind

When adding a new rule, treat it as a small vertical slice:

1. Add a new typed payload to the runtime rule variant.
2. Decide whether it belongs in `victory_rules`, `defeat_rules`, or both.
3. Extend `refresh_rule_metadata()` if the rule needs tracked world data.
4. Extend `summarize_world()` or add adjacent runtime state if the rule needs new inputs.
5. Add the reevaluation triggers the rule depends on.
6. Add translation support for maps and/or missions.
7. Add runtime tests and translation tests.
8. Update this document and the content docs.

If the new rule needs per-entity state, region progress, or subsystem-owned data, prefer adding that explicitly rather than smuggling more meaning into generic string fields.


## File map

The current implementation is centered in these files:

- `game/systems/victory_service.h`
- `game/systems/victory_service.cpp`
- `game/map/mission_victory_rules.h`
- `game/map/mission_victory_rules.cpp`
- `game/map/map_definition.h`
- `docs/MISSION_FRAMEWORK.md`
- `tests/systems/victory_service_test.cpp`
- `tests/map/mission_victory_rules_test.cpp`
- `tests/map/mission_asset_rules_test.cpp`


## Practical takeaway

The victory system is now built around one rule set, one world summary, and one default defeat philosophy: protect the commander, and do not let the commander become the only thing left. Everything else is explicit content.
