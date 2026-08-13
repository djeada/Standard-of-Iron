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
