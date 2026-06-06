#include "pathfinding.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

#include "../map/terrain_service.h"
#include "building_collision_registry.h"
#include "map/terrain.h"

namespace Game::Systems {

namespace {

auto terrain_cell_value(const Game::Map::TerrainService& terrain_service,
                        const Game::Map::TerrainHeightMap* height_map,
                        int x,
                        int z) -> Pathfinding::CellValue {
  if (height_map == nullptr || x < 0 || x >= height_map->get_width() || z < 0 ||
      z >= height_map->get_height()) {
    return Pathfinding::CellValue::Blocked;
  }

  Game::Map::TerrainType const terrain_type = terrain_service.get_terrain_type(x, z);
  if (terrain_type == Game::Map::TerrainType::Mountain) {
    return Pathfinding::CellValue::Blocked;
  }

  if (height_map->isBridgeCell(x, z) || height_map->isHillEntrance(x, z)) {
    return Pathfinding::CellValue::Walkable;
  }

  if (terrain_type == Game::Map::TerrainType::River) {
    return height_map->isBridgeCenterline(x, z) ? Pathfinding::CellValue::Walkable
                                                : Pathfinding::CellValue::Blocked;
  }

  return terrain_service.is_walkable(x, z) ? Pathfinding::CellValue::Walkable
                                           : Pathfinding::CellValue::Blocked;
}

} // namespace

Pathfinding::NavigationGrid::NavigationGrid(int width, int height) {
  resize(width, height);
}

void Pathfinding::NavigationGrid::resize(int width, int height) {
  m_width = std::max(width, 0);
  m_height = std::max(height, 0);
  m_cells.assign(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height),
                 static_cast<std::uint8_t>(CellValue::Walkable));
}

void Pathfinding::NavigationGrid::fill(CellValue value) {
  std::fill(m_cells.begin(), m_cells.end(), static_cast<std::uint8_t>(value));
}

auto Pathfinding::NavigationGrid::in_bounds(int x, int y) const -> bool {
  return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

auto Pathfinding::NavigationGrid::get(int x, int y) const -> CellValue {
  if (!in_bounds(x, y)) {
    return CellValue::Blocked;
  }
  return static_cast<CellValue>(m_cells[static_cast<std::size_t>(y * m_width + x)]);
}

void Pathfinding::NavigationGrid::set(int x, int y, CellValue value) {
  if (!in_bounds(x, y)) {
    return;
  }
  m_cells[static_cast<std::size_t>(y * m_width + x)] = static_cast<std::uint8_t>(value);
}

Pathfinding::Pathfinding(int width, int height)
    : m_width(width)
    , m_height(height)
    , m_navigation_grid(width, height) {
  ensure_working_buffers();
  m_navigation_grid_dirty.store(true, std::memory_order_release);
}

Pathfinding::~Pathfinding() = default;

void Pathfinding::set_grid_offset(float offset_x, float offset_z) {
  m_grid_offset_x = offset_x;
  m_grid_offset_z = offset_z;
}

auto Pathfinding::world_to_grid(float world_x, float world_z) const -> Point {
  return {static_cast<int>(std::round(world_x - m_grid_offset_x)),
          static_cast<int>(std::round(world_z - m_grid_offset_z))};
}

auto Pathfinding::grid_to_world(const Point& grid_pos) const -> QVector3D {
  return {static_cast<float>(grid_pos.x) + m_grid_offset_x,
          0.0F,
          static_cast<float>(grid_pos.y) + m_grid_offset_z};
}

void Pathfinding::set_obstacle(int x, int y, bool is_obstacle) {
  m_navigation_grid.set(x, y, is_obstacle ? CellValue::Blocked : CellValue::Walkable);
}

auto Pathfinding::is_walkable(int x, int y) const -> bool {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
    return false;
  }
  return cell_value(x, y) == CellValue::Walkable;
}

auto Pathfinding::is_tree(int x, int y) const -> bool {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
    return false;
  }
  return cell_value(x, y) == CellValue::Tree;
}

auto Pathfinding::is_boulder(int x, int y) const -> bool {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
    return false;
  }
  return cell_value(x, y) == CellValue::Boulder;
}

auto Pathfinding::is_iron_ore(int x, int y) const -> bool {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
    return false;
  }
  return cell_value(x, y) == CellValue::IronOre;
}

auto Pathfinding::cell_value(int x, int y) const -> CellValue {
  return m_navigation_grid.get(x, y);
}

