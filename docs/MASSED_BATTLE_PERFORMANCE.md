# Full-LOD massed-battle performance

This document describes the full-detail crowd path and its reproducible Arena
benchmarks. Full LOD is an invariant for these scenarios: visible creatures use
the complete indexed mesh in both the color and directional-shadow passes.
There are no billboards, reduced meshes, dropped attachments, reduced shadow
silhouettes, or animation approximations in the results below.

## Benchmark scenarios

| Scenario             | Soldiers per side | Rendered soldiers |
| -------------------- | ----------------: | ----------------: |
| `massed_battle_250`  |               250 |               500 |
| `massed_battle_500`  |               500 |             1,000 |
| `massed_battle_1000` |             1,000 |             2,000 |
| `massed_battle_2000` |             2,000 |             4,000 |

`seven_ai_scale` exercises a different failure mode: seven simultaneous AI
players, 140 initial buildings, 252 simulation units, large starting economies,
and about 2,000 rendered soldiers. It also forces full creature LOD and Ultra
shadows.

An uncapped-like 240 Hz fixed-step run exposes renderer throughput without the
30 Hz frame pacer hiding work below 33.3 ms:

```bash
build/bin/arena_app --batch --scenario massed_battle_1000 \
  --fps 240 --duration 1 --capture-interval 0 \
  --artifact-dir artifacts/full-lod-2000
```

The campaign-equivalent cache path can be checked separately:

```bash
build/bin/arena_app --batch --scenario massed_battle_1000 \
  --fps 30 --duration 1 --capture-interval 0 --prewarm \
  --artifact-dir artifacts/full-lod-prewarm
```

Compare warmed samples and interleave binaries when doing A/B work. Internal
frame timers can omit driver back-pressure, while wall time includes fixed
startup and shutdown. Both should be recorded.

## Architecture

### Shared full bodies and split equipment

The old cache baked a complete body-plus-equipment mesh for every visual
variant. Full bodies are now shared by `(creature spec, species)`. Equipment is
baked once per attachment set into one additional mesh, so a soldier normally
submits a shared body and a combined equipment mesh. This keeps complete visual
geometry while avoiding thousands of copies of the 14,888-vertex humanoid body.

Animation atlases are also shared per species instead of being copied into each
mesh entry. The per-unit variant bucket was removed from mesh identity because
it never changed baked geometry.

### Exact indexed SSBO instancing

Full meshes bypass snapshot meshes and the triangle-expanding compute-cull path.
The backend binds the original vertex buffer as an SSBO, uploads instance and
palette records once per prepared batch, retains the original index buffer, and
calls `glDrawElementsInstanced`. The path is used for both color and shadow
passes and never removes triangles.

At 2,000 soldiers, 4,360 logical rigged commands collapse to 25 actual indexed
instanced draws in the color pass and 25 in the shadow pass. At 4,000 soldiers,
8,720 logical commands collapse to 27 draws per pass.

### Exact rigid/blended skinning ranges

The humanoid body has 13,615 rigid vertices out of 14,888, while the remaining
section genuinely blends bones. Each mesh index buffer is partitioned by
triangle into exact rigid and blended ranges. A rigid triangle fetches only its
single bone matrix; a blended triangle retains the general weighted path and
conditionally fetches every non-zero influence. Mixed meshes therefore cost at
most one extra draw per instanced batch without changing any vertex result.

The same optimization runs in the directional-shadow shader, which is important
because Ultra renders four 4096-square cascades.

### Full-LOD-only prewarm

When the renderer is forced to full LOD, prewarm queues only full-detail work.
It no longer creates Minimal or Billboard snapshot caches that gameplay cannot
use in that mode. This is a cache-residency optimization, not an LOD change: the
render path remains full detail before and after prewarm.

On the benchmark machine, the 2,000-soldier prewarm peak fell from about 4.3 GiB
to 1.16 GiB and wall time fell from about 14 seconds to 5.2 seconds. A normal
non-prewarmed run peaks around 650-730 MiB.

### Immutable frame streams and cached role palettes

