#include "building_collision_registry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../session/session_context.h"
#include "command_service.h"
#include "pathfinding.h"

namespace Game::Systems {

const std::map<std::string, BuildingCollisionRegistry::BuildingSize>
    BuildingCollisionRegistry::s_building_sizes = {
        {"barracks", {4.F, 4.F}},
        {"home", {3.F, 3.F}},
        {"marketplace", {3.F, 3.F}},
        {"temple", {3.F, 3.F}},
        {"wall_segment", {2.0F, 2.0F}},
        {"wall_gate", {2.0F, 2.0F}},

};

float BuildingCollisionRegistry::s_grid_padding =
    BuildingCollisionRegistry::k_default_grid_padding;

auto BuildingCollisionRegistry::instance() -> BuildingCollisionRegistry& {
  return Game::Session::SessionContext::active().building_collision();
}

auto BuildingCollisionRegistry::get_building_size(const std::string& building_type)
    -> BuildingCollisionRegistry::BuildingSize {
  auto it = s_building_sizes.find(building_type);
  if (it != s_building_sizes.end()) {
    return it->second;
  }

  return {2.0F, 2.0F};
}

auto BuildingCollisionRegistry::get_building_grid_padding(
    const std::string& building_type) -> float {

  if (building_type == "wall_segment") {
    return k_wall_segment_grid_padding;
  }

  return s_grid_padding;
}

auto BuildingCollisionRegistry::bucket_coord(float coordinate) -> int {
  return static_cast<int>(std::floor(coordinate / k_spatial_bucket_size));
}

auto BuildingCollisionRegistry::bucket_key(int bucket_x, int bucket_z) -> std::int64_t {
  auto const high = static_cast<std::uint64_t>(static_cast<std::uint32_t>(bucket_x));
  auto const low = static_cast<std::uint32_t>(bucket_z);
  return static_cast<std::int64_t>((high << 32U) | low);
}

void BuildingCollisionRegistry::add_to_spatial_index(
    const BuildingFootprint& footprint) {
  int const bucket_x = bucket_coord(footprint.center_x);
  int const bucket_z = bucket_coord(footprint.center_z);
  m_spatial_buckets[bucket_key(bucket_x, bucket_z)].push_back(footprint.entity_id);
  m_max_half_extent =
      std::max(m_max_half_extent, std::max(footprint.width, footprint.depth) * 0.5F);
}

void BuildingCollisionRegistry::remove_from_spatial_index(
    const BuildingFootprint& footprint) {
  auto const key =
      bucket_key(bucket_coord(footprint.center_x), bucket_coord(footprint.center_z));
  auto bucket = m_spatial_buckets.find(key);
  if (bucket == m_spatial_buckets.end()) {
    return;
  }
  std::erase(bucket->second, footprint.entity_id);
  if (bucket->second.empty()) {
    m_spatial_buckets.erase(bucket);
  }
}

void BuildingCollisionRegistry::register_building(Engine::Core::EntityID entity_id,
                                                  const std::string& building_type,
                                                  float center_x,
                                                  float center_z,
                                                  int owner_id) {

  if (m_entity_to_index.find(entity_id) != m_entity_to_index.end()) {

    update_building_position(entity_id, center_x, center_z);
    return;
  }

  BuildingSize const size = get_building_size(building_type);
  register_building(entity_id, building_type, center_x, center_z, owner_id, size);
}

void BuildingCollisionRegistry::register_building(Engine::Core::EntityID entity_id,
                                                  const std::string& building_type,
                                                  float center_x,
                                                  float center_z,
                                                  int owner_id,
                                                  BuildingSize size) {

  if (m_entity_to_index.find(entity_id) != m_entity_to_index.end()) {

    update_building_position(entity_id, center_x, center_z);
    return;
  }

  BuildingFootprint const footprint(center_x,
                                    center_z,
                                    size.width,
                                    size.depth,
                                    owner_id,
                                    entity_id,
                                    get_building_grid_padding(building_type));

  m_buildings.push_back(footprint);
  m_entity_to_index[entity_id] = m_buildings.size() - 1;
  add_to_spatial_index(m_buildings.back());

  if (auto* pf = CommandService::get_pathfinder()) {

    pf->mark_building_region_dirty(center_x, center_z, size.width, size.depth);
  }
}

void BuildingCollisionRegistry::unregister_building(Engine::Core::EntityID entity_id) {
  auto it = m_entity_to_index.find(entity_id);
  if (it == m_entity_to_index.end()) {
    return;
  }

  size_t const index = it->second;

  remove_from_spatial_index(m_buildings[index]);

  float const center_x = m_buildings[index].center_x;
  float const center_z = m_buildings[index].center_z;
  float const width = m_buildings[index].width;
  float const depth = m_buildings[index].depth;

  if (index != m_buildings.size() - 1) {
    std::swap(m_buildings[index], m_buildings.back());

    m_entity_to_index[m_buildings[index].entity_id] = index;
  }

  m_buildings.pop_back();
  m_entity_to_index.erase(entity_id);

  release_authored_obstacles_within(center_x, center_z, width, depth);

  if (auto* pf = CommandService::get_pathfinder()) {

    pf->mark_building_region_dirty(center_x, center_z, width, depth);

    pf->mark_obstruction_released();
  }
}

void BuildingCollisionRegistry::release_authored_obstacles_within(float center_x,
                                                                  float center_z,
                                                                  float width,
                                                                  float depth) {

  float const half_width = width / 2.0F;
  float const half_depth = depth / 2.0F;

  std::erase_if(m_authored_obstacles, [&](const BuildingFootprint& obstacle) {
    return std::fabs(obstacle.center_x - center_x) <= half_width &&
           std::fabs(obstacle.center_z - center_z) <= half_depth;
  });
}

void BuildingCollisionRegistry::update_building_position(
    Engine::Core::EntityID entity_id, float center_x, float center_z) {
  auto it = m_entity_to_index.find(entity_id);
  if (it == m_entity_to_index.end()) {
    return;
  }

  size_t const index = it->second;

  float const old_x = m_buildings[index].center_x;
  float const old_z = m_buildings[index].center_z;
  float const width = m_buildings[index].width;
  float const depth = m_buildings[index].depth;

  remove_from_spatial_index(m_buildings[index]);
  m_buildings[index].center_x = center_x;
  m_buildings[index].center_z = center_z;
  add_to_spatial_index(m_buildings[index]);

  if (auto* pf = CommandService::get_pathfinder()) {

    pf->mark_building_region_dirty(old_x, old_z, width, depth);
    pf->mark_building_region_dirty(center_x, center_z, width, depth);
  }
}

void BuildingCollisionRegistry::resize_building(Engine::Core::EntityID entity_id,
                                                BuildingSize size) {
  auto it = m_entity_to_index.find(entity_id);
  if (it == m_entity_to_index.end()) {
    return;
  }

  size_t const index = it->second;
  if (m_buildings[index].width == size.width &&
      m_buildings[index].depth == size.depth) {
    return;
  }

  float const center_x = m_buildings[index].center_x;
  float const center_z = m_buildings[index].center_z;
  float const old_width = m_buildings[index].width;
  float const old_depth = m_buildings[index].depth;

  remove_from_spatial_index(m_buildings[index]);
  m_buildings[index].width = size.width;
  m_buildings[index].depth = size.depth;
  add_to_spatial_index(m_buildings[index]);

  if (auto* pf = CommandService::get_pathfinder()) {
    pf->mark_building_region_dirty(center_x, center_z, old_width, old_depth);
    pf->mark_building_region_dirty(center_x, center_z, size.width, size.depth);
  }
}

void BuildingCollisionRegistry::update_building_owner(Engine::Core::EntityID entity_id,
                                                      int owner_id) {
  auto it = m_entity_to_index.find(entity_id);
  if (it == m_entity_to_index.end()) {
    return;
  }

  size_t const index = it->second;
  m_buildings[index].owner_id = owner_id;
}

void BuildingCollisionRegistry::set_building_navigation_blocking(
    Engine::Core::EntityID entity_id, bool blocks_navigation) {
  auto it = m_entity_to_index.find(entity_id);
  if (it == m_entity_to_index.end()) {
    return;
  }

  auto& footprint = m_buildings[it->second];
  if (footprint.blocks_navigation == blocks_navigation) {
    return;
  }
  footprint.blocks_navigation = blocks_navigation;

  if (auto* pf = CommandService::get_pathfinder()) {
    pf->mark_building_region_dirty(
        footprint.center_x, footprint.center_z, footprint.width, footprint.depth);
  }
}

auto BuildingCollisionRegistry::find_building(Engine::Core::EntityID entity_id) const
    -> const BuildingFootprint* {
  auto it = m_entity_to_index.find(entity_id);
  return it == m_entity_to_index.end() ? nullptr : &m_buildings[it->second];
}

auto BuildingCollisionRegistry::is_point_in_building(
    float x, float z, Engine::Core::EntityID ignore_entity_id) const -> bool {
  for (const auto& building : m_buildings) {
    if (ignore_entity_id != 0 && building.entity_id == ignore_entity_id) {
      continue;
    }

    float const half_width = building.width / 2.0F;
    float const half_depth = building.depth / 2.0F;

    float const min_x = building.center_x - half_width;
    float const max_x = building.center_x + half_width;
    float const min_z = building.center_z - half_depth;
    float const max_z = building.center_z + half_depth;

    if (x >= min_x && x <= max_x && z >= min_z && z <= max_z) {
      return true;
    }
  }
  for (const auto& obstacle : m_authored_obstacles) {
    float const half_width = obstacle.width / 2.0F;
    float const half_depth = obstacle.depth / 2.0F;
    if (x >= obstacle.center_x - half_width && x <= obstacle.center_x + half_width &&
        z >= obstacle.center_z - half_depth && z <= obstacle.center_z + half_depth) {
      return true;
    }
  }

  return false;
}

void BuildingCollisionRegistry::set_authored_obstacles(
    std::vector<BuildingFootprint> obstacles) {
  m_authored_obstacles = std::move(obstacles);
}

void BuildingCollisionRegistry::clear_authored_obstacles() {
  m_authored_obstacles.clear();
}

namespace {

[[nodiscard]] auto circle_overlaps_footprint(const BuildingFootprint& building,
                                             float x,
                                             float z,
                                             float radius) -> bool {
  float const half_width = building.width / 2.0F;
  float const half_depth = building.depth / 2.0F;

  float const min_x = building.center_x - half_width;
  float const max_x = building.center_x + half_width;
  float const min_z = building.center_z - half_depth;
  float const max_z = building.center_z + half_depth;

  float const closest_x = std::clamp(x, min_x, max_x);
  float const closest_z = std::clamp(z, min_z, max_z);

  float const dx = x - closest_x;
  float const dz = z - closest_z;

  return (dx * dx) + (dz * dz) <= radius * radius;
}

} // namespace

auto BuildingCollisionRegistry::is_circle_overlapping_building(
    float x,
    float z,
    float radius,
    Engine::Core::EntityID ignore_entity_id) const -> bool {
  for (const auto& building : m_buildings) {
    if (ignore_entity_id != 0 && building.entity_id == ignore_entity_id) {
      continue;
    }
    if (circle_overlaps_footprint(building, x, z, radius)) {
      return true;
    }
  }

  for (const auto& obstacle : m_authored_obstacles) {
    if (circle_overlaps_footprint(obstacle, x, z, radius)) {
      return true;
    }
  }

  return false;
}

auto BuildingCollisionRegistry::get_rect_grid_cells(float center_x,
                                                    float center_z,
                                                    float width,
                                                    float depth,
                                                    float padding,
                                                    float grid_cell_size)
    -> std::vector<std::pair<int, int>> {
  std::vector<std::pair<int, int>> cells;

  float const half_width = width / 2.0F;
  float const half_depth = depth / 2.0F;

  int const min_grid_x =
      static_cast<int>(std::floor((center_x - half_width - padding) / grid_cell_size));
  int const max_grid_x =
      static_cast<int>(std::ceil((center_x + half_width + padding) / grid_cell_size));
  int const min_grid_z =
      static_cast<int>(std::floor((center_z - half_depth - padding) / grid_cell_size));
  int const max_grid_z =
      static_cast<int>(std::ceil((center_z + half_depth + padding) / grid_cell_size));

  for (int gx = min_grid_x; gx < max_grid_x; ++gx) {
    for (int gz = min_grid_z; gz < max_grid_z; ++gz) {
      cells.emplace_back(gx, gz);
    }
  }

  return cells;
}

auto BuildingCollisionRegistry::get_occupied_grid_cells(
    const BuildingFootprint& footprint,
    float grid_cell_size) -> std::vector<std::pair<int, int>> {
  return get_rect_grid_cells(footprint.center_x,
                             footprint.center_z,
                             footprint.width,
                             footprint.depth,
                             footprint.grid_padding,
                             grid_cell_size);
}

void BuildingCollisionRegistry::set_navigation_passages(
    std::vector<NavigationPassage> passages) {
  auto same_rect = [](const NavigationPassage& lhs, const NavigationPassage& rhs) {
    constexpr float k_epsilon = 1.0e-3F;
    return std::fabs(lhs.center_x - rhs.center_x) < k_epsilon &&
           std::fabs(lhs.center_z - rhs.center_z) < k_epsilon &&
           std::fabs(lhs.width - rhs.width) < k_epsilon &&
           std::fabs(lhs.depth - rhs.depth) < k_epsilon;
  };

  if (m_navigation_passages.size() == passages.size() &&
      std::equal(m_navigation_passages.begin(),
                 m_navigation_passages.end(),
                 passages.begin(),
                 same_rect)) {
    return;
  }

  auto* pathfinder = CommandService::get_pathfinder();
  auto mark_dirty = [pathfinder](const std::vector<NavigationPassage>& list) {
    if (pathfinder == nullptr) {
      return;
    }
    for (const auto& passage : list) {
      pathfinder->mark_building_region_dirty(
          passage.center_x, passage.center_z, passage.width, passage.depth);
    }
  };

  mark_dirty(m_navigation_passages);
  m_navigation_passages = std::move(passages);
  mark_dirty(m_navigation_passages);
}

void BuildingCollisionRegistry::clear() {
  m_buildings.clear();
  m_entity_to_index.clear();
  m_navigation_passages.clear();
  m_authored_obstacles.clear();
  m_spatial_buckets.clear();
  m_max_half_extent = 0.0F;

  if (auto* pf = CommandService::get_pathfinder()) {
    pf->mark_navigation_grid_dirty();
    pf->mark_obstruction_released();
  }
}

void BuildingCollisionRegistry::set_grid_padding(float padding) {
  s_grid_padding = padding;

  if (auto* pf = CommandService::get_pathfinder()) {
    pf->mark_navigation_grid_dirty();
  }
}

auto BuildingCollisionRegistry::get_grid_padding() -> float {
  return s_grid_padding;
}

} // namespace Game::Systems
