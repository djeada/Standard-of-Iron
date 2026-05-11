# How the Pathfinding System Actually Works

Imagine you've selected a group of 50 soldiers and right-clicked on a position across the map. Between your troops and that destination is a river, some buildings, and rough terrain. Each soldier needs to find a path around these obstacles, and they need to do it fast enough that the game doesn't stutter, even when multiple players are issuing movement commands simultaneously.

This is the story of how Standard of Iron's pathfinding works. We'll trace the journey from a movement command to a completed path, looking at the obstacle grid, the A* algorithm, the threading model, radius-aware walkability, and the full stuck-recovery system.

## What we'll cover

1. The obstacle grid — what valid vs. invalid means and why it matters
2. Coordinate transformation between world space and grid space
3. A* path search and its performance optimisations
4. Eight-directional movement and corner-cutting prevention
5. Unit-radius walkability — how bigger units need more room
6. The background threading model
7. Obstacle registration: terrain, buildings, and dirty-region updates
8. Segment walkability — checking a movement step before taking it
9. Stuck detection and recovery — what happens when a unit ends up in invalid terrain
10. Formation planning integration


## The core insight: a simple grid with smart processing

Pathfinding in games often involves complex navigation meshes or hierarchical grids. We took a simpler approach: a flat 2D grid where each cell is either walkable or blocked. The complexity goes into making this simple approach fast and responsive.

The grid maps directly to world coordinates with a configurable offset. A cell at grid position (10, 15) corresponds to a specific world position based on the map dimensions. Buildings and terrain features mark cells as obstacles. Units request paths, and the system finds routes around blocked cells.

Here's the high-level architecture:

```
┌────────────────────────────────────────────────────────────────────────────┐
│                        PATHFINDING REQUEST FLOW                            │
│                                                                            │
│  ┌──────────────┐     ┌──────────────┐     ┌──────────────┐               │
│  │ COMMAND      │────▶│ PATH REQUEST │────▶│ WORKER       │               │
│  │ SERVICE      │     │ QUEUE        │     │ THREAD       │               │
│  │              │     │              │     │              │               │
│  │ Translates   │     │ Holds        │     │ Processes    │               │
│  │ world coords │     │ pending      │     │ requests     │               │
│  │ to grid      │     │ requests     │     │ using A*     │               │
│  └──────────────┘     └──────────────┘     └──────────────┘               │
│         │                                         │                        │
│         │                                         ▼                        │
│         │                                  ┌──────────────┐               │
│         │                                  │ RESULT QUEUE │               │
│         │                                  │              │               │
│         │                                  │ Completed    │               │
│         │                                  │ paths ready  │               │
│         │                                  │ for pickup   │               │
│         │                                  └──────────────┘               │
│         │                                         │                        │
│         ▼                                         ▼                        │
│  ┌─────────────────────────────────────────────────────────────────────┐  │
│  │                      MOVEMENT SYSTEM                                │  │
│  │                                                                     │  │
│  │  Main thread picks up completed paths and applies them to units.   │  │
│  │  Units then follow waypoints toward their destination.              │  │
│  └─────────────────────────────────────────────────────────────────────┘  │
│                                                                            │
└────────────────────────────────────────────────────────────────────────────┘
```

The key is the background worker thread. Path computation happens off the main thread, so even complex paths don't cause frame drops. The main thread submits requests and picks up results—it never blocks waiting for pathfinding.


## The architecture in pieces

Everything lives in [game/systems/pathfinding.h](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/pathfinding.h) and [pathfinding.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/pathfinding.cpp). Related files coordinate movement and obstacles:

```
game/systems/
├── pathfinding.h                  # Core pathfinding class and data structures
├── pathfinding.cpp                # A* implementation and worker thread
├── command_service.h              # Translates movement commands to path requests
├── command_service.cpp            # Manages pending requests, coordinate conversion,
│                                  #   and stuck-recovery helpers
├── movement_system.cpp            # Applies paths to unit movement, stuck detection
├── building_collision_registry.h  # Tracks building footprints as obstacles
└── building_collision_registry.cpp
```

The [CommandService](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/command_service.cpp) acts as the coordinator. It owns the pathfinder instance, handles coordinate conversion, and tracks which entities have pending path requests.


## The obstacle grid — valid vs. invalid cells

### What the grid is

The pathfinding grid is a 2D array of obstacle flags stored in `Pathfinding::m_obstacles`. Every cell is one byte: `0` means **walkable (valid)**, any non-zero value means **blocked (invalid)**:

