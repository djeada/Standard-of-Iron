# Performance

How the game is profiled, what was found on the heaviest mission, and why the
hot paths look the way they do. Source files carry no comments (the formatter
strips them), so the reasoning behind each optimisation lives here.

## Measuring

Profile the real game, never only the headless simulation: rendering,
presentation, QML and audio all compete for the same frame.

```bash
cmake -S . -B build-perf -DCMAKE_BUILD_TYPE=Release -DSOI_KEEP_SYMBOLS=ON \
  -DCMAKE_CXX_FLAGS="-fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -g1"
cmake --build build-perf -j4 --target standard_of_iron

PULSE_SERVER=unix:/nonexistent SOI_AUDIO_OFFLINE=1 SOI_SWAP_INTERVAL=0 SOI_GPU_BREAKDOWN=1 \
  perf record -F 300 -g -o aurelia.data -- \
  build-perf/bin/standard_of_iron --mission-file assets/missions/siege_of_aurelia_magna.json \
  --skip-briefing --benchmark-seconds 60 --benchmark-output aurelia.json
perf report -i aurelia.data --sort comm --no-children
```

- `PULSE_SERVER` pointing nowhere plus `SOI_AUDIO_OFFLINE=1` keeps the run silent.
- `SOI_SWAP_INTERVAL=0` removes the vsync cap so frame times are real.
- The benchmark JSON splits the render thread into phases (`render_thread_stages.phase_*`),
  reports GPU colour/shadow time, draw commands per type, navigation counters and
  asset/upload counters. Read those before guessing.
- GPU timer queries measure GPU _timeline_ time. A pass that the CPU feeds slowly shows
  up as GPU time too; check whether the number moves with resolution before calling a
  pass fill-bound.
- Threads: `SoISimulation` runs `World::update` at a fixed 60 Hz, `QSGRenderThread`
  renders, the GUI thread runs QML, and `PrepareWorkerPool` threads prepare humanoids.

## Siege of Aurelia Magna

The largest mission: a 769×769 generated city with ~1,028 buildings (~490k building
parts), ~840 civilians and soldiers, 17 gates and 56 towers. Preset High, 1280×720,
RTX 5060.

| metric                                | before                         | after first pass |
| ------------------------------------- | ------------------------------ | ---------------- |
| presented fps                         | 10.3                           | 22.7             |
| simulation per frame (avg / p95)      | 97 / 243 ms                    | 28 / 43 ms       |
| navigation per tick (avg / p95)       | 10.2 / 74 ms                   | 0.17 / 0.27 ms   |
| A* cells expanded per minute          | 62.3 M                         | 2.0 M            |
| GPU colour pass                       | 68 ms                          | 14.5 ms          |
| instanced mesh batches per frame      | 7,287                          | 45               |
| render play / present phases          | 28 / 38 ms                     | 5.7 / 9.6 ms     |
| creatures failing to load every frame | ~17 (40,896 refused bakes/min) | 0                |

## September 2026 follow-up measurement

The follow-up used `perf record` on the real Aurelia mission, then alternating
baseline/candidate runs at Ultra, 1280×720, uncapped presentation, with the 20-second
camera pan/zoom cycle and simulation profiling enabled. Each Aurelia measurement
lasted 45 seconds after the application's warm-up. The saved binaries were built
on September 26 and compared on September 27; the working tree advanced between
those sessions. These are measurements of that saved pair, not a certification of
every subsequent source change.

The first comparison ran without a competing game or build:

| Aurelia metric                                 | Baseline | Candidate | Change |
| ---------------------------------------------- | -------: | --------: | -----: |
| Capture system, average per tick               | 257.5 µs |   64.2 µs |   −75% |
| Combat system, average per tick                | 1.623 ms |  1.498 ms |    −8% |
| Simulation tick, final 600-tick window average | 7.074 ms |  6.522 ms |    −8% |
| Render-thread CPU, average                     | 4.657 ms |  4.374 ms |    −6% |
| Render-thread CPU, p95                         | 7.732 ms |  6.275 ms |   −19% |
| Presented FPS                                  |    69.26 |     71.38 |    +3% |

