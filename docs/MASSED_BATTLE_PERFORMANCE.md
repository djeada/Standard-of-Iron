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

RTX 5060, ms per frame from the wall-clock slope described above, median of two
interleaved repeats.

| Scenario                  | Before             | After              |       |
| ------------------------- | ------------------ | ------------------ | ----- |
| 500v500 (1000 soldiers)   | 58.6 ms (17.1 FPS) | 33.8 ms (29.6 FPS) | 1.73x |
| 1000v1000 (2000 soldiers) | 96.0 ms (10.4 FPS) | 59.8 ms (16.7 FPS) | 1.61x |

Worst observed frame at 1000v1000 fell from 5113 ms to 170 ms. At 500v500 no
frame exceeded 100 ms after the change, against five before.

## Round two: GPU crowd culling

The remaining GPU cost was not vertex fetch, it was rasterising triangles that
resolve to nothing. `humanoid_full.bprm` carries 24,960 triangles; at the
strategic camera a soldier covers roughly 80 pixels, so the mesh is about 300
triangles per covered pixel. GPUs rasterise in 2x2 quads, so every sub-pixel
triangle still costs at least four fragment invocations.

Swapping the main pass to the 128-triangle minimal mesh took the GPU wait from
43.5 ms to 0.17 ms, which bounded the prize before any of this was written.

### How it works

`RiggedCullPipeline` replaces the chunked instanced draw for large batches. Per
prepared batch it runs one compute dispatch over every (instance, triangle)
pair, which skins the three vertices with the same code the vertex shader uses,
projects them, and drops the triangle when either test fires:

- **Backface**, from the sign of the screen-space area, matched to the live
  `GL_CULL_FACE_MODE` and `GL_FRONT_FACE` state rather than assumed.
- **No covered sample**, when the screen bounding box contains no pixel centre.

The second test is exact, not an approximation. A triangle whose bounding box
misses every sample position produces no fragments, so removing it before the
rasteriser cannot change the image. That is why full detail is preserved: this
is not a LOD scheme, it discards only work that was already going to be thrown
away, and at this zoom that is the overwhelming majority of it.

Survivors are compacted into one index buffer holding global vertex ids, and the
whole army draws with a single `glDrawElementsIndirect`. The vertex shader takes
no attributes; it derives the instance from `gl_VertexID / vertex_count` and
reads vertices, palettes and instance data from SSBOs. Draw calls per batch fall
from about 146 to one, and the per-batch uniform rebinds and instance VBO
orphaning disappear with them.

Triangles that cross the near plane are never culled, since they cannot be
projected safely, and the bounding box carries a small guard band so a
borderline triangle survives rather than flickers.

### Constraints

The path needs compute shaders and indirect draw, so both entry points now
request a 4.3 core context instead of 3.3. Everything is feature-detected:
`GLCapabilities::has_compute_shaders()` and `has_indirect_draw()` gate
initialisation, and a failure leaves `m_rigged_cull_pipeline` null so the
existing instanced path runs unchanged. `SOI_RENDER_DISABLE_GPU_CROWD_CULL`
forces the old path for A/B testing. Batches below 24 instances keep the old
path, where a dispatch would not pay for itself.

The compacted buffer is sized at a quarter of the worst case, capped at 12M
triangles. If survivors exceed it the shader sets an overflow flag and stops
writing rather than running past the end; tracing builds sample that flag every
120 draws and warn. No overflow was observed at any zoom from 0.25x to 1.0x
camera distance.

## Results

RTX 5060, ms per frame from the wall-clock slope described above, median of two
interleaved repeats.

| Scenario                  | Original           | After round 1      | + GPU crowd cull   | Total |
| ------------------------- | ------------------ | ------------------ | ------------------ | ----- |
| 500v500 (1000 soldiers)   | 51.4 ms (19.5 FPS) | 33.8 ms (29.6 FPS) | 28.4 ms (35.2 FPS) | 1.81x |
| 1000v1000 (2000 soldiers) | 87.2 ms (11.5 FPS) | 59.9 ms (16.7 FPS) | 35.9 ms (27.8 FPS) | 2.43x |

With culling on, GPU wait at 1000v1000 drops from 37.2 ms to 4.9 ms. Worst
observed frame at 1000v1000 fell from 5113 ms to 170 ms over the two rounds.

## What is left

Both scenarios are CPU bound again: at 1000v1000 `world_submit` is about 15 ms
of a 22 ms traced frame. The next lever is the one deferred in round one:

- Move keyframe interpolation into the vertex shader, reading the baked palettes
  the bake already uploads to `skin_palette_ubo`. The GPU-driven path removes the
  obstacle that blocked this, since it already feeds palettes through an SSBO
  rather than the batch-sized uniform block.
- Pack palettes to the real bone count. `k_palette_width` is 64 while the
  humanoid rig has 20, so two thirds of every palette upload is padding. This
  now only affects the legacy instanced path and the shadow pass.
- Extend the cull dispatch to the shadow pass, which still uses the old instanced
  path against the minimal mesh.
