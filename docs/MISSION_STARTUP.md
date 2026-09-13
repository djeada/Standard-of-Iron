# Mission Startup Architecture

Mission startup separates preparation from playable simulation. Map parsing, world construction, mission setup, AI initialization, renderer preparation, and first-frame readiness are performed while the loading flow is active. The loading overlay is released only after its minimum display time and the runtime readiness conditions have been satisfied, subject to a bounded maximum wait.

## Startup sequence

`GameEngine::start_skirmish_internal()` begins the load and `SkirmishRuntimeCoordinator` drives the skirmish/mission setup path.

The main phases recorded by the startup profiler are:

1. **`world.load`** — map loading, terrain, authored entities, environment systems, minimap setup, registry rebuilds, and renderer-facing preparation.
2. **`mission.commander_setup` / `mission.setup`** — mission owners, nations, teams, starting forces, objectives, waves, stages, and commander data.
3. **`ai.initial_preparation`** — AI instances and their initial decision jobs.
4. **runtime finalization** — simulation stepping begins and the loading overlay remains until first-frame readiness is satisfied.

The simulation can therefore be active during the final part of the loading overlay.

## First-frame readiness gate

`GameEngine::mission_startup_pending_components()` is the source of truth for mission components that still block the overlay.

The current implementation reports two conditions:

- **terrain scatter** — present when `m_scatter` exists and `is_gpu_ready()` is false;
- **AI initial decisions** — present when an `AISystem` exists and `initial_decisions_ready()` is false.

The overlay also has a maximum wait of 15 seconds (`k_loading_overlay_max_wait_ms = 15000`). When that limit is reached, the engine logs the components that are still pending instead of holding the loading screen indefinitely.

## Shared map context

`Game::Map::MapContextStore` is the shared parser/cache for mission map definitions.

`MapContextStore::acquire()` resolves the map path and caches a parsed `MapDefinition` using:

- resolved path;
- file size; and
- modification timestamp.

The cache retains the three most recent maps (`k_retained_maps = 3`). A matching file stamp reuses the existing immutable `shared_ptr<const MapDefinition>`; a changed file is parsed again.

The store also exposes request, parse, and reuse counters through `MapContextStore::statistics()`.

`MissionStartupTest.ParsesTheMissionMapOnce` verifies that a mission startup uses the shared context rather than independently reparsing the same map for each subsystem.

## AI preparation during loading

`GameEngine::prepare_mission_ai_state()` prepares the first AI decisions before the mission becomes fully playable.

For campaign missions, the skirmish-level AI initialization can be deferred through `defer_ai_initialization`. Once mission owners are registered, the game initializes the AI for that owner set and calls:

```cpp
ai_system->prepare_initial_decisions(*m_world);
```

The engine then waits up to 1500 ms for that initial batch:

```text
k_initial_decision_budget = 1500 ms
```

If the batch is still running after that budget, startup logs the condition. The loading-overlay readiness gate continues to report `AI initial decisions` until `initial_decisions_ready()` becomes true or the overlay reaches its own maximum wait.

### Worker ownership

AI workers do not read the live world. The world-owning thread builds each AI snapshot and submits that snapshot to the worker. This keeps simulation state ownership deterministic while still allowing AI computation to run off-thread.

### Deterministic application tick

AI decision jobs carry a simulation update at which their result is due:

```text
job_due_update = m_update_count + k_decision_latency_updates
```

`process_results()` applies a result on that simulation update. If the worker is late, the simulation waits for it rather than moving the decision to a different tick. Machine speed can therefore change frame cost without changing the command schedule produced from the same simulation state.

## Startup profiling

`Engine::Core::StartupProfiler` records named phase durations, counters, loading-overlay release, the first playable frame, and frame-time distributions immediately after startup.

Set:

```sh
SOI_STARTUP_TRACE=1
```

to emit a text report.

Set:

```sh
SOI_STARTUP_TRACE_FILE=<path>
```

to write the report as structured output for tooling. The two environment variables are independent.

Useful counters include:

- `map.requests`;
- `map.parses`;
- `map.reuses`;
- `ai.owners`; and
- `world.units` when the trace path requests that count.

`map.parses` is the direct duplicate-parse signal. `map.requests - map.parses` is the number of map requests satisfied by an existing parsed context.

## Startup tests

The startup tests use `hold_the_sallow_ford`, a small authored mission that still includes an AI opponent. The map keeps the tests inexpensive while exercising the same startup contracts as the larger campaign battlefields.

Coverage includes:

- map-context reuse and parse counts;
- preparation/readiness of the initial AI decision batch; and
- the startup behavior needed by the loading gate.

The current startup contract is therefore explicit: reuse one parsed map context, initialize mission ownership before final AI setup, prepare the first AI decision batch while loading, and keep the overlay up while terrain scatter or initial AI decisions are not ready.
