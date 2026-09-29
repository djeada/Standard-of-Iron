# terrain_service.cpp split (WP-F, issue #1559)

Before: `game/map/terrain_service.cpp` was 1,335 lines with `rebuild_road_spatial_index` at 110 lines and the road index state spread across ten `TerrainService` members.

After:

| File                          | Lines    | Owns                                                                                                                                                                                                       |
| ----------------------------- | -------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `terrain_service.cpp`         | ~320     | `instance()`, `initialize`, `initialize_keeping_world_props`, `clear`, `seal`, `restore_from_serialized`, terrain field rebuild, authored building obstacle registration, process-unique revision counters |
| `terrain_road_index.{h,cpp}`  | 65 + 200 | `RoadSpatialIndex`: query segments, grid geometry, CSR cell lists; `rebuild` (split into `build_query_segments`, `size_grid`, `fill_cells`) and `is_near`                                                  |
| `terrain_world_props.{h,cpp}` | 23 + 530 | Prop lookup/reservation/harvest, authored-vs-runtime prop bookkeeping, per-prop surface cache, runtime prop generation filter (fields), prop identity state                                                |
| `terrain_surface_queries.cpp` | ~345     | Surface height/normal resolution, footprint heights, walkable/forbidden/hill/terrain-type passthroughs, road/bridge/river/water proximity                                                                  |

Decisions:

- `TerrainService` stays one class and one owner of session state. The new `.cpp` files define its member functions by responsibility; nothing gains friend access. The only genuinely separate object is `RoadSpatialIndex`, which owns its state and is held by value as `m_road_index`.
- `RoadSpatialIndex::rebuild` receives the tile size and grid size rather than the height map, so it has no dependency on `TerrainHeightMap`. `TerrainService::rebuild_road_spatial_index` clears the index when there is no height map, matching the old early return. An empty road list also leaves it cleared.
- The revision counter (`next_props_revision`, one process-wide atomic) stays in `terrain_service.cpp` with `initialize`, so revisions remain process-unique and the bump order in `initialize`, `clear`, `restore_from_serialized` is unchanged.
- `build_runtime_world_props`, `world_props_match`, `has_runtime_harvest_props` are declared in `terrain_world_props.h` because lifecycle code calls them; the coordinate helpers stay file-local to the prop unit.
- `k_min_tile_size` is repeated as a file-local constant in the three units that use it (0.0001); it is not worth a shared header.

Verification: a scratch program loads every `assets/maps/*.json`, initialises a `TerrainService`, and hashes heights, terrain types, walkability, forbidden cells, field slopes/curvature, world props, and a dense grid of surface/road/bridge/river/water/normal/footprint queries plus reserve/harvest. Digests matched before and after (see the work package report).
