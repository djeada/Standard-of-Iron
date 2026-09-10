# Mission startup

A campaign mission has two distinct spans of time: **loading**, which prepares the
mission, and **playable time**, which runs it. Everything a mission needs in order to
deliver a stable first frame belongs in the first span. This document describes the
boundary, the pieces that were on the wrong side of it, and the instruments that keep
them there.

## The boundary

`GameEngine::start_skirmish_internal()` raises the loading overlay, then does the whole
of mission preparation inside a single deferred slot before
`SkirmishRuntimeCoordinator::finalize_load()` clears `m_runtime.loading`:

1. `world.load` — `LevelOrchestrator::load_skirmish()`: map parse, terrain, spawns,
   biome/scatter/fog renderers, minimap, registry rebuilds, unit template prewarm.
2. `mission.commander_setup` / `mission.setup` — owners, nations, teams, authored
   starting units and buildings, wave metadata, mission stages, commander speakers.
3. `ai.initial_preparation` — AI workers, profiles, and the first decision snapshot for
   every AI owner.
4. `finalize_load()` — the simulation starts stepping; the overlay stays up until the
   readiness gate in `GameEngine::update_loading_overlay()` is satisfied.

The simulation _does_ run for the last stretch of the overlay, which is why the readiness
gate matters: `GameEngine::mission_startup_pending_components()` names what is still
missing, and the overlay only lifts when that list is empty (or after the 15 s escape
hatch, which logs what it gave up on). Today the list covers terrain scatter GPU upload
and AI initial decisions. Anything added there must be genuinely required for the first
playable frame — the gate is not a place to hide work that should have finished earlier.

## One parse of the map

`Game::Map::MapContextStore` (`game/map/map_context.h`) is the only place a mission map is
parsed from JSON during a match. It hands out a `MapContext`: an immutable
`shared_ptr<const MapDefinition>` plus the source and resolved paths. Callers ask for a
map by path and get the already-parsed one back; the store keeps the three most recent
maps and re-parses only when the file's size or modification time has changed, so an
edited map is never served stale.

Before this existed, a campaign start parsed the same map six or seven times: the match
loader, the minimap/environment configuration, mission setup, the skirmish commander
setup, the wave builder, the mission-stage coordinate helper, the commander lookup, and
the audio ambience picker each opened the file for themselves. On a large authored map
that is the single largest avoidable cost in the load.

The consumers now share one parse:

| Consumer                                                    | What it needs the map for                                |
| ----------------------------------------------------------- | -------------------------------------------------------- |
| `Game::Map::load_match()`                                   | terrain, spawns, camera, environment, victory config     |
| `LevelOrchestrator::load_skirmish()`                        | starting resources, undead/vein/wildlife config, minimap |
| `MissionSetupCoordinator::apply_mission_setup()`            | authored spawns, coordinate space, commanders            |
| `MissionSetupCoordinator::apply_skirmish_commander_setup()` | spawn anchors                                            |
| `build_pending_mission_waves()`                             | wave entry coordinate conversion                         |
| `make_mission_position_to_world()`                          | mission-stage coordinate conversion                      |
| `commander_troops_for_map()`                                | which force fields which commander                       |
| `AudioCoordinator::apply_mission_ambience()`                | ambience query terms                                     |
| `GameStateRestorer` / `SaveLoadCoordinator`                 | the same, on the save/load path                          |

`MissionStartupTest.ParsesTheMissionMapOnce` is the guard: it drives a mission start and
fails if `MapContextStore::statistics().parses` is anything but 1. A new subsystem that
opens the map JSON itself will fail that test rather than quietly costing another parse.

Both `MissionStartupTest` cases load `hold_the_sallow_ford`, a standalone mission on the
smallest authored map that still fields an AI opponent, rather than a campaign
battlefield. Neither invariant is map-specific — the store is either reused or it is not,
and the AI either prepared its snapshots during loading or it did not — but the map is what
the test pays for. On `battle_of_ticino` the two cases measured 4.7 s and 4.9 s in a Debug
build, the two slowest tests in the whole fast pull-request profile and close enough to the
ten-second per-test budget in `scripts/check-test-speed.py` that a shared runner went over
it. On the ford they measure about 30 ms each and assert exactly the same things.

## AI initial state

`AISnapshotBuilder::build()` walks the whole world for one owner: economy, harvestable
props, friendly units and a dozen components each, hostile contacts, vision sources and a
point grid, strategic objectives. It is not a cheap call, and `AISystem::update()` used to
make it synchronously on the simulation thread the first time each AI came due — which,
with the initial timers staggered across the update interval, landed N of them inside the
first few hundred milliseconds of play.

Two changes moved that work:

- **Prepared during loading.** `AISystem::prepare_initial_decisions(world)` builds every
  owner's first snapshot and submits it to that owner's worker;
  `await_initial_decisions()` blocks until the workers are done. `GameEngine`
  calls both from `prepare_mission_ai_state()`, before `finalize_load()`. The decisions
  themselves are applied by the normal `process_results()` path a few updates into the
  match, so nothing about AI behaviour timing changes — only where the snapshot was built.
- **Staggering moved to the second round.** Every owner has just decided, so resetting all
  the timers to zero would make the next round arrive as one burst. `prepare_initial_decisions()`
  instead seeds each timer with the _negative_ of its stagger, which puts owner `i`'s next
  decision at `interval + stagger(i)`: past the first update interval of play, and still
  spread across the interval after that.

