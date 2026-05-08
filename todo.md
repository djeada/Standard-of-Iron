# Rendering Pipeline Todo

Goal: make the runtime render path modern, scalable, and close to data-only playback while the game is playing. Terrain and static scatter are already mostly in the desired shape; the current hot path is still dominated by per-frame collection, creature preparation, sorting, and a few remaining runtime cache/build paths.

The perf sample that motivated this list showed `RiggedMeshCache::get_or_bake`, `Backend::execute`, `__memmove`, allocator calls, `prepare_humanoid_instances`, `SnapshotMeshCache`, and minimap fog activity on the render thread. A later sample additionally showed `Render::Creature::Pipeline::submit_snapshot_creature(...)` as the top app-side hotspot, with visible time in `Render::GL::RiggedMeshCache::get_or_bake_prehashed(...)`, `__memmove_avx_unaligned_erms`, and render-thread work on `QSGRenderThread`.

## Branch status - 2026-05-08

Implemented and verified on `copilot/start-implementing-todo-md`:
- Draw queue high-water reservation, bucket-aware sorting, and prepared-batch regression tests.
- Rigged creature instancing enabled by default, with `SOI_RENDER_DISABLE_RIGGED_INSTANCING` as the opt-out flag.
- Rigged and snapshot mesh cache frame counters for hits, misses, bakes, and snapshot loads.
- Minimal horse/elephant snapshot rendering no longer falls back to runtime rigged/snapshot baking when the required prebaked snapshot asset is missing.
- Minimap fog recomposition avoids repeated work for unchanged visibility versions, and unit overlay recomposition tracks fog, unit, owner, selection, building, and local-player state.
- **Persistent render registry (`PersistentRenderRegistry`)**: `Renderer::render_world()` no longer calls `world->get_entities_with<RenderableComponent>()` each frame. Entity IDs are pre-classified into unit/building/other lists and kept up to date through component-observer callbacks registered on the world. Tests added in `tests/render/persistent_render_registry_test.cpp`.
- **Cached static building render instances**: `submit_building_instance()` now uses a per-entity cached `StoredRenderInstance` keyed by entity id and invalidates only on archetype/palette/world/default-texture/LOD changes. Added cache-hit/miss/rebuild counters and tests in `tests/render/building_render_common_test.cpp`.
- **Render-world scratch buffer reuse**: `render_world()` now reuses thread-local scratch vectors for unit/building/other entries and a thread-local `PrimitiveBatcher`, removing per-frame vector/batcher construction.
- Regression tests for draw queue memory/sort behavior, cache counters, rigged batching by role palette, minimap dirty handling, and missing prebaked snapshot behavior.

Verification:
- `cmake --build build --target standard_of_iron_tests -j 4`
- `./build/bin/standard_of_iron_tests --gtest_filter='BuildingRenderCommon.*:PersistentRenderRegistry.*'` passed 23/23.
- `./build/bin/standard_of_iron_tests --gtest_brief=1` currently reports 1021 passed, 20 skipped, 19 pre-existing failures (1060 total).

Still open:
- Full terrain/scatter dirty-upload proof.
- Equipment data handles and stable `AttachmentSetId` ownership.
- Moving/confirming simulation query work outside render-thread captures.
- Full QSG render-thread stage documentation and first-use logging.
- Backend execution cleanup.
- Additional perf regression coverage for terrain uploads and building cached instances.

## P1 - Persistent Render Lists and Dirty Updates

### Split render world collection into persistent buckets

Status: done. `PersistentRenderRegistry` maintains three pre-classified entity ID lists (units, buildings, others). `Renderer::render_world()` attaches the registry to the current world on first call and thereafter iterates the pre-classified lists rather than calling `world->get_entities_with<RenderableComponent>()`. The world's new `add_component_observer`, `add_entity_destroyed_observer`, and `add_world_cleared_observer` methods propagate entity/component changes into the registry in real time. 15 unit tests in `tests/render/persistent_render_registry_test.cpp` cover population, reclassification, entity destruction, world clear, re-attach, and duplicate-free guarantees.

