# Architecture

Standard of Iron is organized around one rule: gameplay state belongs to the simulation, while application code, UI, rendering, tools, persistence, and AI consume or transform that state through explicit boundaries.

That rule is visible in both the C++ target graph and the runtime ownership model. The build separates low-level ECS and domain libraries from the application shell, while `SessionContext`, `CommandQueue`, render snapshots, and save snapshots define the main data boundaries inside a running match.

This document describes the architecture as it exists in the repository now. It focuses on ownership, dependency direction, data flow, deterministic execution, persistence, and the checks that keep those contracts from drifting.

## System shape

The main runtime dependency shape is:

```text
animation / scene
        │
        ▼
   engine_core
        │
        ▼
   domain libraries
        │
        ▼
     game_sim
       ├── soi_ai
       ├── soi_missions / soi_campaign
       ├── soi_persistence / soi_runtime
       ├── game_view
       └── render_gl
                │
                ▼
             app_core
                │
                ▼
        standard_of_iron
```

`engine_core` contains the ECS and low-level world/session primitives. Domain libraries add navigation, units, formations, movement, economy, combat, wildlife, terrain, and related gameplay services. `game_sim` composes the simulation-facing runtime used by the live application and by headless tools.

Above that kernel, application code coordinates Qt/QML-facing controllers, match setup, persistence orchestration, and presentation. Rendering links downward to simulation data. Simulation does not reach upward into Qt UI or renderer implementation.

The exact domain split is reflected in the root CMake graph. The important architectural property is direction: a lower gameplay layer does not need to know which screen, renderer, or packaging workflow will eventually present its output.

## Runtime ownership

A running match has several kinds of state, but only one of them is authoritative gameplay state.

| State class               | Owner                                              | Examples                                                              |
| ------------------------- | -------------------------------------------------- | --------------------------------------------------------------------- |
| Authoritative simulation  | `SessionContext` / `World` and simulation services | entities, resources, ownership, combat state, mission state, RNG      |
| Commands                  | `CommandQueue` and command dispatch                | player orders, AI orders, replay commands, scripted commands          |
| Derived simulation caches | subsystem-owned runtime services                   | spatial indexes, movement facts, collision indexes, engagement caches |
| Presentation snapshots    | simulation-published immutable/read-only data      | render snapshot, minimap/read-model state                             |
| Application/UI state      | `app/`, view models, QML                           | selected panel, menus, settings, transient UI interaction             |
| Renderer caches           | renderer/backend                                   | GPU resources, prepared meshes, animation presentation data           |
| Persistent storage        | save database + snapshot formats                   | serialized world/session state, campaign progress, save metadata      |

The distinction matters because it determines what can be rebuilt, what must be serialized, and what can safely lag behind by a presentation frame.

## Session authority

Per-match authority lives in `Game::Session::SessionContext`.

A session owns or exposes the state required to advance one match, including:

- the `World`;
- terrain and map services;
- ownership and nation state;
- economy state;
- the fixed-step simulation clock;
- deterministic RNG state;
- command and replay state; and
- the runtime systems registered for that session.

The world contains entity/component state. Session services contain match-level state that does not naturally belong to one entity. Code that needs per-match services resolves them from the world/session relationship instead of creating an unrelated process-global authority.

### World-to-session binding

`Game::Session::services_for(world)` resolves the session services associated with a world.

Worlds created inside a `SessionContext` have an explicit binding. A world created outside a session can still fall back to the ambient session; this compatibility path is instrumented and can be made fatal with:

```sh
SOI_STRICT_WORLD_BINDING=1
```

or programmatically with:

```cpp
Game::Session::set_strict_world_binding(true);
```

`SessionServiceOwnershipDeathTest.StrictBindingRefusesAWorldWithNoSession` covers the strict path.

The compatibility fallback exists so older call sites can continue to resolve match services, but it is not equivalent to explicit ownership. The repository tracks the remaining ambient access sites and prevents that count from increasing unnoticed.

## Entity identity and ECS boundary

Entities use a 64-bit `EntityID`. The handle packs a 32-bit index and a 32-bit generation, allowing an entity slot to be reused without making an old handle silently identify the new entity occupying that slot.

Components are plain data-oriented types rather than objects inheriting through a polymorphic component base. Systems query the world for the component combinations they need and mutate authoritative components only from simulation-owned code paths.

This keeps entity identity, storage, and iteration independent from presentation classes. A renderer, QML component, or save row does not become the owner of a gameplay entity merely because it refers to the entity's ID.

## Command pipeline

Player input, AI, replay playback, and scripted systems converge on typed commands.

```text
player input ─┐
AI            ├──► typed Game::Command ─► CommandQueue ─► validation/dispatch ─► systems
mission/tools ┤
replay        ┘
```

`CommandQueue` is the match command boundary. The application can ask simulation services whether an action is available so it can show useful feedback, but authoritative mutation occurs through command dispatch rather than through a parallel client-side implementation of the same order.

This gives different command producers the same rules. A move issued by the player, an AI move, and a replayed move enter the same simulation path instead of carrying three independent versions of movement legality.

