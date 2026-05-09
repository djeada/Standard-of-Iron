# Rendering Pipeline Todo

Goal: make the runtime render path modern, scalable, and close to data-only playback while the game is playing. The current hot path is still dominated by creature preparation, sorting, minimap fog cadence, and a few remaining runtime cache/build paths.

The perf sample that motivated this list showed `RiggedMeshCache::get_or_bake`, `Backend::execute`, `__memmove`, allocator calls, `prepare_humanoid_instances`, `SnapshotMeshCache`, and minimap fog activity on the render thread. A later sample additionally showed `Render::Creature::Pipeline::submit_snapshot_creature(...)` as the top app-side hotspot, with visible time in `Render::GL::RiggedMeshCache::get_or_bake_prehashed(...)`, `__memmove_avx_unaligned_erms`, and render-thread work on `QSGRenderThread`.

## P2 - Terrain and World Assets

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
- Reuse frame-local backend buffers where possible (started with rigged instancing submitting command references instead of command-struct copies).
- Add lightweight counters for draw queue command count, reallocations, and copied bytes.

Acceptance:
- Stable frames avoid backend scratch allocations and avoidable command-data copies.
