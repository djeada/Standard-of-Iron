# Architecture

This document describes the layering the code actually has, where each boundary
is enforced, and what is deliberately still open. It is the reference the README
summarises; when the two disagree, this file is wrong and should be fixed.

## Layers

```
animation/  scene/          leaf libraries: plain data, no dependencies on anything below
accessibility_runtime       team identity and motion settings, shared by both ends
soi_software_raster         the CPU triangle rasteriser, used by render_gl and three tools
soi_audio_mastering         the limiter/loudness chain, used by audio_system and the preview tool
     |
engine_core                 ECS: entities, components, world, the ambient session binding
     |
soi_world                   unit catalogues, registries, terrain and maps
     |
soi_navigation              nav grid, pathfinding, spatial index, walls and gates
     |
soi_units                   the unit and building factories, the map-to-world spawner
     |
soi_formations              unit layouts, army formations, formation/structure geometry
     |
soi_movement                movement, local avoidance, order and command services
     |
soi_economy                 production, gathering, delivery, capture, the marketplace
     |
soi_combat                  damage pipelines, projectiles, healing, guard and patrol
     |
soi_wildlife                the ambient creatures
     |
game_sim                    the session, the command pipeline, match-level systems
     |
     +-- soi_campaign       the campaign definition
     |        |
     |        +-- soi_missions      mission content, victory rules, the map/mission catalogue
     |        +-- soi_persistence   the snapshot contract, the save format, the save database
     |
     +-- soi_ai             the computer opponent
     |        |
     |        +-- soi_runtime           which systems a match owns and in what order they tick
     |        +-- soi_mission_runtime   running a mission: forces, waves, campaign progress
     |
     +-- game_view          gameplay services that need a camera
     |
render_gl                   the renderer, consumes simulation state
     |
app_core                    composition root and view models
     |
standard_of_iron            executable: main(), QML registration
```

Dependencies point downwards only. Each arrow is a CMake link edge, so a
violation is a build failure rather than a review comment.

`game_sim` links every kernel target below it PUBLIC, so linking `game_sim`
still means "the whole simulation kernel". `game_systems` is an INTERFACE target
that pulls in the whole gameplay stack — kernel, AI, missions, save stack — at
once. It exists for consumers that genuinely want all of it — the renderer, the
arena. New code should name the domains it uses instead: linking `game_systems`
puts the AI and the save database into a binary that may need neither, and the
link line stops being a statement about what that binary depends on.

The names used in this document are the target names. `soi_components`,
`soi_simulation`, `soi_render_bridge`, `soi_render_gl` and `soi_app` are CMake
aliases for `engine_core`, `game_sim`, `game_view`, `render_gl` and `app_core`
respectively; either spelling links the same archive.

### Inside the kernel

Each kernel target is a layer, and its link line is the layering: a target links
the layers below it and nothing else, so a file that starts including a header
from a layer above fails to link. `scripts/module_rules.json` is the same map at
header granularity — `domain_types`, `components`, `unit_catalog`, `registries`,
`world`, `navigation`, `units`, `formations`, `movement`, `economy`, `combat`,
`wildlife`, `simulation`, `session` — with the direction each is allowed to
point in, and `scripts/check-modules.py` enforces it on every build. It is finer
than the linker in two ways: it sees an include that only pulls in inline code,
and it separates the modules that still share one archive (`unit_catalog`,
`registries` and `world` in `soi_world`; `simulation` and `session` in
`game_sim`). It also fails if `game/CMakeLists.txt` puts a source in a target
its module does not name, so the two descriptions cannot drift.

There is no tolerated backlog. The map used to carry a `baseline` of wrong-way
edges that was ratcheted down as modules were lifted out; the last of them went
when navigation, formations, movement, economy, combat and wildlife became
archives, and `tests/architecture/module_boundary_test.cpp` fails if the key
comes back.

Three of the modules need a word.

`domain_types` is the vocabulary: enumerations and plain value types with no
dependencies of their own, scattered across `game/systems`, `game/units`,
`game/formation` and `game/wildlife` by directory but belonging to no layer,
because every layer names them. `component.h` storing a `SpawnType` is a struct
holding a value, not the ECS reaching into gameplay. Formation roles and army
formation types are here for the same reason: a nation names its doctrine, so
the type has to sit below the registries.

