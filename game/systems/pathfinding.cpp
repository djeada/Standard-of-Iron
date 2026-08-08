#include "pathfinding.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
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

  if (Game::Map::is_water_terrain(terrain_type)) {
    return height_map->isBridgeCenterline(x, z) ? Pathfinding::CellValue::Walkable
                                                : Pathfinding::CellValue::Blocked;
  }

  return terrain_service.is_walkable(x, z) ? Pathfinding::CellValue::Walkable
                                           : Pathfinding::CellValue::Blocked;
}

void merge_dirty_regions(std::vector<DirtyRegion>& regions) {
  constexpr std::size_t k_max_individual_regions = 256U;
  if (regions.size() > k_max_individual_regions) {
    DirtyRegion merged = regions.front();
    for (auto const& region : regions) {
      merged.min_x = std::min(merged.min_x, region.min_x);
      merged.max_x = std::max(merged.max_x, region.max_x);
      merged.min_z = std::min(merged.min_z, region.min_z);
      merged.max_z = std::max(merged.max_z, region.max_z);
    }
    regions.assign(1U, merged);
    return;
  }
  for (std::size_t i = 0; i < regions.size(); ++i) {
    for (std::size_t j = i + 1; j < regions.size();) {
      bool const overlaps_x = regions[i].min_x <= regions[j].max_x + 1 &&
                              regions[j].min_x <= regions[i].max_x + 1;
      bool const overlaps_z = regions[i].min_z <= regions[j].max_z + 1 &&
                              regions[j].min_z <= regions[i].max_z + 1;
      if (!overlaps_x || !overlaps_z) {
        ++j;
        continue;
      }
      regions[i].min_x = std::min(regions[i].min_x, regions[j].min_x);
      regions[i].max_x = std::max(regions[i].max_x, regions[j].max_x);
      regions[i].min_z = std::min(regions[i].min_z, regions[j].min_z);
      regions[i].max_z = std::max(regions[i].max_z, regions[j].max_z);
      regions[j] = regions.back();
      regions.pop_back();
      j = i + 1;
    }
  }
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

auto Pathfinding::cells_covering(float center_x,
                                 float center_z,
                                 float half_x,
                                 float half_z) const -> CellRange {

  constexpr float k_area_epsilon = 1.0e-4F;
  half_x = std::max(0.0F, half_x - k_area_epsilon);
  half_z = std::max(0.0F, half_z - k_area_epsilon);

  Point const low = world_to_grid(center_x - half_x, center_z - half_z);
  Point const high = world_to_grid(center_x + half_x, center_z + half_z);
  return {.min_x = std::min(low.x, high.x),
          .max_x = std::max(low.x, high.x),
          .min_z = std::min(low.y, high.y),
          .max_z = std::max(low.y, high.y)};
}

auto Pathfinding::cell_range_world_bounds(const CellRange& range) const -> WorldRect {
  QVector3D const low = grid_to_world({range.min_x, range.min_z});
  QVector3D const high = grid_to_world({range.max_x, range.max_z});
  float const half_cell = m_grid_cell_size * 0.5F;
  return {.min_x = low.x() - half_cell,
          .max_x = high.x() + half_cell,
          .min_z = low.z() - half_cell,
          .max_z = high.z() + half_cell};
}

void Pathfinding::set_obstacle(int x, int y, bool is_obstacle) {
  std::unique_lock<std::shared_mutex> const lock(m_navigation_mutex);
  m_navigation_grid.set(x, y, is_obstacle ? CellValue::Blocked : CellValue::Walkable);

  rebuild_clearance(x - 1, x + 1, y - 1, y + 1);
  m_navigation_revision.fetch_add(1, std::memory_order_release);
}

auto Pathfinding::is_walkable(int x, int y, Passability passability) const -> bool {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
    return false;
  }
  auto const value = cell_value(x, y);
  if (value == CellValue::Walkable) {
    return true;
  }
  return value == CellValue::Forest && passability == Passability::Light;
}

auto Pathfinding::is_forest(int x, int y) const -> bool {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
    return false;
  }
  return cell_value(x, y) == CellValue::Forest;
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

auto Pathfinding::is_terrain_walkable(int x, int y) const -> bool {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
    return false;
  }
  auto& terrain_service = Game::Map::TerrainService::instance();
  if (!terrain_service.is_initialized()) {
    return true;
  }
  return terrain_cell_value(terrain_service, terrain_service.get_height_map(), x, y) ==
         CellValue::Walkable;
}