Files:
- `render/persistent_render_registry.h` (new)
- `render/persistent_render_registry.cpp` (new)
- `render/scene_renderer.h` (added registry member)
- `render/scene_renderer.cpp` (render_world uses registry)
- `game/core/world.h` / `game/core/world.cpp` (observer API added)
- `tests/render/persistent_render_registry_test.cpp` (new)

### Make static buildings submit from cached render instances

Status: done. `submit_building_instance()` now builds and stores per-entity cached `StoredRenderInstance` data (archetype pointer, palette, world matrix, default texture, LOD). Runtime submission uses `submit_render_instance()` with cached data, and invalidation is local to affected entities when state/owner/damage (archetype/palette), transform (world), or LOD bucket changes. Added cache stats (`hits`, `misses`, `rebuilds`) and regression tests for unchanged reuse and LOD-triggered rebuild.

Files:
- `render/entity/building_archetype_desc.*`
- `render/render_archetype.*`
- `render/scene_renderer.cpp`
- building renderer files under `render/entity/`

Problem:
- Buildings have good archetype support, but non-unit rendering still goes through the generic registry function path each frame.

Work:
- Convert static building renderers to produce cached `RenderInstance` data when state/owner/damage/LOD bucket changes.
- Keep runtime submission to `submit_render_instance()` with cached archetype pointer, palette, world matrix, and selected LOD.

Acceptance:
- Static buildings do not rebuild renderer-specific draw structures per frame.
- Damaged/destroyed/owner-color changes invalidate only affected building instances.

### Stabilize draw queue memory

Status: done for the render-world/queue scope. `DrawQueue` retains and reuses command/sort/prepared-batch capacity via high-water marks and `reserve_for_frame()`. `Renderer::render_world()` now reuses thread-local scratch vectors for unit/building/other lists and a thread-local `PrimitiveBatcher`, removing avoidable per-frame vector and batcher construction in the hot path. Remaining long-term work is perf-capture validation and any additional backend scratch-buffer audits.

Files:
- `render/draw_queue.h`
- `render/scene_renderer.cpp`
- `render/primitive_batch.*`

Problem:
- Perf shows `__memmove`, `malloc`, `free`, and `malloc_consolidate` in the frame. `DrawQueue::clear()` clears vectors and per-frame sorting rebuilds indices/keys/batches.

Work:
- Add explicit reserve/high-water APIs for draw queue items, sort indices, sort keys, and prepared batches.
- Reserve based on previous frame high-water marks and expected battle size.
- Audit frame-local vectors in `render_world()`, creature preparation, primitive batching, and backend scratch buffers.
- Replace avoidable per-frame vector construction with member/thread-local reusable buffers.

Acceptance:
- Allocator samples during stable gameplay are materially reduced.
- Draw queue capacity does not shrink during normal frames.

## P1 - Smarter Sorting and Batching

### Avoid full stable sort for already-bucketed commands

Status: partially done. `DrawQueue` records coarse submission bucket spans and sorts within bucketed ranges when submission order is already monotonic, falling back to the original full stable sort when needed. Tests cover sorted input, within-bucket sorting, and non-monotonic fallback. Remaining work: material/mesh/texture precomputed sort ids and measured sort-time reduction in large stable scenes.

Files:
- `render/draw_queue.h`
- `render/scene_renderer.cpp`
- `render/gl/backend.cpp`

Problem:
- `DrawQueue::sort_for_batching()` computes keys for every command and stable-sorts the whole queue every frame.

Work:
- Submit commands into coarse pass/pipeline buckets during collection.
- Sort only buckets that actually need ordering.
- Preserve append order for terrain/static/scatter batches that are already submitted in correct pass order.
- Consider precomputed sort ids on materials/meshes/textures instead of per-frame pointer interning maps.

Acceptance:
- Sort time decreases on scenes with many stable/static commands.
- Terrain/scatter batches bypass unnecessary full-queue sorting.

### Make rigged instancing a production path or remove dead path

