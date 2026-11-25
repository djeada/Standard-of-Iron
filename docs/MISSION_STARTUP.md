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
        ├─ deferred unit templates drained?
        ├─ mission audio decoded?
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

### Navigation prewarm

`world.nav_prewarm` builds the navigation grid, the forest and world-prop indexes and the
region labels for every passability before the templates are warmed. Without it the
first path query of the match paid for all of that (about 30 ms on Ticino) inside a
simulation tick. `NavGrid::prewarm()` calls `Pathfinding::prewarm_navigation()`; later
edits still go through the normal dirty-region rebuild.

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

See [MISSION_FRAMEWORK.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/MISSION_FRAMEWORK.md) for the schema and runtime ownership of those features.

## Commander setup ordering

Commander and owner configuration must exist before systems that depend on final mission ownership are initialized.

That is why mission startup separates general world creation from mission-specific owner/commander setup and final AI preparation.

AI cannot build a correct first snapshot for an owner that has not yet been registered into the match.

## AI initialization

`SkirmishRuntimeCoordinator::prepare_ai_state()` (called from `GameEngine::configure_loaded_match()`) prepares the first AI decision work during loading.

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

See [AI_ARCHITECTURE.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/AI_ARCHITECTURE.md).

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

The current implementation reports four pending conditions. The last two exist because
anything still lazy at reveal is paid for in the first seconds of play, on the render
thread, where it shows up as frame spikes rather than as loading time.

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

### Unit templates

Reported when `Renderer::has_pending_template_prewarm()` is true: the extended template
set that `prewarm_unit_templates` queues after the core set has not been worked through
yet. While the overlay is up, `process_async_template_prewarm` drops its per-frame item
cap and spends up to 12 ms a frame on the queue, because nothing the player can see
depends on those frames. After reveal it falls back to the preset's small budget. Until
the queue drains, runtime creature bakes are still allowed, so a cache miss after reveal
would bake synchronously inside a draw; draining first is what makes
`runtime_bake_forbidden()` true for the whole match.

### Mission audio

Reported when `AudioSystem::has_pending_mission_decodes()` is true: the decode worker
still has queued or in-flight tracks. Music and ambience beds queue behind effects, and a
bed that is still decoding when the battle starts plays late and competes with the first
frames for CPU.

## Loading-overlay maximum wait

Both waits start after world preparation. The renderer must complete five
gameplay frames before it can reveal the battlefield. Pending startup components
can delay the reveal for up to 15 seconds after preparation. When that limit is
reached, the engine logs the remaining components and reveals the battlefield
once the frame requirement is met.

A separate 60-second timer on the GUI thread bounds the wait when the renderer
produces zero frames or too few frames. Minimizing the window pauses this timer.
When the window is restored, the timer resumes with the remaining time. Frames
from an abandoned load cannot advance the next load. In software rendering mode,
a frame counts once the CPU image has been blitted into the gameplay framebuffer.

If the timer expires, the game reports the last loading stage and returns to the
menu after safely clearing the match. A renderer failure ends the attempt
immediately. The interactive error offers a Quit action if graphics failed or
the engine could not safely clear the stalled match. Direct mission and replay
launches log the error and return a failure exit code.

If the renderer disappears between scheduling a load and completing it,
`complete_match_load` takes the same failed-load cleanup, so the loading flag, the
overlay and its 60-second timer are all cleared with the attempt.

## Windows graphics startup supervision

A default Windows launch (no `QT_OPENGL`, no `--force-software`/`--quality=none`,
no `SOI_NO_STARTUP_SUPERVISOR=1`) runs the game as a child of a small supervisor
process (`app/bootstrap/startup_supervisor.*`). The child tries the graphics
driver first. It reports its progress through a status file: `native_probe`,
`qt_application`, `qml_load`, `scene_graph`, `gameplay_renderer`, and finally
`ready` when `GLView::renderer_ready` fires, which is the gameplay renderer's own
first-frame milestone.

If an attempt does not become ready, the supervisor gives it at most one fresh
attempt with the bundled Mesa runtime (`QT_OPENGL=software`, which needs the
adjacent `opengl32sw.dll`). A failed attempt is one of: the native probe was
rejected (exit 20), the process crashed (including a driver crash inside the
probe), Qt or the gameplay renderer failed before readiness (exit 3 or 21), or
the attempt did not become ready within 90 seconds, in which case it is killed.
A launch that reached readiness is never retried; the supervisor passes through
the child's exit code. If Mesa is missing or fails too, one dialog names each
attempt's failure stage and the startup log, and the launch exits.

`%LOCALAPPDATA%\standard_of_iron\startup.log` (or the file named by
`SOI_STARTUP_LOG_PATH`) is rewritten on every supervised launch. It records the
backend and outcome of each attempt and the child's log up to readiness,
including the OpenGL vendor, renderer, version and profile. `LAUNCH.bat` waits
for the launch and reports its exit code instead of looking for a process by
name. Explicit overrides (`QT_OPENGL`, the debug scripts) skip the supervisor.

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

The startup gate does not wait for every possible renderer cache in the application. It waits on the explicit current readiness contract exposed by terrain scatter, AI initial decisions, the deferred unit templates and the mission audio decodes.

See [RENDERING_ARCHITECTURE.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/RENDERING_ARCHITECTURE.md) for the rendering ownership model.

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

Startup may have released before expensive presentation work was completely prewarmed. Compare startup traces with [FRAME_PACING.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/FRAME_PACING.md) and post-playable asset-work counters.

## Relationship to frame pacing

Startup and frame pacing measure different intervals.

Startup profiling covers map/world/mission/AI preparation and the transition to first playable presentation.

Frame pacing measures the battle after it is in the playable measurement window and expects zero post-playable asset work under the performance gate.

Moving work from “first combat frame” into startup can therefore improve frame pacing, but startup profiling should show the corresponding cost rather than hiding it.

## Relationship to save/load

Loading a saved match and starting a fresh mission share some runtime preparation concerns—world replacement, renderer snapshot publication, derived-state rebuild—but they are not the same flow.

Mission startup begins from authored map/mission data. Save/load begins from a validated serialized world/session snapshot.

Both ultimately need a stable simulation state and fresh presentation state before the player can interact normally.

See [SAVE_LOAD_SYSTEM.md](https://github.com/djeada/Standard-of-Iron/blob/main/docs/SAVE_LOAD_SYSTEM.md).

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
- terrain scatter, initial AI decisions, deferred unit templates and mission audio decodes are the explicit current readiness blockers;
- the loading overlay has a bounded maximum wait; and
- startup profiling records enough phase/counter data to distinguish parsing, world construction, mission setup, and AI preparation.

## Source map

| Concern                   | Source                                                                       |
| ------------------------- | ---------------------------------------------------------------------------- |
| Startup orchestration     | `app/core/game_engine.cpp` and runtime coordinator path                      |
| Map context/cache         | `game/map/map_context.*`                                                     |
| Mission schema/setup      | `game/map/`, mission runtime                                                 |
| AI initial preparation    | AI system + game-engine startup integration                                  |
| Terrain scatter readiness | renderer/scatter path exposed to `GameEngine`                                |
| Startup profiling         | `Engine::Core::StartupProfiler` integration                                  |
| Startup tests             | mission startup tests including `MissionStartupTest.ParsesTheMissionMapOnce` |

Mission startup reuses the parsed map context, constructs the world, and applies
mission ownership before final AI initialization. The overlay waits for gameplay
frames and gives terrain scatter, initial AI decisions, deferred unit templates,
and mission audio up to 15 seconds after preparation.