```cpp
class Pathfinding {
  int m_width, m_height;
  std::vector<std::vector<std::uint8_t>> m_obstacles; // [row][col], 0=walk, 1=blocked
  float m_grid_cell_size{1.0F};
  float m_grid_offset_x{0.0F}, m_grid_offset_z{0.0F};
  // ...
};
```

Each cell corresponds to a 1×1 area of world space (when `grid_cell_size = 1`). A unit standing at world position (3.7, 0, −2.1) occupies the grid cell at approximately (4, −2) after rounding.

### A concrete 14×10 example

Below is a small map. Letters mark what occupies each cell; the number shows the obstacle value that ends up in `m_obstacles`:

```
World layout (14 columns × 10 rows):

  Col: 0  1  2  3  4  5  6  7  8  9 10 11 12 13
       ─  ─  ─  ─  ─  ─  ─  ─  ─  ─  ─  ─  ─  ─
Row 0: .  .  .  .  .  .  .  .  .  .  .  .  .  .
Row 1: .  .  .  W  W  W  W  .  .  .  .  .  .  .
Row 2: .  .  .  W  B  B  W  .  .  .  .  .  .  .
Row 3: .  .  .  W  B  B  W  .  .  R  R  R  .  .
Row 4: .  .  .  W  W  W  W  .  .  R  R  R  .  .
Row 5: .  .  .  .  .  .  .  .  .  R  R  R  .  .
Row 6: .  .  .  .  .  .  .  .  .  .  .  .  .  .
Row 7: .  .  U  .  .  .  .  .  G  .  .  .  .  .
Row 8: .  .  .  .  .  .  .  .  .  .  .  .  .  .
Row 9: .  .  .  .  .  .  .  .  .  .  .  .  .  .

Legend:
  .  = open ground           → m_obstacles value: 0  (walkable)
  W  = building wall         → m_obstacles value: 1  (blocked)
  B  = building interior     → m_obstacles value: 1  (blocked)
  R  = river / water tile    → m_obstacles value: 1  (blocked)
  U  = unit standing here    → m_obstacles value: 0  (unit does NOT mark cells)
  G  = goal (clicked here)   → m_obstacles value: 0  (walkable)

m_obstacles array (0 = walk, 1 = blocked):

       0  1  2  3  4  5  6  7  8  9 10 11 12 13
     ┌──────────────────────────────────────────┐
  0: │ 0  0  0  0  0  0  0  0  0  0  0  0  0  0│
  1: │ 0  0  0  1  1  1  1  0  0  0  0  0  0  0│
  2: │ 0  0  0  1  1  1  1  0  0  0  0  0  0  0│
  3: │ 0  0  0  1  1  1  1  0  0  1  1  1  0  0│
  4: │ 0  0  0  1  1  1  1  0  0  1  1  1  0  0│
  5: │ 0  0  0  0  0  0  0  0  0  1  1  1  0  0│
  6: │ 0  0  0  0  0  0  0  0  0  0  0  0  0  0│
  7: │ 0  0  0  0  0  0  0  0  0  0  0  0  0  0│
  8: │ 0  0  0  0  0  0  0  0  0  0  0  0  0  0│
  9: │ 0  0  0  0  0  0  0  0  0  0  0  0  0  0│
     └──────────────────────────────────────────┘
```

The unit at (2, 7) wants to reach (8, 7). A* searches the obstacle array. It cannot enter any cell with value 1. The shortest path goes around the building (rows 1–4, cols 3–6):

```
Computed path (waypoints, grid coords): (2,7)→(2,6)→(3,5)→(4,5)→(5,5)→(6,5)→(7,6)→(8,7)

Visualised on the map:
  Col: 0  1  2  3  4  5  6  7  8  9 10 11 12 13
Row 0: .  .  .  .  .  .  .  .  .  .  .  .  .  .
Row 1: .  .  .  W  W  W  W  .  .  .  .  .  .  .
Row 2: .  .  .  W  B  B  W  .  .  .  .  .  .  .
Row 3: .  .  .  W  B  B  W  .  .  R  R  R  .  .
Row 4: .  .  .  W  W  W  W  .  .  R  R  R  .  .
Row 5: .  .  .  ●  ●  ●  ●  .  .  R  R  R  .  .   ← path detours below building
Row 6: .  .  ●  .  .  .  .  ●  .  .  .  .  .  .
Row 7: .  .  U  .  .  .  .  .  G  .  .  .  .  .
Row 8: .  .  .  .  .  .  .  .  .  .  .  .  .  .
Row 9: .  .  .  .  .  .  .  .  .  .  .  .  .  .
```

