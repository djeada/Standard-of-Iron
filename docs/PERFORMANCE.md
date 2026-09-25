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