Rigged instance, owned-palette, and role-color uploads use one orphaned stream
per frame and append immutable ranges. A shadow or color draw never overwrites
storage still referenced by an earlier draw. This removes the periodic driver
serialization caused by repeatedly writing offset zero in shared SSBOs and
texture buffers.

Visual role colors are immutable for a creature visual variant. They are now
cached as shared palettes rather than copied as a 32-color array into every
render request and every body/equipment command. The GPU stream similarly
uploads each distinct palette once per frame and stores only its palette index
in each instance. The common main-pass submit path also consumes its prepared
request span directly instead of copying the entire request vector merely to
filter absent shadow-only rows.

BPAT frame palettes remain resident and shared. Runtime work is limited to
selecting baked frame offsets, packing compact changing instance state, and
submitting exact full-mesh draws; no skeleton solve or geometry bake was moved
back into gameplay.

### Retained preparation storage and lean submission

Humanoid, wildlife, live-slot, and draw-pass scratch storage now survives frame
boundaries and is cleared in place. Per-soldier visibility history lives beside
the formation's other dense soldier state instead of in a separately allocated
hash table. Formation-invariant visual specs, mode poses, render-asset handles,
and role palettes are resolved once and reused across the formation.

Rigged commands move through submitter wrappers into the retained draw queue,
avoiding shared-pointer churn. Shadow stream packing writes only transform,
variation, and BPAT palette fields; color, wear, material, and role metadata are
left to the color pass. Diagnostic body-pose matrix work and OpenGL error polling
are also absent from normal release rendering and remain available when their
debug facilities are enabled.

In warmed detailed traces, the complete continuation moved 2,000-soldier
humanoid preparation from about 3.52 ms to 3.27 ms and world submission from
4.38 ms to 3.96 ms. The seven-AI trace moved from about 2.75 ms to 2.54 ms for
humanoid preparation and from 3.96 ms to 3.71 ms for world submission. Frame
timings still move between phases when driver back-pressure lands, so these
application-side phase measurements are more useful than any single internal
frame percentile.

### Removed compatibility paths

The old rigged instancing pipeline, its persistent palette ring, its instanced
vertex shaders, and its dedicated tests have been deleted. Full-detail batches,
including one-instance batches, use the current GPU full-mesh path. The only
remaining direct rigged draw is the explicit one-body fallback when the modern
pipeline is unavailable.

The unused legacy visual-slot ownership mask was also removed from every unit
visual spec and creature definition. It had no reader and only propagated dead
compatibility state through renderer construction.

## Profile result

Linux `perf` on a RelWithDebInfo build found over 60% of sampled CPU time in the
NVIDIA driver and fence waiting. Roughly 10% was directly attributed to
`glClientWaitSync` in the persistent ring buffer, with most remaining samples in
driver code. Application-side simulation and submission were small by
comparison, confirming that the massed-battle limit is GPU/back-pressure rather
than AI or world simulation.

The seven-AI scenario supports the same conclusion: its simulation phase is
normally about 1.6-1.9 ms, while the rendered frame is roughly 34-41 ms.

## Results

Machine: NVIDIA GeForce RTX 5060, driver 590.48.01, OpenGL 4.5, Ultra quality,
full meshes and full-mesh shadows. Values are warmed `trace.jsonl` samples. The
500-2,000 totals use a 240 Hz fixed step; 4,000 uses 30 Hz because every frame is
already slower than the pacer.

| Rendered soldiers |      p50 |      p95 | Approx. p50 FPS |
| ----------------: | -------: | -------: | --------------: |
|               500 | 15.94 ms | 17.23 ms |            62.7 |
|             1,000 | 25.86 ms | 33.87 ms |            38.7 |
|             2,000 | 39.22 ms | 45.13 ms |            25.5 |
|             4,000 | 74.39 ms | 88.34 ms |            13.4 |

The 2,000-soldier full-mesh path before the exact skinning work measured
70.48/75.75 ms p50/p95. Current is 39.22/45.13 ms, a 44% median and 40% p95
reduction without changing LOD, geometry, attachments, shadows, or animation.