A separate 14-worker build started during the second pair. Those repeats and the
30-second Cannae comparison are retained as diagnostic runs and excluded from the
table. Cannae completed on both binaries, but its contended timings do not establish
a performance regression or improvement. This is one clean live-play comparison,
not a deterministic replay study or a frame-pacing gate pass: presentation p95
remained around 32 ms and visible hitches still need work.

An isolated 769×769 fog-encoding microbenchmark initially measured 2.53 → 1.35 ms
per encode with identical checksums. A subsequent three-repeat `perf stat` run
pinned to CPU 2 counted 29.746 → 15.407 billion instructions for 500 encodes
(−48%); pinning avoids incomplete counter coverage when migrating between this
CPU's performance and efficiency cores. The image regression tests compare every
encoded channel against the original tent filter, including edges and dirty regions.

The follow-up `perf` capture still highlights NVIDIA driver work, directional
shadows, render-snapshot publication and per-soldier formation walking. Its sample
percentages are not an A/B timing comparison because that capture overlapped the
competing build.

Raw JSON, logs, `perf` captures/reports, binary hashes, commands and microbenchmark
sources are in the ignored local directory `artifacts/perf/aurelia-20260926/`.
`comparison-manifest.json` describes the saved-binary comparison. The repeatable
suite in `scripts/run-perf-suite.sh` now includes Aurelia as a mission-file fixture.

Focused validation passed 6 capture tests, 38 formation-geometry tests, 2 access
recorder tests and 21 fog/visibility tests. The remaining access-verification test
is explicitly skipped because component access recording is compiled out in Release.

## October 2026 live-play profile

Seven minutes of Aurelia Magna were played by hand at Ultra, 1920x1080, vsync off,
with `perf record -g --call-graph fp` attached to a symbolised Release build
(`-DSOI_RELEASE_DEBUG_INFO=ON -DSOI_KEEP_SYMBOLS=ON` plus frame pointers). Playing
mattered: the worst costs only appear when the cursor rests on the map, troops are
selected and the army is moving through the city.

What the profile showed, and the rule each fix now enforces:

**The GUI thread waited on the simulation.** A 16 ms QML timer called
`OrdersViewModel::set_hover_at_screen` sixty times a second, mouse still or not, and
it took the blocking frame lock that the simulation holds for its whole tick. The
game counted 6,625 contended acquisitions, 41 s of waiting and a 0.92 s worst case in
seven minutes; frames whose render work took 5-7 ms and GPU work 13-25 ms were
presented 60-94 ms apart. Three more timer paths had the same shape
(`CameraViewModel::move`, `OrdersViewModel::action_states`,
`ProductionViewModel::selected_building_id`).
_Rule:_ anything a QML `Timer` reaches uses `ClientHost::try_lock_frame()` and skips,
defers or serves the last published value when the simulation is mid-tick. Reads come
from the per-frame `Published` snapshot; camera pans accumulate and are applied by
`CameraViewModel::publish_frame()` under the lock. `scripts/check-frame-lock.py`
follows every Timer handler (through the QML functions it calls) to the C++ it reaches
and fails on a blocking lock. Hover itself re-evaluates on cursor movement and at
most every 100 ms for a still cursor, and picks project with one cached
view-projection matrix (`Camera::screen_projector`) instead of one locked matrix
fetch per entity.

**Presentation work ran inside the locked tick.** About 40% of the simulation
thread was presentation: the minimap repainting every tower, temple and stronghold
as antialiased stroked polygons twenty times a second (its change hash includes
every moving unit), formation presentation, snapshot publication.
_Rule:_ the locked tick only _collects_; expensive presentation work that needs no
live world state is posted to `DeferredPresentationQueue` and runs in
`SimulationLifecycle`'s unlocked stage after the lock is released. The minimap stages
its markers under the lock and paints in that stage; its published image has its own
mutex, so the GUI never reads a half-painted composite. The minimap unit layer keeps
structures in two cached layers (below and above troops, preserving the draw order)
that repaint only when their own signature changes; troops and selected troops are
sprite blits.

