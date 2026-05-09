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
- Audit draw submission and cache code for avoidable `std::vector` growth, temporary containers, structure copies, and span-to-owned-data conversions.
- Pre-size or reuse hot-path buffers so warmed frames avoid allocator and `memmove` samples.
- Review whether repeated math such as vector normalization in creature submit can be cached or hoisted when visual state is unchanged.

Acceptance:
- Warmed creature-heavy frames avoid runtime mesh bake/load work, minimize allocation/copy samples, and reduce `submit_snapshot_creature()` cost in captures.

