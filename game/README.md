# game/ navigation guide

`game/` is the simulation and its domain services: ECS, session state, commands, movement, combat, formations, economy, AI, missions, maps, persistence formats. It compiles without Qt Quick or the renderer so that headless tools reuse the production paths. The dependency map is `scripts/module_rules.json` (enforced by `scripts/check-modules.py`) together with the target graph in `game/CMakeLists.txt`; the narrative is in [docs/ARCHITECTURE.md](../docs/ARCHITECTURE.md), and the hotspot plan is in [docs/REFACTORING_INVENTORY.md](../docs/REFACTORING_INVENTORY.md).

## Where things live

| Directory                             | Owns                                                                                                                                                                                                                                                 | Start reading at                          |
| ------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ----------------------------------------- |
| `core/`                               | ECS: `World`, entities, components, events, simulation timing, deferred mutations, render snapshot, movement traces                                                                                                                                  | `world.h`, `component.h`                  |
| `session/`                            | `SessionContext` (world, clock, RNG, command queue, registries), `MapSession`, `SessionSnapshot`, `SelectionService`, world digest                                                                                                                   | `session_context.h`                       |
| `command/`                            | Typed command payloads, validator, `CommandQueue`, dispatcher, `CommandSystem`, replay                                                                                                                                                               | `command_queue.h`, `command_dispatcher.h` |
| `systems/`                            | Simulation systems and services, grouped by CMake domain: `navigation/`, `movement/`, `economy/`, `persistence/` (save storage and service), `combat_system/`, `combat_actions/`, `rpg_combat_system/`, `ai_system/`; the rest sits at the top level | `runtime_system_registry.cpp`             |
| `formation/`                          | Unit layouts, army formation planning, registry and runtime                                                                                                                                                                                          | `army_formation_planner.h`                |
| `map/`                                | Map/content definitions and loading, terrain data and service, campaign catalog, victory rules, environment                                                                                                                                          | `map_loader.h`, `terrain_service.h`       |
| `mission/`                            | Mission runtime: campaign manager, wave director, commander messages, tutorial director, difficulty                                                                                                                                                  | `campaign_manager.h`                      |
| `save/`                               | World serialization and the snapshot-field contract                                                                                                                                                                                                  | `serialization.h`, `snapshot_contract.h`  |
| `render_bridge/`                      | Data the client needs from the world: camera services, picking, selection controller, minimap generators, save metadata                                                                                                                              | `selection_controller.h`                  |
| `units/`                              | Unit definitions, factories, troop catalog                                                                                                                                                                                                           |                                           |
| `wildlife/`                           | Wildlife and bird-flock simulation                                                                                                                                                                                                                   | `wildlife_system.h`                       |
| `audio/`                              | Audio engine, cue ids, music, mastering                                                                                                                                                                                                              | `audio_system.h`                          |
| `visuals/`, `util/`, `accessibility/` | Building asset keys and team colours; small shared helpers; accessibility settings                                                                                                                                                                   |                                           |

Map/content versus mission runtime: `game/map/` defines and loads what a match is made of (terrain, props, spawns, campaign and mission definitions as data). `game/mission/` runs what happens during a match (waves, stages, messages, tutorial). Loading never advances mission state; mission state never re-parses map data.

## Ownership rules

- `SessionContext` is the per-match owner. Registry `instance()` calls resolve through the session bound to the world; they are not process globals.
- Gameplay mutations go through `game/command/`. Systems mutate the world during their tick phase; UI code never writes components directly.
- Presentation reads immutable snapshots (`World::acquire_render_snapshot`, `Published<T>` in `app/`).
- Simulation is deterministic: same seed and commands give the same digest (`soi_headless --replay --verify`). Keep iteration order, tie-breaking and RNG consumption stable in refactors.

## Worked call paths

Steps use the format `` `path` `Symbol` `` so `scripts/check-navigation-guides.py` can verify them. The application-side halves of these paths are in [app/README.md](../app/README.md).

### 1. From a submitted command to a simulation effect

1. `game/command/command_queue.cpp` `submit`: finds the world's `SessionContext` and pushes onto the pending queue (mutex-guarded). Non-replay commands are dropped in replay-only mode.
2. `game/systems/runtime_system_registry.cpp` `register_runtime_systems`: registers `CommandSystem` ahead of the systems that read its results.
3. `game/command/command_system.cpp` `CommandSystem::update`: replay digest check and feed, then drain.
4. `game/command/command_queue.cpp` `CommandQueue::drain`: swaps the pending batch out, re-validates each command, counts and reports rejections, notifies the observer, dispatches.
5. `game/command/command_dispatcher.cpp` `dispatch`: visits the payload and calls the domain service (for example `CommandService::move_units`, `CommandService::attack_target`).
6. `game/systems/movement/movement_system.cpp` `MovementSystem::move_unit` and `game/systems/combat_system/attack_processor.cpp` `process_attacks`: consume the components the command changed on later phases of the same tick.

### 2. One simulation tick

1. `game/session/session_context.cpp` `SessionContext::advance`: converts elapsed time into fixed ticks from the `SimulationClock`.
2. `game/core/world.cpp` `World::update`: runs registered systems in declared order, verifying declared component access, then applies deferred mutations.
3. `game/core/world.cpp` `World::publish_render_snapshot`: copies presentation components into the immutable snapshot the render thread reads.

### 3. Loading map content into a world

1. `game/map/map_loader.cpp` `MapLoader::load_from_json_file`: JSON to `MapDefinition` (grid, biome, terrain features, rivers, roads, structures, spawns, mission configuration).
2. `game/map/match_loader.cpp` `load_match`: acquires the parsed map from `MapContextStore`, populates the world through `MapTransformer::apply_to_world`, applies the environment.
3. `game/map/terrain_service.cpp` `TerrainService::initialize`: builds the height map and derived queries; `seal` marks it read-only for the match.

### 4. Capturing and restoring a world

1. `game/save/serialization.cpp` `Serialization::serialize_world`: envelope (capture stamp, owner registry, army formations, terrain) plus `serialize_entity` per entity.
2. `game/save/serialization.cpp` `Serialization::read_capture_stamp`: the stamp the application compares against the live tick and RNG to detect a torn capture.
3. `game/systems/persistence/save_load_service.cpp` `SaveLoadService::begin_save`: worker job; `game/systems/persistence/save_slots.cpp` `SaveStorage::write_slot` writes the packed record inside a transaction.
4. Restore reverses the order: `SaveStorage::read_slot`, `Save::unpack`, `Serialization::deserialize_world` into a staging world first, then into the live world, then `restore_map_session` restores the session-snapshot contributors.

## Conventions

- New components need a snapshot contract entry (`game/save/snapshot_contract.cpp`) and, if presented, a render-snapshot whitelist entry.
- Keep tolerances and constants named with units; keep directory moves separate from behaviour changes.
- Add a new cross-module dependency only in the direction `scripts/module_rules.json` allows; do not add a tolerated baseline.
