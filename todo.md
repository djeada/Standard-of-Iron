# Performance work: what is left

Written 18 Aug 2026, after the optimisation pass that landed the render-snapshot,
combat, capture, terrain-prop, minimap, shadow-pass, instancing-ring and
terrain-octave changes. Numbers below are interleaved A/B against a clean build
of `4709c79b` on a quiet box, using render-thread CPU time
(`thread_cpu_ms_average`), not wall clock.

Full report: <https://claude.ai/code/artifact/493f0438-6c8c-4157-bc36-36dd616e6d03>

## Where the frame goes today

Zama, Ultra, 1600x900, 176 visible soldiers, RTX 5060:

| Cost              | Per frame | Notes                                         |
| ----------------- | --------- | --------------------------------------------- |
| GPU colour pass   | 4.5 ms    | terrain surface 2.1-2.65, rigged soldiers 1.4 |
| GPU shadow pass   | 2.1 ms    | 4 cascades at 4096 squared                    |
| GPU post          | 0.3 ms    |                                               |
| Render-thread CPU | 1.7 ms    | sim 0.5, draw 1.2                             |

The frame is GPU-bound at 1x game speed on this hardware. It is CPU-bound at 4x
game speed, and would be CPU-bound on a weaker CPU. Everything cheaper than the
two items below has already been done.

## 1. Bake the terrain's static per-pixel fields into a texture

The largest single cost anywhere, on **every** preset: 2.1-2.65 ms at Ultra on
Zama, 3.25 ms on Trebia, and 2.0 ms even at Low because no graphics preset
touches it. On an integrated GPU the same shader is 20-30 ms, which is the
difference between shipping to that hardware and not.

Two families of work in `terrain_chunk.frag` are pure functions of world
position and the static height map, so both can be evaluated once at map load
and sampled thereafter. Bake them into the same atlas, in the same pass.

**a. Low-frequency noise.** 27 `gradient_fbm` calls per pixel at five octaves
each, roughly 135 noise evaluations; about fifteen of the calls are functions of
`world_coord * macro_scale + offset` only.

**b. Height-map neighbourhood queries.** `terrain_sky_openness()` (line 218) is
6 directions x 3 rings = **18 dependent texture taps per pixel**, and
`compute_curvature()` (line 141) is 5 more. Both read `u_height_tex`, which
never changes during a match. These may be the better half of the win: 23
dependent taps thrash the texture cache in a way arithmetic does not.

Half of this is **done** (18 Aug 2026): the height-map neighbourhood queries are
baked. The noise half is not.

- [x] Bake sky openness and curvature. Done on the **CPU**, not in an offscreen
      GL pass -- `m_height_data` is already a `std::vector<float>`, so there is
      nothing to read back and no need to share GLSL.
      `TerrainRenderer::bake_terrain_fields()` writes both into the two channels
      of an `RG16F` texture at one texel per tile; `terrain_chunk.frag` reads
      `u_field_tex` once instead of 23 dependent taps.
- [x] Validate. Mean brightness identical on Zama, Crossing the Alps and the
      mountain map; 0.008% of pixels differ by more than 8/255 on the two hilly
      maps; ambient occlusion still reads in the gullies and at the cliff feet.
- [x] Re-measure. Terrain surface (draw type 8) 6.20 ms to 5.75 ms, six
      interleaved arena runs each: **about 7%**, not the hoped-for 60%.

The remaining, larger half is the 27 `gradient_fbm` calls:

- [x] Tag every `gradient_fbm` / `gradient_noise` call as bakeable or per-pixel,
      by frequency. **Done** -- the full table is in RENDERING_ARCHITECTURE.md.
      It shrinks the prize: `rock_coord`, `rill_coord` and `relief_coord` are all
      `mix(world_coord, wall_coord, wall_blend)` or projections onto
      `normal.xz`, so `rock_detail`, `rock_grain`, `rock_strata`, `bedding`,
      `scrub_field`, `rill_field`, `rill_shifted` and the three
      `relief_octave()` calls **move with the shading normal and cannot go into
      a world-space texture at all** -- not for the resolution reason this plan
      gave. Nineteen fields are genuinely bakeable, and they are the five-octave
      `gradient_fbm` ones, so they are still the expensive part.
- [ ] Sixteen of those nineteen sit at or below 0.4 cycles/tile and fit one
      atlas at 1 texel/tile. `fleck_field` (1.15), `tussock` (0.90) and
      `graze_drift` (0.77) would force 4 texels/tile on their own -- leave them
      procedural for the first cut.
