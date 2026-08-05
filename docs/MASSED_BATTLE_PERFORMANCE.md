# Massed battle performance

Rationale for the optimisations behind the `massed_battle_500` and
`massed_battle_1000` Arena scenarios. Source comments are stripped by
`make format`, so the reasoning lives here.

## Scenarios

`massed_battle_500` and `massed_battle_1000` put 25 and 50 twenty-strong squads
per side on the full 96x96 arena floor, so 1000 and 2000 rendered soldiers
respectively. Both force full creature LOD at Ultra quality: every soldier is a
full-detail skinned mesh regardless of camera distance, which is the worst case
the renderer can be asked for.

```bash
build/bin/arena_app --batch --scenario massed_battle_1000 \
  --fps 30 --duration 12 --capture-interval 0 --prewarm \
  --artifact-dir artifacts/massed
```

`--prewarm` matters. Without it, lazy render-time baking of creature variants
dominates the first ten seconds and p95 roughly triples; the campaign path
always prewarms, so measuring without it measures the wrong thing.

## How to measure

Frame time recorded in `trace.jsonl` only covers work inside `paintGL`. When the
CPU is allowed to run ahead of the GPU, driver back-pressure lands outside that
window and the trace under-reports, sometimes by 40 ms per frame. Compare
builds by wall clock over a fixed frame count instead, taking the slope between
two durations so fixed startup and prewarm cancel:

    ms_per_frame = (wall_at_18s - wall_at_6s) / ((18 - 6) * fps)

Repeats must be interleaved between the two binaries; this box drifts enough
between runs that three back-to-back runs of one build are not comparable to
three of the other.

## What was slow, and why

### Shadow casting drew the full-detail body

The directional shadow pass rendered each soldier's full mesh into the cascade
it fell in. At Ultra that is a 4096-square map, and the humanoid full mesh is
`humanoid_full.bprm`, about 125 times the size of `humanoid_minimal.bprm`.
Ablating rigged shadow casters entirely cut a 98 ms frame to 49 ms, so this was
over half the GPU cost.

The pass is depth-only, so it now requests the minimal mesh with no attachments.
That form ships prebaked per species, so it resolves from the registry rather
than baking on demand, and it shares the rig the palette was built for. The cost
is that weapons no longer notch the shadow silhouette.

### The CPU rebuilt what the bake had already produced

`blend_palette_owned` interpolates two baked keyframes. Per bone it was doing
three full `QMatrix4x4::inverted()` calls: two to recover parent-local space and
one to re-apply the inverse bind. At 20 bones and 2000 soldiers that is 120000
general 4x4 inverses per frame.

Three changes, none of which alter output:

- `humanoid_inverse_bind_palette()` builds the inverse bind table once. The bind
  pose is fixed, so inverting it per bone per soldier per frame was pure waste.
- `affine_inverse()` replaces the general cofactor path. Bone matrices always
  have a `0,0,0,1` bottom row, so a 3x3 adjugate plus a translation solve is
  exact and much cheaper.
- Parent inverses are computed once per bone rather than once per child.

`rigid_lerp_matrix` also short-circuits at weight 0 and 1, which skips two
matrix-to-quaternion extractions and a slerp.

### Blended palettes were computed per soldier

A blended palette depends only on the mesh, the two keyframes and the weight,
never on which soldier asked. Soldiers carry individual animation phases, so the
weight is quantised into 32 buckets to make neighbours collide on a shared
cache entry, and the cache is cleared each frame. One bucket is 1/32 of a 30 Hz
keyframe interval, roughly 1 ms of animation, well below anything visible.

### Every rider sampled the whole horse clip

`horse_source_pose_mount_frame` needs two bone deltas but sampled all 50 horse
bones, and `sample_source_clip` heap-allocated its pose and world buffers on
every call, plus a `vector<bool>` inside `evaluate_world_matrices`. Those are
now reused thread-local scratch. The two inverse binds it needs are built once.