`session` is at the _top_, not the bottom. `SessionContext` owns the terrain,
the fog, the registries and the command queue by value, so everything it
contains is below it — and a registry reaching back up to
`SessionContext::active()` to find itself would be a cycle. The `instance()`
accessors therefore resolve through `game/core/ambient_session.h`, a leaf in
`engine_core` that holds only forward declarations and a struct of pointers.
The session fills that struct in when it becomes active; `OwnerRegistry::instance()`
is defined beside `OwnerRegistry` and reads it. Nothing below `game_sim`
includes `game/session/session_context.h`.

`movement` sits _above_ `formations`, and `navigation` sits _below_ `units`.
Moving a unit is formation-aware — a formation's turn radius and speed
multiplier shape the move — while a formation is planned on the nav grid and
a wall segment registers itself with the wall network as it is spawned. The
formation runtime therefore plans and never moves anyone: when a replan leaves
new slot positions behind it raises `moves_pending` on the formation, and
`FormationMoveDispatchSystem` in the movement layer turns that into orders.

### `game_sim` — the simulation kernel

Everything a match needs to run with nothing on screen: ECS systems, the
session, the command pipeline, terrain, pathfinding, units, world serialisation.

It links the kernel layers, `engine_core` and `animation_core` and nothing
else. In particular it does **not** link `scene_core` or `render_gl`. That is what makes headless
balance runs, deterministic replay and a dedicated server possible, and it is
enforced three ways:

- the CMake target simply does not have those libraries available;
- `tests/architecture/layering_test.cpp` fails on an include of `scene/camera.h`
  from anywhere outside the view layer, and on any include of `app/` from
  `game/`;
- `bin/simulation_tests` links only `engine_core` and `game_sim` while covering
  most of the gameplay suite, so a new dependency breaks its link step.

`Qt::Gui` is present for the value types (`QVector3D`, `QImage`). It brings in no
windowing or GL usage of our own.

The kernel holds no draw data. `RenderableComponent` carries a `renderer_id` and
a `visible` flag — that an entity is drawn, and which asset draws it — and
nothing about how it looks. It used to carry a mesh kind, a mesh path, a texture
path and an RGB triple, which is why twenty unit factories, the capture system
and the wall planner all set appearance at spawn and on ownership change, and
why every save file recorded it. The renderer derives what it needs from the
owner and the spawn type instead (`render/entity_appearance.h`). The mesh kind
went entirely: the only values production code ever assigned resolved to the same
primitive, so it selected nothing.

`game/formation/` holds the two formation layers. They are deliberately
separate: `UnitLayoutSystem` owns soldier offsets inside a single troop entity
and is the only thing the renderer reads, while `ArmyFormationPlanner` and
`ArmyFormationRegistry` own multi-unit deployment and are the only things
player commands and AI reach for. See
[docs/FORMATION_ARCHITECTURE.md](FORMATION_ARCHITECTURE.md).

### `soi_mission_runtime` — running a mission

Setting a mission up, spawning the forces its definition names, configuring its
AI opponents, building and firing its attack waves, and recording campaign
progress. `TutorialDirector` is not here: it reads mission content and reports
step changes, so it stayed in `soi_missions` with the rest of the read side.

It lived in `app/mission/` until a layering audit asked what in it was actually
a client concern. Nothing was: no file named a renderer, a camera, a QML type or
a view model. Two edges held it there, and both were inverted rather than
tolerated:

- `MissionSetupCoordinator` wrote the client's HUD counters directly as a side
  effect of setting a mission up. It reports `rebuild_entity_cache` now and the
  client recomputes them, which is the same shape the view models use.
- The registry rebuild a load or a setup needs lived in `app/persistence`. It is
  `game/systems/world_restore.h` now — not in the save stack either, because a
  mission setup needs it and never touches a save file.

It sits above `soi_persistence` and `soi_ai`: a definition names its opponents
with a strategy and a personality, and a campaign records which missions are
complete. Neither module names a mission, so both edges are one-way. _Reading_
mission content needs neither, which is why that half stayed in `soi_missions`
below — and why `campaign_tests` can still check the shipped files without a
match running.

What stayed in `app/mission/` is the part that only makes sense with a player
watching: turning wave effects into announcements, cues and a HUD refresh, and
reading the frame the tutorial director is advanced through.

### `render_gl` — the renderer draws the match it was handed

The renderer holds no opinion about which match it is drawing. Everything it
needs from the simulation arrives on a `Render::WorldView` — terrain, fog,
ownership, nations, the troop catalogues — set once per frame by whoever owns the
session:

```cpp
m_renderer->set_world_view(Render::WorldView::of(session));
```

and read from `renderer.world_view()`, from `world()` on a render pass, or from
`ctx.world_view` inside an entity renderer.

Before this, `render/` resolved its own inputs: 52 call sites across 31 files
reached for `TerrainService::instance()`, `VisibilityService::instance()` and
friends, each of which is `SessionContext::active()` underneath. That is a
process-wide binding, read on the render thread, with no declared relationship to
the world snapshot the renderer had been given.

It is the thing standing between here and multiplayer. A client has to render
state it was **sent**; a spectator and a replay have to render a session nobody
locally is simulating. Neither is expressible while the renderer picks its own
session out of a global.

`scripts/check-render-boundary.py` enforces both halves on every build: nothing
under `render/` may call `Game::…::instance()` or `SessionContext::active()`, and
nothing under `game/` may include `render/`. `render/world_view.cpp` is the single
exempt file, because binding the ambient session is its whole job.

The refactor also turned up a real bug. `TroopProfileService::get_profile` builds
and caches on first ask; the renderer called it per unit per frame from the render
thread while the simulation did the same on its own. `NationRegistry` primes the
cache at load now, and the read side is `find_profile`, which is `const`.

### `game_view` — camera-facing gameplay services

Picking, the camera services, the minimap layers, the selection controller and
the save metadata writer, all under `game/render_bridge/`. These need to
unproject or to frame something, so they consume the kernel and link
`scene_core`. Nothing in the kernel may depend on them, and neither does
`render_gl`.

The split runs through `SelectionSystem` (kernel: which entity ids are selected)
and `SelectionController` (view: turning a click into that state). Selection is
not simulation state in any other sense: it is never serialised, and the renderer
receives an id set rather than reading a component.

The sources used to be spread across `game/view/`, `game/map/minimap/` and five
loose files in `game/systems/`, so the module was only visible by reading the
CMake target and a hand-kept list in `tests/architecture/layering_test.cpp`. One
directory now, named after the module, and that list is a directory prefix.

## The session

`Game::Session::SessionContext` owns everything authoritative for exactly one
match: the world, terrain, fog, ownership, economy, nation assignments,
statistics, the simulation clock, the deterministic RNG and the command queue.

Before it existed this state lived in process-wide singletons, so two matches
could not coexist, tests leaked state into one another, and an AI worker had
nowhere to put a hypothetical world.

Reaching a session, in order of preference:

1. take a `SessionContext&` parameter;
2. `SessionContext::for_world(world)` — a system already holds the world it
   operates on, so it can resolve its session without consulting a global;
3. `SessionContext::active()` — the ambient accessor. The registry `instance()`
   functions (`OwnerRegistry::instance()` and friends) resolve through the same
   binding, `Game::Session::ambient_services()` in `game/core/ambient_session.h`.
   They remain because several hundred call sites still use them; they are no
   longer singletons, only shortcuts to the installed session. A binary that
   never constructs a session gets the default one, exactly as before, because
   the session module installs it as the binding's fallback.

`ScopedSession` installs a session for a scope and restores the previous binding
on exit — that is how a test gets isolation. `ScopedThreadSession` does the same
for one thread, so an AI worker can evaluate a hypothetical world without
disturbing the match the main thread is simulating.

## Time and determinism

`SimulationClock` is the authoritative time base for a match. Frames feed
variable real deltas into `advance()`; the clock converts them into whole fixed
ticks, which `consume_tick()` drains. Anything that needs "how long has this
game been running" reads it rather than a wall clock, because it is the only
time base that honours pause and time scale and the only one a replay can
reproduce. Statistics and play time both come from it.

`DeterministicRng` is the match's random stream — seedable, serialisable, and
reproducible. Purely cosmetic randomness belongs to the renderer and must not
draw from it.

## The command pipeline

Every order, from any source, takes one path:

```
input / AI / replay  ->  CommandQueue::submit
                             |
                     CommandSystem (first system in the tick)
                             |
                     validate  ->  dispatch  ->  world
```

- `Game::Command::Command` is the typed order: a source, an issuing owner, a
  tick stamp and a payload variant. The payloads cover every order a player
  or the AI can give: move, attack, stop, hold, guard, run, patrol, rally
  point, produce, trade, commander ability (aura, rally, flag rally),
  formation mode / deploy / release, start construction, start harvest,
  deliver civilians, repair structure, place wall plan, place building. Every
  payload is plain data — entity ids, positions, enums — and
  `command_codec.{h,cpp}` is the one place it is written out and read back
  (JSON, one object per command). A replay file is that form on disk; a
  network transport would carry the same objects.