**Render caches were keyed on snapshot pointers.** The render thread reads one of
several rotating snapshot worlds, so `Entity*` and component pointers differ from
frame to frame. `UnitRenderCache` treated a pointer change as an invalidation, which
recomputed every building's model matrix, re-resolved its renderer key string and
re-sampled the terrain under every foundation every frame
(`resolve_structure_foundation` alone was 5.5% of the render thread).
_Rule:_ render-side caches key on entity id and compare values, never snapshot
pointers. `unit_render_cache_test` alternates two snapshot copies and asserts nothing
is re-derived.

**The GPU re-rendered static content every frame.** Each shadow cascade cleared and
redrew the terrain and the whole static building batch every frame, and the terrain
surface (a 1,300-line fragment shader with PCSS shadows) was drawn before the
buildings that hide much of it.
_Rule:_ static content is cached by content revision, not redrawn by default. Each
cascade keeps a static depth layer keyed on its light matrix and a hash of the static
casters it accepts; a hit copies that layer and draws only dynamic casters. The copy
is not free (a 4096² layer is 64 MB), so a cascade only uses the cache when its static
triangles exceed one per ten texels; otherwise it draws directly. The shadow light
direction follows the sun in 0.1° steps, so continuous day cycles still hit. The
benchmark report counts `shadow_static_cache` hits, misses and bypasses. Opaque static
buildings are drawn before the terrain surface so the depth test rejects terrain
fragments they cover.

**A background window rendered flat out.** When the game window is not active the
renderer now holds itself to 30 fps (`Render::FrameCadence`); benchmark and film runs
are exempt.

Investigated and left alone: the rigged pipeline orphans its palette and instance
streams once per frame (about four orphans a frame, ~1.8 MB each). That is the
intended streaming pattern, not resubmission.

Measurement of these changes is still to do. A first interleaved pair on the opening
view went from 42 to 50 fps, GPU colour pass 14.8 to 11.7 ms and frame-lock waiting
628 to 7 ms, but another process was using a third of the GPU at the time, so it is
indicative only. The intended comparison records a played session with
`--record-replay` and replays it on both builds with `--replay`.

## Simulation

**Unreachable goals.** A* used to flood the whole reachable region whenever the goal
could not be reached, capped only by `width*height` iterations. `find_path_internal`
now compares region labels first; a goal in another region is replaced by the nearest
cell of the start's region (ties go to the cell nearest the unit, so it stops on its own
side of a sealed compound instead of walking round it), so an impossible order costs
one bounded search. The
iteration cap is gone because every cell can be closed only once.

**Path cache.** Partial results (searches that stopped short of their goal) are cached
too. A partial result depends on the whole reachable region, so any navigation change
drops it; a complete path is dropped only when a change crosses its bounds.

**Region map.** A closed gate's cells are treated identically by the grid and by
`connects()` (one sorted `closed_gate_cells` list), so a gate toggle can never change
which cells connect. After a navigation change the region map is reused when every
changed cell keeps its connect state; labels are then identical to a rebuild.

**Obstruction releases.** Opening a gate used to re-plan every mover with a target.
Only the few nearest units whose route stopped short of their order are re-planned
now; anyone missed is caught by the one-second hold recheck, which itself rules out a
different region with a label comparison before paying for A*.

**Towers and healers.** Towers search targets through the same spatial-index
nearest-enemy query as soldiers (buildings allowed when the query allows them) instead
of scanning the whole army per tower per tick. A healer's "hold fire to heal" check
looks only at allies inside its healing radius and tests distance before the expensive
recoverable-health rule.

**Formation count queries.** `has_formation_slots` needs only the number of bodies,
but used to resolve an entire formation definition, including doctrine strings and
authored layout lookups, every time it was called. It now uses the count-only
resolver shared with the layout signature. Explicit visual counts and single-body
squads return immediately; nation profiles and squad-strength limits still determine
the count when needed.

