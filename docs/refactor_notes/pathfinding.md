# Pathfinding split (issue 1559, WP-F)

`Pathfinding` stays the single owner of the grid, dirty regions, path cache, region maps and
obstruction atomics. `pathfinding.cpp` was split by responsibility; every unit implements
`Pathfinding` members, so no state moved and no friend access was added.

| File                         | Responsibility                                                                                       |
| ---------------------------- | ---------------------------------------------------------------------------------------------------- |
| `pathfinding.cpp`            | grid container, world/grid transforms, walkability queries                                           |
| `pathfinding_dirty.cpp`      | dirty-region marking and merging, obstruction release, navigation-change log, path-cache eviction    |
| `pathfinding_grid_build.cpp` | terrain/building/gate/prop/forest cell application, partial-region rebuild, prewarm                  |
| `pathfinding_clearance.cpp`  | elevation, climb penalty, clearance penalty                                                          |
| `pathfinding_regions.cpp`    | region labelling, reachability, escape/nearest-connected lookups                                     |
| `pathfinding_astar.cpp`      | cached `find_path`, search buffers (thread-local, keyed by `const Pathfinding*`), A*, reconstruction |

`find_path_internal` (192 lines) is now `make_search_request` -> endpoint resolution ->
`goal_in_start_region` -> `run_search` (+ `relax_neighbors`) -> `path_from_outcome`. `SearchRequest`
and `SearchOutcome` are the explicit request/result types. Order of side effects is unchanged:
the search generation is taken only after the start==goal early return, and the region lock is
held only while snapping the goal.

`terrain_cell_value` is a private static member because both the core and the grid build use it.
