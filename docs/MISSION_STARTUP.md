# Mission Startup Architecture

Mission startup separates **preparation** from **playable simulation**. Loading a battle is not one monolithic map parse: the application resolves the authored map, constructs the world, applies mission ownership/setup, initializes AI, prepares renderer-facing terrain/scatter state, starts simulation, and keeps the loading overlay visible until the required first-playable conditions are ready or the bounded maximum wait is reached.

The current startup contract is designed to avoid two opposite failures:

- revealing the battlefield before important runtime state is ready; and
- holding the loading screen forever because one asynchronous component never reports readiness.

## High-level startup flow

The mission/skirmish startup path begins from `GameEngine::start_skirmish_internal()` and is coordinated through `SkirmishRuntimeCoordinator`.

Conceptually:

```text
mission/skirmish request
        │
        ▼
resolve/acquire map context
        │
        ▼
construct world + terrain + authored entities
        │
        ▼
apply mission/player/AI ownership setup
        │
        ▼
initialize runtime systems
        │
        ├─ terrain/scatter preparation
        ├─ minimap/view preparation
        ├─ registries/index rebuilds
        └─ AI instances + initial decisions
        ▼
start fixed-step simulation
        │
        ▼
loading overlay waits on readiness gate
        │
        ├─ terrain scatter GPU ready?
        ├─ initial AI decisions ready?
        ├─ minimum display time met?
        └─ maximum wait not exceeded?
        ▼
first playable presentation
```

The simulation can already be advancing during the last portion of the loading overlay. “Simulation started” and “battlefield revealed” are therefore separate milestones.

## Startup profiler phases

`Engine::Core::StartupProfiler` records named phases and counters across startup.

The main phases currently include:

1. **`world.load`** — map context use, terrain, authored entities, environment systems, minimap setup, registry/index rebuilds, and renderer-facing preparation.
2. **`mission.commander_setup` / `mission.setup`** — owner/nation/team assignment, starting forces, objectives, waves, stages, and commander-related mission state.
3. **`ai.initial_preparation`** — AI instances and first decision jobs.
4. **runtime finalization** — simulation stepping begins while the loading overlay waits for first-playable readiness.

The profiler also records the transition from loading to first playable presentation and early frame timing after startup.

## Shared map context

`Game::Map::MapContextStore` is the shared parsed-map cache used by mission startup.

`MapContextStore::acquire()` resolves the map path and keys the cached `MapDefinition` using:

- resolved path;
- file size; and
- modification timestamp.

A matching stamp reuses the existing immutable:

```cpp
shared_ptr<const MapDefinition>
```

A changed file is parsed again.

The cache retains the three most recently used maps:

```text
k_retained_maps = 3
```

## Why map context is shared

Several startup subsystems need information from the map: world construction, mission setup, environment/terrain services, AI setup, minimap/view preparation, and content-dependent initialization.

Without a shared parsed context, each subsystem could reopen and reparse the same JSON independently. `MapContextStore` gives them one immutable parsed representation for the current startup.

The store exposes statistics through `MapContextStore::statistics()`, including request, parse, and reuse counts.

`MissionStartupTest.ParsesTheMissionMapOnce` verifies that the startup path reuses the shared map context rather than repeatedly parsing the same file.

## Cache invalidation

Map reuse is not based only on path.

The file size and modification timestamp participate in the cache stamp. If an authored map changes on disk, a subsequent acquisition does not blindly reuse the older parsed representation.

This matters for developer/editor workflows where a map can be changed without renaming it.

## World load phase

The `world.load` phase is broader than raw JSON parsing.

It covers the work required to turn the authored map into a runtime world, including areas such as:

- terrain service construction;
- authored entity/spawn creation;
- environmental state;
- collision/navigation-related rebuilds;
- runtime registry preparation;
- minimap/view setup; and
- renderer-facing terrain/scatter preparation.

The startup profiler should therefore be used to distinguish map parsing from total world initialization. A slow `world.load` does not automatically imply that JSON parsing is the bottleneck.

## Mission setup phase

Mission setup layers scenario rules over the constructed world.

The current mission setup path applies concepts such as:

- player/AI owner registration;
- nation and team state;
- mission starting forces/buildings/resources;
- victory/defeat rules;
- scripted waves;
- stages/objectives;
- commander setup/message state; and
- other mission runtime data.