Key observations:
- Units do **not** mark the grid. Two units can occupy the same cell without either being marked blocked.
- Only static obstacles (buildings, water, impassable terrain) mark cells as 1.
- If you place a building on top of a unit, the building's cells become 1 but the unit's position is now on an **invalid cell** — the stuck-recovery system (described below) handles this.


### What "valid" and "invalid" mean at runtime

Throughout the movement and command code you'll see `is_point_allowed` called on the unit's current position:

```cpp
// movement_system.cpp
bool const current_position_allowed =
    is_point_allowed(current_pos_3d, entity->get_id(), unit_radius);
```

This translates the 3D world position to a grid cell and queries the obstacle array. `current_position_allowed = false` means the unit is standing on a cell that `m_obstacles` has marked as 1 — it is inside a building, in water, or otherwise on terrain the pathfinder considers impassable. Everything that follows in the movement update reacts to this boolean.


## Coordinate transformation

World coordinates are floating-point positions where units exist. Grid coordinates are integer indices into the obstacle array. Converting between them is straightforward:

From [command_service.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/command_service.cpp):

```cpp
auto CommandService::world_to_grid(float world_x, float world_z) -> Point {
  if (s_pathfinder) {
    int const grid_x = static_cast<int>(
        std::round(world_x - s_pathfinder->get_grid_offset_x()));
    int const grid_z = static_cast<int>(
        std::round(world_z - s_pathfinder->get_grid_offset_z()));
    return {grid_x, grid_z};
  }
  return {static_cast<int>(std::round(world_x)),
          static_cast<int>(std::round(world_z))};
}

auto CommandService::grid_to_world(const Point &grid_pos) -> QVector3D {
  if (s_pathfinder) {
    return {static_cast<float>(grid_pos.x) + s_pathfinder->get_grid_offset_x(),
            0.0F,
            static_cast<float>(grid_pos.y) + s_pathfinder->get_grid_offset_z()};
  }
  return {static_cast<float>(grid_pos.x), 0.0F, static_cast<float>(grid_pos.y)};
}
```

The Y component is always 0 because pathfinding is 2D — terrain height is applied separately when units actually move.

For a typical 200×200 map centred on the origin, the offset is (−99.5, −99.5). A unit at world (3.0, 0, −7.0) lands on grid cell (102, 92).


## Data structures

Points in the grid use a simple struct:

```cpp
struct Point {
  int x = 0;
  int y = 0;
  constexpr auto operator==(const Point &other) const -> bool {
    return x == other.x && y == other.y;
  }
};
```

Path requests carry all the information needed for processing:

```cpp
struct PathRequest {
  std::uint64_t request_id{};
  Point start;
  Point end;
  float unit_radius{0.0F};
};
```

Results pair the request ID with the computed path:

```cpp
struct PathResult {
  std::uint64_t request_id;
  std::vector<Point> path;
};
```


## The A* algorithm

A* is a best-first search that uses a heuristic to guide expansion toward the goal. Our implementation uses Manhattan distance as the heuristic (sum of horizontal and vertical distances), which is admissible for grid-based movement.

The algorithm maintains two key costs for each cell:
- **g-cost**: The actual cost from the start to this cell
- **f-cost**: g-cost plus the estimated cost to the goal (heuristic)

From [pathfinding.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/pathfinding.cpp):

```cpp
auto Pathfinding::find_path_internal(const Point &start, const Point &end,
                                     float unit_radius) -> std::vector<Point> {
  // Both start and end must be walkable
  if (!is_walkableFunc(start.x, start.y) || !is_walkableFunc(end.x, end.y)) {
    return {};
  }

  const int start_idx = to_index(start);
  const int end_idx   = to_index(end);

  if (start_idx == end_idx) {
    return {start};
  }

  const std::uint32_t generation = next_generation();
  m_open_heap.clear();

  set_g_cost(start_idx, generation, 0);
  set_parent(start_idx, generation, start_idx);
  push_open_node({start_idx, calculate_heuristic(start, end), 0});

  while (!m_open_heap.empty() && iterations < max_iterations) {
    QueueNode const current = pop_open_node();

    if (current.index == end_idx) {
      build_path(start_idx, end_idx, generation, current.g_cost + 1, path);
      return path;
    }

    for (each neighbor of current) {
      if (!walkable || already closed) continue;

      int tentative_gcost = current.g_cost + 1;
      if (tentative_gcost < neighbor's current g_cost) {
        // Update costs and parent, push to heap
      }
    }
  }

  return {};  // No path found
}
```

The heuristic calculation is simple Manhattan distance:

```cpp
auto Pathfinding::calculate_heuristic(const Point &a, const Point &b) -> int {
  return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}
```


