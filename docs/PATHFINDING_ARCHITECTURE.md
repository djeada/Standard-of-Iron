# How the Pathfinding System Actually Works

Imagine you've selected a group of 50 soldiers and right-clicked on a position across the map. Between your troops and that destination is a river, some buildings, and rough terrain. Each soldier needs to find a path around these obstacles, and they need to do it fast enough that the game doesn't stutter, even when multiple players are issuing movement commands simultaneously.

This is the story of how Standard of Iron's pathfinding works. We'll trace the journey from a movement command to a completed path, looking at the A* algorithm, the grid system, the threading model, and how everything integrates with buildings and terrain.

## What we'll cover

We'll start with the core data structures: the grid, obstacles, and coordinate transformations. Then we'll dig into the A* implementation and the optimizations that make it fast. We'll look at how pathfinding requests are queued and processed in a background thread, and finally how the results flow back into the movement system.


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
├── command_service.cpp            # Manages pending requests, coordinate conversion
├── movement_system.cpp            # Applies paths to unit movement
├── building_collision_registry.h  # Tracks building footprints as obstacles
└── building_collision_registry.cpp
```

The [CommandService](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/command_service.cpp) acts as the coordinator. It owns the pathfinder instance, handles coordinate conversion, and tracks which entities have pending path requests.


## Data structures: the grid and obstacles

The pathfinding grid is a 2D array of obstacle flags. Each cell is either walkable (0) or blocked (1):

```cpp
class Pathfinding {
  int m_width, m_height;
  std::vector<std::vector<std::uint8_t>> m_obstacles;
  float m_grid_cell_size{1.0F};
  float m_grid_offset_x{0.0F}, m_grid_offset_z{0.0F};
  // ...
};
```

The offset values map grid coordinates to world coordinates. For a 200x200 world, the offset might be (-99.5, -99.5), centering the grid on the origin.

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

The Y component is always 0 because pathfinding is 2D—we handle terrain height separately when units actually move.


## The A* algorithm

A* is a best-first search that uses a heuristic to guide expansion toward the goal. Our implementation uses Manhattan distance as the heuristic (sum of horizontal and vertical distances), which is admissible for grid-based movement.

The algorithm maintains two key costs for each cell:
- **g-cost**: The actual cost from the start to this cell
- **f-cost**: g-cost plus the estimated cost to the goal (heuristic)

From [pathfinding.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/pathfinding.cpp):

```cpp
auto Pathfinding::find_path_internal(const Point &start, const Point &end,
                                     float unit_radius) -> std::vector<Point> {
  // Validate start and end positions
  if (!is_walkableFunc(start.x, start.y) || !is_walkableFunc(end.x, end.y)) {
    return {};
  }

  const int start_idx = to_index(start);
  const int end_idx = to_index(end);

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
      // Found the goal - reconstruct path using current.g_cost
      build_path(start_idx, end_idx, generation, current.g_cost + 1, path);
      return path;
    }

    // Expand neighbors
    for (each neighbor of current) {
      if (!walkable || already closed) continue;
      
      int tentative_gcost = current.g_cost + 1;
      if (tentative_gcost < neighbor's current g_cost) {
        update neighbor's costs and parent
        add neighbor to open heap
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


## Performance optimization: generation-based clearing

A naive A* implementation clears all working arrays before each search. For a 200x200 grid, that's 40,000 cells to reset. We avoid this with generation counters.

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
  m_g_cost_values[idx] = cost;
}
```

This gives us O(1) "clearing" between searches—we just increment the generation counter. The same technique applies to the closed set and parent tracking.


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

The comparison prioritizes lower f-cost, using g-cost as a tiebreaker (preferring nodes closer to the start when f-costs are equal):

```cpp
auto Pathfinding::heap_less(const QueueNode &lhs, const QueueNode &rhs) -> bool {
  if (lhs.f_cost != rhs.f_cost) {
    return lhs.f_cost < rhs.f_cost;
  }
  return lhs.g_cost < rhs.g_cost;
}
```


## Eight-directional movement

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
      
      // Prevent corner cutting through obstacles
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

The diagonal check prevents units from cutting through corners. If a unit is at (5, 5) and wants to move diagonally to (6, 6), both (6, 5) and (5, 6) must be walkable. Otherwise, the unit would clip through a wall corner.


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

      if (m_stop_worker.load() && m_request_queue.empty()) {
        break;
      }

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

The main thread picks up completed paths in the movement system:

```cpp
auto Pathfinding::fetch_completed_paths() -> std::vector<PathResult> {
  std::vector<PathResult> results;
  std::lock_guard<std::mutex> const lock(m_result_mutex);
  while (!m_result_queue.empty()) {
    results.push_back(std::move(m_result_queue.front()));
    m_result_queue.pop();
  }
  return results;
}
```


## Obstacle management

Obstacles come from two sources: terrain and buildings.

Terrain obstacles are set during map loading. The terrain service marks cells as blocked for water, cliffs, and other impassable features.

Building obstacles are managed by the [BuildingCollisionRegistry](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/building_collision_registry.cpp). When a building is placed, it marks the grid cells it occupies:

```cpp
void BuildingCollisionRegistry::register_building(
    unsigned int entity_id, const std::string &building_type, 
    float center_x, float center_z, int owner_id) {
  
  BuildingSize const size = get_building_size(building_type);
  
  // Register footprint
  m_buildings.push_back(footprint);
  m_entity_to_index[entity_id] = m_buildings.size() - 1;

  // Mark pathfinding grid as dirty
  if (auto *pf = CommandService::get_pathfinder()) {
    pf->mark_building_region_dirty(center_x, center_z, size.width, size.depth);
  }
}
```


## Dirty region updates

The grid doesn't update every frame—that would be wasteful. Instead, changes mark regions as dirty, and the grid updates lazily when the next path request needs it:

```cpp
void Pathfinding::mark_region_dirty(int min_x, int max_x, int min_z, int max_z) {
  min_x = std::max(0, min_x);
  max_x = std::min(m_width - 1, max_x);
  min_z = std::max(0, min_z);
  max_z = std::min(m_height - 1, max_z);

  std::lock_guard<std::mutex> const lock(m_dirty_mutex);
  m_dirty_regions.emplace_back(min_x, max_x, min_z, max_z);
  m_obstacles_dirty.store(true);
}
```

Before computing a path, the system processes dirty regions:

```cpp
void Pathfinding::update_building_obstacles() {
  if (!m_obstacles_dirty.load()) {
    return;
  }

  std::lock_guard<std::mutex> const lock(m_mutex);
  process_dirty_regions();
  m_obstacles_dirty.store(false);
}
```

This means building a new barracks only updates the cells that barracks occupies, not the entire grid.


## Movement system integration

The [MovementSystem](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/movement_system.cpp) picks up completed paths and applies them to units. Each unit's MovementComponent stores its current waypoints:

```cpp
void MovementSystem::update(Engine::Core::World *world, float delta_time) {
  CommandService::process_path_results(*world);  // Pick up completed paths
  
  auto entities = world->get_entities_with<Engine::Core::MovementComponent>();
  for (auto *entity : entities) {
    move_unit(entity, world, delta_time);
  }
}
```

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


## Finding walkable positions

Sometimes a requested destination isn't walkable—maybe the player clicked on a building. The system finds the nearest valid position:

```cpp
auto Pathfinding::find_nearest_walkable_point(const Point &point,
                                              int max_search_radius,
                                              const Pathfinding &pathfinder,
                                              float unit_radius) -> Point {
  if (is_walkableFunc(point.x, point.y)) {
    return point;
  }

  for (int radius = 1; radius <= max_search_radius; ++radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (std::abs(dx) != radius && std::abs(dy) != radius) {
          continue;  // Only check the perimeter of this ring
        }

        int const check_x = point.x + dx;
        int const check_y = point.y + dy;

        if (is_walkableFunc(check_x, check_y)) {
          return {check_x, check_y};
        }
      }
    }
  }

  return point;  // No walkable point found
}
```

This spiral search efficiently finds the closest walkable cell without checking every cell in the search area.


## Formation planning integration

When moving groups, the [FormationPlanner](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/formation_planner.h) calculates target positions that maintain formation while respecting walkability:

```cpp
static auto find_nearest_walkable(const QVector3D &position,
                                  Pathfinding *pathfinder,
                                  int max_search_radius = 5) -> QVector3D {
  if (pathfinder == nullptr) {
    return position;
  }

  // Get the grid offset from the pathfinder
  float const offset_x = pathfinder->get_grid_offset_x();
  float const offset_z = pathfinder->get_grid_offset_z();

  int const center_grid_x =
      static_cast<int>(std::round(position.x() - offset_x));
  int const center_grid_z =
      static_cast<int>(std::round(position.z() - offset_z));

  if (pathfinder->is_walkable(center_grid_x, center_grid_z)) {
    return position;
  }

  // Search for nearby walkable position...
}
```

The planner checks if the destination area is mostly walkable. If not, it adjusts the formation center to a better location.


## Performance characteristics

| Metric | Value |
|--------|-------|
| Grid resolution | 1 cell = 1 world unit |
| Movement directions | 8 (cardinals + diagonals) |
| Path cost model | Uniform (1 per step) |
| Thread count | 1 dedicated worker |
| Max iterations | width × height cells |
| Memory per cell | 1 byte obstacle + working arrays |

For a 200×200 map, the working arrays total about 320KB (generation counters, g-costs, parents). Paths typically compute in under 1ms for reasonable distances.


## Common issues and debugging

**Units won't move to a location**: Check if the destination is walkable. Use `pathfinder->is_walkable(grid_x, grid_z)` to verify. Buildings or terrain may be blocking it.

**Paths go through buildings**: The obstacle grid might be stale. Ensure `mark_building_region_dirty()` is called when buildings are placed or removed.

**Movement feels laggy**: Path results are applied on the next frame after they're ready. If the worker thread is overloaded, requests queue up. Consider reducing simultaneous path requests during mass unit selection.

**Units stuck at obstacles**: The movement system checks walkability during movement. If an obstacle appears in the path, the unit stops and may request a new path. Check `repath_cooldown` values if units seem slow to react.

**Diagonal corner cutting**: The neighbor collection should prevent this, but verify `is_walkable()` returns correct values for the cells between the start and diagonal target.


## Adding new obstacle types

To add a new obstacle type (say, destructible walls):

1. Create a registry similar to BuildingCollisionRegistry
2. When walls are created, mark their cells in the pathfinding grid
3. When destroyed, mark the region dirty so paths can go through

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
| Submit a path request | [command_service.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/command_service.cpp) - move_units() |
| Adjust grid resolution | [pathfinding.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/pathfinding.cpp) - m_grid_cell_size |
| Mark obstacles | [pathfinding.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/pathfinding.cpp) - set_obstacle() |
| Handle building placement | [building_collision_registry.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/building_collision_registry.cpp) |
| Apply paths to units | [movement_system.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/movement_system.cpp) |
| Find nearest walkable point | [pathfinding.cpp](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/pathfinding.cpp) - find_nearest_walkable_point() |
| Formation target adjustment | [formation_planner.h](https://github.com/djeada/Standard-of-Iron/blob/main/game/systems/formation_planner.h) |

The code follows snake_case for functions and variables, PascalCase for types. All cross-thread access uses mutexes. The grid is protected by m_mutex during path computation, and the request/result queues have their own mutexes.

When in doubt, start with CommandService::move_units() and follow the data: world coordinates → grid coordinates → path request → worker thread → path result → movement component. That covers the full journey of a unit's path.