See [MISSION_FRAMEWORK.md](MISSION_FRAMEWORK.md) for the schema and runtime ownership of those features.

## Commander setup ordering

Commander and owner configuration must exist before systems that depend on final mission ownership are initialized.

That is why mission startup separates general world creation from mission-specific owner/commander setup and final AI preparation.

AI cannot build a correct first snapshot for an owner that has not yet been registered into the match.

## AI initialization

`GameEngine::prepare_mission_ai_state()` prepares the first AI decision work during loading.

For campaign missions, skirmish-level AI initialization can be deferred through the `defer_ai_initialization` path until mission owners are known.

After owner registration, startup initializes AI for the actual mission owner set and calls:

```cpp
ai_system->prepare_initial_decisions(*m_world);
```

This means the first strategic decision batch is prepared before the game is fully revealed to the player.

## Initial AI decision budget

The engine waits up to:

```text
k_initial_decision_budget = 1500 ms
```

for the initial AI batch during preparation.

If the work is still running after that budget, startup logs the condition rather than blocking indefinitely at that synchronous wait point.

The loading-overlay readiness gate remains separate and can continue to report `AI initial decisions` as pending until the AI reports readiness or the overall overlay maximum wait is reached.

This gives startup two different safeguards:

- a 1.5 s bounded initial wait for the first AI batch; and
- a larger loading-overlay maximum wait protecting the overall reveal flow.

## AI worker ownership

AI workers reason from immutable snapshots.

The world-owning thread builds each `AISnapshot` and submits it to the worker. Worker code does not walk the mutable ECS world while startup/simulation continues.

That ownership model is the same one used after the mission becomes playable; startup does not create a special shared-world AI path.

See [AI_ARCHITECTURE.md](AI_ARCHITECTURE.md).

## Deterministic AI result timing

An AI decision job carries the simulation update at which its result is due:

```text
job_due_update = m_update_count + k_decision_latency_updates
```

`process_results()` applies the result on that update.

If the worker is late, the simulation waits for it rather than shifting the decision to a later update. Faster or slower worker hardware can therefore affect wall-clock cost without changing the simulation update on which the AI command enters the match.

That is important during startup as well as ordinary gameplay: precomputing the first decision is a performance optimization around a deterministic scheduling contract, not a change to gameplay timing.

## First-frame readiness gate

`GameEngine::mission_startup_pending_components()` is the source of truth for the runtime components that can keep the loading overlay active.

The current implementation reports two pending conditions.

### Terrain scatter

Reported when:

- `m_scatter` exists; and
- `m_scatter->is_gpu_ready()` is false.

This represents renderer-facing terrain scatter preparation that should finish before the battlefield is normally revealed.

### AI initial decisions

Reported when:

- an `AISystem` exists; and
- `initial_decisions_ready()` is false.

This prevents the match from being revealed while the computer opponent's initial strategic state is still unprepared under normal startup conditions.

## Loading-overlay maximum wait

The loading overlay has a hard maximum wait:

```text
k_loading_overlay_max_wait_ms = 15000
```

If the readiness conditions are still pending when that limit is reached, the engine logs which components remain pending instead of holding the loading screen indefinitely.

This is a fallback for robustness, not an assertion that the pending work completed successfully.

The difference matters operationally:

- normal release means readiness conditions became satisfied;
- timeout release means the runtime chose not to block the player forever and recorded the unresolved components for diagnosis.

## Minimum display time

The loading flow also respects its minimum display timing before reveal.

That prevents a very fast load from producing a visually unstable flash of the loading surface while still allowing readiness to extend the overlay when actual startup work needs more time.

The minimum display rule and readiness gate solve different UX problems and should not be conflated.

## Simulation during final loading

The fixed-step simulation can be active while the loading overlay remains on screen.

That allows systems to advance toward their first stable presentation state while the player still sees the loading surface.

The startup profiler distinguishes overlay release and first playable frame so this overlap can be measured rather than guessed.

## Terrain-scatter readiness

Terrain scatter is part of rendering/presentation, but its readiness participates in mission startup because a battlefield can be technically simulated while still visually incomplete.

The startup gate does not wait for every possible renderer cache in the application. It waits on the explicit current readiness contract exposed by terrain scatter and AI initial decisions.

See [RENDERING_ARCHITECTURE.md](RENDERING_ARCHITECTURE.md) for the rendering ownership model.

## Startup tracing

Enable startup tracing with:

```sh
SOI_STARTUP_TRACE=1
```

This emits the text report path used by the current startup profiler integration.

To write structured output for tooling, set:

```sh
SOI_STARTUP_TRACE_FILE=<path>
```

The two environment variables are independent.

This allows interactive logging and artifact generation to be enabled separately.

## Useful counters

Current startup counters include values such as:

- `map.requests`;
- `map.parses`;
- `map.reuses`;
- `ai.owners`; and
- `world.units` when the trace path requests that count.

The most direct signal for duplicate parsing is:

```text
map.parses
```

while:

```text
map.requests - map.parses
```

is the number of acquisitions served from an already parsed context.

## Interpreting a slow startup

Startup traces are most useful when broken down by phase.

### High `map.parses`

Investigate map-context reuse or invalidation. Multiple requests are not themselves a problem; multiple parses of an unchanged map are.

### Long `world.load`

Inspect terrain construction, entity spawning, indexes/registries, environment setup, renderer-facing terrain preparation, and other world initialization. Do not assume the parser is responsible without the counters.

### Long `mission.setup`

Inspect mission owner/setup/objective/wave/commander initialization rather than terrain parsing.

### Long `ai.initial_preparation`

Inspect snapshot construction, worker reasoning, number of AI owners, and whether the worker misses the 1500 ms preparation budget.

### Loading overlay reaches 15 s

Inspect the pending-component log. The timeout should identify whether terrain scatter, AI initial decisions, or both failed to become ready within the normal reveal window.

### First playable frames are slow

Startup may have released before expensive presentation work was completely prewarmed. Compare startup traces with [FRAME_PACING.md](FRAME_PACING.md) and post-playable asset-work counters.

## Relationship to frame pacing

Startup and frame pacing measure different intervals.

Startup profiling covers map/world/mission/AI preparation and the transition to first playable presentation.

Frame pacing measures the battle after it is in the playable measurement window and expects zero post-playable asset work under the performance gate.

Moving work from “first combat frame” into startup can therefore improve frame pacing, but startup profiling should show the corresponding cost rather than hiding it.

## Relationship to save/load

Loading a saved match and starting a fresh mission share some runtime preparation concerns—world replacement, renderer snapshot publication, derived-state rebuild—but they are not the same flow.

Mission startup begins from authored map/mission data. Save/load begins from a validated serialized world/session snapshot.

Both ultimately need a stable simulation state and fresh presentation state before the player can interact normally.

See [SAVE_LOAD_SYSTEM.md](SAVE_LOAD_SYSTEM.md).

## Startup tests

Startup tests use the authored `hold_the_sallow_ford` mission. It is small enough for inexpensive automated coverage while still containing an AI opponent and exercising the real mission startup contracts.

Coverage includes:

- shared map-context acquisition;
- parse/reuse statistics;
- preparation of initial AI decisions;
- AI readiness behavior; and
- loading-gate behavior needed before reveal.

`MissionStartupTest.ParsesTheMissionMapOnce` specifically locks the shared-map-context contract.

## Architectural invariants

The current mission startup path depends on these invariants:

- the map is parsed through the shared `MapContextStore`;
- unchanged map contexts can be reused;
- mission ownership/setup is established before final AI preparation;
- AI workers reason from snapshots rather than the mutable world;
- AI results keep deterministic due simulation updates;
- terrain scatter and initial AI decisions are the explicit current readiness blockers;
- the loading overlay has a bounded maximum wait; and
- startup profiling records enough phase/counter data to distinguish parsing, world construction, mission setup, and AI preparation.

## Source map

| Concern | Source |
| --- | --- |
| Startup orchestration | `app/core/game_engine.cpp` and runtime coordinator path |
| Map context/cache | `game/map/map_context.*` |
| Mission schema/setup | `game/map/`, mission runtime |
| AI initial preparation | AI system + game-engine startup integration |
| Terrain scatter readiness | renderer/scatter path exposed to `GameEngine` |
| Startup profiling | `Engine::Core::StartupProfiler` integration |
| Startup tests | mission startup tests including `MissionStartupTest.ParsesTheMissionMapOnce` |

The current startup contract is explicit: reuse the parsed map context, construct the world, apply mission ownership before final AI initialization, prepare the first AI decision batch during loading, and keep the overlay active while terrain scatter or initial AI decisions remain pending—subject to the 15-second maximum wait.