At 4,000 soldiers, the corresponding median moved from 135.70 ms to 74.39 ms,
about a 45% reduction. `seven_ai_scale` moved from roughly 59.2/63.3 ms to an
Arena-reported 33.84/40.55 ms p50/p95 while retaining its seven AI economies and
full-detail army rendering.

The immutable-stream and shared-palette pass was measured against commit
`ac57124f` on the same RTX 5060 machine. The 500-2,000 rows use the uncapped-like
240 Hz fixed step; 4,000 soldiers and seven AI use 30 Hz because their frames
already exceed the pacer. Values below exclude the first warm-up interval.

| Scenario / rendered soldiers | Baseline avg | Current avg | Baseline p95 | Current p95 |
| ---------------------------: | -----------: | ----------: | -----------: | ----------: |
|                          500 |     13.83 ms |    12.87 ms |     17.50 ms |    17.86 ms |
|                        1,000 |     23.14 ms |    21.85 ms |     29.04 ms |    26.33 ms |
|                        2,000 |     37.00 ms |    33.94 ms |     43.03 ms |    39.99 ms |
|                        4,000 |     71.88 ms |    64.34 ms |     82.43 ms |    74.10 ms |
|        seven AI / ~1,650 vis |     35.15 ms |    26.46 ms |     49.07 ms |    35.24 ms |

The largest benefit in mixed gameplay is tail stability: seven-AI p95 improves
about 28%, and the massed-battle path no longer exhibits the previous 100-200 ms
SSBO-overwrite stalls. At 2,000 and 4,000 soldiers sustained averages improve
about 8% and 10.5%, respectively, without changing LOD, mesh topology,
attachments, shadows, or BPAT animation.

An older revision of this document quoted faster numbers obtained with a
minimal body-only shadow mesh. Those measurements are intentionally not used as
the current baseline because that shadow path violated full-detail comparison.

## Correctness checks

- Full requests never use snapshot meshes.
- The original full indexed mesh is used in color and shadow passes.
- Equipment remains present as a combined attachment mesh.
- Rigid ranges contain only exactly `(1, 0, 0, 0)` weighted vertices; all other
  triangles stay on the general blend path.
- Full-LOD prewarm work-item tests reject lower-detail queue entries.
- A deterministic before/after frame comparison changed only 7,356 of 1,007,340
  pixels, with normalized RMSE 0.00725; visual inspection showed complete armies,
  equipment, formations, and shadows.

## Remaining bottleneck

Scaling is close to linear once startup is removed, and the driver/fence profile
shows the GPU is the limiting resource. Future work must preserve the full-LOD
invariant. Viable directions are packed vertex/palette formats, pre-skinning an
exact pose once for reuse across color and shadow passes, and GPU timer queries
to separate vertex shading from cascade raster cost. Reduced meshes, billboards,
attachment removal, and simplified shadow casters are explicitly out of scope.

## Preparation pass: per-soldier CPU work

The measurements above are GPU/back-pressure limited, but the CPU side of a
full-LOD frame is not free: `world_submit` runs `prepare_humanoid_instances`
for every unit and the humanoid family accounts for the bulk of it. This pass
attacked that cost without touching LOD, geometry, attachments, shadows or the
animation result. Everything below is per soldier per frame unless stated.

### What the profile said

- The whole `append_prepared_soldier` lambda ran on the render thread, serially
  across units, with a `DrawContext` copy per soldier (a heap allocation: the
  context carries a `std::string`), several `QMatrix4x4::rotate` calls (sin/cos
  and 4×4 multiplies) and a terrain height sample even when nobody moved.
- Diagnostics-only work — the hit-reaction transform, `mapVector` + `acos` for
  the root tilt, `resolve_pose` for the debug sample — was evaluated for every
  soldier because it lived in the argument list of `record_soldier_debug`.
- `resolve_humanoid_animation_selection` (crossfade, ambient, hold, combat layer
  policy) ran for every soldier every frame even when the answer could not have
  changed.