Status: done for the current path. Rigged creature instancing is enabled by default and can be disabled with `SOI_RENDER_DISABLE_RIGGED_INSTANCING`. Role-color palettes are included in batching compatibility so unlike palettes split batches.

Files:
- `render/draw_queue.h`
- `render/gl/backend.cpp`
- `render/gl/backend/rigged_character_pipeline.*`

Problem:
- `DrawQueue` prepares `RiggedCreatureInstanced`, but `Backend::execute()` gates rigged instancing behind `SOI_RENDER_ENABLE_RIGGED_INSTANCING`.

Work:
- Validate correctness/perf of rigged instancing for snapshot meshes and rigged BPAT meshes.
- If good, make it default with a debug disable flag.
- If not good, document why and simplify the prepared batch path so it does not imply active scalability.

Acceptance:
- Large groups of identical snapshot/rigged creature commands either draw through the instanced path by default or the code is simplified to the actual supported path.

## P2 - Terrain and World Assets

### Keep terrain/scatter as the model for static data

Files:
- `render/ground/*`
- `render/terrain_scene_proxy.h`

Current state:
- Terrain scatter is close to the target: instance data is generated in `configure()` and frame submit passes GPU buffers through `TerrainScatterCmd`.

Work:
- Ensure all terrain feature types follow the same pattern: load/configure builds immutable GPU buffers; frame submit only references buffers plus small time/visibility parameters.
- Add dirty flags so `Scatter::sync_direct_state()` and similar upload paths can prove they do not upload in stable frames.

Acceptance:
- Stable terrain frames perform no terrain/scatter CPU generation and no buffer upload except intentional animated/time uniforms.

### Separate minimap fog cadence from render frame cadence

Status: partially done. Fog updates now skip unchanged visibility versions, unit overlay recomposition tracks fog version changes, and marker hashing includes rendered marker state plus local owner. Remaining work: fixed-cadence scheduling/off-thread recomputation and dirty-region texture uploads.

Files:
- `app/core/minimap_manager.*`
- `game/map/minimap/*`
- call sites in engine/render loop

Problem:
- `MinimapManager::update_fog()` appears in the render-thread sample.

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
- Render should consume already-produced visual state: combat active, attack phase, hit reaction, target direction, and mode flags.

Acceptance:
- Render-thread perf captures do not include combat query rebuilds or target search as regular costs.

### Perf findings from 2026-05-06 sample

Status: partially done. Added frame counters for rigged mesh cache hits/misses/bakes and snapshot cache hits/misses/loads/bakes, plus regression coverage. Minimal horse/elephant snapshot submit no longer performs runtime rigged/snapshot bake fallback when a required prebaked snapshot is missing. Remaining work: counters for skin UBO bytes, draw queue reallocations/copied bytes, cache-hit no-allocation proof, and warmed battle perf validation.

Files:
- `render/creature/pipeline/creature_pipeline.cpp`
- `render/rigged_mesh_cache.*`
- `render/snapshot_mesh_cache.*`
- `render/draw_queue.*`
- `render/gl/backend.cpp`

Problem:
- A perf sample shows `Render::Creature::Pipeline::submit_snapshot_creature(...)` as the top app-side hotspot (~6.6%), with additional time in `Render::GL::RiggedMeshCache::get_or_bake_prehashed(...)` (~1.7%).
- `__memmove_avx_unaligned_erms` and related allocator / kernel memory-accounting samples are prominent on both the main app thread and `QSGRenderThread`, indicating copy-heavy and allocation-heavy render-path behavior.
- `Render::GL::Backend::execute(...)` is visible but materially smaller than creature submit/caching work, so the primary bottleneck is data preparation rather than final GL dispatch.

Work:
- Trace `submit_snapshot_creature()` line-by-line and identify all per-creature work that can be moved to registration, prewarm, or frame-shared state.
- Ensure snapshot mesh cache hits on hot frames do not allocate, load blobs, or rebuild mesh data.
- Ensure rigged mesh cache hits on hot frames do not bake, upload, or repack skin data.
- Audit draw submission and cache code for avoidable `std::vector` growth, temporary containers, structure copies, and span-to-owned-data conversions.
- Add lightweight counters for:
  - rigged mesh cache hits / misses / bakes
  - snapshot mesh cache hits / misses / loads / bakes
  - per-frame bytes uploaded for skin UBO / palette data
  - draw queue command count, reallocations, and copied bytes
