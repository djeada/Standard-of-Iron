# Architecture

Standard of Iron separates simulation authority from application orchestration and presentation. The build graph enforces most of that separation directly: lower-level libraries cannot reach upward without creating a forbidden link dependency, and CI checks selected dependency and documentation claims against the repository.

## Layering

The main runtime shape is:

```text
animation / scene
        │
   engine_core
        │
   domain libraries
        │
     game_sim
       ├── soi_ai
       ├── soi_missions / soi_campaign
       ├── soi_persistence / soi_runtime
       ├── game_view
       └── render_gl
                │
             app_core
                │
        standard_of_iron
```

`engine_core` provides the ECS and low-level world/session primitives. Domain libraries add units, navigation, formations, economy, combat, map behavior, wildlife, and related simulation services. `game_sim` is the simulation-facing composition layer used by the live application and headless tools.

Application code owns Qt/QML-facing controllers and orchestration. Rendering consumes simulation snapshots and view data instead of becoming the owner of gameplay state.

## Render dependency boundary

The root CMake graph is explicit: `render_gl` links `game_sim`.

It does not link the higher AI, mission, campaign, or persistence facades merely because those systems can eventually produce something visible. Tools that need only the production simulation and renderer can therefore link the kernel without pulling in save databases or campaign orchestration.

`game_view` contains camera/picking/minimap-facing services that are not part of the authoritative simulation kernel.

## Session authority

Per-match state lives in `Game::Session::SessionContext`.

A session owns or exposes the authoritative state used by the match, including:

- the `World`;
- terrain/map services;
- player ownership and nation state;
- economy state;
- simulation clock;
- deterministic RNG;
- command/replay state; and
- registered runtime systems.

Subsystems that need match state should resolve it from the world/session relationship instead of inventing process-global state.

`Game::Session::SessionSnapshot` is the shared contract for authoritative non-entity state that must survive save/load and deterministic replay boundaries. Individual systems register contributors rather than making the persistence layer know every subsystem's internal fields.

## World-to-session binding

`Game::Session::services_for(world)` resolves the services associated with a world.

Worlds created inside a `SessionContext` have an explicit binding. A world created outside a session can still fall back to the ambient session; this compatibility path is instrumented and can be made fatal with:

```sh
SOI_STRICT_WORLD_BINDING=1
```

or programmatically with `Game::Session::set_strict_world_binding(true)`.

`SessionServiceOwnershipDeathTest.StrictBindingRefusesAWorldWithNoSession` covers the strict path.

## Command pipeline

Player input, AI, replay playback, and scripted systems converge on typed commands rather than mutating gameplay systems through separate control paths.

`CommandQueue` is the match command boundary. Accepted commands are part of the deterministic command stream used by replay recording and verification.

The fixed simulation step validates and dispatches commands before the systems that consume their effects. Replay playback can therefore replace live command producers while exercising the same simulation path.

## Fixed simulation and presentation

Simulation advances on a fixed tick. Rendering and UI presentation can run at a different cadence.

Authoritative systems must derive gameplay timing from simulation ticks/state rather than presentation frame time. Renderer-facing state is published through snapshots so drawing does not need to own or mutate the live world.

This separation is used by:

- the live Qt application;
- `soi_headless`;
- deterministic replay verification;
- `balance_sim`;
- Arena scenarios; and
- performance/determinism tooling.

## Persistence boundary

World entities are serialized through `game/save/serialization.*`. Non-entity authoritative subsystem state is captured through `Game::Session::SessionSnapshot`.

`game/save/snapshot_contract.*` classifies data as:

- authoritative serialized;
- derived/rebuilt;
- presentation-only; or
- campaign-level.

This keeps transient caches and presentation effects from becoming accidental save-format authority.

See [SAVE_LOAD_SYSTEM.md](SAVE_LOAD_SYSTEM.md) for the storage, migration, compression, integrity, and restoration path.

## Architecture enforcement

Architecture rules are checked in more than one way.

### Link boundaries

CMake target dependencies make many layer violations impossible without changing the declared build graph.

### Source-policy checks

`scripts/check-pr-policy.py` runs compiler-free architecture/source checks as part of the pull-request policy lane.

### Documentation accuracy

`scripts/check-architecture-doc.py` checks architecture claims that have machine-readable sources. It currently verifies:

- ambient session-access counts against `scripts/ambient_instance_budget.json`;
- full-world scan counts against `scripts/world_scan_budget.json` and `scripts/world_scan_nested_allow.json`; and
- the public dependency that `render_gl` has in the root `CMakeLists.txt`.

A numeric architecture claim with a machine-readable source must agree with that source in the same change.

## Known limitations

The limitations in this section are repository-backed rather than roadmap estimates.

**56 call sites still reach per-match state through the ambient access path:** `app/world` 2, `game/core` 1, `game/formation` 4, `game/map` 14, `game/systems` 22, `game/units` 9, `game/visuals` 1, `game/wildlife` 3. `scripts/ambient_instance_budget.json` is the source of truth, and `scripts/check-architecture-doc.py` fails when this sentence disagrees with it.

**74 full-world entity scans remain, of which 0 are allow-listed as loop-nested scans.** `scripts/world_scan_budget.json` and `scripts/world_scan_nested_allow.json` are the source of truth for those counts, and the same documentation check verifies them.

**Unbound worlds still have a compatibility fallback to the ambient session.** `services_for(world)` reports the unbound lookup and returns ambient services unless strict world binding is enabled. The strict environment/configuration paths above turn the same condition into a fatal error.

These are current implementation constraints. They are not a list of proposed features or a schedule for future work.

## Related architecture references

- [RENDERING_ARCHITECTURE.md](RENDERING_ARCHITECTURE.md) — render preparation, creature assets, and backend behavior.
- [AI_ARCHITECTURE.md](AI_ARCHITECTURE.md) — AI snapshots, workers, strategies, and command production.
- [FORMATION_ARCHITECTURE.md](FORMATION_ARCHITECTURE.md) — unit/army formation planning and runtime state.
- [SAVE_LOAD_SYSTEM.md](SAVE_LOAD_SYSTEM.md) — persistence and snapshot boundaries.
- [MISSION_STARTUP.md](MISSION_STARTUP.md) — map-context reuse, startup readiness, and initial AI preparation.