## Performance optimisation: generation-based clearing

A naive A* implementation clears all working arrays before each search. For a 200×200 grid, that's 40,000 cells to reset. We avoid this with generation counters.

Each search increments a generation counter. Instead of clearing the g-cost array, we store a generation value alongside each cost. A cell's cost is only valid if its stored generation matches the current search's generation:

```cpp
auto Pathfinding::get_g_cost(int index, std::uint32_t generation) const -> int {
  if (m_g_cost_generation[index] == generation) {
    return m_g_cost_values[index];
  }
  return std::numeric_limits<int>::max();  // Treated as unvisited
}

void Pathfinding::set_g_cost(int index, std::uint32_t generation, int cost) {
  m_g_cost_generation[idx] = generation;
  m_g_cost_values[idx]     = cost;
}
```

This gives us O(1) "clearing" between searches — we just increment the generation counter. The same technique applies to the closed set and parent tracking.


## Eight-directional movement and corner cutting

Units can move in eight directions: the four cardinals plus diagonals. The neighbor collection checks all eight and filters out invalid moves:

```cpp
auto Pathfinding::collect_neighbors(const Point &point,
                                    std::array<Point, 8> &buffer) const -> std::size_t {
  std::size_t count = 0;
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
      if (dx == 0 && dy == 0) continue;

      const int x = point.x + dx;
      const int y = point.y + dy;

      if (x < 0 || x >= m_width || y < 0 || y >= m_height) continue;

      // Prevent corner-cutting through obstacles
      if (dx != 0 && dy != 0) {
        if (!is_walkable(point.x + dx, point.y) ||
            !is_walkable(point.x, point.y + dy)) {
          continue;
        }
      }

      buffer[count++] = Point{x, y};
    }
  }
  return count;
}
```

**Why the corner-cutting check matters:**

```
  4  5  6
  ┌──┬──┬──┐
5 │  │##│  │   ## = blocked cell
  ├──┼──┼──┤
6 │  │  │  │   Without the check, a unit at (4,6) could
  └──┴──┴──┘   step diagonally to (5,5) passing through
               the corner of the blocked cell at (5,5).
               The check requires (5,6) AND (4,5) to both
               be walkable before allowing the diagonal.
```

If a unit at (4, 6) wants to move diagonally to (5, 5), both (5, 6) and (4, 5) must be walkable. Otherwise, the unit would visually clip through a wall corner.


## Unit-radius walkability

Thin units (infantry, small creatures) fit in a single cell. Larger units (cavalry, elephants) need clear space around them. The `unit_radius` stored in each `PathRequest` selects between two walkability checks:

```cpp
auto is_walkable_for_radius(const Pathfinding &pathfinder, int x, int y,
                             float unit_radius) -> bool {
  if (unit_radius <= k_unit_radius_threshold) {   // threshold = 0.5 world units
    return pathfinder.is_walkable(x, y);
  }
  return pathfinder.is_walkable_with_radius(x, y, unit_radius);
}
```

`is_walkable_with_radius` expands the obstacle check: a cell is only considered walkable if a circle of `unit_radius` centred on that cell does not overlap any blocked cell. In practice this means large units cannot squeeze through narrow gaps that infantry can pass:

```
Obstacle layout (# = blocked, . = open):
  . . . # # . .
  . . . # # . .    A unit with radius 1.5 checking cell (1,1):
  . . . # # . .    the radius circle touches column 3 (blocked)
  . . . # # . .    → is_walkable_with_radius returns false
                   → large unit cannot pass through this corridor
```

The `unit_radius` for each spawn type comes from `TroopConfig::get_selection_ring_size`. All `is_point_allowed`, `find_recovery_cell`, and A* calls use the same radius so that large units get consistent clearance everywhere.


## The priority queue

A* needs to always expand the node with the lowest f-cost. We use a binary heap for this:

```cpp
void Pathfinding::push_open_node(const QueueNode &node) {
  m_open_heap.push_back(node);
  std::size_t index = m_open_heap.size() - 1;
  while (index > 0) {
    std::size_t const parent = (index - 1) / 2;
    if (heap_less(m_open_heap[parent], m_open_heap[index])) {
      break;
    }
    std::swap(m_open_heap[parent], m_open_heap[index]);
    index = parent;
  }
}
```

The comparison prioritises lower f-cost, using g-cost as a tiebreaker (preferring nodes closer to the start when f-costs are equal):

```cpp
auto Pathfinding::heap_less(const QueueNode &lhs, const QueueNode &rhs) -> bool {
  if (lhs.f_cost != rhs.f_cost) {
    return lhs.f_cost < rhs.f_cost;
  }
  return lhs.g_cost < rhs.g_cost;
}
```


