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
#include "game/core/nav_profile.h"
#include "gate_service.h"
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

Pathfinding::NavigationGrid::NavigationGrid(int width, int height)
    : m_width(std::max(width, 0))
    , m_height(std::max(height, 0))
    , m_cells(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height),
              static_cast<std::uint8_t>(CellValue::Walkable)) {
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
  return at_unchecked(x, y);
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
    , m_navigation_grid(width, height)
    , m_terrain(&Game::Map::TerrainService::instance()) {
  m_navigation_grid_dirty.store(true, std::memory_order_release);
}

Pathfinding::~Pathfinding() {
  release_search_buffers(this);
}

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

  update_navigation_grid();

  std::unique_lock<std::shared_mutex> const lock(m_navigation_mutex);
  m_navigation_grid.set(x, y, is_obstacle ? CellValue::Blocked : CellValue::Walkable);

  rebuild_clearance(x - 1, x + 1, y - 1, y + 1);
  m_navigation_revision.fetch_add(1, std::memory_order_release);
}

auto Pathfinding::is_walkable(int x, int y, Passability passability) const -> bool {
  if (!in_bounds(x, y)) {
    return false;
  }
  auto const value = m_navigation_grid.at_unchecked(x, y);
  if (value == CellValue::Walkable) {
    return true;
  }
  return value == CellValue::Forest && passability == Passability::Light;
}

auto Pathfinding::is_forest(int x, int y) const -> bool {
  return cell_is(x, y, CellValue::Forest);
}

auto Pathfinding::is_tree(int x, int y) const -> bool {
  return cell_is(x, y, CellValue::Tree);
}

auto Pathfinding::is_boulder(int x, int y) const -> bool {
  return cell_is(x, y, CellValue::Boulder);
}

auto Pathfinding::is_iron_ore(int x, int y) const -> bool {
  return cell_is(x, y, CellValue::IronOre);
}

auto Pathfinding::cell_value(int x, int y) const -> CellValue {
  return m_navigation_grid.get(x, y);
}

auto Pathfinding::is_terrain_walkable(int x, int y) const -> bool {
  if (x < 0 || x >= m_width || y < 0 || y >= m_height) {
    return false;
  }
  auto& terrain_service = *m_terrain;
  if (!terrain_service.is_initialized()) {
    return true;
  }
  return terrain_cell_value(terrain_service, terrain_service.get_height_map(), x, y) ==
         CellValue::Walkable;
}

auto Pathfinding::is_world_position_walkable(const QVector3D& world_position,
                                             Passability passability,
                                             float clearance_radius) const -> bool {
  Engine::Core::count_nav(Engine::Core::NavCounter::PositionTests);
  Point const grid = world_to_grid(world_position.x(), world_position.z());
  if (!is_walkable(grid.x, grid.y, passability)) {
    return false;
  }

  if (clearance_radius <= 0.0F) {
    return true;
  }

  float const radius = std::min(clearance_radius, k_max_body_clearance);
  float const half_cell = m_grid_cell_size * 0.5F;
  float const center_u = world_position.x() - m_grid_offset_x;
  float const center_v = world_position.z() - m_grid_offset_z;
  CellRange const box = body_cell_range(center_u, center_v, radius, half_cell);

  for (int cell_z = box.min_z; cell_z <= box.max_z; ++cell_z) {
    for (int cell_x = box.min_x; cell_x <= box.max_x; ++cell_x) {
      if (cell_x == grid.x && cell_z == grid.y) {
        continue;
      }
      if (is_walkable(cell_x, cell_z, passability)) {
        continue;
      }

      if (cell_gap(center_u, center_v, cell_x, cell_z, half_cell).squared() <
          radius * radius) {
        return false;
      }
    }
  }
  return true;
}