- [ ] Add a one-time offscreen bake at map load that renders the bakeable fields
      into a world-space texture atlas, four fields per RGBA texture, 2-4
      texels/m (Zama at 800x800 tiles gives 1600-3200 square per texture, so
      three or four textures).
- [x] The bake must use the **same** GLSL functions as the runtime shader, so the
      values match; share them through `assets/shaders/include/`. **Done** --
      `assets/shaders/include/terrain_noise.glsl` now holds the hash, value
      noise, gradient noise, fbm and cellular helpers, and `terrain_chunk.frag`
      calls them through thin wrappers. The fbm takes its octave count and
      footprint as arguments rather than reading `u_noise_octaves` and calling
      `fwidth` itself, which is exactly what a bake pass needs: it passes a
      footprint of 0 to get every octave. Verified behaviour-preserving -- the
      extraction moves the Zama arena frame by mean 1.94/255, which is _below_
      the 2.28 run-to-run noise floor of the same binary rendered twice.
- [ ] Swap the tagged calls for texture samples in the fragment shader.

Watch out: `gradient_fbm` fades its octaves by `fwidth(p)`, so it is a function
of the screen footprint as well as the world position and is **not** the pure
function of world position this plan assumed. A bake has to fix the octave count
and let the baked texture's mip chain do the distance fade instead.

Revised expectation: the 23 dependent taps were worth 7%, not the better half.
The noise is where the remaining cost is.

### Not worth baking: fog

Checked and measured, so nobody repeats the investigation. On Trebia, a river
map with mist volumes live, every fog-related cost together is under 0.3 ms of a
~7 ms GPU frame, and that figure is the whole post pass including sky, ambient
occlusion, godrays, blur and FXAA. The fog batch itself (draw-command type 5) is
0.005 ms.

- Fog of war is **already** the baked solution: `visibility_mask.glsl` is a
  single `texture()` fetch into a mask the `VisibilityService` maintains. It also
  has to stay dynamic.
- Ground mist (`ground_mist()` in `post_composite.frag`) is two value-noise taps
  and both scroll with `u_time`, so a static world-space bake does not apply.
  Scrolling UVs through a baked field would work and would save approximately
  nothing: two evaluations, not 135.
- Distance fog is analytic, a range function with no noise at all.

## 1b. A crash blocks long Zama runs

Found while validating the above, and **pre-existing at `ed7d5b71`** -- reproduced
in a clean detached-worktree build, so it is not from any of this work. Roughly
half of all Zama runs longer than ~15 s segfault on the render thread:

```
Thread "QSGRenderThread" SIGSEGV
Render::GL::VertexArray::bind            render/gl/buffer.cpp:108
Render::GL::Mesh::prepare_draw           render/gl/mesh.cpp:107
MeshInstancingPipeline::flush            render/gl/backend/mesh_instancing_pipeline.cpp:174
Backend::render_directional_shadows
Backend::execute_scene                   render/gl/backend.cpp:987
```

`MeshInstancingPipeline::m_current_mesh` is a bare `Mesh*`. `flush()` null-checks
it, which a freed pointer passes, and then faults reading the `m_vao` unique_ptr
inside the dead Mesh. The draw queue is double-buffered, so a Mesh submitted into
queue N can be destroyed before queue N is played back.

- [ ] Give the pipeline ownership, or a generation-checked handle, instead of a
      bare `Mesh*`. Item 2 will widen this window, not close it.

## 2. Move the simulation off the render thread

Measured on Zama, Ultra, 176 soldiers, with the fixed instruments (two runs):
`present` 22.4/26.5 ms -- the thread is blocked on the compositor for most of the
frame -- then `sim` 2.40/2.32, `play` 2.26/0.47, `submit` 0.69/1.28, `shadow`
0.45/0.41, `minimap` 0.23, `snapshot` 0.16, `collect` 0.09, `sort` 0.07,
`view_model_sync` 0.03. `World::update` is 2.09/2.00 of the sim, 87% of it.

So of ~5-6 ms of render-thread CPU, about 2.5 ms -- the sim plus the minimap --
does not belong on that thread at all. At 1x the thread is not saturated; at 4x
the orchestrator runs up to 8 sim steps per frame and that is where it bites.

89% of process cycles are on `QSGRenderThread` with 20 cores idle. Sim rate and
frame rate are the same number, so every hitch in a game system is a dropped
frame, and there is no way to run a fixed simulation step while drawing at
display rate. This is the only change that raises the ceiling rather than the
floor. Budget a week, not a day.

- [ ] The seam already exists: `World::publish_render_snapshot`. The render
      thread should consume the last published snapshot and never touch the live
      world.