## The threading model

Pathfinding runs on a dedicated background thread. The main thread submits requests and picks up results without blocking:

```cpp
Pathfinding::Pathfinding(int width, int height)
    : m_width(width), m_height(height) {
  m_obstacles.resize(height, std::vector<std::uint8_t>(width, 0));
  ensure_working_buffers();
  m_worker_thread = std::thread(&Pathfinding::worker_loop, this);
}
```

The worker loop waits for requests, processes them, and queues results:

```cpp
void Pathfinding::worker_loop() {
  while (true) {
    PathRequest request;
    {
      std::unique_lock<std::mutex> lock(m_request_mutex);
      m_request_condition.wait(lock, [this]() {
        return m_stop_worker.load() || !m_request_queue.empty();
      });

      if (m_stop_worker.load() && m_request_queue.empty()) break;

      request = m_request_queue.front();
      m_request_queue.pop();
    }

    auto path = find_path(request.start, request.end, request.unit_radius);

    {
      std::lock_guard<std::mutex> const lock(m_result_mutex);
      m_result_queue.push({request.request_id, std::move(path)});
    }
  }
}
```

The main thread picks up completed paths at the start of each movement update frame:

```cpp
void MovementSystem::update(Engine::Core::World *world, float delta_time) {
  CommandService::process_path_results(*world);  // drain result queue
  auto entities = world->get_entities_with<Engine::Core::MovementComponent>();
  for (auto *entity : entities) {
    move_unit(entity, world, delta_time);
  }
}
```


## Obstacle management

Obstacles come from two sources: terrain and buildings.

**Terrain obstacles** are set during map loading. The terrain service marks cells as blocked for water, cliffs, and other impassable features.

**Building obstacles** are managed by the [BuildingCollisionRegistry](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/building_collision_registry.cpp). When a building is placed, it marks the grid cells it occupies:

```cpp
void BuildingCollisionRegistry::register_building(
    unsigned int entity_id, const std::string &building_type,
    float center_x, float center_z, int owner_id) {

  BuildingSize const size = get_building_size(building_type);

  m_buildings.push_back(footprint);
  m_entity_to_index[entity_id] = m_buildings.size() - 1;

  if (auto *pf = CommandService::get_pathfinder()) {
    pf->mark_building_region_dirty(center_x, center_z, size.width, size.depth);
  }
}
```


## Dirty region updates

The grid doesn't update every frame — that would be wasteful. Instead, changes mark regions as dirty, and the grid updates lazily before the next path search:

```cpp
void Pathfinding::mark_region_dirty(int min_x, int max_x, int min_z, int max_z) {
  std::lock_guard<std::mutex> const lock(m_dirty_mutex);
  m_dirty_regions.emplace_back(min_x, max_x, min_z, max_z);
  m_obstacles_dirty.store(true);
}
```

Before computing a path, the worker thread processes dirty regions:

```cpp
void Pathfinding::update_building_obstacles() {
  if (!m_obstacles_dirty.load()) return;
  std::lock_guard<std::mutex> const lock(m_mutex);
  process_dirty_regions();
  m_obstacles_dirty.store(false);
}
```

This means placing a new barracks only updates the cells that barracks occupies, not the entire grid.

**Important race condition note:** There is a small window between `mark_region_dirty` being called on the main thread and the worker thread incorporating that change. A path computed in this window may still go through cells that are about to become blocked. The movement system's segment walkability check (see below) catches this at move time and triggers a re-path.


## Segment walkability — checking a step before taking it

Before a unit moves along each waypoint segment, the movement system verifies that the segment is still clear. This catches obstacles that appeared after the path was computed:

```cpp
auto is_segment_walkable(const QVector3D &from, const QVector3D &to,
                         Engine::Core::EntityID ignore_entity,
                         float unit_radius) -> bool {
  // 1. The destination cell must be walkable.
  Point const end_grid = CommandService::world_to_grid(to.x(), to.z());
  if (!is_walkable_func(end_grid.x, end_grid.y)) return false;

  // 2. Sample the segment at 0.5-unit intervals; any blocked sample fails.
  constexpr float sample_interval = 0.5F;
  int const num_samples = static_cast<int>(length / sample_interval);
  for (int i = 1; i <= num_samples; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(num_samples + 1);
    QVector3D const sample_pos = from + direction * t;
    if (!is_walkable_func(world_to_grid(sample_pos))) return false;
  }

  return true;
}
```

