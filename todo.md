# Render thread and multiplayer readiness: what is left

State as of `perf/close-the-snapshot-boundary`. All three items from the previous
version of this file have landed; what follows is the measured result, the
places where the plan met reality, and the follow-ups that are now worth doing.

## Where the frame goes today

Battle of Zama, ultra preset, 30 s benchmark, ABBA against `b17bbc6c` (the
commit before this work) with both binaries built from the same tree and run
back to back at load ~2-4. Scene identical in all four runs: 1068 draw calls,
176 visible soldiers.

```
                         A1 base   B1 new   B2 new   A2 base
thread_cpu_ms (render)    2.633    1.596    1.456    2.635     -40%
cpu_work_ms               2.671    1.683    1.490    2.674
  of which frame phase      -      0.214    0.154      -       update_presentation
sim tick ms               1.151    1.030    0.947    1.156     own thread in B
presented fps            58.78    58.59    58.79    58.59      vsync-capped
gpu colour ms             4.19     4.26     4.11     4.29
```

The render thread lost the whole simulation and kept only the presentation
frame; the simulation runs at a fixed 60 Hz on `SoISimulation`. The frame is
vsync-capped either way - the point is the architecture, the numbers are the
proof it did not regress.

## Landed

**1. `TerrainService` is sealed after setup.** `seal()` / `is_sealed()`;
`remove_non_persistent_props()` and `add_world_prop_at_world()` warn and no-op
when sealed; `initialize()`, `restore_from_serialized()` and `clear()` unseal.
`LevelOrchestrator` and `SaveLoadCoordinator` seal right after
`UndeadAwakeningSystem::configure()` has placed its shrine.

What the plan got wrong: `release_world_prop()` is **not** setup-only. It,
`reserve_world_prop()` and `harvest_world_prop()` are the gather loop's runtime
writes (`gather_loop_system`, `command_dispatcher`, `production_system`), so
they stay open after the seal - felling a tree removes it from `world_props`
mid-match and bumps the revision. "Terrain is immutable during a match" is true
of the height field, roads, rivers and prop _shape_, not of the prop list. The
seal guards structure, not harvesting. `TerrainServiceTest.SealBlocksSetupMutationsAndInitializeUnseals`
pins both halves.

**2. The simulation runs on its own thread.** `GameEngine` owns a `QThread`
named `SoISimulation`, started by the first successful `GLRenderer::render()`
and stopped by `~GLRenderer` and `~GameEngine`. It ticks
`GameEngine::simulate(dt)` at a fixed 60 Hz cadence with wall-clock `dt`
clamped to 100 ms: mission waves/stages/messages, then
`RuntimeFrameOrchestrator::advance_simulation` (`session.advance`, `World::update`,
environment clock). Everything else that used to share `update()` - camera
follow, order markers, renderer animation time, rain, visibility, minimap,
victory, the `sync_*` view-model pushes - is `GameEngine::update_presentation(dt)`
and runs on the render thread at the top of each frame (profiler phase `frame`).
`simulate`, `update_presentation` and `render` serialise on `m_frame_mutex`
(recursive), and GUI-thread view-model entries take it via
`ClientHost::lock_frame()` right after `ensure_initialized()`, so selection,
picking, hover and placement no longer race the tick. `WorldFreeze` gates both
worker threads. The `sim` profiler phase and the benchmark's `update_ms` report
the off-thread tick via `take_simulation_tick_us()`.
`RenderThreadBoundaryTest.GlRendererDoesNotRunTheSimulationOnTheRenderThread`
forbids `m_engine->update(` in `gl_view.cpp`.

The lock is narrow. `render()` holds it only to copy the camera into
`m_render_camera`, sync the selection ids and publish the first snapshot, and
again for `render_effects`; `render_world`, the scene walk and backend playback
run unlocked against the published snapshot and the camera _copy_ while the sim
ticks. The renderer and `CameraVisibility` are pointed at the copy every frame,
so every writer of the live `Camera` (GUI input under `lock_frame`,
`update_presentation`, the commander rig) is serialised by the frame lock and
the scene walk never sees a half-written view. That was made possible by moving the commander camera
write out of the sim step: `CommanderControlController::update_simulation`
(sim) no longer touches the camera, and
`CommanderControlController::update_camera_presentation` runs from
`update_presentation` on the render thread. The composite `update()` still does
both for the controller tests. Everything `render_world` reads is already a
published handoff: the world snapshot, `VisibilityService::snapshot_ptr`,
`BirdFlockManager::frame()`, and the sealed terrain.