- On the submit side `contact_y_for_playback` walked the bone palette (matrix
  inversions, sole-point transforms) per soldier, twice when frame-lerping, and
  tested clip _names_ for `riding_`/`showcase_`. Layered poses (full-body blend,
  upper-body overlay) each rebuilt an owned palette through the hierarchical
  rigid blend with no sharing between soldiers of the same unit.

### What changed

| Area                | Change                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| ------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| Prepare loop        | One `DrawContext` and one unit base matrix per unit; per-soldier composition only translates and yaws. Diagnostics-only computations are gated on `CombatAnimationDiagnostics` being enabled. Unit-invariant inputs (commander jump override, archer bow-ready flag) are hoisted out of the lambda.                                                                                                                                                                                                                                                                                                                                                        |
| Terrain grounding   | `HumanoidLayoutCacheComponent::ground_samples` caches the surface and model height per soldier and reuses it while the soldier's XZ has not moved by more than 1 mm.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                       |
| Animation selection | `HumanoidLayoutCacheComponent::selection_cache` keeps the last resolved selection per soldier. When the soldier is _steady_ (pure idle/walk/run, no combat, hit, hold, guard, ambient, crossfade or overlay) and its archetype and movement state are unchanged, the selection is reused and only the playback phase is recomputed. Every soldier still fully re-resolves once every 8 frames (seed-staggered), so a stale cache can never survive longer than that. Full LOD geometry is unaffected; only the resolution cadence changed.                                                                                                                 |
| Parallel prepare    | `HumanoidRendererBase` now implements `IParallelPreparer` (`ensure_prepare_components` + `prepare`) and registers it alongside its `RenderFunc`. `Renderer::render_world` plans every unit serially, adds any render-side components on the main thread, prepares humanoid units on a small persistent worker pool (`PrepareWorkerPool`, at most 3 workers plus the caller) and submits the prepared results serially in the original order. The first prepare of each renderer handle runs serially so lazily baked per-renderer caches are warm before workers touch them. Parallel prepare is skipped while combat animation diagnostics are recording. |
| Thread safety       | `CreatureRenderAssetHandleRegistry` guards its lookup with a `shared_mutex`; `VisibilityBudgetTracker`'s contact-shadow bookkeeping is mutex-guarded; the humanoid/quadruped render stats and the prepare-touched `FrameProfile` accumulators are atomics. `humanoid_preparation_us` now measures the wall time of the whole prepare pass instead of summing per-unit durations, so it stays meaningful with workers.                                                                                                                                                                                                                                      |
| BPAT v3             | Baked per-frame contact heights, per-clip flags and an explicit clip-variant table (see `CREATURE_BPAT_FORMAT.md`). `contact_y_for_playback`, `humanoid_clip_contact_y` and `horse_clip_contact_y` are table lookups; the clip-name tests and the duplicated idle-variant name check are gone.                                                                                                                                                                                                                                                                                                                                                             |
| Layered blends      | Full-body and upper-body blends go through the same per-frame result cache the frame-lerp already used, keyed by `(mesh entry, both frame pairs, lerp buckets, weight bucket, stage)`, so soldiers of one unit in the same clip pair share one owned palette instead of each rebuilding the hierarchical blend. Blend weights quantise to 32 buckets that hit 0 and 1 exactly.                                                                                                                                                                                                                                                                             |
| Purged              | `adjust_world_to_palette_contact`, `species_to_bpat_id`, the never-emitted temporal-skip cull (`should_render_temporal`, `TemporalSkipParams`, `CullReason::Temporal`, `SoldierCullReason::Temporal`, `soldiers_skipped_temporal`) and the duplicated humanoid renderer registration lambdas (`register_humanoid_renderer` replaces fourteen copies, including two builder sites that constructed a second unused static renderer).                                                                                                                                                                                                                        |

### Measurement method