If `is_segment_walkable` returns false, the movement code tries to skip ahead up to four waypoints (`try_advance_past_blocked_segment`). If all remaining waypoints are also blocked, the unit requests a fresh path and stops moving until the new path arrives.

**Critical escape-mode exception:** When a unit is already on invalid ground (e.g. a building was placed on top of it), every intermediate sample along any segment will also be invalid (they are inside the same obstacle). If the normal segment check were applied, the unit could never build any velocity toward the recovery cell — it would be trapped indefinitely. The movement system therefore skips the intermediate-point check when:

1. The unit's current cell is invalid (`!current_position_allowed`), **and**
2. The immediate segment destination **is** walkable.

In that narrow case, the system only verifies that the unit is heading toward valid terrain; the post-movement walkability gate (`was_on_valid_tile` logic) already explicitly allows motion through invalid terrain for escaping units.


## Stuck detection and recovery

Despite best efforts, units can end up on invalid cells — a building placed on a standing unit, a save/load that restores a unit to a cell that is now occupied, or a race condition between obstacle updates and path delivery. The stuck-recovery system is a layered set of defences that guarantees a unit always escapes.

### Layer 1 — Immediate recovery on invalid ground (no target)

At the top of every `move_unit` frame update, if the unit is on invalid ground, has no pending path, and has no valid target:

```cpp
bool const needs_recovery = !movement->path_pending &&
                            movement->repath_cooldown <= 0.0F &&
                            !current_position_allowed;
bool const has_no_valid_target = !movement->has_target || !destination_allowed;

if (needs_recovery && has_no_valid_target &&
    CommandService::try_queue_local_recovery_move(...)) {
  movement->repath_cooldown = repath_cooldown_seconds;  // 0.4 s
}
```

`repath_cooldown` gates the check so that `try_queue_local_recovery_move` — which resets velocity to zero — is not called every single frame. Without the cooldown gate the unit would have its velocity wiped on every tick and could never accelerate away.

### Layer 2 — `find_recovery_cell`: scanning outward for the nearest valid cell

`try_queue_local_recovery_move` calls `find_recovery_cell`, which searches a square ring expanding outward from the unit's grid cell up to radius 16:

```cpp
auto find_recovery_cell(const Point &origin, const Pathfinding &pathfinder,
                        float unit_radius, Point &recovery_cell) -> bool {
  // Try progressively smaller radii to be permissive under tight constraints
  std::array<float, 4> const candidate_radii = {
      unit_radius,
      std::max(k_unit_radius_threshold, unit_radius * 0.85F),
      k_unit_radius_threshold,
      0.0F
  };

  for (float const candidate_radius : candidate_radii) {
    for (int radius = 1; radius <= k_recovery_search_radius; ++radius) {
      // Check perimeter of square ring at this radius
      for (int dy = -radius; dy <= radius; ++dy) {
        for (int dx = -radius; dx <= radius; ++dx) {
          if (std::abs(dx) != radius && std::abs(dy) != radius) continue;
          if (is_walkable_for_radius(pathfinder, origin.x+dx, origin.y+dy,
                                     candidate_radius)) {
            recovery_cell = {origin.x+dx, origin.y+dy};
            return true;
          }
        }
      }
    }
  }
  return false;
}
```

Example — building placed over a unit, nearest open cell is at ring radius 2:

```
  Grid around the unit (U):

    # # # # #
    # # # # #
    # # U # #     # = blocked (building)
    # # # # #     . = walkable
    # # # . .

  ring r=1: all # → not found
  ring r=2: bottom-right corner → (origin.x+2, origin.y+2) is '.' → found!
```

The recovery target is then set as a **direct move** (no A* path, just `target_x/y`), and the unit accelerates toward that cell. Once it reaches valid ground, the normal `!has_target` re-path logic fires to continue toward the original goal.

### Layer 3 — Emergency fallback when `find_recovery_cell` fails

If the entire 32×32 area around the unit is blocked (unit is deep inside a large structure), `find_recovery_cell` returns false. Rather than giving up, `try_queue_local_recovery_move` escalates to `find_nearest_walkable_point` with a 64-cell search radius and ignores the unit-radius clearance requirement:

```cpp
if (!find_recovery_cell(current_grid, *pathfinder, unit_radius, recovery_cell)) {
  // Emergency: search wider, ignore tight radius fit
  constexpr int k_emergency_search_radius = 64;
  Point const nearest = Pathfinding::find_nearest_walkable_point(
      current_grid, k_emergency_search_radius, *pathfinder, 0.0F);
  if (!pathfinder->is_walkable(nearest.x, nearest.y)) {
    return false;  // Nothing found anywhere nearby — truly no valid ground
  }
  recovery_cell = nearest;
}
```