auto Pathfinding::is_world_position_walkable(const QVector3D& world_position) const
    -> bool {
  Point const grid = world_to_grid(world_position.x(), world_position.z());
  return is_walkable(grid.x, grid.y);
}

auto Pathfinding::is_world_segment_walkable(const QVector3D& from,
                                            const QVector3D& to) const -> bool {
  auto segment_point_walkable = [this](const QVector3D& point) -> bool {
    return is_world_position_walkable(point);
  };

  if (!segment_point_walkable(to)) {
    return false;
  }

  QVector3D const direction = to - from;
  float const length = direction.length();
  if (length < 0.5F) {
    return true;
  }

  constexpr float sample_interval = 0.5F;
  int const num_samples = static_cast<int>(length / sample_interval);
  for (int i = 1; i <= num_samples; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(num_samples + 1);
    QVector3D const sample_pos = from + direction * t;
    if (!segment_point_walkable(sample_pos)) {
      return false;
    }
  }

  return true;
}

auto Pathfinding::path_waypoint_world_position(const Point& path_cell) const
    -> QVector3D {
  return grid_to_world(path_cell);
}

void Pathfinding::mark_navigation_grid_dirty() {
  std::lock_guard<std::mutex> const lock(m_dirty_mutex);
  m_full_update_required = true;
  m_navigation_grid_dirty.store(true, std::memory_order_release);
}

void Pathfinding::mark_region_dirty(int min_x, int max_x, int min_z, int max_z) {

  min_x = std::max(0, min_x);
  max_x = std::min(m_width - 1, max_x);
  min_z = std::max(0, min_z);
  max_z = std::min(m_height - 1, max_z);

  if (min_x > max_x || min_z > max_z) {
    return;
  }

  std::lock_guard<std::mutex> const lock(m_dirty_mutex);
  m_dirty_regions.emplace_back(min_x, max_x, min_z, max_z);
  m_navigation_grid_dirty.store(true, std::memory_order_release);
}

void Pathfinding::mark_building_region_dirty(float center_x,
                                             float center_z,
                                             float width,
                                             float depth) {
  float const padding = BuildingCollisionRegistry::get_grid_padding();
  float const half_width = width / 2.0F + padding;
  float const half_depth = depth / 2.0F + padding;

  int const min_x =
      static_cast<int>(std::floor(center_x - half_width - m_grid_offset_x));
  int const max_x =
      static_cast<int>(std::ceil(center_x + half_width - m_grid_offset_x));
  int const min_z =
      static_cast<int>(std::floor(center_z - half_depth - m_grid_offset_z));
  int const max_z =
      static_cast<int>(std::ceil(center_z + half_depth - m_grid_offset_z));

  mark_region_dirty(min_x, max_x, min_z, max_z);
}

void Pathfinding::process_dirty_regions() {
  std::vector<DirtyRegion> regions_to_process;

  {
    std::lock_guard<std::mutex> const lock(m_dirty_mutex);
    if (m_full_update_required) {

      m_dirty_regions.clear();
      m_full_update_required = false;

      m_navigation_grid.fill(CellValue::Walkable);

      auto& terrain_service = Game::Map::TerrainService::instance();
      if (terrain_service.is_initialized()) {
        const Game::Map::TerrainHeightMap* height_map =
            terrain_service.get_height_map();

        for (int z = 0; z < m_height; ++z) {
          for (int x = 0; x < m_width; ++x) {
            m_navigation_grid.set(
                x, z, terrain_cell_value(terrain_service, height_map, x, z));
          }
        }
      }

      auto& registry = BuildingCollisionRegistry::instance();
      const auto& buildings = registry.get_all_buildings();

      for (const auto& building : buildings) {
        auto cells = Game::Systems::BuildingCollisionRegistry::get_occupied_grid_cells(
            building, m_grid_cell_size);
        for (const auto& cell : cells) {
          int const grid_x = static_cast<int>(std::round(cell.first - m_grid_offset_x));
          int const grid_z =
              static_cast<int>(std::round(cell.second - m_grid_offset_z));

          if (grid_x >= 0 && grid_x < m_width && grid_z >= 0 && grid_z < m_height) {
            m_navigation_grid.set(grid_x, grid_z, CellValue::Blocked);
          }
        }
      }

      apply_resource_prop_cells(0, m_width - 1, 0, m_height - 1);
      force_map_passage_cells_walkable(0, m_width - 1, 0, m_height - 1);

      return;
    }

    regions_to_process = std::move(m_dirty_regions);
    m_dirty_regions.clear();
  }

  if (regions_to_process.empty()) {
    return;
  }

  for (const auto& region : regions_to_process) {
    update_region(region.min_x, region.max_x, region.min_z, region.max_z);
  }
}