Accepted commands also form the deterministic replay stream. Replay playback can replace live command producers while exercising the same command application path.

## Fixed-step simulation

Gameplay advances in fixed simulation steps. Presentation may run at a different cadence.

That separation has several consequences:

- gameplay timers derive from simulation time, not rendering frame time;
- command ordering is tied to simulation updates;
- AI decisions are scheduled against simulation updates;
- replay digests compare deterministic simulation state rather than visual frames; and
- the renderer can skip or interpolate presentation frames without changing the authoritative battle.

The live application, `soi_headless`, replay verification, `balance_sim`, Arena scenarios, and performance tooling all reuse this simulation-facing contract.

## Determinism

Determinism is an architectural requirement because replays and headless verification depend on it.

The main ingredients are:

- fixed simulation updates;
- typed, ordered commands;
- deterministic RNG owned by the session;
- AI results applied on scheduled simulation updates rather than on whichever wall-clock instant a worker finishes;
- save/replay snapshots that capture authoritative non-entity state; and
- world digests used by replay verification.

Presentation randomness and visual interpolation can remain renderer-side as long as they do not feed back into authoritative gameplay decisions.

## Render dependency boundary

The root CMake graph is explicit: `render_gl` links `game_sim`.

The renderer consumes simulation output; simulation does not link the renderer. Higher AI, campaign, mission, and persistence facades are also not pulled into the renderer merely because their results eventually become visible.

`game_view` contains camera-, picking-, and minimap-facing services that sit between raw simulation and presentation concerns without becoming part of the renderer backend itself.

### Render snapshots

The renderer does not need to walk the mutable live world while drawing. Simulation publishes render snapshots containing the state required for presentation. The render thread acquires those published snapshots and builds draw work from them.

That boundary provides three useful properties:

1. simulation can continue to own mutation rules;
2. rendering works with a stable view of the frame; and
3. renderer caches can be discarded or rebuilt without changing gameplay state.

The production renderer does not use a live-world fallback when a render snapshot is unavailable. The frame can be skipped rather than violating ownership.

See [RENDERING_ARCHITECTURE.md](RENDERING_ARCHITECTURE.md) for the full scene-walk and backend path.

## Persistence boundary

Persistence captures authoritative state without serializing every cache and presentation object in the process.

World entities are serialized through `game/save/serialization.*`. Non-entity authoritative subsystem state is captured through `Game::Session::SessionSnapshot`.

`game/save/snapshot_contract.*` classifies data as:

- authoritative serialized;
- derived/rebuilt;
- presentation-only; or
- campaign-level.

That classification is important because a save file should not accidentally promote a cache to a second source of truth. Spatial indexes, transient movement facts, and renderer interpolation state are rebuilt. Simulation time, deterministic RNG, explored visibility, mission progress, AI strategic state, and other authoritative fields are explicitly preserved where required.

Load restoration is staged before the live match is replaced, and the storage layer adds compression, checksums, database schema migration, integrity checks, and quarantine/recovery. See [SAVE_LOAD_SYSTEM.md](SAVE_LOAD_SYSTEM.md).

## AI boundary

AI is a command producer, not an alternate simulation.

The main thread builds immutable AI snapshots from the match. Worker threads reason over those snapshots and produce typed AI commands. Commands are filtered and later applied through the gameplay command path on their scheduled simulation update.

Workers therefore do not own the live ECS world. This keeps AI threading from turning ordinary gameplay components into shared mutable state.

See [AI_ARCHITECTURE.md](AI_ARCHITECTURE.md).

## Mission and campaign boundary

Maps, missions, and campaigns form separate data layers.

Maps define battlefield geometry and world authoring. Missions reference maps and add player/AI setup, objectives, scripted waves, stages, and events. Campaigns order mission IDs and store progression outside the mission schema.

The runtime translates mission data into the same simulation systems used elsewhere: AI setup becomes AI configuration, mission objectives become victory/defeat rules, and mission waves become runtime assault units rather than a private combat implementation.

See [MISSION_FRAMEWORK.md](MISSION_FRAMEWORK.md).

## Application and UI boundary

`app/` owns application composition and Qt-facing orchestration. It can expose read models, preferences, selection state, command availability, save/load status, and similar information to QML, but it is not a second gameplay simulation.

The UI design system centralizes presentation tokens and persistent UI preferences. Command panels and tooltips may explain current gameplay values, but those values are provided by application/simulation read models rather than duplicated as balance constants in QML.

This distinction is what allows headless tools to use `game_sim` without Qt Quick and allows simulation tests to exercise gameplay without constructing the UI.

## Tools reuse production paths

Repository tools are most useful when they exercise production code rather than approximations.

Examples include:

- `soi_headless` using the simulation runtime without a window;
- replay verification using the command stream and world digest;
- `balance_sim` using current troop/combat data;
- Arena scenarios using production mission, movement, formation, combat, animation, and rendering paths;
- content validation loading the same authored data structures as runtime; and
- performance gates consuming runtime profiling counters and reports.

A tool can add orchestration or measurement, but it should not invent a second rule set for the subsystem it is measuring.