auto Pathfinding::is_world_segment_walkable(const QVector3D& from,
                                            const QVector3D& to,
                                            Passability passability,
                                            float clearance_radius) const -> bool {
  Engine::Core::count_nav(Engine::Core::NavCounter::SegmentTests);

  if (clearance_radius > 0.0F) {
    QVector3D const delta = to - from;
    float const length = std::hypot(delta.x(), delta.z());
    float const sample_spacing = std::max(0.2F, m_grid_cell_size * 0.35F);
    int const samples =
        std::max(1, static_cast<int>(std::ceil(length / sample_spacing)));
    for (int sample = 0; sample <= samples; ++sample) {
      float const t = static_cast<float>(sample) / static_cast<float>(samples);
      if (!is_world_position_walkable(
              from + delta * t, passability, clearance_radius)) {
        return false;
      }
    }
  }

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
  m_obstruction_center_located.store(false, std::memory_order_relaxed);
  m_obstruction_revision.fetch_add(1, std::memory_order_acq_rel);
}

void Pathfinding::mark_obstruction_released_at(float center_x, float center_z) {
  m_obstruction_center_x.store(center_x, std::memory_order_relaxed);
  m_obstruction_center_z.store(center_z, std::memory_order_relaxed);
  m_obstruction_center_located.store(true, std::memory_order_relaxed);
  m_obstruction_revision.fetch_add(1, std::memory_order_acq_rel);
}

auto Pathfinding::last_obstruction_release() const -> ObstructionRelease {
  if (!m_obstruction_center_located.load(std::memory_order_relaxed)) {
    return {};
  }
  return {.center = QVector3D(m_obstruction_center_x.load(std::memory_order_relaxed),
                              0.0F,
                              m_obstruction_center_z.load(std::memory_order_relaxed)),
          .located = true};
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
  Engine::Core::count_nav(
      Engine::Core::NavCounter::DirtyCellsRebuilt,
      static_cast<std::uint64_t>(std::max(0, max_x - min_x + 1)) *
          static_cast<std::uint64_t>(std::max(0, max_z - min_z + 1)));

  auto& terrain_service = *m_terrain;
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
  apply_gate_blocker_cells(min_x, max_x, min_z, max_z);

  rebuild_clearance(min_x - 1, max_x + 1, min_z - 1, max_z + 1);
}