void Pathfinding::update_region(int min_x, int max_x, int min_z, int max_z) {
  auto& terrain_service = Game::Map::TerrainService::instance();
  const Game::Map::TerrainHeightMap* height_map = nullptr;

  if (terrain_service.is_initialized()) {
    height_map = terrain_service.get_height_map();
  }

  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      CellValue const value =
          terrain_service.is_initialized()
              ? terrain_cell_value(terrain_service, height_map, x, z)
              : CellValue::Walkable;
      m_navigation_grid.set(x, z, value);
    }
  }

  auto& registry = BuildingCollisionRegistry::instance();
  const auto& buildings = registry.get_all_buildings();

  for (const auto& building : buildings) {
    auto cells = Game::Systems::BuildingCollisionRegistry::get_occupied_grid_cells(
        building, m_grid_cell_size);
    for (const auto& cell : cells) {
      int const grid_x = static_cast<int>(std::round(cell.first - m_grid_offset_x));
      int const grid_z = static_cast<int>(std::round(cell.second - m_grid_offset_z));

      if (grid_x >= min_x && grid_x <= max_x && grid_z >= min_z && grid_z <= max_z &&
          grid_x >= 0 && grid_x < m_width && grid_z >= 0 && grid_z < m_height) {
        m_navigation_grid.set(grid_x, grid_z, CellValue::Blocked);
      }
    }
  }

  apply_resource_prop_cells(min_x, max_x, min_z, max_z);
  force_map_passage_cells_walkable(min_x, max_x, min_z, max_z);
}

void Pathfinding::force_map_passage_cells_walkable(int min_x,
                                                   int max_x,
                                                   int min_z,
                                                   int max_z) {
  auto& terrain_service = Game::Map::TerrainService::instance();
  if (!terrain_service.is_initialized()) {
    return;
  }

  const auto* height_map = terrain_service.get_height_map();
  if (height_map == nullptr) {
    return;
  }

  min_x = std::max(0, min_x);
  max_x = std::min(m_width - 1, max_x);
  min_z = std::max(0, min_z);
  max_z = std::min(m_height - 1, max_z);
  if (min_x > max_x || min_z > max_z) {
    return;
  }

  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      if (height_map->isBridgeCell(x, z) || height_map->isBridgeCenterline(x, z) ||
          height_map->isHillEntrance(x, z)) {
        m_navigation_grid.set(x, z, CellValue::Walkable);
      }
    }
  }
}

void Pathfinding::apply_resource_prop_cells(int min_x,
                                            int max_x,
                                            int min_z,
                                            int max_z) {
  auto& terrain_service = Game::Map::TerrainService::instance();
  const auto* height_map = terrain_service.get_height_map();
  float const tile_size =
      (height_map != nullptr) ? std::max(height_map->get_tile_size(), 0.0001F) : 1.0F;
  const auto& world_props = terrain_service.world_props();
  for (const auto& prop : world_props) {
    CellValue resource_cell = CellValue::Blocked;
    if (Game::Map::is_tree_world_prop_type(prop.type)) {
      resource_cell = CellValue::Tree;
    } else if (Game::Map::is_boulder_world_prop_type(prop.type)) {
      resource_cell = CellValue::Boulder;
    } else if (Game::Map::is_iron_ore_world_prop_type(prop.type)) {
      resource_cell = CellValue::IronOre;
    } else {
      continue;
    }
    float grid_x_f = prop.x;
    float grid_z_f = prop.z;
    if (terrain_service.coord_system() == Game::Map::CoordSystem::World) {
      grid_x_f = prop.x / tile_size - m_grid_offset_x;
      grid_z_f = prop.z / tile_size - m_grid_offset_z;
    }
    int const grid_x = static_cast<int>(std::round(grid_x_f));
    int const grid_z = static_cast<int>(std::round(grid_z_f));
    if (grid_x < min_x || grid_x > max_x || grid_z < min_z || grid_z > max_z ||
        grid_x < 0 || grid_x >= m_width || grid_z < 0 || grid_z >= m_height) {
      continue;
    }
    m_navigation_grid.set(grid_x, grid_z, resource_cell);
  }
}

