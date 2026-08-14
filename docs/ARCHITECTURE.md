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
engine_core                 ECS: entities, components, world
     |
game_sim                    the simulation kernel
     |
     +-- soi_campaign       the campaign definition
     |        |
     |        +-- soi_missions      mission content, victory rules, the map/mission catalogue
     |        +-- soi_persistence   the snapshot contract, the save format, the save database
     |
     +-- soi_ai             the computer opponent
     |        |
     |        +-- soi_runtime      which systems a match owns and in what order they tick
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

`game_systems` is an INTERFACE target that pulls in the whole gameplay stack at
once. It exists for consumers that genuinely want all of it — the renderer, the
arena. New code should name the domains it uses instead: linking `game_systems`
puts the AI and the save database into a binary that may need neither, and the
link line stops being a statement about what that binary depends on.

The names used in this document are the target names. `soi_components`,
`soi_simulation`, `soi_render_bridge`, `soi_render_gl` and `soi_app` are CMake
aliases for `engine_core`, `game_sim`, `game_view`, `render_gl` and `app_core`
respectively; either spelling links the same archive.

### Inside `game_sim`

The kernel is still one archive, so CMake cannot separate combat from movement
from terrain. `scripts/module_rules.json` declares the module map anyway —
`domain_types`, `components`, `unit_catalog`, `registries`, `world`, `units`,
`navigation`, `formations`, `economy`, `combat`, `wildlife`, `simulation`,
`session` — along with the direction each is allowed to point in, and
`scripts/check-modules.py` enforces it on every build.

Two of those need a word. `domain_types` is the vocabulary: enumerations with no
dependencies of their own, scattered across `game/systems`, `game/units` and
`game/wildlife` by directory but belonging to no layer, because every layer names
them. `component.h` storing a `SpawnType` is a struct holding a value, not the
ECS reaching into gameplay, and separating the two is what makes the remaining
counts mean something.

`session` is at the _top_, not the bottom. `SessionContext` owns the terrain, the
fog, the registries and the command queue by value, so everything it contains is
below it — and a registry reaching back up to `SessionContext::active()` to find
itself is a cycle. That is why those `instance()` definitions were collected into
`game/session/ambient_registries.cpp`, which is the only file below the session
that knows the ambient binding exists.

Edges that point the wrong way today are counted per module pair in that file's
`baseline`. The count may go down and may not go up, and paying one down has to
be recorded, so the debt is visible rather than ambient. A module whose inbound
count reaches zero can be lifted into a target of its own — that is exactly how
`soi_ai`, `soi_missions`, `soi_campaign`, `soi_persistence` and `soi_runtime`
left the kernel, and the remaining counts say what each of the others would
cost.

### `game_sim` — the simulation kernel

Everything a match needs to run with nothing on screen: ECS systems, the
session, the command pipeline, terrain, pathfinding, units, world serialisation.

It links `engine_core` and `animation_core` and nothing else. In particular it
does **not** link `scene_core` or `render_gl`. That is what makes headless
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

`game/formation/` holds the two formation layers. They are deliberately
separate: `UnitLayoutSystem` owns soldier offsets inside a single troop entity
and is the only thing the renderer reads, while `ArmyFormationPlanner` and
`ArmyFormationRegistry` own multi-unit deployment and are the only things
player commands and AI reach for. See
[docs/FORMATION_ARCHITECTURE.md](FORMATION_ARCHITECTURE.md).

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
the save metadata writer. These need to unproject or to frame something, so they
consume the kernel and link `scene_core`. Nothing in the kernel may depend on
them.

The split runs through `SelectionSystem` (kernel: which entity ids are selected)
and `SelectionController` (view: turning a click into that state).

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
   functions (`OwnerRegistry::instance()` and friends) resolve through this. They
   remain because several hundred call sites still use them; they are no longer
   singletons, only shortcuts to the installed session.

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
  tick stamp and a payload variant (move, attack, stop, hold, guard, run,
  patrol, rally, produce).
- `command_validator.cpp` is the single place ownership, liveness and target
  legality are checked, which is what stops player and AI orders drifting apart.
- `command_dispatcher.cpp` is the only code that turns an order into calls on
  the movement, order and production services.
- `CommandSystem` drains the queue at the top of every tick, so orders always
  land at the same point relative to movement and combat.
- `CommandQueue::set_observer` is the tap a replay recorder attaches to; it sees
  exactly the accepted commands, in execution order.

Submitting is thread-safe (the AI runs on a worker); draining belongs to the
simulation thread.

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
`soi_runtime`; `ai_tests` adds `soi_ai`; `campaign_tests` adds `soi_missions`;
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

`GameEngine` is the composition root: it owns the session, the renderer, the
controllers and the services, and drives the frame.

It is also, historically, the QML API. That surface is being moved onto view
models one coherent slice at a time, each exposed from `GameEngine` as a single
CONSTANT property:

- `game.saves` — `app/viewmodels/save_slots_view_model.h`
- `game.placement` — `app/viewmodels/placement_view_model.h`: formation
  placement, builder construction and building placement. It reaches back for
  the few things only the root knows (lazy initialisation, the window mapping,
  the local owner, the cursor) through `PlacementHost`, which is the shape a
  further extraction should follow.

`tests/architecture/qml_surface_test.cpp` caps what is left — currently 103
invokables and 39 properties — so the surface can only shrink, and fails if a
member is removed without lowering the ceiling.

Slices still on `GameEngine` and worth extracting next: campaign progression,
commander/FPV control, and camera control.

## Known limitations

These are real and deliberate, not oversights:

- Most gameplay code still reaches per-match state through the ambient
  `instance()` accessors rather than an explicit `SessionContext&`. The isolation
  mechanism is in place; the call-site migration is incremental.
- Components are pooled per type, so instances of one type are contiguous, but
  they still derive from a small polymorphic base with a virtual destructor.
  Access is by dense type-id array index, not by RTTI.
- `GameEngine` remains large. See above.
- `game_sim` is still one target for eleven modules. The AI, the mission and
  campaign loaders, the save stack and the system composition root have been
  lifted out into targets of their own, because nothing in the kernel reached
  into them. The rest — `world`, `units`, `navigation`, `formations`, `economy`,
  `combat`, `wildlife` — genuinely include each other, so CMake cannot separate
  them yet. `scripts/module_rules.json` records exactly how many edges stand in
  the way of each. Extraction is bottom-up: a module can become a target only
  once everything below it already is one, so the order is `domain_types`,
  `components`, `unit_catalog`, `registries`, `world`, `units`, `navigation`,
  `formations`, `economy`, `combat`. The first three are at zero;
  `scripts/check-modules.py` prints what the rest still cost.
- `registries` is the next layer to clear, and its twelve remaining edges are
  three separate problems: `NationRegistry::initialize_defaults()` is a content
  bootstrap wearing a registry's clothes (it loads formations, troops and
  nations, then primes the profile cache), `GateService` and
  `WallNetworkService` are navigation consumers filed as registries, and
  `NationCollapseService` reaches into the economy. None is large; all three are
  code work.