The ownership model is unchanged and there is no second source of truth: a snapshot is
built on the thread that owns the world and moved into the worker. Workers never read the
world.

`AISystem::update()`'s submit path is unchanged apart from being factored into
`submit_decision_job()`, which `prepare_initial_decisions()` shares — the steady-state
decision cadence, delta times and job latency are the same as before.

**The apply tick is chosen by the simulation, not by the clock on the wall.**
`submit_decision_job()` stamps the job with `job_due_update = m_update_count +
k_decision_latency_updates`, and `process_results()` applies it on exactly that update: it
blocks on `AIWorker::wait_idle()` until the result exists rather than giving the worker a
wall-clock budget and slipping the job to a later update when it overruns. A busy machine
therefore costs frame time, never a different decision — the same seed and the same
commands apply the same plan on the same tick on every machine.
`m_decision_wait_budget` survives only as a diagnostic threshold:
`decisions_over_wait_budget()` counts the waits that exceeded it, and
`longest_decision_wait_us()` reports the worst one. Neither reading feeds back into the
simulation. `AIWorkerPool::enqueue()` hands a job straight back to
`AIWorker::discard_pending_job()` when the pool is already stopping, so a blocking wait
can never outlive the pool that was supposed to satisfy it.

**Buffer recycling was tried and rejected.** Handing the spent snapshot back from the
worker so the next `build()` could refill its vectors is the obvious third change, and it
works, but it makes each decision measurably quicker to submit. Before the apply tick was
pinned that showed up as AI commands landing a tick or so earlier, which
`CommanderDuelTest.CommanderArrowsCarryTheCommanderStyle` detects: the AI-ordered rival
staggers the commander sooner and it looses two signature volleys in twenty seconds
instead of three. The measured prize was not worth it — `ai.initial_preparation` costs
0.26 ms for Ticino's three AI owners and 0.56 ms for Campania's four, so snapshot
allocation is not where campaign startup spends its time.

`AISystem::reinitialize()` used to run twice per campaign start — once in
`LevelOrchestrator` for the skirmish case and again in `MissionSetupCoordinator` after the
mission's owners exist. `load_skirmish()` now takes `defer_ai_initialization`, which
`GameEngine` sets for campaign missions; `prepare_mission_ai_state()` re-runs
`reinitialize()` itself if the instance count does not match the registered AI owners, so
a malformed mission definition cannot leave the AI uninitialised.

## Instrumentation

`Engine::Core::StartupProfiler` (`game/core/startup_profiler.h`) accumulates a phase
timeline for one mission start. `ScopedStartupPhase` records a named span; phases with the
same name accumulate. `add_counter()` records integers. The profiler also records the
overlay release timestamp and, from that point, per-frame presentation times, from which
it derives first-frame, first-1-second and first-5-second distributions.

Set `SOI_STARTUP_TRACE=1` to have the report logged about five seconds after the overlay
lifts:

```
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

That is a real capture of `second_punic_war/battle_of_ticino` on a contended developer
machine, not an illustration. `world.load` and `mission.setup` are outer spans that contain
the phases listed above them, so the `phase total` line double-counts; read the nesting,
not the sum.

Read it and the picture the issue assumed changes. On this mission the whole of mission
setup, AI reinitialisation and AI initial preparation together cost under 6 ms, against
7986 ms of audio preload and 4287 ms of unit-template prewarm. `map.reuses = 6` is the
six JSON parses the shared context removed. The same shape holds on Campania, the largest
authored map: `world.map_and_spawns` 3090 ms, `render.template_prewarm` 5625 ms,
`mission.setup` 15 ms, `ai.initial_preparation` 0.56 ms for four AI owners.

So the AI snapshots the issue nominated as the likely cause of the startup hitch were
worth well under a millisecond, and the phases that dominate a campaign start were already
inside the loading phase. What remains after this change is a first-second frame-time tail
(Ticino p99 58 ms against a 7.6 ms average; Campania p99 119 ms against 42 ms) that the
phase timings do not explain, because it happens after the overlay lifts. Attributing that
is the next piece of work and needs the frame-level instrument, not the phase one.

The two counters that have to walk the unit storage to fill (`world.units` and the `items=`
on `world.map_and_spawns`) are only collected when a report is actually wanted, so a plain
run pays nothing for them. They count through a component view rather than
`collect_entities_with`, so neither materialises a vector of every unit.

`SOI_STARTUP_TRACE_FILE=<path>` writes the same report as JSON — phases, counters, the
overlay-release and first-frame timestamps, and both frame-time distributions — which is
the form a benchmark harness should collect as an artifact. The two variables are
independent: either one alone produces its output.

`map.parses` is the duplicate-parse detector. `map.requests` minus `map.parses` is how
many consumers were served from the shared context.

## What is deliberately not here

- Wave spawning. Pending waves are metadata at startup; the units appear when the wave
  director reaches the authored ready time, which for most missions is tens or hundreds of
  seconds in. A spawn spike is a real cost but a separate one, and moving spawns earlier to
  flatten a trace would change the mission.
- A shared world snapshot that all AI owners derive from. It is the right long-term shape
  (`AISnapshotBuilder` walks the same friendly/enemy sets once per owner), but the
  instrumentation says it would buy well under a millisecond at startup, which is not what
  a change to the snapshot contract should be justified by.