### Layer 4 — `time_stuck` force-recovery for units with a stale valid target

A unit may be on invalid ground but still have `has_target = true` with `destination_allowed = true`. The Layer 1 check has a `has_no_valid_target` guard and deliberately skips recovery in that case — the unit is supposed to keep moving toward its existing target. But if the path is stale (e.g. a building appeared along the route), the unit won't actually move, and `has_no_valid_target` prevents Layer 1 from helping.

The `time_stuck` / `last_position_x|z` / `unstuck_cooldown` fields in `MovementComponent` detect this:

```cpp
// Every frame — track whether the unit is making physical progress
{
  float const dpx = transform->position.x - movement->last_position_x;
  float const dpz = transform->position.z - movement->last_position_z;
  bool const moved_enough = (dpx * dpx + dpz * dpz) > k_stuck_check_dist_sq; // 0.1 units
  bool const is_trying_to_move = movement->has_target || !current_position_allowed;

  if (!is_trying_to_move || moved_enough) {
    // Reset: unit is idle or making progress
    movement->last_position_x = transform->position.x;
    movement->last_position_z = transform->position.z;
    movement->time_stuck = 0.0F;
  } else {
    movement->time_stuck += delta_time;
  }
}

// After 1.5 s of no movement on invalid ground, force recovery even if a
// valid target exists.
bool const force_recovery = !current_position_allowed &&
                            !movement->path_pending &&
                            movement->unstuck_cooldown <= 0.0F &&
                            movement->time_stuck >= k_time_stuck_threshold; // 1.5 s
```

When `force_recovery` fires it calls `try_queue_local_recovery_move` (all three layers above), resets `time_stuck`, and sets `unstuck_cooldown = 1.5 s` so it doesn't re-fire immediately.

### Full recovery timeline

```
t = 0 s   Building placed on a standing unit.
           current_position_allowed = false
           has_target = false, repath_cooldown = 0

t = 0.016 Layer 1 fires: try_queue_local_recovery_move called.
           find_recovery_cell finds nearest open cell.
           has_target = true, target = recovery_cell
           repath_cooldown = 0.4 s   (gate closes)

t = 0.4 s repath_cooldown expires.  Unit has been moving
           toward recovery cell all this time (segment
           check bypassed because escaping_invalid_ground
           && destination_tile_walkable).

           If unit is now on valid ground → normal movement resumes.
           If still stuck (very large building) → Layer 1 re-fires.

t = 1.5 s If unit has not moved at all (truly surrounded) →
           Layer 4 force_recovery fires regardless of has_target.
           Layer 3 emergency fallback searched if Layer 2 returned false.
           unstuck_cooldown = 1.5 s set.

t = 3.0 s If still stuck, cycle repeats.
```


## Movement system integration

Units follow waypoints toward their final destination. When they reach a waypoint, they advance to the next one:

```cpp
if (dist2 < arrive_radius_sq) {
  if (movement->has_waypoints()) {
    movement->advance_waypoint();
    const auto &wp = movement->current_waypoint();
    movement->target_x = wp.first;
    movement->target_y = wp.second;
  } else {
    // Reached destination
    movement->has_target = false;
  }
}
```

After reaching the recovery cell (`has_target` clears), the movement system's `!has_target` branch automatically re-requests a full A* path to the original `goal_x/goal_y`:

```cpp
if (!movement->path_pending && movement->repath_cooldown <= 0.0F &&
    goal_dist_sq > k_stuck_distance_sq && destination_allowed) {
  CommandService::move_units(*world, ids, {final_goal}, opts);
  movement->repath_cooldown = repath_cooldown_seconds;
}
```


## Finding walkable positions

Sometimes a requested destination isn't walkable — maybe the player clicked on a building. The system finds the nearest valid position:

```cpp
auto Pathfinding::find_nearest_walkable_point(const Point &point,
                                              int max_search_radius,
                                              const Pathfinding &pathfinder,
                                              float unit_radius) -> Point {
  if (is_walkableFunc(point.x, point.y)) return point;

  for (int radius = 1; radius <= max_search_radius; ++radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (std::abs(dx) != radius && std::abs(dy) != radius) continue;
        if (is_walkableFunc(point.x + dx, point.y + dy)) {
          return {point.x + dx, point.y + dy};
        }
      }
    }
  }

  return point;  // No walkable point found within radius
}
```

This ring-by-ring spiral search efficiently finds the closest walkable cell without examining every cell in the search area. The same pattern is used by `find_recovery_cell` internally.


## Formation planning integration