- Pre-size or reuse hot-path buffers so warmed frames avoid allocator and `memmove` samples.
- Review whether repeated math such as vector normalization in creature submit can be cached or hoisted when visual state is unchanged.

Acceptance:
- `submit_snapshot_creature()` is no longer the dominant CPU hotspot in warmed battle captures.
- Warmed gameplay frames show zero snapshot loads/bakes and zero rigged mesh bakes.
- Render-thread captures show substantially reduced `memmove` / allocator / memcg samples.
- Added counters make it obvious when a frame regresses into runtime baking, uploads, or queue churn.

### Treat `QSGRenderThread` as a strict no-allocation hot path

Files:
- `render/scene_renderer.*`
- `render/creature/pipeline/*`
- `render/gl/*`
- `ui/*` if Qt scene graph integration participates

Problem:
- The sample shows meaningful work on `QSGRenderThread`, including creature submission, backend execution, memory copies, zlib/png activity, and GL driver/kernel overhead. Any avoidable allocation or asset preparation on this thread directly risks frame pacing.

Work:
- Document which rendering stages are allowed to execute on `QSGRenderThread` and which must complete earlier.
- Move asset decoding, cache population, blob parsing, and other non-draw preparation off the render thread.
- Reuse thread-local or persistent scratch buffers for render submission rather than growing temporary buffers during frame assembly.
- Add debug/perf logging for render-thread cache misses and first-use initialization so unexpected hot-path work is visible immediately.

Acceptance:
- Normal gameplay on a warmed scene performs command assembly and draw submission on `QSGRenderThread` without asset decoding, mesh baking, or frequent buffer growth.
- First-use / cold-start work is clearly separated from steady-state render-thread behavior.

## P3 - Backend Cleanup After Hot Path Fixes

### Break up `Backend::execute()` without adding overhead

Files:
- `render/gl/backend.cpp`
- `render/gl/backend/*_pipeline.*`

Problem:
- `Backend::execute()` is a large switch that owns many pipeline details. It is functional but not especially clean or easy to extend.

Work:
- Move per-command/batch execution into pipeline-owned methods that accept prepared batch spans.
- Keep state caching (`m_lastBoundShader`, texture state, depth/blend scopes) centralized or explicitly passed so cleanup does not regress perf.

Acceptance:
- Adding a new command type does not require expanding a monolithic backend switch with low-level GL setup.
- Perf is unchanged or better versus the current backend.

### Add automated performance regression tests

Status: partially done. Added tests for draw queue high-water behavior, cache hit/bake/miss counters, rigged role-palette batching, minimap dirty recomposition, and missing prebaked snapshot no-fallback behavior. Remaining work: stable terrain no-upload and cached building render-instance tests.

Files:
- `tests/render/*`
- new benchmark or perf harness if needed

Work:
- Add synthetic tests for:
  - warmed creature submit with zero bakes
  - draw queue sort high-water behavior
  - stable terrain frame with no uploads
  - stable building frame with cached render instances
- Add counters that tests can assert without needing a GPU where possible.

Acceptance:
- CI catches accidental reintroduction of runtime creature baking, allocation-heavy queue churn, or static terrain regeneration.

## Definition of Done

The rendering pipeline is close to the desired goal when a large warmed battle shows:

- zero rigged mesh bakes during normal gameplay
- zero snapshot mesh bakes/loads during normal gameplay
- zero skin UBO uploads during normal gameplay
- stable or near-zero render-thread allocator samples
- creature submit dominated by compact handle reads and command writes
- terrain/static objects submitted from persistent GPU buffers or cached render instances
- render collection no longer scans/probes all renderable entities every frame
- sort cost is bounded by dynamic/changed commands, not all static commands
- render-thread captures do not regularly include simulation combat queries