**Capture rings.** Capture checks collect living non-barracks units into a compact
position/owner/type array once per update, on the first eligible barracks. Each ring
scans that array instead of repeating ECS joins for every building. The original
unit order, distance boundary, production-cost weighting and tie behavior are
preserved. The array is rebuilt next update so movement and deaths are reflected.

**Wildlife and settlements.** Wildlife tiering asks "anything within the near radius,
else the far radius" and stops at the first hit. Settlement residents find danger
through the spatial index, and a resident whose walk ends short of its errand rests
briefly instead of re-pathing every think tick.

**Fog of war.** The published snapshot is the only copy of the visibility grid; jobs
read the snapshot they started from and the worker's result is moved into the next one.
Vision sources are sorted and deduplicated, a job runs only when that set changes, and
each disc is stamped as row spans (half-width computed once per row).

**Neutral owners.** In map data `player_id` 0 marks a neutral barracks that can be
chosen as a starting base, and -1 marks a structure that never is. In the world both
are neutral: `MapTransformer` maps every owner ≤ 0 to the neutral owner. Before, owner
0 was registered as a computer player whenever no team overrides were passed, and every
order it issued (2,394 rally points in ten minutes on Copper Canyons) was rejected.

## Rendering

**Sort.** Each draw command's key, resource pointers (including `material_id`) and
submission index are computed once into a contiguous `SortEntry`, and the queue sorts
those with `std::sort`. The index is the final tie-break, so the order equals the old
stable sort. Missing `material_id` used to interleave parts that share a mesh and
fragment them into thousands of batches.

**Per-batch GL state.** Program state is tracked by what is actually bound, so an
instanced batch no longer binds the base program and then switches away. Per-frame
uniforms upload only on a real program switch. Instance attribute enable/divisor live
in the mesh VAO and are set once; only the pointers (buffer + ring offset) are re-issued
per chunk. A batch binds its VAO once.

**Buffers.** Rigged-creature stream buffers are created once and orphaned only when
the previous frame wrote to them, at their demand-grown size; the mesh instance ring
doubles on wrap (up to 16 MB) instead of wrapping many times a frame.

**Decorative civilians.** They chose the Minimal creature LOD from screen size even on
presets whose prewarm skips Minimal, so their meshes were never preloaded and they
silently vanished every frame. They now follow the preset's LOD policy. Rigged-cache
diagnostics are formatted only on a real miss.

**Static building batch.** Buildings never move, yet every frame used to expand each
visible building into its parts (66k draw commands on Aurelia), sort them, and — in the
shadow pass — gather, sort and re-upload them once per cascade. Now
`submit_building_instance` hands one instance record per building to
`ISubmitter::render_instance`, and the renderer places opaque buildings into
`StaticBuildingBatch` (`render/static_building_batch.cpp`).

- Each archetype (nation, type, damage state, farm stage) is baked once, lazily, into
  one merged mesh (`build_merged_building_mesh`, cached on the archetype): every opaque
  basic-shader part is transformed into archetype space and carries its fixed colour,
  palette slot and material ids per vertex. Parts with their own texture get their own
  index range.
- A building is one 96-byte instance record: world matrix, its (at most two) palette
  colours with their material class, damage tier and the fog-unseen flag. The colour
  pass draws one instanced call per archetype (and texture range) with
  `building_merged.vert` + `basic_instanced.frag`; each shadow cascade uploads the
  buildings it accepts and draws each archetype once with the ordinary instanced depth
  shader.
- Instancing whole buildings replaced instancing tiny parts (12-triangle cubes): the GPU
  was vertex/instance-bound, not fragment-bound, and small instances under-fill warps.
- Joining is free, so there is no settle rule, no per-entity slot table and no
  compaction: fog-unseen state, palette and damage are per-instance values, and a
  building whose inputs change every frame just writes a different record. Per-frame
  upload is the visible records only (about 100 KB on Aurelia).