`arena_app --batch --scenario <id> --fps 60 --capture-interval 0 --profile` on the
RTX 5060 machine, warmed `trace.jsonl` samples (first 60 frames dropped), baseline
binary built from `main` (`1afd892a`) with v2 blobs, candidate with v3 blobs,
binaries and blob sets interleaved A/B/A/B/A/B and re-installed before every
run. `p50`/`p95` are frame time; `submit` is `world_submit`, `prep` is
`humanoid_preparation` (both medians, ms).

`massed_battle_1000` (2,000 rendered soldiers, forced full LOD, Ultra):

| Run | Baseline p50 | Baseline submit | Baseline prep | Candidate p50 | Candidate submit | Candidate prep |
| --: | -----------: | --------------: | ------------: | ------------: | ---------------: | -------------: |
|   1 |     16.47 ms |         4.78 ms |       3.20 ms |      16.93 ms |          2.97 ms |        1.50 ms |
|   2 |     16.74 ms |         4.74 ms |       3.20 ms |      17.33 ms |          2.95 ms |        1.47 ms |
|   3 |     17.36 ms |         4.79 ms |       3.21 ms |      15.65 ms |          2.94 ms |        1.48 ms |

`massed_battle_2000` (4,000 rendered soldiers), one round:

| Build     |      p50 |      p95 |  submit |    prep | render_execute |
| --------- | -------: | -------: | ------: | ------: | -------------: |
| Baseline  | 47.01 ms | 84.90 ms | 9.69 ms | 6.35 ms |       33.11 ms |
| Candidate | 45.46 ms | 77.39 ms | 5.82 ms | 2.69 ms |       38.08 ms |

`campaign_scale_battle` (production LOD, ~380 visible soldiers), two rounds:

| Build     |            p50 |         submit |           prep |
| --------- | -------------: | -------------: | -------------: |
| Baseline  | 3.34 / 3.42 ms | 1.08 / 1.10 ms | 0.48 / 0.49 ms |
| Candidate | 3.17 / 3.27 ms | 0.80 / 0.83 ms | 0.33 / 0.33 ms |

Across the three scenarios the CPU-side prepare phase drops 31-58% and
`world_submit` 26-40%. Visible-soldier, draw-call and instanced-draw counts are
identical between the builds in every run (`report.json` metrics are the
regression detector; frames themselves are not pixel-stable).

Frame time at 2,000 and 4,000 full-LOD soldiers barely moves on this machine
because the frame is GPU-bound: `nvidia-smi` reads 100% utilisation at P0
during the run, and `render_execute` (the phase that waits on the GPU) grows by
almost exactly the CPU time the prepare pass gives back. The saving is real —
it is the render thread's own time — but it only becomes frame time once the
GPU stops being the ceiling (a smaller crowd, production LOD, or the GPU-side
items in _Remaining bottleneck_). Note for future A/B work: an earlier session
on the same box measured the same 1,000-per-side scenario at ~7.5 ms p50 with
`render_execute` around 1.5 ms; after a reboot the GPU throughput halved and
stayed there for both binaries. Always interleave and never compare against
numbers from a different boot.

### Not done, and why

- **GPU-side layered blend.** The humanoid full-body/upper-body blend is a
  hierarchical, parent-local rigid interpolation with FK reconstruction
  (`blend_palette_owned`); it cannot be reproduced by the per-vertex matrix lerp
  the instanced shaders already do, so it was not moved into the vertex shader.
  The remaining path is a compute pre-pass over the resident frame streams that
  writes blended palettes per distinct `(clip pair, weight bucket)` combination.
  With the shared blend cache in place the CPU cost is already amortised across a
  unit, so this is deferred until profiling shows blends on the critical path.
- **Skin atlas in GPU layout at bake time.** `rigged_entry_ensure_skin_atlas_from_blob`
  still multiplies `palette × inverse_bind` per frame per bone at load. That is
  load/hitch cost, not per-frame cost, and it depends on the bind palette of the
  baked mesh entry, so it stays a follow-up.
- **Prebaked humanoid rigged meshes.** Unchanged (deliberate: humanoid archetype ×
  attachment-set combinatorics); prewarm still hides the first-sight bake.
