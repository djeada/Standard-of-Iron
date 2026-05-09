# Rendering Pipeline Todo

Goal: make the runtime render path modern, scalable, and close to data-only playback while the game is playing. The current hot path is still dominated by creature preparation, sorting, minimap fog cadence, and a few remaining runtime cache/build paths.

The perf sample that motivated this list showed `RiggedMeshCache::get_or_bake`, `Backend::execute`, `__memmove`, allocator calls, `prepare_humanoid_instances`, `SnapshotMeshCache`, and minimap fog activity on the render thread. A later sample additionally showed `Render::Creature::Pipeline::submit_snapshot_creature(...)` as the top app-side hotspot, with visible time in `Render::GL::RiggedMeshCache::get_or_bake_prehashed(...)`, `__memmove_avx_unaligned_erms`, and render-thread work on `QSGRenderThread`.

## P2 - Terrain and World Assets

### Keep terrain/scatter as the model for static data

Files:
- `render/ground/*`
- `render/terrain_scene_proxy.h`

Problem:
- Terrain scatter is close to the target, but stable-frame upload behavior still needs a proof path.

Work:
- Ensure all terrain feature types follow the same pattern: load/configure builds immutable GPU buffers; frame submit only references buffers plus small time/visibility parameters.
- Add dirty flags so `Scatter::sync_direct_state()` and similar upload paths can prove they do not upload in stable frames.
- Add perf regression coverage for terrain uploads.

Acceptance:
- Stable terrain frames perform no terrain/scatter CPU generation and no buffer upload except intentional animated/time uniforms.

### Separate minimap fog cadence from render frame cadence

Files:
- `app/core/minimap_manager.*`
- `game/map/minimap/*`
- call sites in engine/render loop

Problem:
- `MinimapManager::update_fog()` appears in the render-thread sample.
- Fog/minimap texture updates still need fixed-cadence scheduling, off-thread recomputation where safe, and dirty-region uploads.

Work:
- Throttle fog/minimap texture updates to a fixed cadence or dirty-region updates.
- Ensure expensive fog recomputation happens off the render thread where safe.
- Upload only changed texture regions when possible.

Acceptance:
- Minimap fog no longer appears as a regular QSG render thread cost in stable gameplay captures.

## P2 - Equipment and Data-Driven Extensibility

### Move equipment selection toward data handles

Files:
- `render/equipment/equipment_registry.*`
- `render/equipment/register_equipment.cpp`
- `assets/visuals/unit_visuals.json`
- nation/unit style files under `render/entity/nations/`

Problem:
- Equipment renderers are registered cleanly by category/id, but many combinations are still code-authored and resolved through renderer/style code.

Work:
- Define data-side equipment loadouts per unit/nation/variant.
- Resolve equipment IDs to compact renderer/attachment handles during unit visual setup.
- Store attachment specs and role-color needs in the creature archetype/visual handle.

Acceptance:
- Adding a helmet/armor/weapon variant mostly requires data plus one renderer implementation, not editing multiple unit renderer paths.

### Normalize attachment ownership

Files:
- `render/static_attachment_spec.*`
- `render/equipment/*`
- `render/creature/archetype_registry.*`
- `render/creature/pipeline/*`

Problem:
- Attachment sets affect rigged mesh cache keys. Runtime hashing/comparison of attachment spans is visible risk.

Work:
- Assign each unique attachment set a stable `AttachmentSetId` at registration/prewarm time.
- Store attachment hashes and resolved specs in the archetype descriptor or render asset handle.
- Use `AttachmentSetId` in cache keys instead of hashing spans during gameplay.

Acceptance:
- Creature hot path does not call `static_attachments_hash()` per rendered creature.

## P2 - Thread and System Boundaries

### Keep simulation queries out of render-thread captures

Files:
- `app/core/game_engine.cpp`
- `game/systems/combat_system*`
- `render/scene_renderer.cpp`

Problem:
- The sample includes combat query rebuild/find-nearest-enemy on `QSGRenderThread`.

Work:
- Verify whether simulation update is intentionally running on the same thread before render.
- If not intentional, move combat query rebuilds and nearest-enemy searches fully into simulation update.
- Render should consume visual state produced before rendering: combat active, attack phase, hit reaction, target direction, and mode flags.

Acceptance:
- Render-thread perf captures do not include combat query rebuilds or target search as regular costs.

### Add render-thread stage documentation and first-use logging

Files:
- `app/core/game_engine.cpp`
- `render/scene_renderer.cpp`
- `render/gl/backend.cpp`