When moving groups, the [FormationPlanner](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/formation_planner.h) calculates target positions that maintain formation while respecting walkability:

```cpp
static auto find_nearest_walkable(const QVector3D &position,
                                  Pathfinding *pathfinder,
                                  int max_search_radius = 5) -> QVector3D {
  if (pathfinder == nullptr) return position;

  float const offset_x = pathfinder->get_grid_offset_x();
  float const offset_z = pathfinder->get_grid_offset_z();

  int const center_grid_x =
      static_cast<int>(std::round(position.x() - offset_x));
  int const center_grid_z =
      static_cast<int>(std::round(position.z() - offset_z));

  if (pathfinder->is_walkable(center_grid_x, center_grid_z)) return position;

  // Search outward for a walkable cell and return world-space position...
}
```

The planner checks whether the destination area is mostly walkable. If not, it adjusts the formation centre to a better location before submitting individual path requests for each member.


## Performance characteristics

| Metric | Value |
|--------|-------|
| Grid resolution | 1 cell = 1 world unit |
| Movement directions | 8 (cardinals + diagonals) |
| Path cost model | Uniform (1 per step) |
| Thread count | 1 dedicated worker |
| Max iterations | width × height cells |
| Memory per cell | 1 byte obstacle + working arrays |
| Normal recovery search | up to radius 16 (32×32 area) |
| Emergency recovery search | up to radius 64 |
| Stuck detection threshold | 1.5 s of no movement |

For a 200×200 map, the working arrays total about 320 KB (generation counters, g-costs, parents). Paths typically compute in under 1 ms for reasonable distances.


## Common issues and debugging

**Units won't move to a location**: Check if the destination is walkable with `pathfinder->is_walkable(grid_x, grid_z)`. Buildings or terrain may be blocking it. The system snaps click targets to the nearest walkable cell automatically.

**Paths go through buildings**: The obstacle grid might be stale. Ensure `mark_building_region_dirty()` is called when buildings are placed or removed. Paths computed during the dirty-window are caught by `is_segment_walkable` at move time.

**Units end up inside a building (on invalid ground)**: This is handled automatically by the four-layer recovery system described above. If a unit is visually stuck longer than ~2–3 seconds, check that the pathfinder is initialised and `BuildingCollisionRegistry::register_building` was called for the building in question.

**Movement feels laggy**: Path results are applied on the next frame after they're ready. If the worker thread is overloaded, requests queue up. Consider reducing simultaneous path requests during mass unit selection.

**Diagonal corner cutting**: The neighbour collection prevents this, but verify `is_walkable()` returns correct values for the cells between the start and diagonal target.

**Large units squeeze through narrow gaps**: Ensure `TroopConfig::get_selection_ring_size` returns the correct radius for the spawn type. All walkability checks in pathfinding, movement, and recovery use the same radius value.


## Adding new obstacle types

To add a new obstacle type (say, destructible walls):

1. Create a registry similar to `BuildingCollisionRegistry`.
2. When walls are created, mark their cells in the pathfinding grid.
3. When destroyed, mark the region dirty so paths can route through them.

```cpp
void WallRegistry::destroy_wall(unsigned int entity_id) {
  auto cells = get_wall_cells(entity_id);
  // ... remove from registry ...
  if (auto *pf = CommandService::get_pathfinder()) {
    for (const auto &cell : cells) {
      pf->mark_region_dirty(cell.x, cell.x, cell.z, cell.z);
    }
  }
}
```


## Finding your way around

| What you want to do | Where to look |
|---------------------|---------------|
| Submit a path request | `command_service.cpp` — `move_units()` |
| Adjust grid resolution | `pathfinding.cpp` — `m_grid_cell_size` |
| Mark obstacles | `pathfinding.cpp` — `set_obstacle()` |
| Handle building placement | `building_collision_registry.cpp` |
| Apply paths to units | `movement_system.cpp` |
| Find nearest walkable point | `pathfinding.cpp` — `find_nearest_walkable_point()` |
| Recovery from invalid position | `command_service.cpp` — `try_queue_local_recovery_move()`, `find_recovery_cell()` |
| Stuck detection | `movement_system.cpp` — `time_stuck` / `force_recovery` block |
| Formation target adjustment | `formation_planner.h` |

The code follows snake_case for functions and variables, PascalCase for types. All cross-thread access uses mutexes. The grid is protected by `m_mutex` during path computation; the request and result queues have their own mutexes.

When in doubt, start with `CommandService::move_units()` and follow the data: world coordinates → grid coordinates → path request → worker thread → path result → movement component → waypoint following. That covers the full journey of a unit's path from click to destination.

