# Mission Startup Architecture

A campaign mission has two distinct spans of time: **loading**, when the game prepares a stable first playable frame, and **playable time**, when the simulation is allowed to become the player's problem.

The startup architecture exists to keep expensive one-time work on the loading side of that boundary without changing deterministic simulation behavior. It also provides instrumentation for deciding which startup costs are actually worth optimizing.

## The loading boundary

`GameEngine::start_skirmish_internal()` raises the loading overlay and performs mission preparation in one deferred slot before `SkirmishRuntimeCoordinator::finalize_load()` clears `m_runtime.loading`.

The startup sequence is:

1. **`world.load`** — `LevelOrchestrator::load_skirmish()` parses the map, creates terrain and spawns, configures biome/scatter/fog rendering, initializes the minimap, rebuilds registries, and prewarms unit templates.
2. **`mission.commander_setup` / `mission.setup`** — registers owners, nations, teams, starting units and structures, wave metadata, mission stages, and commander speakers.
3. **`ai.initial_preparation`** — creates AI workers and profiles and prepares the first decision snapshot for every AI owner.
4. **`finalize_load()`** — allows the simulation to begin stepping while the loading overlay remains visible until the readiness gate is satisfied.

The last distinction matters: the simulation can already be running while the loading overlay is still present.

`GameEngine::mission_startup_pending_components()` reports anything that is still required for the first playable frame. The overlay is released only when that list becomes empty, or after the 15-second escape hatch logs the components that failed to become ready.

The current gate covers terrain-scatter GPU upload and initial AI decisions. New entries should be added only when the component is genuinely required for a stable first playable frame. The readiness gate is not a substitute for moving ordinary setup work into the loading phase.

## Parse the mission map once

`Game::Map::MapContextStore` in `game/map/map_context.h` is the shared owner of parsed mission-map data during a match.

A `MapContext` contains:

- an immutable `shared_ptr<const MapDefinition>`;
- the source path; and
- the resolved path.

Consumers request a map by path and receive the existing parsed context when possible. The store keeps the three most recent maps and reparses only when the file size or modification time changes, so edited maps are not served from a stale cache.

### Shared consumers

The following systems all reuse the same parsed map:

| Consumer                                                    | Why it needs the map                                      |
| ----------------------------------------------------------- | --------------------------------------------------------- |
| `Game::Map::load_match()`                                   | terrain, spawns, camera, environment, victory config      |
| `LevelOrchestrator::load_skirmish()`                        | resources, undead/vein/wildlife config, minimap           |
| `MissionSetupCoordinator::apply_mission_setup()`            | authored spawns, coordinate space, commanders             |
| `MissionSetupCoordinator::apply_skirmish_commander_setup()` | spawn anchors                                             |
| `build_pending_mission_waves()`                             | wave-entry coordinate conversion                          |
| `make_mission_position_to_world()`                          | mission-stage coordinate conversion                       |
| `commander_troops_for_map()`                                | commander ownership                                       |
| `AudioCoordinator::apply_mission_ambience()`                | ambience query terms                                      |
| `GameStateRestorer` / `SaveLoadCoordinator`                 | the same information on the save/load path                |

Before `MapContextStore`, those subsystems could each reopen and parse the same JSON. On a large authored map, repeated parsing was one of the clearest avoidable startup costs.

`MissionStartupTest.ParsesTheMissionMapOnce` protects the contract. It starts a mission and requires `MapContextStore::statistics().parses == 1`. A subsystem that independently opens the mission JSON will therefore fail the test instead of quietly adding another parse.

### Keep the startup test cheap

The startup tests use `hold_the_sallow_ford`, a standalone mission on the smallest authored map that still includes an AI opponent.

The invariants under test are not map-specific: the parsed context must be reused and initial AI decisions must be prepared during loading. Using a small representative map keeps those tests in the fast pull-request profile without weakening what they prove.

On `battle_of_ticino`, the same two tests measured roughly 4.7 and 4.9 seconds in a Debug build and approached the ten-second per-test budget in `scripts/check-test-speed.py` on shared runners. On the ford they take roughly 30 ms while checking the same contracts.