Quantising rider phase was tried and reverted:
`HorseSourceAssetTest.RiderSocketFollowsAuthoredBackBone` requires the saddle to
track the authored back bone to within 1e-5 at any phase, and that is a contract
worth keeping. Removing the allocations gets the win without touching accuracy.

### The palette ring buffer overflowed into a synchronising path

`bind_streamed_palette_batch` reserved a whole batch stride per call regardless
of how many instances were actually drawn. With a 4096-instance ring and a
16-instance stride that allowed only 256 batches per frame. A full-detail 2000
soldier scene needs about 292 between the main and shadow passes, so the tail of
every frame fell back to `glBufferData` plus `glBufferSubData`, which is what
produced the multi-second hitches.

The ring now consumes only the instances written, while still requiring a batch
stride of addressable slack so the bound uniform range stays valid, and its
capacity is 8192.

`PersistentRingBuffer` also had no fences at all. It rotated between regions but
never waited, so the CPU could overwrite a region the GPU was still reading.
`end_frame()` now places a fence and `begin_frame()` waits on the region it is
about to reuse. This makes frame times honest as well as correct: work that used
to hide outside `paintGL` is now attributed to the frame that caused it.

## A trap worth remembering

`PooledPaletteAllocator::free_list()` is a thread-local, and so is the blend
cache that returns palettes to it. Thread-locals are destroyed in reverse
construction order, and the cache was constructed first, so at teardown the pool
was already gone when the cache released its last palettes: heap corruption
inside `deallocate`. The free list is now deliberately leaked so deallocation
order cannot matter.

## Results

RTX 5060, ms per frame from the wall-clock slope described above.

Absolute numbers on this box drift by up to 20% between sessions, so only
tightly interleaved A/B pairs are quoted as gains. Each pair below ran its two
variants alternately within one session, three repeats each.

| Comparison                                        | Before   | After    | Gain  |
| ------------------------------------------------- | -------- | -------- | ----- |
| GPU crowd cull on vs off (same binary, 1000v1000) | 58.97 ms | 49.91 ms | 1.18x |
| Baked palettes + cleanup (1000v1000)              | 62.22 ms | 51.81 ms | 1.20x |
| Shadow pass through the cull path (1000v1000)     | 51.81 ms | 50.36 ms | 1.03x |

End to end, original against current:

| Scenario                  | Original           | Current            | Total |
| ------------------------- | ------------------ | ------------------ | ----- |
| 500v500 (1000 soldiers)   | 51.1 ms (19.6 FPS) | 34.1 ms (29.4 FPS) | 1.50x |
| 1000v1000 (2000 soldiers) | 85.3 ms (11.7 FPS) | 58.9 ms (17.0 FPS) | 1.45x |

CPU-side, which is far less sensitive to machine drift: `world_submit` at
1000v1000 went from 19.4 ms to 6.8 ms, and `humanoid_preparation` from 3.7 ms to
2.0 ms. Worst observed frame fell from 5113 ms to under 70 ms.

### Two measurement traps, both of which produced wrong numbers here

Earlier revisions of this document claimed 2.43x and 1.78x totals. Both were
wrong, for different reasons, and both looked plausible at the time.

The first was a single unrepeated wall-clock measurement that happened to land
35.9 ms/frame for a binary that repeated runs put at 60-63 ms. One sample is not
a measurement on this machine.

The second was self-inflicted. The stats readback in `RiggedCullPipeline` used
`glGetBufferSubData`, which is a full GPU sync. It ran every 120 draws and drained
the pipeline, so the frames around it absorbed all the queued GPU work in one
120 ms spike while every other frame measured 8 ms. Median frame time looked six
times better than the truth; only wall clock over a fixed frame count exposed it.
The readback is now behind `SOI_CULL_STATS` so it never runs unless asked for.

The rule that survives: on this box, believe wall clock over a fixed frame
count, interleave the variants, and repeat. Frame timers inside `paintGL` can
be wrong in both directions.

## A GL lifetime trap that crashed two scenarios