auto Pathfinding::is_world_position_walkable(const QVector3D& world_position,
                                             Passability passability) const -> bool {
  Point const grid = world_to_grid(world_position.x(), world_position.z());
  return is_walkable(grid.x, grid.y, passability);
}

auto Pathfinding::is_world_segment_walkable(const QVector3D& from,
                                            const QVector3D& to,
                                            Passability passability) const -> bool {

  constexpr float k_boundary_epsilon = 1.0e-5F;

  if (!is_world_position_walkable(to, passability)) {
    return false;
  }

  float const start_u = from.x() - m_grid_offset_x + 0.5F;
  float const start_v = from.z() - m_grid_offset_z + 0.5F;
  float const end_u = to.x() - m_grid_offset_x + 0.5F;
  float const end_v = to.z() - m_grid_offset_z + 0.5F;

  int cell_x = static_cast<int>(std::floor(start_u));
  int cell_z = static_cast<int>(std::floor(start_v));
  int const end_x = static_cast<int>(std::floor(end_u));
  int const end_z = static_cast<int>(std::floor(end_v));

  float const delta_u = end_u - start_u;
  float const delta_v = end_v - start_v;

  int const step_x = delta_u > 0.0F ? 1 : (delta_u < 0.0F ? -1 : 0);
  int const step_z = delta_v > 0.0F ? 1 : (delta_v < 0.0F ? -1 : 0);

  auto next_boundary = [](float start, float delta, int step, int cell) -> float {
    if (step == 0) {
      return std::numeric_limits<float>::infinity();
    }
    float const boundary =
        step > 0 ? static_cast<float>(cell + 1) : static_cast<float>(cell);
    return (boundary - start) / delta;
  };

  float travel_x = next_boundary(start_u, delta_u, step_x, cell_x);
  float travel_z = next_boundary(start_v, delta_v, step_z, cell_z);
  float const stride_x =
      step_x == 0 ? std::numeric_limits<float>::infinity() : 1.0F / std::abs(delta_u);
  float const stride_z =
      step_z == 0 ? std::numeric_limits<float>::infinity() : 1.0F / std::abs(delta_v);

  int const max_steps = std::abs(end_x - cell_x) + std::abs(end_z - cell_z) + 2;
  for (int taken = 0; taken < max_steps; ++taken) {
    if (cell_x == end_x && cell_z == end_z) {
      return true;
    }

    if (travel_x < travel_z - k_boundary_epsilon) {
      cell_x += step_x;
      travel_x += stride_x;
    } else if (travel_z < travel_x - k_boundary_epsilon) {
      cell_z += step_z;
      travel_z += stride_z;
    } else {

      if (!is_walkable(cell_x + step_x, cell_z, passability) ||
          !is_walkable(cell_x, cell_z + step_z, passability)) {
        return false;
      }
      cell_x += step_x;
      cell_z += step_z;
      travel_x += stride_x;
      travel_z += stride_z;
    }

    if (!is_walkable(cell_x, cell_z, passability)) {
      return false;
    }
  }

  return cell_x == end_x && cell_z == end_z;
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

void Pathfinding::mark_obstruction_released() {
  m_obstruction_revision.fetch_add(1, std::memory_order_acq_rel);
}

auto Pathfinding::obstruction_revision() const -> std::uint64_t {
  return m_obstruction_revision.load(std::memory_order_acquire);
}

void Pathfinding::process_dirty_regions() {
  std::vector<DirtyRegion> regions_to_process;

  {
    std::lock_guard<std::mutex> const lock(m_dirty_mutex);
    if (m_full_update_required) {

      m_dirty_regions.clear();
      m_full_update_required = false;

      m_navigation_grid.fill(CellValue::Walkable);
      update_region(0, m_width - 1, 0, m_height - 1);

      return;
    }

    regions_to_process = std::move(m_dirty_regions);
    m_dirty_regions.clear();
  }

  if (regions_to_process.empty()) {
    return;
  }

  merge_dirty_regions(regions_to_process);

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

  apply_forest_cells(min_x, max_x, min_z, max_z);
  apply_resource_prop_cells(min_x, max_x, min_z, max_z);

  auto& registry = BuildingCollisionRegistry::instance();
  registry.for_each_building_in_region(
      static_cast<float>(min_x) + m_grid_offset_x,
      static_cast<float>(max_x) + m_grid_offset_x,
      static_cast<float>(min_z) + m_grid_offset_z,
      static_cast<float>(max_z) + m_grid_offset_z,
      [this, min_x, max_x, min_z, max_z](BuildingFootprint const& building) {
        apply_building_cells(building, min_x, max_x, min_z, max_z);
      });

  force_navigation_passages_walkable(min_x, max_x, min_z, max_z);
  force_map_passage_cells_walkable(min_x, max_x, min_z, max_z);

  rebuild_clearance(min_x - 1, max_x + 1, min_z - 1, max_z + 1);
}

void Pathfinding::force_navigation_passages_walkable(int min_x,
                                                     int max_x,
                                                     int min_z,
                                                     int max_z) {
  min_x = std::max(0, min_x);
  max_x = std::min(m_width - 1, max_x);
  min_z = std::max(0, min_z);
  max_z = std::min(m_height - 1, max_z);
  if (min_x > max_x || min_z > max_z) {
    return;
  }

  constexpr float k_touch_epsilon = 1.0e-3F;

  auto const& registry = BuildingCollisionRegistry::instance();
  for (const auto& passage : registry.navigation_passages()) {
    auto const range = cells_covering(
        passage.center_x, passage.center_z, passage.width * 0.5F, passage.depth * 0.5F);
    for (int grid_z = std::max(range.min_z, min_z);
         grid_z <= std::min(range.max_z, max_z);
         ++grid_z) {
      for (int grid_x = std::max(range.min_x, min_x);
           grid_x <= std::min(range.max_x, max_x);
           ++grid_x) {
        auto const cell = cell_range_world_bounds(
            {.min_x = grid_x, .max_x = grid_x, .min_z = grid_z, .max_z = grid_z});
        if (registry.is_rect_overlapping_blocking_building(cell.min_x + k_touch_epsilon,
                                                           cell.max_x - k_touch_epsilon,
                                                           cell.min_z + k_touch_epsilon,
                                                           cell.max_z - k_touch_epsilon,
                                                           passage.source_entity_id)) {
          continue;
        }
        m_navigation_grid.set(grid_x, grid_z, CellValue::Walkable);
      }
    }
  }
}

void Pathfinding::apply_building_cells(
    const BuildingFootprint& building, int min_x, int max_x, int min_z, int max_z) {
  if (!building.blocks_navigation) {
    return;
  }
  auto const range = cells_covering(building.center_x,
                                    building.center_z,
                                    (building.width * 0.5F) + building.grid_padding,
                                    (building.depth * 0.5F) + building.grid_padding);
  int const from_x = std::max({range.min_x, min_x, 0});
  int const to_x = std::min({range.max_x, max_x, m_width - 1});
  int const from_z = std::max({range.min_z, min_z, 0});
  int const to_z = std::min({range.max_z, max_z, m_height - 1});
  for (int grid_z = from_z; grid_z <= to_z; ++grid_z) {
    for (int grid_x = from_x; grid_x <= to_x; ++grid_x) {
      m_navigation_grid.set(grid_x, grid_z, CellValue::Blocked);
    }
  }
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

  constexpr float k_touch_epsilon = 1.0e-3F;
  auto const& registry = BuildingCollisionRegistry::instance();

  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      if (m_navigation_grid.get(x, z) == CellValue::Walkable) {
        continue;
      }
      if (!height_map->isBridgeCell(x, z) && !height_map->isBridgeCenterline(x, z) &&
          !height_map->isHillEntrance(x, z)) {
        continue;
      }

      auto const cell =
          cell_range_world_bounds({.min_x = x, .max_x = x, .min_z = z, .max_z = z});
      if (registry.is_rect_overlapping_blocking_building(cell.min_x + k_touch_epsilon,
                                                         cell.max_x - k_touch_epsilon,
                                                         cell.min_z + k_touch_epsilon,
                                                         cell.max_z -
                                                             k_touch_epsilon)) {
        continue;
      }
      m_navigation_grid.set(x, z, CellValue::Walkable);
    }
  }
}