## Prepare the first AI decision during loading

`AISnapshotBuilder::build()` is an expensive world read. For one owner it gathers economy state, harvestable props, friendly and hostile units, component state, vision sources, strategic objectives, and a point grid.

Building that snapshot synchronously on the simulation thread during the opening seconds of play would front-load AI work into the exact period in which the player is first interacting with the mission.

The startup path moves only the **first** round of that work into loading.

### Initial preparation

`AISystem::prepare_initial_decisions(world)` builds every owner's first snapshot on the world-owning thread and submits each snapshot to the corresponding AI worker.

`await_initial_decisions()` waits for those worker jobs to finish. `GameEngine::prepare_mission_ai_state()` calls both functions before `finalize_load()`.

The resulting decisions still enter the simulation through the ordinary `process_results()` path a few updates into the match. AI behavior timing therefore remains the same; only the expensive snapshot construction has moved to a time when the loading overlay is still present.

Workers never read the live world. The ownership model remains single-source: the world-owning thread constructs a snapshot and transfers that immutable decision input to the worker.

### Stagger the second round, not the first

After loading, every AI owner has already completed one decision round. If all owner timers were reset to zero, the next round would arrive as one synchronized burst.

`prepare_initial_decisions()` instead initializes each timer to the negative of its normal stagger. Owner `i` therefore becomes due at:

```text
interval + stagger(i)
```

The first post-load round begins after the initial interval and remains spread across the following decision window.

`AISystem::update()` still uses the same submission path, factored through `submit_decision_job()`. Steady-state cadence, delta times, and worker latency semantics remain unchanged.

## Deterministic AI timing beats wall-clock timing

The simulation, not machine speed, decides when an AI result becomes visible.

`submit_decision_job()` stamps every decision with:

```text
job_due_update = m_update_count + k_decision_latency_updates
```

`process_results()` applies that result on exactly the due update. If the worker is late, the simulation waits through `AIWorker::wait_idle()` rather than slipping the decision to a later update.

That means a slow machine can pay more frame time, but it cannot produce a different command schedule from the same seed. The same plan applies on the same simulation tick.

`m_decision_wait_budget` is diagnostic only. `decisions_over_wait_budget()` counts waits that exceeded the threshold and `longest_decision_wait_us()` reports the longest observed wait. Neither value feeds back into simulation behavior.

`AIWorkerPool::enqueue()` also returns a job to `AIWorker::discard_pending_job()` when the pool is already stopping, ensuring that a blocking wait cannot outlive the worker pool expected to satisfy it.

## Avoid optimizations that change timing semantics

Snapshot-buffer recycling was tested because reusing the worker's previous allocation can make `AISnapshotBuilder::build()` cheaper.

The optimization worked mechanically, but it also made decision submission measurably faster. Before the apply tick was pinned deterministically, that changed when some AI commands landed. `CommanderDuelTest.CommanderArrowsCarryTheCommanderStyle` exposed the behavior difference through a changed number of signature volleys in a fixed twenty-second duel.

The measured startup benefit did not justify the semantic risk. `ai.initial_preparation` is already a small cost—about 0.26 ms for Ticino's three AI owners and 0.56 ms for Campania's four in the recorded captures—so allocation recycling is not where campaign startup time is spent.

The lesson is broader than this particular experiment: startup optimization should target measured bottlenecks, and deterministic timing is part of the behavioral contract.

## Reinitialize campaign AI once

Campaign startup once invoked `AISystem::reinitialize()` twice: first through `LevelOrchestrator`'s skirmish path and again in `MissionSetupCoordinator` after mission owners had been registered.

`load_skirmish()` now accepts `defer_ai_initialization`, which `GameEngine` enables for campaign missions. `prepare_mission_ai_state()` performs the eventual reinitialization after mission ownership is known.

It also checks that the resulting instance count matches the registered AI owners and reinitializes if necessary, preventing malformed mission data from leaving AI state incomplete.

## Startup instrumentation