Problem:
- QSG render-thread captures need clearer stage boundaries and first-use logging to distinguish expected one-time work from steady-frame cost.

Work:
- Document the expected QSG render-thread stages.
- Add first-use logging around expensive render setup/cache paths.
- Keep logs lightweight and disabled or rate-limited for normal gameplay.

Acceptance:
- Perf captures can distinguish one-time initialization from recurring render-thread costs.

## P2 - Backend Execution Cleanup

### Reduce backend scratch and dispatch overhead

Files:
- `render/gl/backend.cpp`
- `render/draw_queue.*`

Problem:
- `Render::GL::Backend::execute(...)` remains visible in captures.
- Backend scratch-buffer allocation/copy behavior still needs auditing.

Work:
- Audit backend scratch buffers for avoidable per-frame allocation and copying.
- Reuse frame-local backend buffers where possible.
- Add lightweight counters for draw queue command count, reallocations, and copied bytes.

Acceptance:
- Stable frames avoid backend scratch allocations and avoidable command-data copies.

## P2 - Creature Submit and Cache Hot Path

### Reduce snapshot and rigged creature submit cost

Files:
- `render/creature/pipeline/creature_pipeline.cpp`
- `render/rigged_mesh_cache.*`
- `render/snapshot_mesh_cache.*`
- `render/draw_queue.*`
- `render/gl/backend.cpp`

Problem:
- A perf sample shows `Render::Creature::Pipeline::submit_snapshot_creature(...)` as the top app-side hotspot.
- `__memmove_avx_unaligned_erms` and allocator/kernel memory-accounting samples are prominent on both the main app thread and `QSGRenderThread`.
- `Render::GL::Backend::execute(...)` is visible but materially smaller than creature submit/caching work, so the primary bottleneck is data preparation rather than final GL dispatch.

Work:
- Trace `submit_snapshot_creature()` line-by-line and identify all per-creature work that can be moved to registration, prewarm, or frame-shared state.
- Ensure snapshot mesh cache hits on hot frames do not allocate, load blobs, or rebuild mesh data.
- Ensure rigged mesh cache hits on hot frames do not bake, upload, or repack skin data.
- Audit draw submission and cache code for avoidable `std::vector` growth, temporary containers, structure copies, and span-to-owned-data conversions.
- Add lightweight counters for per-frame bytes uploaded for skin UBO / palette data.
- Pre-size or reuse hot-path buffers so warmed frames avoid allocator and `memmove` samples.
- Review whether repeated math such as vector normalization in creature submit can be cached or hoisted when visual state is unchanged.
- Add warmed-battle perf validation.

Acceptance:
- Warmed creature-heavy frames avoid runtime mesh bake/load work, minimize allocation/copy samples, and reduce `submit_snapshot_creature()` cost in captures.

## P2 - Fog of War and Map Boundary Fog (next iterations)

Files:
- `assets/shaders/fog_instanced.{vert,frag}`
- `render/ground/fog_renderer.*`
- `render/ground/map_boundary_fog_renderer.*`
- `render/gl/backend/cylinder_pipeline.*` (fog instancing path)

Context:
- The first pass (procedural cloud noise in `fog_instanced.frag`, radial puffs,
  larger overlapping quads, narrower clear ring + much wider boundary band)
  removed the obvious "tile grid" look and the thin gray stripe at the map
  edge. Several deeper improvements are still on the table; all of them keep
  CPU work flat and push the work to the GPU, with prebake/cache for any data
  that depends only on map dimensions or the visibility version.

Work:
- **Single-quad, mask-textured fog of war.** Replace the per-cell `FogInstanceData`
  stream with one large ground-plane quad covering the map and a small R8 (or
  RG8 to encode `unexplored / explored / visible` plus a smoothing channel)
  texture that holds the visibility cells. Update the texture only when the
  visibility version changes (already gated in `game_engine.cpp`). Sample with
  GL_LINEAR + procedural cloud noise in the fragment shader so the edges read
  as soft mist instead of stamped puffs. Eliminates instance generation
  entirely and makes 256x256 maps cost the same as 32x32.
- **Volumetric/edge-aware boundary fog from a handful of quads.** Replace the
  banded grid with 4 oriented "wall" quads (one per edge) plus 4 corner quads,
  driven by a fragment shader that takes the map AABB as a uniform and
  computes distance-to-map-rect for the alpha ramp and noise warp. Constant
  ~8 instances regardless of map size; per-pixel falloff is GPU-only.
