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

#include "game/core/nav_profile.h"
#include "gate_service.h"
#include "map/forest_outline.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "systems/building_collision_registry.h"

namespace Game::Systems {

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
  note_navigation_change(x - 1, x + 1, y - 1, y + 1);
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

auto Pathfinding::is_terrain_segment_walkable(const QVector3D& from,
                                              const QVector3D& to) const -> bool {
  Point const start = world_to_grid(from.x(), from.z());
  float const length = std::hypot(to.x() - from.x(), to.z() - from.z());
  int const steps = std::max(1, static_cast<int>(std::ceil(length / 0.25F)));
  for (int i = 1; i <= steps; ++i) {
    float const t = static_cast<float>(i) / static_cast<float>(steps);
    Point const cell = world_to_grid(from.x() + (to.x() - from.x()) * t,
                                     from.z() + (to.z() - from.z()) * t);
    if ((cell.x != start.x || cell.y != start.y) &&
        !is_terrain_walkable(cell.x, cell.y)) {
      return false;
    }
  }
  return true;
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

  float const radius = clearance_radius;
  float const half_cell = m_grid_cell_size * 0.5F;
  float const center_u = world_position.x() - m_grid_offset_x;
  float const center_v = world_position.z() - m_grid_offset_z;

  constexpr float k_gap_margin = 1.0e-4F;
  float const offset_from_cell =
      std::max(std::abs(center_u - static_cast<float>(grid.x)),
               std::abs(center_v - static_cast<float>(grid.y)));
  if (radius < 1.0F - offset_from_cell - half_cell - k_gap_margin) {
    return true;
  }

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

  constexpr float k_segment_end = 1.0F;

  int const max_steps = std::abs(end_x - cell_x) + std::abs(end_z - cell_z) + 2;
  for (int taken = 0; taken < max_steps; ++taken) {
    if (cell_x == end_x && cell_z == end_z) {
      return true;
    }

    if (std::min(travel_x, travel_z) >= k_segment_end - k_boundary_epsilon) {
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
auto Pathfinding::building_collisions() -> BuildingCollisionRegistry& {
  return buildings();
}

auto Pathfinding::buildings() -> BuildingCollisionRegistry& {
  return BuildingCollisionRegistry::instance();
}

} // namespace Game::Systems