void Pathfinding::update_navigation_grid() {
  auto& terrain_service = Game::Map::TerrainService::instance();
  std::uint64_t const world_props_revision =
      terrain_service.is_initialized() ? terrain_service.world_props_revision() : 0;
  if (world_props_revision !=
      m_applied_world_props_revision.load(std::memory_order_acquire)) {
    std::lock_guard<std::mutex> const dirty_lock(m_dirty_mutex);
    if (m_dirty_regions.empty() && !m_full_update_required) {
      m_full_update_required = true;
    }
    m_navigation_grid_dirty.store(true, std::memory_order_release);
  }

  if (!m_navigation_grid_dirty.load(std::memory_order_acquire)) {
    return;
  }

  std::lock_guard<std::mutex> const lock(m_mutex);

  if (!m_navigation_grid_dirty.load(std::memory_order_acquire)) {
    return;
  }

  process_dirty_regions();
  m_applied_world_props_revision.store(world_props_revision, std::memory_order_release);

  m_navigation_grid_dirty.store(false, std::memory_order_release);
}

auto Pathfinding::find_path(const Point& start,
                            const Point& end) -> std::vector<Point> {

  if (m_navigation_grid_dirty.load(std::memory_order_acquire)) {
    update_navigation_grid();
  }

  std::lock_guard<std::mutex> const lock(m_mutex);
  return find_path_internal(start, end);
}

auto Pathfinding::find_path_internal(const Point& start,
                                     const Point& end) -> std::vector<Point> {
  ensure_working_buffers();

  auto const is_walkableFunc = [this](int x, int y) -> bool {
    return is_walkable(x, y);
  };

  if (!is_walkableFunc(start.x, start.y) || !is_walkableFunc(end.x, end.y)) {
    Point resolved_start = start;
    Point resolved_end = end;
    if ((!is_walkableFunc(start.x, start.y) &&
         !resolve_walkable_endpoint(start, resolved_start)) ||
        (!is_walkableFunc(end.x, end.y) &&
         !resolve_walkable_endpoint(end, resolved_end))) {
      return {};
    }

    if (resolved_start == start && resolved_end == end) {
      return {};
    }

    return find_path_internal(resolved_start, resolved_end);
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

  const int max_iterations = std::max(m_width * m_height, 1);
  int iterations = 0;

  int final_cost = -1;
  int best_reachable_idx = start_idx;
  int best_reachable_h = calculate_heuristic(start, end);
  int best_reachable_g = 0;

  while (!m_open_heap.empty() && iterations < max_iterations) {
    ++iterations;

    QueueNode const current = pop_open_node();

    if (current.g_cost > get_g_cost(current.index, generation)) {
      continue;
    }

    if (is_closed(current.index, generation)) {
      continue;
    }

    set_closed(current.index, generation);

    Point const current_point = to_point(current.index);
    int const current_h = calculate_heuristic(current_point, end);
    if (current_h < best_reachable_h ||
        (current_h == best_reachable_h && current.g_cost < best_reachable_g)) {
      best_reachable_idx = current.index;
      best_reachable_h = current_h;
      best_reachable_g = current.g_cost;
    }

    if (current.index == end_idx) {
      final_cost = current.g_cost;
      break;
    }

    std::array<Point, 8> neighbors{};
    const std::size_t neighbor_count = collect_neighbors(current_point, neighbors);

    for (std::size_t i = 0; i < neighbor_count; ++i) {
      const Point& neighbor = neighbors[i];
      if (!is_walkableFunc(neighbor.x, neighbor.y)) {
        continue;
      }

      const int neighbor_idx = to_index(neighbor);
      if (is_closed(neighbor_idx, generation)) {
        continue;
      }

      const int tentative_gcost = current.g_cost + 1;
      if (tentative_gcost >= get_g_cost(neighbor_idx, generation)) {
        continue;
      }

      set_g_cost(neighbor_idx, generation, tentative_gcost);
      set_parent(neighbor_idx, generation, current.index);

      const int h_cost = calculate_heuristic(neighbor, end);
      push_open_node({neighbor_idx, tentative_gcost + h_cost, tentative_gcost});
    }
  }

  if (final_cost < 0) {
    if (best_reachable_idx == start_idx) {
      return {start};
    }
    std::vector<Point> partial_path;
    partial_path.reserve(static_cast<std::size_t>(best_reachable_g + 1));
    build_path(
        start_idx, best_reachable_idx, generation, best_reachable_g + 1, partial_path);
    return partial_path;
  }

  std::vector<Point> path;
  path.reserve(final_cost + 1);
  build_path(start_idx, end_idx, generation, final_cost + 1, path);
  return path;
}

auto Pathfinding::resolve_walkable_endpoint(const Point& requested,
                                            Point& resolved) const -> bool {
  auto const is_walkable_func = [this](int x, int y) -> bool {
    return is_walkable(x, y);
  };

  if (is_walkable_func(requested.x, requested.y)) {
    resolved = requested;
    return true;
  }

  Point const clamped_origin{
      std::clamp(requested.x, 0, std::max(m_width - 1, 0)),
      std::clamp(requested.y, 0, std::max(m_height - 1, 0)),
  };

  if (is_walkable_func(clamped_origin.x, clamped_origin.y)) {
    resolved = clamped_origin;
    return true;
  }

  int const max_search_radius =
      std::max({clamped_origin.x,
                std::max(0, m_width - 1 - clamped_origin.x),
                clamped_origin.y,
                std::max(0, m_height - 1 - clamped_origin.y)});

  for (int radius = 1; radius <= max_search_radius; ++radius) {
    bool found_candidate = false;
    int best_distance_sq = std::numeric_limits<int>::max();
    Point best_candidate{};

    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (std::abs(dx) != radius && std::abs(dy) != radius) {
          continue;
        }

        int const check_x = clamped_origin.x + dx;
        int const check_y = clamped_origin.y + dy;
        if (!is_walkable_func(check_x, check_y)) {
          continue;
        }

        int const requested_dx = check_x - requested.x;
        int const requested_dy = check_y - requested.y;
        int const distance_sq =
            requested_dx * requested_dx + requested_dy * requested_dy;
        if (distance_sq < best_distance_sq) {
          best_distance_sq = distance_sq;
          best_candidate = {check_x, check_y};
          found_candidate = true;
        }
      }
    }

    if (found_candidate) {
      resolved = best_candidate;
      return true;
    }
  }

  return false;
}