- [ ] Stage one: audit and fix everything that reads the live world off the sim
      thread. **Started 18 Aug 2026.**
    - [x] `MinimapManager::update_units` reads the published snapshot now, not the
          live world. The orchestrator acquires it and hands it over, so the
          manager itself stays snapshot-agnostic.
          `RuntimeFrameOrchestratorTest.MinimapReadsThePublishedSnapshotNotTheLiveWorld`
          proves it: an unpublished live-world write does not change the minimap
          image, and publishing it does.
    - [ ] `SelectedUnitsModel::refresh` is connected with `Qt::AutoConnection`
          from a signal emitted on the render thread, so it already runs on the
          GUI thread and reads `ClientContext::world` live. That is a cross-thread
          live-world read **today**, before any threading work. It is blocked on
          the snapshot: it needs `UnitComponent`, `StaminaComponent` and
          `BuildingComponent` (all copied) plus the ten components
          `classify_unit_activity()` reads, of which
          **`CivilianDeliveryComponent` is not in `copy_render_components`**.
          Converting without adding it would silently drop the civilian
          "delivering" activity. Add the component _with_ the conversion, never
          before it -- a copy nothing reads is pure cost on every sim step.
    - [ ] `ActivityViewModel` mixes both: `record_hit` is a read, but
          `clear_inspect_target` mutates the selection system. This has to be
          classified per call site, not per file; a blanket swap would be wrong.
    - [ ] Still to audit: picking, the HUD view models, and the input path.
          `InputCommandHandler`, `CommandController`, `ProductionManager` and
          `ArmyFormationController` each hold a raw `Engine::Core::World*` and are
          driven from QML on the GUI thread, so they mutate the live world while
          the sim runs on the render thread. `World::update` holds
          `m_entity_mutex` for its whole duration and the structural `World::`
          calls take it too, so the entity registry is serialised -- but direct
          component-field writes through `Entity::get_component<T>()` are covered
          by nothing at all.
    - [ ] Land the boundary as a type rather than a convention: a const-only read
          view that holds the snapshot's `shared_ptr` alive for the duration of
          the read. Then ratchet the count of remaining live-world reads down, the
          way the `game_sim` module map already ratchets inbound edges.
- [ ] Stage two: run `World::update` on its own thread at a fixed step.
- [ ] Watch out for replay determinism (`scripts/check-replay-determinism.sh`)
      and for the sim already being frame-rate dependent.
- [ ] Validate with `arena_app --batch --all` and the campaign missions, not just
      a benchmark: this can change gameplay timing.

## 3. Cheaper items, worth doing alongside

- [x] **Per-cascade shadow resolution.** Signed off and done. Cascades 0-1 stay
      at the preset resolution, 2-3 drop to half, in a second texture array:
      67 M texels to 42 M at Ultra. Arena frames are pixel-identical to
      `ed7d5b71`. `SOI_SHADOW_CASCADE_SPLIT=0` bisects it.
      **The depth bias must scale with the texel size** or the far cascades
      shadow-acne themselves black; see RENDERING_ARCHITECTURE.md.
      Still unmeasured on Ultra: every Zama benchmark attempt since has been
      too contended to reach 30 frames.
- [x] **Combat spatial queries.** `entities_by_id` (an `unordered_map` cleared
      and refilled every tick) is now a stamped table indexed by
      `Handle::index_of`, so the per-tick rebuild allocates nothing and lookup
      is an array index. `find_nearest_enemy` reads a precomputed
      `CandidateRecord` -- building and wildlife flags, plus a per-owner-pair
      hostility table -- instead of four component fetches and an
      `OwnerRegistry::instance()` call per candidate. Health, pending-removal
      and owner are still read live, so a unit that dies mid-tick stops being a
      target exactly as before. `SpatialGrid::clear()` keeps its cell vectors so
      the grid stops reallocating them every tick. Replay determinism passes.
- [x] **Presets that scale the right axis.** Already true at `ed7d5b71`:
      `apply_preset` sets `cascade_count` and `resolution` per preset (Low off,
      Medium 2x1024, High 3x2048, Ultra 4x4096) and
      `ensure_directional_shadow_resources` honours both. Nothing to do.

## 4. Fix the instruments

They cost more time than any single optimisation on this pass.

**Done, 18 Aug 2026.** All four, plus a fifth found on the way.