void Pathfinding::apply_resource_prop_cells(int min_x,
                                            int max_x,
                                            int min_z,
                                            int max_z) {
  for (int grid_z = min_z; grid_z <= max_z; ++grid_z) {
    for (int grid_x = min_x; grid_x <= max_x; ++grid_x) {
      auto const prop = m_world_prop_cells.find(to_index(grid_x, grid_z));
      if (prop != m_world_prop_cells.end()) {
        m_navigation_grid.set(grid_x, grid_z, prop->second);
      }
    }
  }
}

void Pathfinding::rebuild_forest_index() {
  m_forest_cells.assign(
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height), false);

  auto& terrain_service = Game::Map::TerrainService::instance();
  if (!terrain_service.is_initialized() || terrain_service.groves().empty()) {
    return;
  }

  const auto* height_map = terrain_service.get_height_map();
  float const tile_size =
      height_map != nullptr ? std::max(height_map->get_tile_size(), 0.0001F) : 1.0F;
  bool const world_space =
      terrain_service.coord_system() == Game::Map::CoordSystem::World;

  for (auto const& grove : terrain_service.groves()) {
    float center_x = grove.x;
    float center_z = grove.z;
    float radius = grove.radius;
    if (world_space) {
      center_x = grove.x / tile_size - m_grid_offset_x;
      center_z = grove.z / tile_size - m_grid_offset_z;
      radius = grove.radius / tile_size;
    }

    int const min_x = std::max(0, static_cast<int>(std::floor(center_x - radius)));
    int const max_x =
        std::min(m_width - 1, static_cast<int>(std::ceil(center_x + radius)));
    int const min_z = std::max(0, static_cast<int>(std::floor(center_z - radius)));
    int const max_z =
        std::min(m_height - 1, static_cast<int>(std::ceil(center_z + radius)));
    float const radius_sq = radius * radius;

    for (int grid_z = min_z; grid_z <= max_z; ++grid_z) {
      for (int grid_x = min_x; grid_x <= max_x; ++grid_x) {
        float const dx = static_cast<float>(grid_x) - center_x;
        float const dz = static_cast<float>(grid_z) - center_z;
        if (dx * dx + dz * dz > radius_sq) {
          continue;
        }

        QVector3D const world = grid_to_world({grid_x, grid_z});
        if (terrain_service.is_point_on_road(world.x(), world.z())) {
          continue;
        }
        m_forest_cells[static_cast<std::size_t>(to_index(grid_x, grid_z))] = true;
      }
    }
  }
}