auto Pathfinding::calculate_heuristic(const Point& a, const Point& b) -> int {
  return std::abs(a.x - b.x) + std::abs(a.y - b.y);
}

void Pathfinding::ensure_working_buffers() {
  const std::size_t total_cells =
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);

  if (m_closed_generation.size() != total_cells) {
    m_closed_generation.assign(total_cells, 0);
    m_g_cost_generation.assign(total_cells, 0);
    m_g_cost_values.assign(total_cells, std::numeric_limits<int>::max());
    m_parent_generation.assign(total_cells, 0);
    m_parent_values.assign(total_cells, -1);
  }

  const std::size_t min_open_capacity = std::max<std::size_t>(total_cells / 8, 64);
  if (m_open_heap.capacity() < min_open_capacity) {
    m_open_heap.reserve(min_open_capacity);
  }
}

auto Pathfinding::next_generation() -> std::uint32_t {
  auto next = ++m_generation_counter;
  if (next == 0) {
    reset_generations();
    next = ++m_generation_counter;
  }
  return next;
}

void Pathfinding::reset_generations() {
  std::fill(m_closed_generation.begin(), m_closed_generation.end(), 0);
  std::fill(m_g_cost_generation.begin(), m_g_cost_generation.end(), 0);
  std::fill(m_parent_generation.begin(), m_parent_generation.end(), 0);
  std::fill(
      m_g_cost_values.begin(), m_g_cost_values.end(), std::numeric_limits<int>::max());
  std::fill(m_parent_values.begin(), m_parent_values.end(), -1);
  m_generation_counter = 0;
}

auto Pathfinding::is_closed(int index, std::uint32_t generation) const -> bool {
  return index >= 0 && static_cast<std::size_t>(index) < m_closed_generation.size() &&
         m_closed_generation[static_cast<std::size_t>(index)] == generation;
}

void Pathfinding::set_closed(int index, std::uint32_t generation) {
  if (index >= 0 && static_cast<std::size_t>(index) < m_closed_generation.size()) {
    m_closed_generation[static_cast<std::size_t>(index)] = generation;
  }
}

auto Pathfinding::get_g_cost(int index, std::uint32_t generation) const -> int {
  if (index < 0 || static_cast<std::size_t>(index) >= m_g_cost_generation.size()) {
    return std::numeric_limits<int>::max();
  }
  if (m_g_cost_generation[static_cast<std::size_t>(index)] == generation) {
    return m_g_cost_values[static_cast<std::size_t>(index)];
  }
  return std::numeric_limits<int>::max();
}