**3. The snapshot list is split.** `copy_render_components()` is now
`copy_authoritative_snapshot_components()` (39) + `copy_presentation_snapshot_components()`
(12), both exported from `world.h`. `SnapshotContractTest.RenderSnapshotSplitMatchesTheContract`
checks each list against `snapshot_contract.cpp`: a `PresentationOnly` component
in the authoritative list fails, and vice versa. A network payload is the
authoritative function applied to a fresh `World`.

## Crash sweep (19 Aug 2026)

A `build-asan` tree (`-DENABLE_SANITIZER=address,undefined`, RelWithDebInfo)
ran all nine gtest binaries, `soi_headless --seconds 60` and the GUI Zama
benchmark into combat (180 s, since the sanitized app renders ~0.35 fps):
zero ASan/UBSan reports, clean simulation-thread start/stop and shutdown. The
unsanitized suites (3,827 tests across eleven binaries) all exit 0. A static
pass over every `entities_with<T>()` loop found no body that adds/removes `T`
or creates/destroys entities while holding the span. `make all` was red at
HEAD on `check_ambient_instances` (`app/economy` 16/15, `app/mission` 4/2 from
#1283/#1284); the budget file is synced to what is merged, and the two seals
added here go through `scene.session->terrain()` instead of the singleton.

## Next

Nothing blocking. The multiplayer-shaped work from here is outside this file:
a network transport feeding `CommandQueue`, and a publisher that applies
`copy_authoritative_snapshot_components` to a wire world.

## Closed - do not redo these

**The simulation was never the dominant render-thread cost.** The commander
portrait was. `PortraitRenderer` owns a full `Render::GL::Renderer` and drew a
second 3D scene on `QSGRenderThread`; its `render()` called `release_scene()`
whenever the commander stopped speaking, destroying and rebuilding an entire
renderer per message and re-reading mesh blobs from disk inside `render()`.
Deleting one line took cpu_work 9.167 -> 2.361 ms, blocked time 3.090 -> 0.062,
and fps 49.3 -> 58.8.

**The Zama crash was not a mid-frame geometry rebuild.** `Backend::~Backend()`
called `SharedGeometryCache::instance().release_all()`, and that cache is a
process-global static shared with the game renderer. Fixed with a live-`Backend`
refcount.

**`GameEngine::ensure_initialized` does not belong off the render thread.** It
is guarded, runs once, and creates GL resources - it needs that thread's
context. The simulation thread is started _after_ it succeeds, for that reason.

**Do not publish `TerrainService` as a `shared_ptr<const>` snapshot.**
`TerrainService::instance()` has 89 call sites; the seal gives the guarantee in
one file.

**The minimap's dirty hash cannot be fixed by pixel quantisation.** The hash
covers every unit at once. What would pay: hoist the visibility cull out of
`UnitLayer::update`, or move the whole thing off the render thread - it
produces a `QImage` for QML, not part of the frame.

**Selection should not become a command.** Selection is client-local UI state;
the `CommandQueue` is the authoritative, replay-recorded, eventually-networked
stream. Routing selection through it would put per-client cursor state into
replays and onto the wire. `ClientHost::lock_frame()` is the right boundary for
client-local writes.

**`BirdFlockManager` was never unsynchronised.** It publishes a
`shared_ptr<const BirdFrame>` under its own mutex from `publish_frame()`, and
`bird_flock_renderer` reads `frame()`, nothing else. The earlier note was stale.

**The sim has no remaining hot spot worth chasing.** Largest symbols are
`CombatSystem::update` ~0.046 ms/frame, `SpatialGrid` hashing ~0.035 ms, and
`is_valid_enemy_of_owner` ~0.026 ms, under a ~0.08 ms noise floor.

## Measuring on this machine

Read this before quoting any number.

- **Check `uptime` first.** Other Claude sessions build on this box and drive
  load to 80+. A run at load 22 versus load 3 differs by 4x.
- **Interleave ABBA, not ABAB.** Load decays monotonically after a build, so
  A-then-B flatters B. A uniform "regression" across phases the diff cannot
  touch (`shadow`, `sort`) is the tell for measurement drift.
- **Reject runs where the mission failed to populate.** Check
  `visible_soldiers_average ~= 176`; bad runs come out around 295 draws and 0
  soldiers. Draw calls now legitimately drift 1046-1068 between good runs.
- **8 s never reaches combat.** Use 30 s to measure anything that scales with
  battle intensity.
- **Never end a build pipeline with `tail`.** `cmake --build … | tail` exits
  with tail's status and a stale binary fakes a green run.
- **A wait loop must not `pgrep` its own command line.**
- **`perf` on `build-perf`**: use `--comms=QSGRenderThread,SoISimulation` to
  separate the threads now; `QRasterPaintEngine` is on the main thread.