- [x] The phases now tile the whole render-thread frame -- `sim`, `snapshot`,
      `collect`, `submit`, `sort`, `shadow`, `play`, `present` -- so
      `total_us()` is the frame interval, not 12% of it. `PhaseScope` is
      self-exclusive, so `shadow` nested in `play` counts once. There is
      deliberately no `cull` phase: culling is inline in every entity renderer,
      so the only honest place to attribute it is `submit`, and a permanent
      0.00 ms line kept suggesting culling was free.
- [x] `ui/qml/ProfilingOverlay.qml` mounts above every other layer in
      `Main.qml`; **F10** toggles it, `SOI_PROFILING_HUD=1` starts it on.
- [x] The benchmark reads the real preset from `GraphicsSettings::quality()` and
      reports `valid: false` with an `error` below 30 frames instead of
      statistics. It also emits a `render_thread_stages` object: per-phase
      averages plus `world / visibility / minimap / weather_lighting / victory /
view_model_sync`, which is the inventory item 2 needs.
- [x] The continuity probe is its own switch, `SOI_RUNTIME_CONTINUITY=1`.
- [x] **Also fixed:** the benchmark armed its window on the first non-loading
      frame and never re-armed, so when loading resumed the measured window
      elapsed during the load -- that is the real cause of `frames: 1`, not the
      window length. It now re-arms and discards samples whenever
      `is_loading()` goes true.

## Measuring on this machine

**The box has been shared throughout this pass.** `/proc/loadavg` sat between 8
and 40 for most of it because another session was building and running tests, so
every `--benchmark-seconds` run on Zama returned 1-4 frames and the new refusal
correctly marked them `valid: false`. The arena's `SOI_GPU_BREAKDOWN=1` numbers
above were taken at load 2.6-3.7, which is the only window that was usable.

The box is shared and the numbers move by more than most real changes do. The
same Zama scene measured anywhere from 2.2 to 60 ms of "CPU work" depending on
what else was running; at load ~40 both binaries drop to one frame every three
seconds because the compositor starves.

- Check `/proc/loadavg` first. Above ~5, do not measure.
- Compare against a clean build of the base commit in a worktree, interleaved,
  at least two runs each, and report the spread.
- Prefer `thread_cpu_ms_average` (render-thread CPU) and the GPU timestamp
  queries over wall clock. Both are in the benchmark JSON.
- `perf record --call-graph lbr -e cpu_core/cycles/P` shares are robust to
  outside load; wall-clock milliseconds are not. Release builds are linked with
  `-s`, so symbol attribution needs `build-perf` (`SOI_KEEP_SYMBOLS=ON`).

Useful switches, all env-gated and free when unset:

```bash
SOI_SWAP_INTERVAL=0    # uncapped frame rate, otherwise vsync hides everything
SOI_GPU_BREAKDOWN=1    # GPU ms per draw-command type, logged every 120 frames
SOI_GL_DEBUG=1         # KHR_debug logger (note: changes driver behaviour)
SOI_SHADOW_INSTANCING=0  # bisect the shadow-pass instancing path
```

## Scenarios not yet exercised

`docs/MASSED_BATTLE_PERFORMANCE.md` documents `massed_battle_250` through
`massed_battle_2000` and `seven_ai_scale`. This pass only measured the campaign
battles, a skirmish and three arena scenarios. The massed-battle set is where
the sim cost should dominate, and it is the right acceptance test for item 2.

**First baseline taken, 18 Aug 2026** -- `massed_battle_1000`, Ultra, 240 frames
at load 0.65, `SOI_GPU_BREAKDOWN=1`, two consecutive reports:

```
4=0.025ms  6=0.241ms  8=3.645ms  14=19.097ms  post=0.223ms
4=0.021ms  6=0.227ms  8=3.240ms  14=18.693ms  post=0.217ms
```

Type 14 is `RiggedCreature`. **At 1000 units the frame is dominated by the
rigged soldiers at ~19 ms, with terrain a distant second at ~3.4 ms** -- the
reverse of the 176-soldier campaign battles this plan was written from, where
terrain was the largest single cost.

That reframes the priorities above. Item 1's remaining noise bake is worth ~3 ms
of GPU at best in a massed battle; the rigged-creature path is worth six times
that. Before spending the week on item 2, measure whether the 19 ms is
vertex-skinning, overdraw or state changes -- `SOI_SHADOW_INSTANCING=0` and the
`rigged_cull` compute path are the two switches to bisect with. The scenario
passed with no issues, so the cascade split and the field bake are both clean at
1000 units.

```bash
build/bin/arena_app --batch --scenario massed_battle_1000 \
  --fps 240 --duration 1 --capture-interval 0 \
  --artifact-dir artifacts/todo-baseline
```