## Architecture enforcement

The architecture is guarded at several levels.

### Link boundaries

CMake target dependencies prevent many invalid upward dependencies from compiling or linking without an explicit build-graph change.

### Source-policy checks

`scripts/check-pr-policy.py` runs compiler-free architecture/source checks in the pull-request policy lane. The repository also contains focused checkers for command boundaries, module direction, ambient access, world scans, documentation claims, and related invariants.

### Documentation accuracy

`scripts/check-architecture-doc.py` compares selected documentation claims with machine-readable repository sources. It currently verifies:

- ambient session-access counts against `scripts/ambient_instance_budget.json`;
- full-world scan counts against `scripts/world_scan_budget.json` and `scripts/world_scan_nested_allow.json`; and
- the public dependency that `render_gl` has in the root `CMakeLists.txt`.

`tests/architecture/documentation_accuracy_test.cpp` also checks key statements, including the presence of `game_sim`, `game_view`, `SessionContext`, `CommandQueue`, and this document's `Known limitations` section.

A number written in an architecture article is therefore not merely explanatory prose when a repository check owns the same fact.

## Performance-sensitive boundaries

Several architecture choices also exist to keep large battles tractable:

- shared spatial indexes avoid repeated full-world proximity scans;
- render snapshots avoid locking/reading the mutable ECS throughout a frame;
- fixed-step systems can profile their own simulation phases;
- renderer work is collected into queue/back-end stages;
- AI workers reason from compact snapshots; and
- asset and frame-pacing instrumentation separates simulation cost, render-thread cost, GPU timing, upload traffic, and post-playable asset work.

Performance work should improve those boundaries without making caches authoritative. A faster spatial index is still derived state; a faster renderer is still a consumer of the match.

## Failure and replacement boundaries

Operations that replace large parts of runtime state are coordinated explicitly.

Save loading stages a candidate world before replacing the active one. World clearing advances content/version state used to invalidate stale render data. `WorldFreeze` coordinates destructive world replacement/rebuild operations so simulation and presentation do not continue through the mutation window.

The same ownership principle applies: replacement happens at a defined boundary rather than allowing arbitrary systems to partially rebuild the world while other threads continue reading it.

## Known limitations

The limitations in this section are repository-backed rather than roadmap estimates.

**56 call sites still reach per-match state through the ambient access path:** `app/world` 2, `game/core` 1, `game/formation` 4, `game/map` 14, `game/systems` 22, `game/units` 9, `game/visuals` 1, `game/wildlife` 3. `scripts/ambient_instance_budget.json` is the source of truth, and `scripts/check-architecture-doc.py` fails when this sentence disagrees with it.

**72 full-world entity scans remain, of which 0 are allow-listed as loop-nested scans.** `scripts/world_scan_budget.json` and `scripts/world_scan_nested_allow.json` are the source of truth for those counts, and the same documentation check verifies them.

**Unbound worlds still have a compatibility fallback to the ambient session.** `services_for(world)` reports the unbound lookup and returns ambient services unless strict world binding is enabled. The strict environment/configuration paths above turn the same condition into a fatal error.

These are current implementation constraints. They are not a list of proposed features or a schedule for future work.

## Reading the code by concern

| Concern                 | Primary locations                                       |
| ----------------------- | ------------------------------------------------------- |
| ECS and entity identity | `game/core/`, `engine_core` target                      |
| Session ownership       | `game/session/`                                         |
| Typed commands          | `game/command/`                                         |
| AI                      | `game/systems/ai_system/`                               |
| Missions/campaign       | `game/map/`, mission/campaign assets                    |
| Persistence             | `game/save/`, `game/systems/save_*`, `app/persistence/` |
| Rendering               | `render/`, `scene/`, `animation/`                       |
| UI/application          | `app/`, `ui/`                                           |
| Validation/policy       | `scripts/`, `tests/architecture/`                       |

## Related architecture references

- [RENDERING_ARCHITECTURE.md](RENDERING_ARCHITECTURE.md) — snapshot consumption, scene submission, graphics profiles, and render backends.
- [AI_ARCHITECTURE.md](AI_ARCHITECTURE.md) — snapshots, workers, strategies, behaviors, and command production.
- [FORMATION_ARCHITECTURE.md](FORMATION_ARCHITECTURE.md) — unit layouts, army formations, terrain fitting, cohesion, and traversal.
- [COMBAT_SYSTEM.md](COMBAT_SYSTEM.md) — combat update order, targeting, contact, damage, and special processors.
- [SAVE_LOAD_SYSTEM.md](SAVE_LOAD_SYSTEM.md) — save snapshot composition, storage, migration, recovery, and restoration.
- [MISSION_FRAMEWORK.md](MISSION_FRAMEWORK.md) — mission schema, AI setup, waves, objectives, and campaign membership.
- [MISSION_STARTUP.md](MISSION_STARTUP.md) — map-context reuse, startup readiness, and initial AI preparation.
- [FRAME_PACING.md](FRAME_PACING.md) — presentation performance budgets and qualification workflow.