void Pathfinding::apply_forest_cells(int min_x, int max_x, int min_z, int max_z) {
  if (m_forest_cells.empty()) {
    return;
  }
  for (int grid_z = min_z; grid_z <= max_z; ++grid_z) {
    for (int grid_x = min_x; grid_x <= max_x; ++grid_x) {
      auto const index = static_cast<std::size_t>(to_index(grid_x, grid_z));
      if (index >= m_forest_cells.size() || !m_forest_cells[index]) {
        continue;
      }

      if (m_navigation_grid.get(grid_x, grid_z) == CellValue::Walkable) {
        m_navigation_grid.set(grid_x, grid_z, CellValue::Forest);
      }
    }
  }
}

void Pathfinding::rebuild_world_prop_index() {
  auto& terrain_service = Game::Map::TerrainService::instance();
  const auto* height_map = terrain_service.get_height_map();
  float const tile_size =
      height_map != nullptr ? std::max(height_map->get_tile_size(), 0.0001F) : 1.0F;
  std::unordered_map<int, CellValue> next;
  next.reserve(terrain_service.world_props().size());
  for (auto const& prop : terrain_service.world_props()) {
    CellValue value = CellValue::Blocked;
    if (Game::Map::is_tree_world_prop_type(prop.type)) {
      value = CellValue::Tree;
    } else if (Game::Map::is_boulder_world_prop_type(prop.type)) {
      value = CellValue::Boulder;
    } else if (Game::Map::is_iron_ore_world_prop_type(prop.type)) {
      value = CellValue::IronOre;
    } else if (Game::Map::is_settlement_world_prop_type(prop.type)) {
      value = CellValue::Blocked;
    } else {
      continue;
    }
    float grid_x_value = prop.x;
    float grid_z_value = prop.z;
    if (terrain_service.coord_system() == Game::Map::CoordSystem::World) {
      grid_x_value = prop.x / tile_size - m_grid_offset_x;
      grid_z_value = prop.z / tile_size - m_grid_offset_z;
    }
    int const grid_x = static_cast<int>(std::round(grid_x_value));
    int const grid_z = static_cast<int>(std::round(grid_z_value));
    if (grid_x >= 0 && grid_x < m_width && grid_z >= 0 && grid_z < m_height) {
      next[to_index(grid_x, grid_z)] = value;
    }
  }

  {
    std::lock_guard<std::mutex> const dirty_lock(m_dirty_mutex);
    if (!m_full_update_required) {
      for (auto const& [index, value] : m_world_prop_cells) {
        auto const replacement = next.find(index);
        if (replacement == next.end() || replacement->second != value) {
          Point const point = to_point(index);
          m_dirty_regions.emplace_back(point.x, point.x, point.y, point.y);
        }
      }
      for (auto const& [index, value] : next) {
        auto const previous = m_world_prop_cells.find(index);
        if (previous == m_world_prop_cells.end() || previous->second != value) {
          Point const point = to_point(index);
          m_dirty_regions.emplace_back(point.x, point.x, point.y, point.y);
        }
      }
    }
  }
  m_world_prop_cells = std::move(next);
}