void Pathfinding::apply_gate_blocker_cells(int min_x, int max_x, int min_z, int max_z) {
  min_x = std::max(0, min_x);
  max_x = std::min(m_width - 1, max_x);
  min_z = std::max(0, min_z);
  max_z = std::min(m_height - 1, max_z);
  if (min_x > max_x || min_z > max_z) {
    return;
  }
  for (auto const& blocker : GateService::blockers()) {
    float const center_x = (blocker.min_x + blocker.max_x) * 0.5F;
    float const center_z = (blocker.min_z + blocker.max_z) * 0.5F;
    float const half_x = (blocker.max_x - blocker.min_x) * 0.5F;
    float const half_z = (blocker.max_z - blocker.min_z) * 0.5F;
    auto const range = cells_covering(center_x, center_z, half_x, half_z);
    for (int grid_z = std::max(range.min_z, min_z);
         grid_z <= std::min(range.max_z, max_z);
         ++grid_z) {
      for (int grid_x = std::max(range.min_x, min_x);
           grid_x <= std::min(range.max_x, max_x);
           ++grid_x) {
        m_navigation_grid.set(grid_x, grid_z, CellValue::Blocked);
      }
    }
  }
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

  float const routing_half_x = (building.width * 0.5F) + building.grid_padding;
  float const routing_half_z = (building.depth * 0.5F) + building.grid_padding;
  float min_x_world = building.center_x - routing_half_x;
  float max_x_world = building.center_x + routing_half_x;
  float min_z_world = building.center_z - routing_half_z;
  float max_z_world = building.center_z + routing_half_z;
  if (building.body_width > 0.0F && building.body_depth > 0.0F) {
    min_x_world =
        std::min(min_x_world, building.body_center_x - (building.body_width * 0.5F));
    max_x_world =
        std::max(max_x_world, building.body_center_x + (building.body_width * 0.5F));
    min_z_world =
        std::min(min_z_world, building.body_center_z - (building.body_depth * 0.5F));
    max_z_world =
        std::max(max_z_world, building.body_center_z + (building.body_depth * 0.5F));
  }
  auto const range = cells_covering((min_x_world + max_x_world) * 0.5F,
                                    (min_z_world + max_z_world) * 0.5F,
                                    (max_x_world - min_x_world) * 0.5F,
                                    (max_z_world - min_z_world) * 0.5F);
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
  auto& terrain_service = *m_terrain;
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

  auto& terrain_service = *m_terrain;
  if (!terrain_service.is_initialized() || terrain_service.forests().empty()) {
    return;
  }

  const auto* height_map = terrain_service.get_height_map();
  float const tile_size =
      height_map != nullptr ? std::max(height_map->get_tile_size(), 0.0001F) : 1.0F;
  bool const world_space =
      terrain_service.coord_system() == Game::Map::CoordSystem::World;

  for (auto const& forest : terrain_service.forests()) {
    float center_x = forest.x;
    float center_z = forest.z;
    float radius = forest.radius;
    if (world_space) {
      center_x = forest.x / tile_size - m_grid_offset_x;
      center_z = forest.z / tile_size - m_grid_offset_z;
      radius = forest.radius / tile_size;
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
  auto& terrain_service = *m_terrain;
  const auto* height_map = terrain_service.get_height_map();
  float const tile_size =
      height_map != nullptr ? std::max(height_map->get_tile_size(), 0.0001F) : 1.0F;
  std::unordered_map<int, CellValue> next;
  std::unordered_map<int, CellValue> footprints;
  next.reserve(terrain_service.world_props().size());
  footprints.reserve(terrain_service.world_props().size() * 4U);

  auto claim_footprint = [&footprints](int index, CellValue value) {
    auto const existing = footprints.find(index);
    if (existing == footprints.end()) {
      footprints.emplace(index, value);
      return;
    }
    if (existing->second == CellValue::Blocked) {
      existing->second = value;
    }
  };

  for (auto const& prop : terrain_service.world_props()) {
    if (!Game::Map::is_solid_world_prop_type(prop.type)) {
      continue;
    }
    CellValue value = CellValue::Blocked;
    if (Game::Map::is_tree_world_prop_type(prop.type)) {
      value = CellValue::Tree;
    } else if (Game::Map::is_boulder_world_prop_type(prop.type)) {
      value = CellValue::Boulder;
    } else if (Game::Map::is_iron_ore_world_prop_type(prop.type)) {
      value = CellValue::IronOre;
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

    float const bounds_cells =
        Game::Map::world_prop_ground_bounding_radius(prop.type, prop.scale) / tile_size;
    int const min_x =
        std::max(0, static_cast<int>(std::floor(grid_x_value - bounds_cells)));
    int const max_x =
        std::min(m_width - 1, static_cast<int>(std::ceil(grid_x_value + bounds_cells)));
    int const min_z =
        std::max(0, static_cast<int>(std::floor(grid_z_value - bounds_cells)));
    int const max_z = std::min(
        m_height - 1, static_cast<int>(std::ceil(grid_z_value + bounds_cells)));

    for (int cell_z = min_z; cell_z <= max_z; ++cell_z) {
      for (int cell_x = min_x; cell_x <= max_x; ++cell_x) {
        float const offset_x = (static_cast<float>(cell_x) - grid_x_value) * tile_size;
        float const offset_z = (static_cast<float>(cell_z) - grid_z_value) * tile_size;
        if (Game::Map::world_prop_overlap_depth(prop.type,
                                                prop.scale,
                                                0.0F,
                                                0.0F,
                                                prop.rotation,
                                                offset_x,
                                                offset_z,
                                                0.0F) <= 0.0F) {
          continue;
        }

        QVector3D const world = grid_to_world({cell_x, cell_z});
        if (terrain_service.is_point_on_road(world.x(), world.z())) {
          continue;
        }
        claim_footprint(to_index(cell_x, cell_z), value);
      }
    }
  }

  for (auto const& [index, value] : footprints) {
    next.emplace(index, value);
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
  auto& terrain_service = *m_terrain;
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
  Engine::Core::count_nav(Engine::Core::NavCounter::GridRebuilds);
  process_dirty_regions();
  m_applied_terrain_topology_revision.store(terrain_topology_revision,
                                            std::memory_order_release);
  m_applied_world_props_revision.store(world_props_revision, std::memory_order_release);
  m_navigation_revision.fetch_add(1, std::memory_order_release);

  m_navigation_grid_dirty.store(false, std::memory_order_release);
}

auto Pathfinding::find_path(const Point& start,
                            const Point& end,
                            Passability passability,
                            float clearance_radius) -> std::vector<Point> {
  Engine::Core::NavScope const scope(Engine::Core::NavCounter::IndividualRoutes);

  if (m_navigation_grid_dirty.load(std::memory_order_acquire)) {
    update_navigation_grid();
  }

  std::uint64_t const revision = navigation_revision();
  int const clearance_quarters =
      static_cast<int>(std::ceil(std::max(0.0F, clearance_radius) * 4.0F));
  PathCacheKey const key{
      start.x, start.y, end.x, end.y, passability, clearance_quarters};
  {
    std::lock_guard<std::mutex> const cache_lock(m_path_cache_mutex);
    if (m_path_cache_revision != revision) {
      m_path_cache.clear();
      m_path_cache_revision = revision;
    }
    if (auto const cached = m_path_cache.find(key); cached != m_path_cache.end()) {
      Engine::Core::count_nav(Engine::Core::NavCounter::RouteCacheHits);
      return cached->second;
    }
  }
  Engine::Core::count_nav(Engine::Core::NavCounter::RouteCacheMisses);

  std::shared_lock<std::shared_mutex> const navigation_lock(m_navigation_mutex);
  auto path = find_path_internal(
      start, end, passability, static_cast<float>(clearance_quarters) * 0.25F);
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

auto Pathfinding::label_at(const RegionMap& map,
                           const Point& cell) const -> std::uint32_t {
  if (cell.x < 0 || cell.x >= m_width || cell.y < 0 || cell.y >= m_height) {
    return k_unreachable_region;
  }
  auto const index = static_cast<std::size_t>(to_index(cell));
  return index < map.labels.size() ? map.labels[index] : k_unreachable_region;
}

void Pathfinding::rebuild_region_map(RegionMap& map, Passability passability) const {
  auto const cell_count = static_cast<std::size_t>(std::max(m_width, 0)) *
                          static_cast<std::size_t>(std::max(m_height, 0));
  map.labels.assign(cell_count, k_unreachable_region);
  if (cell_count == 0U) {
    return;
  }

  std::vector<int> frontier;
  std::uint32_t next_label = k_unreachable_region;
  for (int seed_y = 0; seed_y < m_height; ++seed_y) {
    for (int seed_x = 0; seed_x < m_width; ++seed_x) {
      int const seed_index = to_index(seed_x, seed_y);
      if (map.labels[static_cast<std::size_t>(seed_index)] != k_unreachable_region ||
          !is_walkable(seed_x, seed_y, passability)) {
        continue;
      }

      ++next_label;
      map.labels[static_cast<std::size_t>(seed_index)] = next_label;
      frontier.clear();
      frontier.push_back(seed_index);

      while (!frontier.empty()) {
        Point const current = to_point(frontier.back());
        frontier.pop_back();

        std::array<Point, 8> neighbors{};
        std::size_t const neighbor_count =
            collect_neighbors(current, neighbors, passability);
        for (std::size_t i = 0; i < neighbor_count; ++i) {
          Point const& neighbor = neighbors[i];
          if (!is_walkable(neighbor.x, neighbor.y, passability)) {
            continue;
          }
          auto const neighbor_index = static_cast<std::size_t>(to_index(neighbor));
          if (map.labels[neighbor_index] != k_unreachable_region) {
            continue;
          }
          map.labels[neighbor_index] = next_label;
          frontier.push_back(static_cast<int>(neighbor_index));
        }
      }
    }
  }
}

void Pathfinding::region_labels(const Point& first,
                                const Point& second,
                                Passability passability,
                                std::uint32_t& first_label,
                                std::uint32_t& second_label) {
  if (m_navigation_grid_dirty.load(std::memory_order_acquire)) {
    update_navigation_grid();
  }

  std::uint64_t const revision = navigation_revision();
  std::shared_lock<std::shared_mutex> const navigation_lock(m_navigation_mutex);
  std::lock_guard<std::mutex> const region_lock(m_region_mutex);

  auto& map = m_region_maps[static_cast<std::size_t>(passability)];
  if (!map.built || map.revision != revision) {
    rebuild_region_map(map, passability);
    map.revision = revision;
    map.built = true;
  }

  first_label = label_at(map, first);
  second_label = label_at(map, second);
}

auto Pathfinding::region_of(const Point& cell,
                            Passability passability) -> std::uint32_t {
  std::uint32_t label = k_unreachable_region;
  std::uint32_t ignored = k_unreachable_region;
  region_labels(cell, cell, passability, label, ignored);
  return label;
}

auto Pathfinding::can_reach(const Point& start,
                            const Point& end,
                            Passability passability) -> bool {
  std::uint32_t start_label = k_unreachable_region;
  std::uint32_t end_label = k_unreachable_region;
  region_labels(start, end, passability, start_label, end_label);
  return start_label != k_unreachable_region && start_label == end_label;
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
  combine(key.clearance_quarters);
  return result;
}

auto Pathfinding::find_path_internal(const Point& start,
                                     const Point& end,
                                     Passability passability,
                                     float clearance_radius) -> std::vector<Point> {
  SearchBuffers& buffers = search_buffers_for(this);
  ensure_working_buffers(buffers);

  auto const is_walkableFunc = [this, passability](int x, int y) -> bool {
    return is_world_position_walkable(grid_to_world({x, y}), passability, 0.0F);
  };

  int const clearance_weight =
      clearance_radius > 0.0F
          ? std::max(1,
                     static_cast<int>(
                         std::lround(clearance_radius * k_clearance_avoid_weight)))
          : 1;

  if (!is_walkableFunc(start.x, start.y) || !is_walkableFunc(end.x, end.y)) {
    Point resolved_start = start;
    Point resolved_end = end;
    if ((!is_walkableFunc(start.x, start.y) &&
         !resolve_walkable_endpoint(
             start, resolved_start, passability, clearance_radius)) ||
        (!is_walkableFunc(end.x, end.y) &&
         !resolve_walkable_endpoint(
             end, resolved_end, passability, clearance_radius))) {
      return {};
    }

    if (resolved_start == start && resolved_end == end) {
      return {};
    }

    return find_path_internal(
        resolved_start, resolved_end, passability, clearance_radius);
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
    Engine::Core::count_nav(Engine::Core::NavCounter::CellsExpanded);
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
      if (step_x != 0 && step_z != 0 &&
          (!is_walkableFunc(current_point.x + step_x, current_point.y) ||
           !is_walkableFunc(current_point.x, current_point.y + step_z))) {
        continue;
      }
      const bool turns = (arrival_x != 0 || arrival_z != 0) &&
                         (step_x != arrival_x || step_z != arrival_z);
      const int tentative_gcost =
          current.g_cost +
          ((step_x != 0 && step_z != 0) ? k_diagonal_step_cost : k_straight_step_cost) +
          (clearance_penalty(neighbor.x, neighbor.y) * clearance_weight) +
          (turns ? k_turn_penalty : 0);
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
                                            Passability passability,
                                            float clearance_radius) const -> bool {
  auto const is_walkable_func = [this, passability, clearance_radius](int x,
                                                                      int y) -> bool {
    return is_world_position_walkable(
        grid_to_world({x, y}), passability, clearance_radius);
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

    for_each_ring_cell(radius, [&](int dx, int dy) {
      int const check_x = clamped_origin.x + dx;
      int const check_y = clamped_origin.y + dy;
      if (!is_walkable_func(check_x, check_y)) {
        return;
      }

      int const requested_dx = check_x - requested.x;
      int const requested_dy = check_y - requested.y;
      int const distance_sq = requested_dx * requested_dx + requested_dy * requested_dy;
      if (distance_sq < best_distance_sq) {
        best_distance_sq = distance_sq;
        best_candidate = {check_x, check_y};
        found_candidate = true;
      }
    });

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

  int const pad_min_x = std::max(0, min_x - k_clearance_radius);
  int const pad_max_x = std::min(m_width - 1, max_x + k_clearance_radius);
  int const pad_min_z = std::max(0, min_z - k_clearance_radius);
  int const pad_max_z = std::min(m_height - 1, max_z + k_clearance_radius);
  int const span_x = pad_max_x - pad_min_x + 1;
  int const span_z = pad_max_z - pad_min_z + 1;
  auto const far = static_cast<std::uint8_t>(k_clearance_radius + 1);

  std::vector<std::uint8_t> distance(
      static_cast<std::size_t>(span_x) * static_cast<std::size_t>(span_z), far);
  auto at = [&](int x, int z) -> std::uint8_t& {
    return distance[(static_cast<std::size_t>(z - pad_min_z) *
                     static_cast<std::size_t>(span_x)) +
                    static_cast<std::size_t>(x - pad_min_x)];
  };

  for (int z = pad_min_z; z <= pad_max_z; ++z) {
    for (int x = pad_min_x; x <= pad_max_x; ++x) {
      if (!is_walkable(x, z)) {
        at(x, z) = 0;
      }
    }
  }

  auto relax = [&](std::uint8_t& cell, std::uint8_t neighbour) {
    if (neighbour < far && neighbour + 1 < cell) {
      cell = static_cast<std::uint8_t>(neighbour + 1);
    }
  };
  for (int z = pad_min_z; z <= pad_max_z; ++z) {
    for (int x = pad_min_x; x <= pad_max_x; ++x) {
      std::uint8_t& cell = at(x, z);
      if (z > pad_min_z) {
        relax(cell, at(x, z - 1));
        if (x > pad_min_x) {
          relax(cell, at(x - 1, z - 1));
        }
        if (x < pad_max_x) {
          relax(cell, at(x + 1, z - 1));
        }
      }
      if (x > pad_min_x) {
        relax(cell, at(x - 1, z));
      }
    }
  }
  for (int z = pad_max_z; z >= pad_min_z; --z) {
    for (int x = pad_max_x; x >= pad_min_x; --x) {
      std::uint8_t& cell = at(x, z);
      if (z < pad_max_z) {
        relax(cell, at(x, z + 1));
        if (x < pad_max_x) {
          relax(cell, at(x + 1, z + 1));
        }
        if (x > pad_min_x) {
          relax(cell, at(x - 1, z + 1));
        }
      }
      if (x < pad_max_x) {
        relax(cell, at(x + 1, z));
      }
    }
  }

  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      int const reach = at(x, z);
      if (reach == 0 || reach > k_clearance_radius) {
        continue;
      }
      int const graded = (k_clearance_ring_penalty * (k_clearance_radius + 1 - reach)) /
                         k_clearance_radius;
      m_clearance_penalty[static_cast<std::size_t>(to_index(x, z))] =
          static_cast<std::uint8_t>(std::max(graded, k_edge_step_penalty));
    }
  }
}

auto Pathfinding::search_buffers_by_grid()
    -> std::unordered_map<const Pathfinding*, SearchBuffers>& {
  thread_local std::unordered_map<const Pathfinding*, SearchBuffers> buffers_by_grid;
  return buffers_by_grid;
}

auto Pathfinding::search_buffers_for(const Pathfinding* pathfinding) -> SearchBuffers& {
  return search_buffers_by_grid()[pathfinding];
}

void Pathfinding::release_search_buffers(const Pathfinding* pathfinding) {
  search_buffers_by_grid().erase(pathfinding);
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
  Engine::Core::count_nav(Engine::Core::NavCounter::HeapOperations);
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
  Engine::Core::count_nav(Engine::Core::NavCounter::HeapOperations);
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
