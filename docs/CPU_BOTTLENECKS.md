# CPU Bottleneck Analysis

This document analyzes the major CPU bottlenecks in the Standard-of-Iron codebase and provides actionable recommendations to improve performance.

## Table of Contents

1. [Entity-Component System Queries](#1-entity-component-system-queries)
2. [Pathfinding System](#2-pathfinding-system)
3. [Combat System - Attack Processing](#3-combat-system---attack-processing)
4. [Visibility System](#4-visibility-system)
5. [Rendering - World Iteration](#5-rendering---world-iteration)
6. [AI System - Snapshot Building](#6-ai-system---snapshot-building)
7. [Movement System](#7-movement-system)
8. [Building Collision Registry](#8-building-collision-registry)
9. [Formation System](#9-formation-system)
10. [Auto-Engagement System](#10-auto-engagement-system)

---

## 1. Entity-Component System Queries

### Bottleneck Description

The `World::get_entities_with<T>()` template method iterates over all entities in the world and checks each one for a specific component type. This O(n) operation is called frequently by multiple systems every frame.

**Location:** `game/core/world.h:42-51`

```cpp
template <typename T> auto get_entities_with() -> std::vector<Entity *> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::vector<Entity *> result;
  for (auto &[entity_id, entity] : m_entities) {
    if (entity->template has_component<T>()) {
      result.push_back(entity.get());
    }
  }
  return result;
}
```

### Impact

- Called multiple times per frame by: `MovementSystem`, `CombatSystem`, `AISystem`, `GuardSystem`, `VisibilityService`, `SceneRenderer`, and others.
- Each call locks `m_entity_mutex` and performs a full map traversal.
- Returns a new vector allocation each time.

### Recommended Solutions

1. **Component Index Cache**: Maintain per-component-type indices that track which entities have each component type. Update these indices when components are added/removed.

```cpp
// In World class
std::unordered_map<std::type_index, std::unordered_set<EntityID>> m_component_indices;

template <typename T> auto get_entities_with() -> std::vector<Entity *> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  auto it = m_component_indices.find(std::type_index(typeid(T)));
  if (it == m_component_indices.end()) {
    return {};
  }
  std::vector<Entity *> result;
  result.reserve(it->second.size());
  for (EntityID id : it->second) {
    if (auto *entity = get_entity(id)) {
      result.push_back(entity);
    }
  }
  return result;
}
```

2. **Archetypes/Sparse Sets**: Use an archetype-based ECS (like EnTT) or sparse set storage for cache-friendly iteration.

3. **Query Caching**: Cache query results for a frame and invalidate when entities/components change.

---

## 2. Pathfinding System

### Bottleneck Description

The A* pathfinding implementation performs single-threaded path calculations with a dedicated worker thread, but several inefficiencies exist:

**Location:** `game/systems/pathfinding.cpp`

1. **Obstacle Grid Updates** (lines 57-117): Iterates over the entire grid (`m_width * m_height`) to reset and rebuild obstacles.
2. **Per-Path Mutex Lock** (line 126): Every `findPath()` call locks `m_mutex`, serializing path requests.
3. **Heap Operations** (lines 411-451): Custom heap implementation without SIMD optimization.

### Impact

- Large maps (e.g., 200x200) require 40,000 cell iterations for obstacle updates.
- Multiple units requesting paths simultaneously are serialized.
- Path calculations can stall the game loop.

### Recommended Solutions

1. **Dirty Region Tracking**: Instead of resetting the entire obstacle grid, track dirty regions and only update changed cells.

```cpp
struct DirtyRegion {
  int min_x, max_x, min_z, max_z;
};
std::vector<DirtyRegion> m_dirty_regions;

void updateBuildingObstacles() {
  for (const auto& region : m_dirty_regions) {
    for (int z = region.min_z; z <= region.max_z; ++z) {
      for (int x = region.min_x; x <= region.max_x; ++x) {
        // Update only affected cells
      }
    }
  }
  m_dirty_regions.clear();
}
```

2. **Hierarchical Pathfinding (HPA*)**: Precompute a high-level graph of navigable regions and use it for long-distance paths.

3. **Multi-threaded Path Worker Pool**: Replace single worker thread with a thread pool to process multiple path requests in parallel.

4. **Jump Point Search (JPS)**: For uniform-cost grids, JPS can be 10-30x faster than A*.

5. **Flow Fields**: For many units moving to the same destination, compute a single flow field instead of individual paths.

---

## 3. Combat System - Attack Processing

### Bottleneck Description

The attack processor iterates over all units with `UnitComponent` and performs nested loops and repeated lookups.

**Location:** `game/systems/combat_system/attack_processor.cpp:286-659`

1. **Full Unit Iteration** (line 287): `world->get_entities_with<Engine::Core::UnitComponent>()` called every frame.
2. **Fallback Target Search** (lines 525-556): When no explicit target exists, iterates over all units again to find one in range.
3. **Repeated Component Lookups**: Multiple `get_component<>()` calls per entity per frame.

### Impact

- O(n²) worst case for target finding.
- With 500 units, potentially 250,000 distance calculations per frame.

### Recommended Solutions

1. **Spatial Partitioning**: Use a grid-based or quadtree spatial hash for range queries.

```cpp
class SpatialGrid {
  std::unordered_map<int, std::vector<EntityID>> m_cells;
  float m_cell_size;
  
  auto get_entities_in_range(float x, float z, float range) -> std::vector<EntityID> {
    int min_cell_x = static_cast<int>((x - range) / m_cell_size);
    int max_cell_x = static_cast<int>((x + range) / m_cell_size);
    // Only check cells within range
  }
};
```

2. **Cache Component Pointers**: Store frequently accessed component pointers in a combat-specific cache updated at frame start.

3. **Batch Distance Calculations**: Use SIMD instructions to compute multiple distances in parallel.

4. **Combat Groups**: Group nearby units and process group-vs-group interactions instead of individual unit checks.

---

## 4. Visibility System

### Bottleneck Description

The visibility system updates fog-of-war by iterating over all cells within vision range of all units.

**Location:** `game/map/visibility_service.cpp:246-294`

```cpp
for (const auto &source : payload.sources) {
  for (int dz = -source.cell_radius; dz <= source.cell_radius; ++dz) {
    // ...
    for (int dx = -source.cell_radius; dx <= source.cell_radius; ++dx) {
      // Distance check and cell update
    }
  }
}
```

### Impact

- For 100 units with vision radius 12 on a 1.0 tile size, each unit checks ~576 cells.
- Total: 57,600 cell checks per visibility update.

### Recommended Solutions

1. **Coarse-to-Fine Updates**: Use larger cells for visibility with finer detail near camera.

2. **Incremental Updates**: Only update visibility for units that have moved since last frame.

```cpp
struct VisionSource {
  int center_x, center_z;
  int last_center_x, last_center_z;  // Track previous position
  
  bool has_moved() const {
    return center_x != last_center_x || center_z != last_center_z;
  }
};
```

3. **SIMD Cell Updates**: Process multiple cells in parallel using SSE/AVX instructions.

4. **GPU Compute**: Offload visibility calculations to a compute shader.

5. **Precomputed Vision Masks**: Store circular vision patterns as bitmasks and OR them onto the visibility grid.

---

## 5. Rendering - World Iteration

### Bottleneck Description

The scene renderer iterates over all renderable entities, performing visibility checks, LOD calculations, and component lookups.

**Location:** `render/scene_renderer.cpp:576-846`

1. **Two Full Passes** (lines 601-617 and 634-816): First pass counts visible units, second pass renders them.
2. **Per-Entity Frustum Culling** (line 668): Individual frustum checks for each entity.
3. **Component Lookups in Hot Path**: Multiple `get_component<>()` calls in the inner render loop.

### Impact

- With 1000 entities, 2000+ iterations per frame plus component lookups.

### Recommended Solutions

1. **Spatial Acceleration Structure**: Use an octree or BVH for frustum culling to reject large groups of off-screen entities quickly.

2. **Render Batches**: Group entities by renderer type and LOD level before iterating.

```cpp
struct RenderBatch {
  std::string renderer_id;
  HumanoidLOD lod;
  std::vector<Entity*> entities;
};
std::vector<RenderBatch> m_render_batches;
```

3. **Combined Visibility Pass**: Merge the unit counting and rendering passes into a single iteration.

4. **Cache Render State**: Store transform, renderable, and unit components in a contiguous render cache updated once per frame.

5. **Instance Culling on GPU**: Use GPU-driven rendering with instance culling in a compute shader.

---

## 6. AI System - Snapshot Building

### Bottleneck Description

The AI snapshot builder gathers data about all friendly and enemy units every AI update cycle.

**Location:** `game/systems/ai_system/ai_snapshot_builder.cpp:9-111`

1. **Full Entity Iteration** (lines 14 and 80): Two separate calls to gather friendlies and enemies.
2. **Per-Entity Component Access**: Multiple component lookups for each entity.
3. **Vector Copies**: Snapshot data is copied into the AI job.

### Impact

- AI updates are throttled to `m_update_interval` but still process all units when triggered.
- Memory allocation overhead from vector copies.

### Recommended Solutions

1. **Incremental Snapshots**: Only update entities that have changed since last snapshot.

```cpp
struct EntityChangeTracker {
  std::uint64_t last_snapshot_version;
  std::unordered_set<EntityID> changed_entities;
};
```

2. **Shared Snapshot Pool**: Use object pooling for snapshot data to avoid allocations.

3. **Lazy Component Caching**: Cache AI-relevant component data in a dedicated structure updated by systems as they modify state.

4. **Spatial Queries for Enemies**: Instead of iterating all enemies, use spatial queries to find relevant enemies near AI-controlled units.

---

## 7. Movement System

### Bottleneck Description

The movement system updates all entities with `MovementComponent`, performing walkability checks and path following.

**Location:** `game/systems/movement_system.cpp:89-410`

1. **Per-Step Walkability Checks** (lines 51-85): `isSegmentWalkable` samples multiple points along a movement vector.
2. **Pathfinder Lookups** (lines 33-38): Grid coordinate conversion and walkability checks for each point.
3. **Path Vector Operations** (lines 305, 243): `path.erase(path.begin())` is O(n) on vectors.

### Impact

- Long paths with many waypoints cause repeated O(n) erasures.
- Walkability checks on every movement step.

### Recommended Solutions

1. **Use Deque for Paths**: Replace `std::vector` path storage with `std::deque` for O(1) front removal.

```cpp
// In MovementComponent
std::deque<std::pair<float, float>> path;
```

2. **Path Index Instead of Erase**: Track current waypoint index instead of erasing completed waypoints.

```cpp
class MovementComponent : public Component {
  std::vector<std::pair<float, float>> path;
  size_t current_waypoint_index{0};
};
```

3. **Batch Walkability Checks**: Pre-validate entire path segments less frequently instead of checking every frame.

4. **Terrain Height Cache**: Cache terrain height lookups in a per-entity movement cache.

---

## 8. Building Collision Registry

### Bottleneck Description

The building collision registry checks all buildings for point-in-building tests.

**Location:** `game/systems/building_collision_registry.cpp:110-130`

```cpp
auto BuildingCollisionRegistry::isPointInBuilding(
    float x, float z, unsigned int ignoreEntityId) const -> bool {
  for (const auto &building : m_buildings) {
    // AABB check for each building
  }
  return false;
}
```

### Impact

- Called from movement system, pathfinding, and terrain service.
- Linear scan of all buildings for each point check.

### Recommended Solutions

1. **Grid-Based Building Index**: Maintain a spatial grid mapping cells to buildings.

```cpp
std::unordered_map<std::pair<int,int>, std::vector<size_t>> m_building_grid;

bool isPointInBuilding(float x, float z, unsigned int ignoreEntityId) const {
  int cell_x = static_cast<int>(x / m_cell_size);
  int cell_z = static_cast<int>(z / m_cell_size);
  auto it = m_building_grid.find({cell_x, cell_z});
  if (it == m_building_grid.end()) return false;
  // Only check buildings in this cell
}
```

2. **Precomputed Collision Grid**: For static buildings, bake collision data into the pathfinding obstacle grid.

---

## 9. Formation System

### Bottleneck Description

Formation calculations create temporary vectors and perform repeated unit categorization.

**Location:** `game/systems/formation_system.cpp:45-224`

1. **Unit Categorization** (lines 91-104): Each formation calculation iterates all units and categorizes by type.
2. **Vector Allocations**: Multiple temporary vectors created for each unit type category.

### Impact

- Formation calculations during unit selection/movement create garbage.

### Recommended Solutions

1. **Pre-categorized Unit Lists**: Maintain separate lists by unit type, updated when units spawn/die.

2. **Object Pool for Positions**: Use a pool allocator for formation position vectors.

3. **Cache Formation Results**: Store calculated formation positions and reuse if center/unit count unchanged.

---

## 10. Auto-Engagement System

### Bottleneck Description

The auto-engagement system searches for nearby enemies for each idle melee unit.

**Location:** `game/systems/combat_system/auto_engagement.cpp:11-87` and `combat_utils.cpp:98-155`

1. **find_nearest_enemy** (combat_utils.cpp:98): Called for each auto-engaging unit, iterates all units.
2. **Repeated Entity Queries**: Each call to `find_nearest_enemy` calls `get_entities_with<UnitComponent>()`.

### Impact

- O(n²) behavior when many units are idle and looking for targets.

### Recommended Solutions

1. **Shared Enemy Query**: Perform the entity query once per frame and share the result.

```cpp
void AutoEngagement::process(Engine::Core::World *world, float delta_time) {
  // Query once
  auto all_units = world->get_entities_with<Engine::Core::UnitComponent>();
  
  for (auto *unit : units) {
    // Pass pre-queried list to find_nearest_enemy
    auto *nearest = find_nearest_enemy(unit, all_units, detection_range);
  }
}
```

2. **Spatial Hash for Units**: Maintain a spatial hash of unit positions updated each frame.

3. **Staggered Processing**: Only process a fraction of units each frame using a round-robin approach.

```cpp
void AutoEngagement::process(Engine::Core::World *world, float delta_time) {
  // Process 1/4 of units each frame
  int batch_size = units.size() / 4 + 1;
  int start = m_current_batch * batch_size;
  int end = std::min(start + batch_size, (int)units.size());
  
  for (int i = start; i < end; ++i) {
    // Process unit
  }
  m_current_batch = (m_current_batch + 1) % 4;
}
```

---

## Summary Table

| Priority | System | Est. Impact | Complexity | Recommended First Step |
|----------|--------|-------------|------------|------------------------|
| 1 | ECS Queries | High | Medium | Implement component index cache |
| 2 | Combat Attack Processing | High | Medium | Add spatial partitioning |
| 3 | Pathfinding | High | High | Implement dirty region tracking |
| 4 | Auto-Engagement | Medium | Low | Share entity query per frame |
| 5 | Visibility | Medium | Medium | Incremental updates |
| 6 | Rendering | Medium | High | Merge passes, add spatial culling |
| 7 | Movement | Low-Medium | Low | Use deque or path index |
| 8 | Building Collision | Low-Medium | Low | Grid-based building index |
| 9 | AI Snapshot | Low | Medium | Incremental snapshots |
| 10 | Formation | Low | Low | Pre-categorized unit lists |

---

## General Recommendations

### Memory Access Patterns

- **Structure of Arrays (SoA)**: Convert component data to SoA layout for better cache utilization during batch processing.
- **Data-Oriented Design**: Group frequently accessed data together and separate rarely-used fields.

### Threading

- **Job System**: Implement a job system for parallel execution of independent systems.
- **Read-Write Locks**: Replace `recursive_mutex` with `shared_mutex` for read-heavy operations.

### Profiling

- **Instrumentation**: Add timing instrumentation to each system's `update()` method.
- **Frame Budget Visualization**: Display per-system frame time breakdown in debug UI.

### Future Architecture

Consider migrating to a mature ECS library like **EnTT** which provides:
- Sparse set storage for cache-friendly iteration
- Built-in component groups for multi-component queries
- View-based iteration without allocations
- Better memory layout for component data