void Pathfinding::update_navigation_grid() {
  auto& terrain_service = Game::Map::TerrainService::instance();
  std::uint64_t const terrain_topology_revision =
      terrain_service.navigation_topology_revision();
  std::uint64_t const world_props_revision =
      terrain_service.is_initialized() ? terrain_service.world_props_revision() : 0;
  if (terrain_topology_revision !=
          m_applied_terrain_topology_revision.load(std::memory_order_acquire) ||
      world_props_revision !=
          m_applied_world_props_revision.load(std::memory_order_acquire)) {
    m_navigation_grid_dirty.store(true, std::memory_order_release);
  }

  if (!m_navigation_grid_dirty.load(std::memory_order_acquire)) {
    return;
  }

  std::unique_lock<std::shared_mutex> const lock(m_navigation_mutex);

  if (!m_navigation_grid_dirty.load(std::memory_order_acquire)) {
    return;
  }

  if (terrain_topology_revision !=
      m_applied_terrain_topology_revision.load(std::memory_order_acquire)) {
    rebuild_forest_index();
    std::lock_guard<std::mutex> const dirty_lock(m_dirty_mutex);
    m_full_update_required = true;
  }
  if (world_props_revision !=
      m_applied_world_props_revision.load(std::memory_order_acquire)) {
    rebuild_world_prop_index();
  }
  process_dirty_regions();
  m_applied_terrain_topology_revision.store(terrain_topology_revision,
                                            std::memory_order_release);
  m_applied_world_props_revision.store(world_props_revision, std::memory_order_release);
  m_navigation_revision.fetch_add(1, std::memory_order_release);

  m_navigation_grid_dirty.store(false, std::memory_order_release);
}

auto Pathfinding::find_path(const Point& start,
                            const Point& end,
                            Passability passability) -> std::vector<Point> {

  if (m_navigation_grid_dirty.load(std::memory_order_acquire)) {
    update_navigation_grid();
  }

  std::uint64_t const revision = navigation_revision();
  PathCacheKey const key{start.x, start.y, end.x, end.y, passability};
  {
    std::lock_guard<std::mutex> const cache_lock(m_path_cache_mutex);
    if (m_path_cache_revision != revision) {
      m_path_cache.clear();
      m_path_cache_revision = revision;
    }
    if (auto const cached = m_path_cache.find(key); cached != m_path_cache.end()) {
      return cached->second;
    }
  }

  std::shared_lock<std::shared_mutex> const navigation_lock(m_navigation_mutex);
  auto path = find_path_internal(start, end, passability);
  std::lock_guard<std::mutex> const cache_lock(m_path_cache_mutex);
  if (m_path_cache_revision != revision || navigation_revision() != revision) {
    return path;
  }
  constexpr std::size_t k_max_cached_paths = 256U;
  if (m_path_cache.size() >= k_max_cached_paths) {
    m_path_cache.clear();
  }
  m_path_cache.emplace(key, path);
  return path;
}

auto Pathfinding::PathCacheKeyHash::operator()(const PathCacheKey& key) const noexcept
    -> std::size_t {
  std::size_t result = std::hash<int>{}(key.start_x);
  auto combine = [&result](int value) {
    result ^= std::hash<int>{}(value) + 0x9e3779b9U + (result << 6U) + (result >> 2U);
  };
  combine(key.start_y);
  combine(key.end_x);
  combine(key.end_y);
  combine(static_cast<int>(key.passability));
  return result;
}