- Unseen and palette colours are resolved on the CPU per instance and the fixed-colour
  material class is precomputed for both the seen and unseen colour, so the
  colour-dependent material classification never runs on the GPU and cannot disagree
  with `resolve_material_id`.
- Transparent parts, parts with a custom shader, ghosts, alpha fades, shader overrides,
  palettes wider than two, and null-entity callers (previews, tests) stay on the dynamic
  path exactly as before.
- One `ShadowCascadeCull::accepts` decides whether a caster can reach anything that
  samples a cascade; it replaced two copies of the cascade-bounds check.
- The old per-entity building instance cache, the building LOD selection (every
  archetype had only the full-detail slice) and `SOI_SHADOW_INSTANCING` are gone.

**Minimap.** Troop dots are copied from a per-owner antialiased sprite instead of
rasterising an ellipse per unit; the layer image is premultiplied ARGB32.

**Fog mask filtering.** The fog mask's 3×3 tent blur is separable. Three reusable
rows hold horizontal fog/sight sums; the vertical pass consumes those rows and
rotates the oldest row for reuse. This avoids recalculating the same horizontal
samples for three successive output rows. Dirty-region bounds, edge clamping,
channel encoding and visual detail stay the same. The renderer retains the row
scratch storage across uploads.

**No LOD.** Detail reduction is not an acceptable fix: Ultra draws everything at full
detail. Remove per-draw, per-frame and per-upload waste instead.

## GUI thread

The HUD's 100 ms `productionRefresh` timer bumped `selection_tick`, re-evaluating ~40
production-panel bindings (each calling into C++) ten times a second even with nothing
selected. The tick is now bumped only while a building or builder with a production
panel is selected; selection changes still bump it immediately.

## Build time

Every translation unit parsed ~120k lines of standard-library and Qt headers. One
precompiled header (`cmake/soi_pch.h`, applied to every project target by
`cmake/PrecompiledHeaders.cmake`) cuts a clean build of the game and tools from
2758 to 2041 CPU-seconds (−26%) and from 728 to 508 s wall at `-j3`.

- The header holds only headers that never change. Every Qt header defines the
  `slots`/`signals`/`emit` keyword macros, so no identifier may use those names.
- Configure with `-DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON` to build without it.
- ccache only hits PCH builds with `CCACHE_SLOPPINESS=pch_defines,time_macros,...`,
  which the CI workflows set.
- Release links use LTO; re-linking one small tool costs ~160 CPU-seconds because
  whole-program optimisation reruns over every static library.

### The CI Debug build from scratch

Measured 28 Sep 2026 on the pull-request lane's configuration (Debug, every test
binary plus `content_validator`, 1610 steps, `-j4`, no ccache, both before the
creature bake below stopped running twice):

|                                           | wall  | compile CPU | 14 test links | objects |
| ----------------------------------------- | ----- | ----------- | ------------- | ------- |
| `-g3 -ggdb3`, GNU ld                      | 839 s | 2236 s      | 67 s          | 5.0 GB  |
| `SOI_DEBUG_INFO=lines`, `SOI_LINKER=mold` | 732 s | 1789 s      | 4 s           | 1.3 GB  |

The objects are also what ccache stores and what CI uploads, so a quarter of the
size is what lets one cache hold a whole build. Both knobs live in
`cmake/BuildSpeed.cmake`, default off; CI turns them on.

- At `-O0`, parsing is the small part: GCC's `-ftime-report` on `world.cpp` puts 14% in
  parsing, 27% in template instantiation and 57% in code generation.
- Unity builds would attack that 57% and do not work here. See the note in
  `cmake/BuildSpeed.cmake`: 129 of 234 batches failed on clashing anonymous-namespace helpers.
- The creature bake (`bake_creature_assets`) is the longest single step: 168 s of
  `-O0` baker. It used to run twice, once per output directory; it now bakes into
  the build tree and copies into `assets/creatures`.