- **"Out of map" backdrop.** Behind the boundary fog, render a single darkened
  / desaturated procedural plane (or extend the terrain shader with a
  `beyond_map` branch) so any pixel that leaks through the fog reads as void
  rather than the clear color or repeated scatter. No CPU cost; one extra
  full-screen-ish quad in the background pass.
- **Animated drift.** Add a `u_time` uniform to the fog shader and offset the
  cloud noise sample by `time * slow_vec2`. Pure shader change; no CPU cost.
  Gate behind a graphics setting so low-end machines can disable it.
- **Edge vignette on the playable side.** Subtly darken/desaturate the last
  ~2 tiles inside the map so the transition into the boundary fog is
  continuous (currently the playable terrain is fully bright right up against
  the fog). Fold into the terrain fragment shader using the existing map
  bounds.
- **Height-aware fog ground projection.** Currently fog quads sit at a fixed Y
  (~0.2) which can clip through hills. Sample the height map (or the same
  prebaked terrain heightmap texture used by the ground renderer) in the fog
  vertex shader so puffs drape over terrain. Constant cost, no per-frame CPU.
- **Fog culling against the camera frustum.** Even after instance reduction,
  on giant maps we still feed boundary puffs that are off-screen. A simple
  precomputed AABB per puff group + a frustum check at submit time (or via a
  GPU compute / indirect draw) keeps the GPU draw count proportional to what
  is visible.

Acceptance:
- No measurable per-frame CPU cost from fog rendering; visibility-driven CPU
  work happens only when the visibility version changes.
- No visible tile grid in either fog of war or boundary fog at any zoom level.
- The map edge transitions smoothly into an opaque haze; nothing of the world
  beyond the map is visible (no tan clear color, no repeating scatter).
- Boundary fog instance count is O(1) in map area (single-digit number of
  draws / instances) once the volumetric pass lands.

## P2 - Render thread CPU hotspots (perf data 2026-05-08)

Captured perf hot spots on the QSGRenderThread (excluding driver / kernel
samples that we cannot influence):

- `Render::Creature::Pipeline::submit_snapshot_creature` 4.30% + 0.62%
- `Render::GL::RiggedMeshCache::get_or_bake_prehashed` 0.96%
- `MinimapManager::update_fog` 0.66%
- `QtPrivate::findString` 1.49% (Qt string hashing/comparison)
- `QVector3D::normalize()` 0.34%

### Done in this pass
- Hoisted the per-creature `dynamic_cast<Renderer*>` out of
  `submit_snapshot_creature` / `submit_rigged_creature`. Renderer pointer is
  resolved once per `submit_requests` call and threaded through. Removes two
  `dynamic_cast`s per visible creature per frame.
- Pre-normalized `k_shadow_light_dir` once into `k_shadow_light_dir_normalized`
  and `k_shadow_dir_xz_normalized`; replaced 6 `QVector3D/2D::normalize()`
  calls per shadow with constant references.
- Added `Backend::troop_shadow_shader()` accessor; replaced per-creature
  `backend->shader(QStringLiteral("troop_shadow"))` (a QHash<QString>
  lookup, the likely `QtPrivate::findString` source) with a direct pointer
  read. Two call sites in `creature_prepared_state.cpp`.

### Still to do
- **`RiggedMeshCache::get_or_bake_prehashed` 0.96%** — the unordered_map keyed
  on a struct (spec*, lod, variant_bucket, skin_species_id, attachments_hash)
  is hit on every fallback creature submit. Candidates: precompute the hash
  into the key once per creature per frame (the hash is stable across
  consecutive frames for the same creature), or replace with a dense vector
  indexed by a compact (spec_index, lod, variant_bucket) tuple. Also consider
  caching the resulting `RiggedMeshEntry*` on the per-entity render state so
  the cache lookup happens only when the key actually changes.
- **`MinimapManager::update_fog` 0.66%** — already gated on
  `visibility_version`, so it's amortized but still copies the entire base
  image and walks every minimap pixel doing four-tap bilinear blends. Move
  the fog blend to the GPU: upload the small visibility cell buffer as an
  R8 texture and let a fragment shader sample + blend over the base color
  in the minimap pass. Eliminates the copy + per-pixel CPU loop entirely.
- **`QSGRenderThread / libQt6Core findString` 1.49%** — most likely caused by
  Qt scene graph internals (QML hashing of materials/properties), not our
  code. After the troop_shadow fix, re-profile and confirm. If still hot,
  audit any per-frame `QVariantMap`, `QHash<QString>` or
  `QQmlProperty(...)` lookups in the QML/UI layer.
- **Profile-guided micro-passes**: re-capture perf after these changes; the
  flame graph above predates the dynamic_cast / shader-lookup hoists.