auto Pathfinding::find_path_internal(const Point& start,
                                     const Point& end,
                                     Passability passability) -> std::vector<Point> {
  SearchBuffers& buffers = search_buffers_for(this);
  ensure_working_buffers(buffers);

  auto const is_walkableFunc = [this, passability](int x, int y) -> bool {
    return is_walkable(x, y, passability);
  };

  if (!is_walkableFunc(start.x, start.y) || !is_walkableFunc(end.x, end.y)) {
    Point resolved_start = start;
    Point resolved_end = end;
    if ((!is_walkableFunc(start.x, start.y) &&
         !resolve_walkable_endpoint(start, resolved_start, passability)) ||
        (!is_walkableFunc(end.x, end.y) &&
         !resolve_walkable_endpoint(end, resolved_end, passability))) {
      return {};
    }

    if (resolved_start == start && resolved_end == end) {
      return {};
    }

    return find_path_internal(resolved_start, resolved_end, passability);
  }

  const int start_idx = to_index(start);
  const int end_idx = to_index(end);

  if (start_idx == end_idx) {
    return {start};
  }

  const std::uint32_t generation = next_generation(buffers);

  buffers.open_heap.clear();

  set_g_cost(buffers, start_idx, generation, 0);
  set_parent(buffers, start_idx, generation, start_idx);

  push_open_node(buffers, {start_idx, calculate_heuristic(start, end), 0});

  const int max_iterations = std::max(m_width * m_height, 1);
  int iterations = 0;

  int final_cost = -1;
  int best_reachable_idx = start_idx;
  int best_reachable_h = calculate_heuristic(start, end);
  int best_reachable_g = 0;

  while (!buffers.open_heap.empty() && iterations < max_iterations) {
    QueueNode const current = pop_open_node(buffers);

    if (current.g_cost > get_g_cost(buffers, current.index, generation)) {
      continue;
    }

    if (is_closed(buffers, current.index, generation)) {
      continue;
    }

    ++iterations;
    set_closed(buffers, current.index, generation);

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

    int arrival_x = 0;
    int arrival_z = 0;
    if (current.index != start_idx) {
      Point const came_from = to_point(get_parent(buffers, current.index, generation));
      arrival_x = current_point.x - came_from.x;
      arrival_z = current_point.y - came_from.y;
    }

    std::array<Point, 8> neighbors{};
    const std::size_t neighbor_count =
        collect_neighbors(current_point, neighbors, passability);

    for (std::size_t i = 0; i < neighbor_count; ++i) {
      const Point& neighbor = neighbors[i];
      if (!is_walkableFunc(neighbor.x, neighbor.y)) {
        continue;
      }

      const int neighbor_idx = to_index(neighbor);
      if (is_closed(buffers, neighbor_idx, generation)) {
        continue;
      }

      const int step_x = neighbor.x - current_point.x;
      const int step_z = neighbor.y - current_point.y;
      const bool turns = (arrival_x != 0 || arrival_z != 0) &&
                         (step_x != arrival_x || step_z != arrival_z);
      const int tentative_gcost =
          current.g_cost +
          ((step_x != 0 && step_z != 0) ? k_diagonal_step_cost : k_straight_step_cost) +
          clearance_penalty(neighbor.x, neighbor.y) + (turns ? k_turn_penalty : 0);
      if (tentative_gcost >= get_g_cost(buffers, neighbor_idx, generation)) {
        continue;
      }

      set_g_cost(buffers, neighbor_idx, generation, tentative_gcost);
      set_parent(buffers, neighbor_idx, generation, current.index);

      const int h_cost = calculate_heuristic(neighbor, end);
      push_open_node(buffers,
                     {neighbor_idx, tentative_gcost + h_cost, tentative_gcost});
    }
  }

  if (final_cost < 0) {
    if (best_reachable_idx == start_idx) {
      return {start};
    }
    std::vector<Point> partial_path;
    int const partial_cells = (best_reachable_g / k_straight_step_cost) + 1;
    partial_path.reserve(static_cast<std::size_t>(partial_cells));
    build_path(start_idx,
               best_reachable_idx,
               generation,
               partial_cells,
               buffers,
               partial_path);
    return partial_path;
  }

  std::vector<Point> path;
  int const path_cells = (final_cost / k_straight_step_cost) + 1;
  path.reserve(static_cast<std::size_t>(path_cells));
  build_path(start_idx, end_idx, generation, path_cells, buffers, path);
  return path;
}