- `command_validator.cpp` is the single place ownership, liveness and target
  legality are checked, which is what stops player and AI orders drifting apart.
- `command_dispatcher.cpp` is the only code that turns an order into calls on
  the movement, order, production, marketplace, formation and builder
  services. App code (`app/`), the AI applier and the arena harness submit
  payloads; none of them writes gameplay components to give an order. The
  app keeps only what a client needs for feedback: it may _ask_ a service
  whether an order would be accepted (`ProductionService::can_start_production`,
  `MarketplaceSystem::can_buy`, `ArmyFormationService::preview`) to phrase a
  refusal, then submits and lets the dispatcher rule.
- Costs and timings that both issuers share live in one table:
  `assets/data/construction/catalog.json`, loaded at content bootstrap by
  `construction_cost_catalog.{h,cpp}` (which keeps a built-in copy as the
  fallback), holds resource costs and build times for every buildable, and the
  dispatcher charges the issuer when a crew is assigned.
- The client boundary is enforced, not just described:
  `scripts/check-command-boundary.py` fails the build if anything under
  `app/` or `ui/` calls a mutating service entry point the dispatcher owns
  (`OrderService::*`, `CommandService::move_units`, `ProductionService::start_production`,
  `WallPlanService::commit`, `StructurePlacementService::place`, …) or writes a
  builder's task fields. The commander's first-person control mode is the one
  allowed exception, because it is an input mode rather than an order.
- `CommandSystem` drains the queue at the top of every tick, so orders always
  land at the same point relative to movement and combat.
- `CommandQueue::set_observer` is the tap a replay recorder attaches to; it sees
  exactly the accepted commands, in execution order.

Submitting is thread-safe (the AI runs on a worker); draining belongs to the
simulation thread.

### Replays

`game/command/replay.{h,cpp}` records and plays back a match through the
pipeline above. `ReplayRecorder` attaches to the queue's observer and writes
the launch (`ReplayHeader`: what was started and how) followed by every
accepted command with the tick it was applied on, as JSON lines. Every
`digest_interval` ticks it also writes the session digest
(`game/session/world_digest.h`: every entity's id, owner, kind, position,
heading and health, every owner's stock, the tick, the rng draw count).
`ReplayPlayer` submits the recorded commands at the top of the tick they were
recorded on and compares the live digest against each recorded one; the first
tick that differs is kept as `divergence()`. While a player is set on a session
the queue is _replay-only_: local input and the AI are dropped at the door, so
the file is the only input the simulation sees.

The game exposes this as `--record-replay <file>` and
`--replay <file> [--replay-verify]` (exit 0 if every digest matched, 12 at the
first divergence); `soi_headless` does the same for the battlefield scenarios
with no window. The AI's decisions are recorded as commands like everyone
else's, and they land deterministically: `AISystem` applies a decision a fixed
number of updates after the snapshot it was taken from (waiting for the worker
if it is not done yet) rather than whenever the worker thread happens to
finish, so two runs of one match agree on the tick every AI order lands on. A
networked match would run the AI on the host and send its commands the same
way.

### Determinism, checked

Two checks run under `ctest`: `simulation_determinism` runs every battlefield
scenario twice from one seed and requires the world digest to agree on every
tick (`battlefield_gameplay_verifier --determinism-runs 2` prints the first
divergent tick and the entity lines that differ), and
`headless_replay_round_trip` records a headless bot skirmish, replays it and
requires every digest to match. `scripts/check-replay-determinism.sh` does the
record-and-replay round trip through the real game on a display. Gameplay
randomness draws from the session's `DeterministicRng` (reachable through the
ambient binding as `services.rng`); `std::random_device` and wall clocks are
for audio and presentation only.

### Stepping the match