void Pathfinding::set_g_cost(int index, std::uint32_t generation, int cost) {
  if (index >= 0 && static_cast<std::size_t>(index) < m_g_cost_generation.size()) {
    const auto idx = static_cast<std::size_t>(index);
    m_g_cost_generation[idx] = generation;
    m_g_cost_values[idx] = cost;
  }
}

auto Pathfinding::has_parent(int index, std::uint32_t generation) const -> bool {
  return index >= 0 && static_cast<std::size_t>(index) < m_parent_generation.size() &&
         m_parent_generation[static_cast<std::size_t>(index)] == generation;
}

auto Pathfinding::get_parent(int index, std::uint32_t generation) const -> int {
  if (has_parent(index, generation)) {
    return m_parent_values[static_cast<std::size_t>(index)];
  }
  return -1;
}

void Pathfinding::set_parent(int index, std::uint32_t generation, int parent_index) {
  if (index >= 0 && static_cast<std::size_t>(index) < m_parent_generation.size()) {
    const auto idx = static_cast<std::size_t>(index);
    m_parent_generation[idx] = generation;
    m_parent_values[idx] = parent_index;
  }
}

auto Pathfinding::collect_neighbors(const Point& point,
                                    std::array<Point, 8>& buffer) const -> std::size_t {
  std::size_t count = 0;
  for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
      if (dx == 0 && dy == 0) {
        continue;
      }

      const int x = point.x + dx;
      const int y = point.y + dy;

      if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
        continue;
      }

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

void Pathfinding::build_path(int start_index,
                             int end_index,
                             std::uint32_t generation,
                             int expected_length,
                             std::vector<Point>& out_path) const {
  out_path.clear();
  if (expected_length > 0) {
    out_path.reserve(static_cast<std::size_t>(expected_length));
  }
  int current = end_index;

  while (current >= 0) {
    out_path.push_back(to_point(current));
    if (current == start_index) {
      std::reverse(out_path.begin(), out_path.end());
      return;
    }

    if (!has_parent(current, generation)) {
      out_path.clear();
      return;
    }

    const int parent = get_parent(current, generation);
    if (parent == current || parent < 0) {
      out_path.clear();
      return;
    }
    current = parent;
  }

  out_path.clear();
}

auto Pathfinding::heap_less(const QueueNode& lhs, const QueueNode& rhs) -> bool {
  if (lhs.f_cost != rhs.f_cost) {
    return lhs.f_cost < rhs.f_cost;
  }
  return lhs.g_cost < rhs.g_cost;
}

void Pathfinding::push_open_node(const QueueNode& node) {
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

auto Pathfinding::pop_open_node() -> Pathfinding::QueueNode {
  QueueNode top = m_open_heap.front();
  QueueNode const last = m_open_heap.back();
  m_open_heap.pop_back();
  if (!m_open_heap.empty()) {
    m_open_heap[0] = last;
    std::size_t index = 0;
    const std::size_t size = m_open_heap.size();
    while (true) {
      std::size_t const left = index * 2 + 1;
      std::size_t const right = left + 1;
      std::size_t smallest = index;

      if (left < size && !heap_less(m_open_heap[smallest], m_open_heap[left])) {
        smallest = left;
      }
      if (right < size && !heap_less(m_open_heap[smallest], m_open_heap[right])) {
        smallest = right;
      }
      if (smallest == index) {
        break;
      }
      std::swap(m_open_heap[index], m_open_heap[smallest]);
      index = smallest;
    }
  }
  return top;
}

auto Pathfinding::find_nearest_walkable_point(const Point& point,
                                              int max_search_radius,
                                              const Pathfinding& pathfinder) -> Point {
  auto const is_walkableFunc = [&pathfinder](int x, int y) -> bool {
    return pathfinder.is_walkable(x, y);
  };

  if (is_walkableFunc(point.x, point.y)) {
    return point;
  }

  for (int radius = 1; radius <= max_search_radius; ++radius) {
    for (int dy = -radius; dy <= radius; ++dy) {
      for (int dx = -radius; dx <= radius; ++dx) {
        if (std::abs(dx) != radius && std::abs(dy) != radius) {
          continue;
        }

        int const check_x = point.x + dx;
        int const check_y = point.y + dy;

        if (is_walkableFunc(check_x, check_y)) {
          return {check_x, check_y};
        }
      }
    }
  }

  return point;
}

} // namespace Game::Systems