auto Pathfinding::resolve_walkable_endpoint(const Point& requested,
                                            Point& resolved,
                                            Passability passability) const -> bool {
  auto const is_walkable_func = [this, passability](int x, int y) -> bool {
    return is_walkable(x, y, passability);
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

  int const dx = std::abs(a.x - b.x);
  int const dy = std::abs(a.y - b.y);
  int const diagonal = std::min(dx, dy);
  int const straight = std::max(dx, dy) - diagonal;
  int const octile =
      (diagonal * k_diagonal_step_cost) + (straight * k_straight_step_cost);
  return octile * k_heuristic_weight_numerator / k_heuristic_weight_denominator;
}

auto Pathfinding::clearance_penalty(int x, int y) const -> int {
  auto const index = static_cast<std::size_t>(to_index(x, y));
  if (index >= m_clearance_penalty.size()) {
    return 0;
  }
  return m_clearance_penalty[index];
}

void Pathfinding::rebuild_clearance(int min_x, int max_x, int min_z, int max_z) {
  auto const total =
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
  if (m_clearance_penalty.size() != total) {
    m_clearance_penalty.assign(total, 0);
  }

  min_x = std::max(0, min_x);
  max_x = std::min(m_width - 1, max_x);
  min_z = std::max(0, min_z);
  max_z = std::min(m_height - 1, max_z);
  if (min_x > max_x || min_z > max_z) {
    return;
  }

  for (int z = min_z; z <= max_z; ++z) {
    auto* row = &m_clearance_penalty[static_cast<std::size_t>(to_index(min_x, z))];
    std::fill(row, row + (max_x - min_x + 1), std::uint8_t{0});
  }

  auto const penalty = static_cast<std::uint8_t>(k_edge_step_penalty);
  for (int z = min_z - 1; z <= max_z + 1; ++z) {
    for (int x = min_x - 1; x <= max_x + 1; ++x) {
      if (is_walkable(x, z)) {
        continue;
      }
      int const from_z = std::max(z - 1, min_z);
      int const to_z = std::min(z + 1, max_z);
      int const from_x = std::max(x - 1, min_x);
      int const to_x = std::min(x + 1, max_x);
      for (int nz = from_z; nz <= to_z; ++nz) {
        for (int nx = from_x; nx <= to_x; ++nx) {
          if (nx == x && nz == z) {
            continue;
          }
          m_clearance_penalty[static_cast<std::size_t>(to_index(nx, nz))] = penalty;
        }
      }
    }
  }
}

auto Pathfinding::search_buffers_for(const Pathfinding* pathfinding) -> SearchBuffers& {
  thread_local std::unordered_map<const Pathfinding*, SearchBuffers> buffers_by_grid;
  return buffers_by_grid[pathfinding];
}

void Pathfinding::ensure_working_buffers(SearchBuffers& buffers) const {
  const std::size_t total_cells =
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);

  if (buffers.closed_generation.size() != total_cells) {
    buffers.closed_generation.assign(total_cells, 0);
    buffers.g_cost_generation.assign(total_cells, 0);
    buffers.g_cost_values.assign(total_cells, std::numeric_limits<int>::max());
    buffers.parent_generation.assign(total_cells, 0);
    buffers.parent_values.assign(total_cells, -1);
    buffers.generation_counter = 0;
  }

  const std::size_t min_open_capacity = std::max<std::size_t>(total_cells / 8, 64);
  if (buffers.open_heap.capacity() < min_open_capacity) {
    buffers.open_heap.reserve(min_open_capacity);
  }
}

auto Pathfinding::next_generation(SearchBuffers& buffers) -> std::uint32_t {
  auto next = ++buffers.generation_counter;
  if (next == 0) {
    reset_generations(buffers);
    next = ++buffers.generation_counter;
  }
  return next;
}

void Pathfinding::reset_generations(SearchBuffers& buffers) {
  std::fill(buffers.closed_generation.begin(), buffers.closed_generation.end(), 0);
  std::fill(buffers.g_cost_generation.begin(), buffers.g_cost_generation.end(), 0);
  std::fill(buffers.parent_generation.begin(), buffers.parent_generation.end(), 0);
  std::fill(buffers.g_cost_values.begin(),
            buffers.g_cost_values.end(),
            std::numeric_limits<int>::max());
  std::fill(buffers.parent_values.begin(), buffers.parent_values.end(), -1);
  buffers.generation_counter = 0;
}

auto Pathfinding::is_closed(const SearchBuffers& buffers,
                            int index,
                            std::uint32_t generation) -> bool {
  return index >= 0 &&
         static_cast<std::size_t>(index) < buffers.closed_generation.size() &&
         buffers.closed_generation[static_cast<std::size_t>(index)] == generation;
}