`SessionContext::advance(real_dt, max_steps, per_tick)` is the one place wall
time becomes ticks: the clock turns the delta into whole ticks and each one
runs `SessionContext::step()` (the world's systems, once, for `tick_seconds`)
or the host's wrapper around it. The frame orchestrator in `app/` calls it
with a wrapper that adds commander-mode input; `soi_headless` calls it with
none. That is the dedicated-server shape: a session, `soi_runtime`'s system
registry, `soi_ai`, and a loop — no Qt Quick, no renderer linked in.

## Simulation and presentation ownership

Mutable entities belong to the simulation thread. `World::update()` publishes a
detached render world through an atomic two-buffer handoff when a renderer has
requested snapshots. Rendering holds the snapshot's mutex and may maintain
renderer-only animation caches there, but it does not hold the authoritative
world mutex while culling, sorting, or invoking renderer callbacks. Unchanged
idle entities are retained in the reusable snapshot buffer by a presentation
signature; active entities are refreshed every tick.

Headless callers disable creature and motion presentation with
`World::set_presentation_enabled(false)`. Commands remain the write boundary for
external producers. Sparse ECS membership is exposed as non-owning entity-ID
spans, and hot systems either iterate components directly with `World::each()`
or resolve IDs into retained scratch storage.

## Persistence

`game/save/snapshot_contract.h` classifies every piece of match state as
authoritative-serialised, derived-rebuilt, presentation-only or campaign-level,
with a rationale for each, and carries the single snapshot version number that
the save format defers to.

`tests/save/snapshot_contract_test.cpp` enforces it: a component that is declared
but unclassified fails the build's test run, an authoritative component the
serialiser never touches fails, and a derived component the serialiser _does_
write fails.

## Test binaries and what they enforce

The suite is nine binaries, split by link surface rather than by convenience.
`simulation_tests` links the kernel; `combat_balance_tests` adds
`soi_runtime`; `ai_tests` adds `soi_ai`; `campaign_tests` adds `soi_missions`
and `soi_mission_runtime`;
`persistence_tests` adds `soi_persistence`; `render_tests` adds `render_gl`;
`app_tests` adds `app_core` and `ui_shell`; `arena_tests` links the arena
harness; and `tools_tests` links the editor and balance harnesses with no
renderer at all. `tests/README.md` has the table.

Three of those binaries also link `soi_persistence` for a handful of "survives a
save" assertions. That does not weaken the boundary: `soi_persistence` sits
_above_ `game_sim`, and a kernel file reaching down into it is a
`simulation -> persistence` include, which `scripts/check-modules.py` fails the
build on.

Three rules make the split load-bearing: a test binary links production targets
rather than re-compiling production `.cpp` files, it links only the domains it
uses, and no test is excluded from the default run.
`tests/architecture/module_boundary_test.cpp` checks the first and the third
against the files that declare them. `scripts/run-tests.sh` owns the suite list;
the Makefile and all four CI workflows call it, and the same test fails if that
list and `soi_test_binaries` drift apart.

## The application layer

`app/` is laid out by domain, not by kind. Each directory is one concern, and
`CMakeLists.txt` lists the sources in the same grouping so the build file reads
as a table of contents:

| directory         | what lives there                                                 |
| ----------------- | ---------------------------------------------------------------- |
| `app/core`        | the composition root, the client context, the frame it drives    |
| `app/session`     | bringing one match up: world, renderer, level, skirmish          |
| `app/mission`     | the client half of a mission: wave effects, tutorial observation |
| `app/commander`   | the first-person control mode and its camera                     |
| `app/input`       | pointer and key gestures, before they become orders              |
| `app/orders`      | gestures turned into `Game::Command` payloads, and the feedback  |
| `app/economy`     | production, trade, the resource coach                            |
| `app/persistence` | saving and restoring a match                                     |
| `app/world`       | world state read back as client presentation state               |
| `app/audio`       | cue routing and the QML-facing audio proxy                       |
| `app/viewmodels`  | the QML API, one coherent slice per model                        |
| `app/models`      | Qt item models and image providers                               |

### The composition root and its slices

`GameEngine` owns the session, the renderer, the controllers and the services,
and drives the frame. It used to be the QML API as well. That surface now lives
on view models, each exposed from the root as a single CONSTANT property:

`game.camera`, `game.minimap`, `game.orders`, `game.placement`,
`game.production`, `game.commander`, `game.setup`, `game.saves`, `game.waves`,
`game.activity`, `game.economy`, `game.tutorial`.

Two things make that a boundary rather than a rename:

- **`App::Core::ClientContext`** (`app/core/client_context.h`) is the match named
  once — a struct of non-owning pointers to the world, the cameras, the picking
  and selection services, the input handler, the production manager, the
  viewport. The root fills it in as it builds the parts and keeps it current;
  every view model holds a `const ClientContext&` and reads what it needs.

    This replaced an earlier shape where each slice declared its own abstract
    `…Host` of accessors that the root implemented. Five such interfaces between
    them re-exported the same dozen pointers under a dozen names, which is the god
    object with extra steps rather than a boundary.

- **`App::Core::ClientHost`** is the two effects a slice cannot perform for
  itself: `ensure_initialized()` (the renderer comes up on the first frame, and
  a QML entry point can arrive before that) and `set_cursor_mode()` (which
  repaints the root's window cursor).

Everything else a slice needs from the root travels _outward_ as a signal, so
the root listens instead of being called: `CommanderViewModel::game_mode_changed`
(the renderer draws an RPG world differently), `active_camera_requested`,
`rts_selection_restored`, `MatchSetupViewModel::launch_requested` (only the root
can tear down and rebuild a match), `ProductionViewModel::refused`,
`SaveSlotsViewModel::save_requested`. Queries that are really world reads became
free functions instead — `App::World::unit_queries` is the model for that, and
it is what let `SelectedUnitsModel` stop depending on `GameEngine` at all.

`tests/architecture/qml_surface_test.cpp` caps what is left — currently 8
invokables and 33 properties, 13 of them the CONSTANT view-model handles — so
the surface can only shrink, and it fails if a member is removed without
lowering the ceiling.

### The other app-layer objects that were one file too many

Three of them held two jobs each, and the second job was extracted rather than
merely moved:

- `ProductionManager` placed buildings _and_ answered every HUD production
  question. The readouts are plain world reads with no placement state, so they
  are free functions in `app/economy/production_readouts.h`; resolving a
  "collect" order to a tree, a walkable tile and an idle builder is a different
  problem again and lives in `app/economy/harvest_targeting.h`.
- `CommandController` issued orders _and_ owned army deployment — two thirds of
  its state and half its code, sharing nothing but the act of submitting an
  order. Deployment is `App::Controllers::ArmyFormationController`, reached
  through `command_controller.formation()`, and the shared act is
  `App::Orders::OrderIssuer`.
- `MissionSetupCoordinator` set a mission up _and_ ran its attack waves.
  Waves are `game/mission/mission_waves.h`.

## Known limitations

These are real and deliberate, not oversights:

- Most gameplay code still reaches per-match state through the ambient
  `instance()` accessors rather than an explicit `SessionContext&`. The isolation
  mechanism is in place; the call-site migration is incremental, and
  `scripts/check-ambient-instances.py` keeps it from going backwards: the
  count per directory may only fall (`scripts/ambient_instance_budget.json`
  is the ceiling; `--write` lowers it after a clean-up).
- Components are pooled per type, so instances of one type are contiguous, but
  they still derive from a small polymorphic base with a virtual destructor.
  Access is by dense type-id array index, not by RTTI.
- `GameEngine` is still ~3,000 lines: the match lifecycle (bringing a world up,
  saving and restoring it), the frame loop and the per-frame UI sync. Those are
  the root's own work rather than a QML surface, but the file is bigger than one
  reading.
- `CommanderControlController` is still ~2,100 lines, and its `update()` is
  ~560 of them. It is a first-person character controller, so movement, dodge,
  jump, combat and camera genuinely interlock — but the input half (keys, mouse
  capture, view angles) touches no world state and could be lifted out.
- Types under `app/` are inconsistently namespaced: the newer ones are in
  `App::Core` / `App::ViewModels` / `App::World`, while `GameEngine`,
  `ProductionManager`, `MinimapManager`, `InputCommandHandler` and about a dozen
  others are still at global scope.
- Commander first-person control (`CommanderModeCoordinator`) drives the
  controlled commander's components directly; it is a local input mode, not
  an order, and `check-command-boundary.py` allow-lists those two files.
- The placement ghost (`ConstructionPreviewComponent`) is an entity in the
  match's world that only the local client should see. It is presentation
  state living in simulation storage; a networked client would have to keep
  it out of what it sends, or the ghost moves to a renderer-side overlay.
- `soi_headless` cannot yet host a real skirmish from a map file: the map-to-
  match setup (spawn points, the player table, starting stock, wall
  networks) lives in `app/session/skirmish_loader.cpp` next to the renderers it
  feeds. Lifting its non-render half into `game/` is the remaining step to a
  dedicated server; the loop, the queue and the replay are already there.
- `soi_world` still holds three modules (`unit_catalog`, `registries`,
  `world`) and `game_sim` two (`simulation`, `session`). Nothing above needs
  them apart, so they share an archive and `scripts/check-modules.py` keeps
  their internal order; splitting them further is a CMake edit, not code work.
