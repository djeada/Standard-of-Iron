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
#include "pathfinding.h"
#include "systems/building_collision_registry.h"

namespace Game::Systems {

namespace {
auto hill_entrance_opens_cell(const Game::Map::TerrainHeightMap& height_map,
                              Game::Map::TerrainType terrain_type,
                              int x,
                              int z) -> bool {
  return height_map.isHillEntrance(x, z) &&
         terrain_type != Game::Map::TerrainType::Mountain &&
         !Game::Map::is_water_terrain(terrain_type);
}

} // namespace

auto Pathfinding::terrain_cell_value(const Game::Map::TerrainService& terrain_service,
                                     const Game::Map::TerrainHeightMap* height_map,
                                     int x,
                                     int z) -> CellValue {
  if (height_map == nullptr || x < 0 || x >= height_map->get_width() || z < 0 ||
      z >= height_map->get_height()) {
    return Pathfinding::CellValue::Blocked;
  }

  Game::Map::TerrainType const terrain_type = terrain_service.get_terrain_type(x, z);
  if (terrain_type == Game::Map::TerrainType::Mountain) {
    return Pathfinding::CellValue::Blocked;
  }

  if (height_map->isBridgeCell(x, z) ||
      hill_entrance_opens_cell(*height_map, terrain_type, x, z)) {
    return Pathfinding::CellValue::Walkable;
  }

  if (Game::Map::is_water_terrain(terrain_type)) {
    return height_map->isBridgeCenterline(x, z) ? Pathfinding::CellValue::Walkable
                                                : Pathfinding::CellValue::Blocked;
  }

  return terrain_service.is_walkable(x, z) ? Pathfinding::CellValue::Walkable
                                           : Pathfinding::CellValue::Blocked;
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

  std::vector<CellValue> base;
  base.reserve(static_cast<std::size_t>(max_x - min_x + 3) *
               static_cast<std::size_t>(max_z - min_z + 3));
  for (int z = min_z - 1; z <= max_z + 1; ++z) {
    for (int x = min_x - 1; x <= max_x + 1; ++x) {

      bool const slope =
          x >= 0 && z >= 0 && x < m_width && z < m_height &&
          terrain_service.is_initialized() &&
          terrain_service.get_terrain_type(x, z) == Game::Map::TerrainType::Hill &&
          terrain_cell_value(terrain_service, height_map, x, z) == CellValue::Blocked;
      base.push_back(slope ? CellValue::Blocked : CellValue::Walkable);
    }
  }

  auto& registry = buildings();
  registry.for_each_building_in_region(
      static_cast<float>(min_x) + m_grid_offset_x,
      static_cast<float>(max_x) + m_grid_offset_x,
      static_cast<float>(min_z) + m_grid_offset_z,
      static_cast<float>(max_z) + m_grid_offset_z,
      [this, min_x, max_x, min_z, max_z](BuildingFootprint const& building) {
        apply_building_cells(building, min_x, max_x, min_z, max_z);
      });

  open_padding_lanes(base, min_x, max_x, min_z, max_z);
  force_navigation_passages_walkable(min_x, max_x, min_z, max_z);
  force_map_passage_cells_walkable(min_x, max_x, min_z, max_z);
  apply_gate_blocker_cells(min_x, max_x, min_z, max_z);

  rebuild_elevation(min_x - 1, max_x + 1, min_z - 1, max_z + 1);
  rebuild_clearance(min_x - 1, max_x + 1, min_z - 1, max_z + 1);
}

void Pathfinding::apply_gate_blocker_cells(int min_x, int max_x, int min_z, int max_z) {
  for (int const index : closed_gate_cells(min_x, max_x, min_z, max_z)) {
    Point const cell = to_point(index);
    m_navigation_grid.set(cell.x, cell.y, CellValue::Blocked);
  }
}

auto Pathfinding::closed_gate_cells(int min_x,
                                    int max_x,
                                    int min_z,
                                    int max_z) const -> std::vector<int> {
  std::vector<int> cells;
  if (!clamp_to_grid(min_x, max_x, min_z, max_z)) {
    return cells;
  }
  for (auto const& blocker : GateService::blockers()) {
    auto const range = cells_covering((blocker.min_x + blocker.max_x) * 0.5F,
                                      (blocker.min_z + blocker.max_z) * 0.5F,
                                      (blocker.max_x - blocker.min_x) * 0.5F,
                                      (blocker.max_z - blocker.min_z) * 0.5F);
    for (int grid_z = std::max(range.min_z, min_z);
         grid_z <= std::min(range.max_z, max_z);
         ++grid_z) {
      for (int grid_x = std::max(range.min_x, min_x);
           grid_x <= std::min(range.max_x, max_x);
           ++grid_x) {
        cells.push_back(to_index(grid_x, grid_z));
      }
    }
  }
  std::sort(cells.begin(), cells.end());
  return cells;
}

void Pathfinding::force_navigation_passages_walkable(int min_x,
                                                     int max_x,
                                                     int min_z,
                                                     int max_z) {
  if (!clamp_to_grid(min_x, max_x, min_z, max_z)) {
    return;
  }

  constexpr float k_touch_epsilon = 1.0e-3F;

  auto const& registry = buildings();
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

auto Pathfinding::building_cell_ranges(const BuildingFootprint& building) const
    -> BuildingCellRanges {
  float const half_x = building.width * 0.5F;
  float const half_z = building.depth * 0.5F;
  float hard_min_x = building.center_x - half_x;
  float hard_max_x = building.center_x + half_x;
  float hard_min_z = building.center_z - half_z;
  float hard_max_z = building.center_z + half_z;
  if (building.body_width > 0.0F && building.body_depth > 0.0F) {
    hard_min_x =
        std::min(hard_min_x, building.body_center_x - (building.body_width * 0.5F));
    hard_max_x =
        std::max(hard_max_x, building.body_center_x + (building.body_width * 0.5F));
    hard_min_z =
        std::min(hard_min_z, building.body_center_z - (building.body_depth * 0.5F));
    hard_max_z =
        std::max(hard_max_z, building.body_center_z + (building.body_depth * 0.5F));
  }
  float const pad = building.grid_padding;
  auto const range_of = [this](float lo_x, float hi_x, float lo_z, float hi_z) {
    return cells_covering((lo_x + hi_x) * 0.5F,
                          (lo_z + hi_z) * 0.5F,
                          (hi_x - lo_x) * 0.5F,
                          (hi_z - lo_z) * 0.5F);
  };
  return {.hard = range_of(hard_min_x, hard_max_x, hard_min_z, hard_max_z),
          .padded = range_of(std::min(hard_min_x, building.center_x - half_x - pad),
                             std::max(hard_max_x, building.center_x + half_x + pad),
                             std::min(hard_min_z, building.center_z - half_z - pad),
                             std::max(hard_max_z, building.center_z + half_z + pad))};
}

void Pathfinding::apply_building_cells(
    const BuildingFootprint& building, int min_x, int max_x, int min_z, int max_z) {
  if (!building.blocks_navigation) {
    return;
  }

  auto const range = building_cell_ranges(building).padded;
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

namespace {

auto range_contains(const CellRange& range, int x, int z) -> bool {
  return x >= range.min_x && x <= range.max_x && z >= range.min_z && z <= range.max_z;
}

} // namespace

void Pathfinding::open_padding_lanes(
    const std::vector<CellValue>& base, int min_x, int max_x, int min_z, int max_z) {
  struct Blocker {
    CellRange hard;
    CellRange padded;
    bool soft_padding;
  };
  std::vector<Blocker> blockers;
  buildings().for_each_building_in_region(
      static_cast<float>(min_x - 2) + m_grid_offset_x,
      static_cast<float>(max_x + 2) + m_grid_offset_x,
      static_cast<float>(min_z - 2) + m_grid_offset_z,
      static_cast<float>(max_z + 2) + m_grid_offset_z,
      [this, &blockers](const BuildingFootprint& building) {
        if (!building.blocks_navigation) {
          return;
        }
        auto const ranges = building_cell_ranges(building);
        blockers.push_back(
            {.hard = ranges.hard,
             .padded = ranges.padded,
             .soft_padding = !building.wall_link && !building.wall_tower});
      });
  if (blockers.empty()) {
    return;
  }

  int const span_x = max_x - min_x + 3;
  auto const base_blocked = [&](int x, int z) {
    if (x < 0 || z < 0 || x >= m_width || z >= m_height) {
      return false;
    }
    int const index = (z - (min_z - 1)) * span_x + (x - (min_x - 1));
    return base[static_cast<std::size_t>(index)] == CellValue::Blocked;
  };

  std::vector<Point> lanes;
  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      if (m_navigation_grid.get(x, z) != CellValue::Blocked ||
          !is_terrain_walkable(x, z)) {
        continue;
      }
      int owners = 0;
      bool hard = false;
      for (const auto& blocker : blockers) {
        if (range_contains(blocker.hard, x, z) || !blocker.soft_padding) {
          hard = hard || range_contains(blocker.padded, x, z);
        }
        if (range_contains(blocker.padded, x, z)) {
          ++owners;
        }
      }
      if (hard || owners == 0) {
        continue;
      }
      bool opens = false;
      for (int dz = -1; dz <= 1 && !opens; ++dz) {
        for (int dx = -1; dx <= 1 && !opens; ++dx) {
          opens = (dx != 0 || dz != 0) && base_blocked(x + dx, z + dz);
        }
      }
      if (opens) {
        lanes.push_back({x, z});
      }
    }
  }
  for (const auto& cell : lanes) {
    m_navigation_grid.set(cell.x, cell.y, CellValue::Walkable);
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

  if (!clamp_to_grid(min_x, max_x, min_z, max_z)) {
    return;
  }

  constexpr float k_touch_epsilon = 1.0e-3F;
  auto const& registry = buildings();

  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      if (m_navigation_grid.get(x, z) == CellValue::Walkable) {
        continue;
      }
      if (!height_map->isBridgeCell(x, z) && !height_map->isBridgeCenterline(x, z) &&
          !hill_entrance_opens_cell(
              *height_map, terrain_service.get_terrain_type(x, z), x, z)) {
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

    float const reach = radius * Game::Map::k_forest_outline_reach;
    int const min_x = std::max(0, static_cast<int>(std::floor(center_x - reach)));
    int const max_x =
        std::min(m_width - 1, static_cast<int>(std::ceil(center_x + reach)));
    int const min_z = std::max(0, static_cast<int>(std::floor(center_z - reach)));
    int const max_z =
        std::min(m_height - 1, static_cast<int>(std::ceil(center_z + reach)));

    for (int grid_z = min_z; grid_z <= max_z; ++grid_z) {
      for (int grid_x = min_x; grid_x <= max_x; ++grid_x) {
        float const dx = static_cast<float>(grid_x) - center_x;
        float const dz = static_cast<float>(grid_z) - center_z;
        float const edge =
            radius * Game::Map::forest_outline_scale(forest.outline_seed, dx, dz);
        if (dx * dx + dz * dz > edge * edge) {
          continue;
        }

        QVector3D const world = grid_to_world({grid_x, grid_z});
        if (terrain_service.is_point_on_road(world.x(), world.z())) {
          continue;
        }
        if (height_map != nullptr && height_map->is_fields(grid_x, grid_z)) {
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
  const DirtyRegion rebuilt = process_dirty_regions();
  m_applied_terrain_topology_revision.store(terrain_topology_revision,
                                            std::memory_order_release);
  m_applied_world_props_revision.store(world_props_revision, std::memory_order_release);
  m_navigation_revision.fetch_add(1, std::memory_order_release);
  note_navigation_change(rebuilt.min_x, rebuilt.max_x, rebuilt.min_z, rebuilt.max_z);

  m_navigation_grid_dirty.store(false, std::memory_order_release);
}

void Pathfinding::prewarm_navigation() {
  update_navigation_grid();
  for (std::size_t index = 0; index < k_passability_count; ++index) {
    (void)region_of(Point{}, static_cast<Passability>(index));
  }
}
} // namespace Game::Systems