`Engine::Core::StartupProfiler` in `game/core/startup_profiler.h` records a timeline for one mission start.

`ScopedStartupPhase` records named spans, and repeated spans with the same name accumulate. `add_counter()` records integer diagnostics. The profiler also marks the loading-overlay release and then samples presentation-frame times, producing distributions for the first frame, first second, and first five seconds of play.

Enable the text report with:

```sh
SOI_STARTUP_TRACE=1
```

A representative capture looks like this:

```text
SOI_STARTUP mission=:/assets/maps/map_battle_ticino.json
  phase audio.mission_preload = 7985.93 ms
  phase world.map_and_spawns = 2307.37 ms items=316
  phase world.map_systems = 67.54 ms
  phase world.registry_rebuild = 0.02 ms
  phase render.template_prewarm = 4287.48 ms
  phase world.load = 6682.23 ms
  phase audio.mission_ambience = 0.03 ms
  phase mission.commander_setup = 0.00 ms
  phase mission.pending_waves = 0.01 ms
  phase ai.reinitialize = 0.62 ms
  phase mission.registry_rebuild = 0.24 ms
  phase mission.setup = 5.03 ms
  phase ai.initial_preparation = 0.26 ms
  phase total = 21336.75 ms
  count ai.owners = 3
  count map.requests = 7
  count map.parses = 1
  count map.reuses = 6
  count world.units = 316
  overlay released at = 15806.42 ms
  first playable frame at = 15834.42 ms (11.96 ms)
  frames first 1s n=50 avg=7.64 p95=20.51 p99=58.57 worst=58.57
  frames first 5s n=220 avg=7.40 p95=17.81 p99=21.44 worst=58.57
```

This is a real capture of `second_punic_war/battle_of_ticino` on a contended developer machine, not a target budget.

`world.load` and `mission.setup` are outer spans that contain some of the named phases above them, so `phase total` double-counts nested work. Read the hierarchy rather than treating the total as a simple sum of every line.

### What the trace showed

For that capture, mission setup, AI reinitialization, and initial AI preparation together cost under 6 ms. Audio preload consumed roughly 8 seconds and template prewarm roughly 4.3 seconds.

Campania showed the same shape: expensive map/spawn and template-prewarm phases, while mission setup and initial AI work remained small.

The instrumentation therefore changed the optimization priority. The initial AI snapshots were not the startup bottleneck; the larger remaining problem was the first-second frame-time tail after the overlay lifted, which requires frame-level analysis rather than another startup-phase optimization.

Counters that require walking unit storage, such as `world.units` and the `items=` count on `world.map_and_spawns`, are collected only when a report is requested. Normal startup does not pay for those diagnostics.

## Machine-readable startup traces

Set:

```sh
SOI_STARTUP_TRACE_FILE=<path>
```

The profiler writes the same information as JSON: phases, counters, overlay release, first-frame time, and the frame-time distributions for the first second and first five seconds.

`SOI_STARTUP_TRACE` and `SOI_STARTUP_TRACE_FILE` are independent. Either one can be enabled alone.

`map.parses` is the duplicate-parse guard. `map.requests - map.parses` is the number of map requests served from the shared context.

## Work deliberately excluded from startup

### Future mission waves

Pending waves are only metadata at startup. Their units should spawn when the wave director reaches the authored ready time, often tens or hundreds of seconds later.

Pre-spawning those units merely to flatten a startup or runtime trace would change the mission and is therefore not a valid optimization.

### One shared multi-owner AI world snapshot

A single world snapshot from which every AI owner derives its view may be a useful long-term architecture because `AISnapshotBuilder` currently walks overlapping friendly and hostile sets once per owner.

Startup instrumentation does not justify that redesign, however: initial preparation costs well under a millisecond in the measured campaign examples. A change to the snapshot contract should be motivated by broader steady-state or scalability evidence rather than startup alone.

The mission-startup rule is therefore straightforward: perform first-frame prerequisites while the player is still loading, preserve deterministic simulation timing, and optimize the phases the profiler actually identifies as expensive.