void Pathfinding::set_closed(SearchBuffers& buffers,
                             int index,
                             std::uint32_t generation) {
  if (index >= 0 &&
      static_cast<std::size_t>(index) < buffers.closed_generation.size()) {
    buffers.closed_generation[static_cast<std::size_t>(index)] = generation;
  }
}

auto Pathfinding::get_g_cost(const SearchBuffers& buffers,
                             int index,
                             std::uint32_t generation) -> int {
  if (index < 0 ||
      static_cast<std::size_t>(index) >= buffers.g_cost_generation.size()) {
    return std::numeric_limits<int>::max();
  }
  if (buffers.g_cost_generation[static_cast<std::size_t>(index)] == generation) {
    return buffers.g_cost_values[static_cast<std::size_t>(index)];
  }
  return std::numeric_limits<int>::max();
}

void Pathfinding::set_g_cost(SearchBuffers& buffers,
                             int index,
                             std::uint32_t generation,
                             int cost) {
  if (index >= 0 &&
      static_cast<std::size_t>(index) < buffers.g_cost_generation.size()) {
    const auto idx = static_cast<std::size_t>(index);
    buffers.g_cost_generation[idx] = generation;
    buffers.g_cost_values[idx] = cost;
  }
}

auto Pathfinding::has_parent(const SearchBuffers& buffers,
                             int index,
                             std::uint32_t generation) -> bool {
  return index >= 0 &&
         static_cast<std::size_t>(index) < buffers.parent_generation.size() &&
         buffers.parent_generation[static_cast<std::size_t>(index)] == generation;
}

auto Pathfinding::get_parent(const SearchBuffers& buffers,
                             int index,
                             std::uint32_t generation) -> int {
  if (has_parent(buffers, index, generation)) {
    return buffers.parent_values[static_cast<std::size_t>(index)];
  }
  return -1;
}

void Pathfinding::set_parent(SearchBuffers& buffers,
                             int index,
                             std::uint32_t generation,
                             int parent_index) {
  if (index >= 0 &&
      static_cast<std::size_t>(index) < buffers.parent_generation.size()) {
    const auto idx = static_cast<std::size_t>(index);
    buffers.parent_generation[idx] = generation;
    buffers.parent_values[idx] = parent_index;
  }
}

auto Pathfinding::collect_neighbors(const Point& point,
                                    std::array<Point, 8>& buffer,
                                    Passability passability) const -> std::size_t {
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
        if (!is_walkable(point.x + dx, point.y, passability) ||
            !is_walkable(point.x, point.y + dy, passability)) {
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
                             const SearchBuffers& buffers,
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

    if (!has_parent(buffers, current, generation)) {
      out_path.clear();
      return;
    }

    const int parent = get_parent(buffers, current, generation);
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

void Pathfinding::push_open_node(SearchBuffers& buffers, const QueueNode& node) {
  auto& heap = buffers.open_heap;
  heap.push_back(node);
  std::size_t index = heap.size() - 1;
  while (index > 0) {
    std::size_t const parent = (index - 1) / 2;
    if (heap_less(heap[parent], heap[index])) {
      break;
    }
    std::swap(heap[parent], heap[index]);
    index = parent;
  }
}

auto Pathfinding::pop_open_node(SearchBuffers& buffers) -> Pathfinding::QueueNode {
  auto& heap = buffers.open_heap;
  QueueNode top = heap.front();
  QueueNode const last = heap.back();
  heap.pop_back();
  if (!heap.empty()) {
    heap[0] = last;
    std::size_t index = 0;
    const std::size_t size = heap.size();
    while (true) {
      std::size_t const left = index * 2 + 1;
      std::size_t const right = left + 1;
      std::size_t smallest = index;

      if (left < size && !heap_less(heap[smallest], heap[left])) {
        smallest = left;
      }
      if (right < size && !heap_less(heap[smallest], heap[right])) {
        smallest = right;
      }
      if (smallest == index) {
        break;
      }
      std::swap(heap[index], heap[smallest]);
      index = smallest;
    }
  }
  return top;
}

auto Pathfinding::find_nearest_walkable_point(const Point& point,
                                              int max_search_radius,
                                              const Pathfinding& pathfinder,
                                              Passability passability) -> Point {
  auto const is_walkableFunc = [&pathfinder, passability](int x, int y) -> bool {
    return pathfinder.is_walkable(x, y, passability);
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
