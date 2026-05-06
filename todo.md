# Rendering Pipeline Todo

Goal: make the runtime render path modern, scalable, and close to data-only playback while the game is playing. Terrain and static scatter are already mostly in the desired shape; the current hot path is creature asset resolution/cache work plus per-frame command collection/sort/playback.

The perf sample that motivated this list showed `RiggedMeshCache::get_or_bake`, `Backend::execute`, `__memmove`, allocator calls, `prepare_humanoid_instances`, `SnapshotMeshCache`, and minimap fog work on the hot path. The tasks below are based on the current code, not assumptions.

## P1 - Persistent Render Lists and Dirty Updates

### Split render world collection into persistent buckets

Files:
- `render/scene_renderer.cpp`
- `render/unit_render_cache.h`
- `render/unit_render_cache.cpp`
- `game/core/world.*`

Problem:
- `Renderer::render_world()` walks every renderable entity each frame, probes components, classifies units/buildings/other entries, and rebuilds temporary vectors.

Work:
- Maintain persistent render registries for units, buildings, projectiles/effects, and terrain/static objects.
- Update registry membership when components are added/removed, visibility changes, renderer id changes, or entity is destroyed.
- Keep per-entry cached component pointers, renderer function/handle, selection state, fog state, and transform dirty flags.

Acceptance:
- The normal frame path iterates already-classified render entries instead of calling `world->get_entities_with<RenderableComponent>()` and component-probing every renderable.
- Entity/component changes update render entries through explicit dirty events.

### Make static buildings submit from cached render instances

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