`ballista_impact` and `catapult_impact` segfaulted at process exit. The stack was
always the same: a function-static `unique_ptr<Mesh>` destroyed by an atexit
handler, `Mesh::~Mesh` reaching `Buffer::~Buffer`, and that faulting inside
`QOpenGLContext::currentContext()`.

The destructors already guarded on `currentContext() == nullptr`. The guard was
the bug: `currentContext()` reads Qt thread-local storage, and by the time exit
handlers run Qt has already torn that storage down, so the probe faults rather
than returning null.

`Render::GL::gl_objects_can_be_released()` checks `QCoreApplication::instance()`
first, which is a plain global pointer and safe to read at any point, and only
then asks about the context. Every GL destructor guard goes through it. This is a
root fix rather than a per-static one: any future namespace- or function-static
GL object is covered, which matters because chasing them individually did not
work -- fixing three static mesh caches just moved the crash to a fourth.

A sweep of all 199 arena scenarios now reports zero crashes.

## Deduplication in render/

- `ballista_geometry.cpp` holds the ballista once. Rome and Carthage had 1196
  lines between them for a machine whose geometry was byte-identical; the only
  real difference was that Rome finishes its fittings in bronze and Carthage in
  gold, which is now a single `accent` slot in `BallistaPalette`. The extraction
  was verified by diffing both originals against the shared body: identical apart
  from whitespace. Two dead palette fields went with it.
- `apply_mount_loadout` applies the eleven mount equipment slots and the debug
  names that every mounted troop config repeats. Four renderers lost fourteen
  lines each of field-by-field copying.
- `is_prewarmable_spawn` was a 22-case switch over `SpawnType` answering the same
  question as the 22-case switch over `TroopType` beside it. It now derives from
  the troop predicate via `spawn_typeToTroopType`, so the two lists cannot drift.
- `primitive_geometry.cpp` had three parallel switches over `PrimitiveShape`.
  Whether a shape spans two bones, and which unit mesh it uses, are now one
  traits table, so adding a shape means adding a row rather than remembering
  three call sites.

The catapult pair was left alone deliberately. It looks like the ballista at a
glance but the two nations genuinely differ in geometry -- different proportions
and extra bronze rails on the Carthaginian frame -- so merging them would have
meant parameterising real visual differences rather than removing duplication.

## OpenGL floor

OpenGL 3.3 Core is still the hardware floor and both entry points still request
it. The crowd-culling path is reached through extension probes
(`GL_ARB_compute_shader`, `GL_ARB_draw_indirect`,
`GL_ARB_shader_storage_buffer_object`), which NVIDIA advertises on a 3.3 context;
where they are absent, or where the driver refuses GLSL 4.30 on a 3.3 context,
pipeline construction fails and the renderer keeps the instanced path. macOS caps
desktop GL at 4.1 and will always take the fallback.

`scripts/validate_opengl_requirements.py` enforces this: baseline shaders must
declare `#version 330 core`, the four 4.30 shaders must be listed in
`OPTIONAL_GL43_SHADERS` and referenced from a pipeline that calls both capability
probes, compute shaders must be embedded in `assets.qrc`, and no entry point may
request a context above 3.3. The Linux release workflow runs the packaged
renderer self-test twice, once with `SOI_RENDER_DISABLE_GPU_CROWD_CULL=1`, so the
fallback is exercised on every release.

## What is left

The frame is GPU bound at 2000 full-detail soldiers; CPU submission is now about
7 ms of a 50 ms frame. Turning the cull path off costs only 18%, which says most
of the remaining GPU time is not triangle setup any more. The next honest step is
to measure the GPU directly with timer queries rather than inferring it from
frame timers, because the phase timers cannot see inside the driver.

Candidates once that data exists:

- Pack palettes to the real bone count. `k_palette_width` is 64 while the
  humanoid rig has 20, so the static baked buffer and the legacy path both carry
  two thirds padding.
- The mount pose path (`resolve_mount_render_state`) is the largest remaining
  per-instance CPU cost in the scene walk.
- Shadow cascade resolution at Ultra is 4x4096; the cull path made the geometry
  cheap but not the fill.
