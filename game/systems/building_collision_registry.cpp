#include "building_collision_registry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/world.h"

namespace Game::Systems {

auto BuildingCollisionRegistry::instance() -> BuildingCollisionRegistry& {
  return *Game::Session::ambient_services().building_collision;
}

const std::map<std::string, BuildingCollisionRegistry::BuildingSize>
    BuildingCollisionRegistry::s_building_sizes = {
        {"barracks", {4.F, 4.F}},
        {"home", {4.3F, 4.4F}},
        {"marketplace", {5.5F, 5.5F}},
        {"temple", {6.2F, 4.4F}},
        {"farm", {13.6F, 13.6F}},
        {"wall_segment", {2.0F, 2.0F}},
        {"wall_gate", {2.0F, 2.0F}},

};

const std::map<std::string, BuildingCollisionRegistry::BuildingBody>
    BuildingCollisionRegistry::s_building_bodies = {
        {"barracks", {8.65F, 4.20F, 2.325F, 0.0F}},
        {"home", {2.36F, 2.42F, 0.0F, 0.03F}},
        {"marketplace", {2.80F, 2.80F, 0.0F, 0.0F}},
        {"temple", {3.17F, 2.32F, -0.175F, 0.0F}},
        {"farm", {1.98F, 2.04F, 0.0F, 0.03F}},
        {"defense_tower", {2.60F, 2.60F, 0.0F, 0.0F}},
        {"wall_segment", {2.02F, 0.76F, 0.0F, 0.0F}},
        {"wall_gate", {2.02F, 0.76F, 0.0F, 0.0F}},

};

float BuildingCollisionRegistry::s_grid_padding =
    BuildingCollisionRegistry::k_default_grid_padding;

namespace {

BuildingCollisionRegistry::RegionDirtyHook g_region_dirty_hook = nullptr;
BuildingCollisionRegistry::GridDirtyHook g_grid_dirty_hook = nullptr;
BuildingCollisionRegistry::ObstructionReleasedHook g_obstruction_released_hook =
    nullptr;

void announce_region_dirty(float center_x, float center_z, float width, float depth) {
  if (g_region_dirty_hook != nullptr) {
    g_region_dirty_hook(center_x, center_z, width, depth);
  }
}

void announce_grid_dirty() {
  if (g_grid_dirty_hook != nullptr) {
    g_grid_dirty_hook();
  }
}

void announce_obstruction_released(
    const BuildingCollisionRegistry::ObstructionRelease& release) {
  if (g_obstruction_released_hook != nullptr) {
    g_obstruction_released_hook(release);
  }
}

void announce_obstruction_released_at(float center_x, float center_z) {
  announce_obstruction_released(
      {.center_x = center_x, .center_z = center_z, .located = true});
}

void announce_obstruction_released_everywhere() {
  announce_obstruction_released({});
}

} // namespace

void BuildingCollisionRegistry::set_region_dirty_hook(RegionDirtyHook hook) {
  g_region_dirty_hook = hook;
}

void BuildingCollisionRegistry::set_grid_dirty_hook(GridDirtyHook hook) {
  g_grid_dirty_hook = hook;
}

void BuildingCollisionRegistry::set_obstruction_released_hook(
    ObstructionReleasedHook hook) {
  g_obstruction_released_hook = hook;
}

BuildingCollisionRegistry::BuildingCollisionRegistry() {

  Engine::Core::World::set_entity_destroyed_hook([](Engine::Core::EntityID id) {
    BuildingCollisionRegistry::instance().unregister_building(id);
  });
}

auto BuildingCollisionRegistry::get_building_size(const std::string& building_type)
    -> BuildingCollisionRegistry::BuildingSize {
  auto it = s_building_sizes.find(building_type);
  if (it != s_building_sizes.end()) {
    return it->second;
  }

  return {2.0F, 2.0F};
}

auto BuildingCollisionRegistry::get_building_body(const std::string& building_type)
    -> BuildingCollisionRegistry::BuildingBody {
  auto it = s_building_bodies.find(building_type);
  if (it != s_building_bodies.end()) {
    return it->second;
  }

  BuildingSize const nav = get_building_size(building_type);
  return {nav.width, nav.depth, 0.0F, 0.0F};
}

auto BuildingCollisionRegistry::rotate_body_offset(BuildingBody body,
                                                   float facing_degrees)
    -> BuildingCollisionRegistry::BuildingBody {
  constexpr float k_deg_to_rad = 3.14159265358979323846F / 180.0F;
  float const radians = facing_degrees * k_deg_to_rad;
  float const cosine = std::cos(radians);
  float const sine = std::sin(radians);
  BuildingSize const rotated =
      axis_aligned_size({body.width, body.depth}, facing_degrees);
  return {rotated.width,
          rotated.depth,
          (body.offset_x * cosine) + (body.offset_z * sine),
          (body.offset_z * cosine) - (body.offset_x * sine)};
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
                                                  int owner_id,
                                                  float facing_degrees) {

  if (m_entity_to_index.find(entity_id) != m_entity_to_index.end()) {

    update_building_position(entity_id, center_x, center_z);
    return;
  }

  BuildingSize const size =
      axis_aligned_size(get_building_size(building_type), facing_degrees);
  register_building(entity_id, building_type, center_x, center_z, owner_id, size);
  apply_building_body(entity_id, building_type, facing_degrees);
}

auto BuildingCollisionRegistry::axis_aligned_size(BuildingSize size,
                                                  float facing_degrees)
    -> BuildingCollisionRegistry::BuildingSize {
  constexpr float k_deg_to_rad = 3.14159265358979323846F / 180.0F;
  float const radians = facing_degrees * k_deg_to_rad;
  float const cosine = std::abs(std::cos(radians));
  float const sine = std::abs(std::sin(radians));
  return {(cosine * size.width) + (sine * size.depth),
          (sine * size.width) + (cosine * size.depth)};
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

  announce_region_dirty(center_x, center_z, size.width, size.depth);
}

void BuildingCollisionRegistry::apply_building_body(Engine::Core::EntityID entity_id,
                                                    const std::string& building_type,
                                                    float facing_degrees) {
  auto it = m_entity_to_index.find(entity_id);
  if (it == m_entity_to_index.end()) {
    return;
  }
  auto& footprint = m_buildings[it->second];
  BuildingBody const body =
      rotate_body_offset(get_building_body(building_type), facing_degrees);
  footprint.body_width = body.width;
  footprint.body_depth = body.depth;
  footprint.body_center_x = footprint.center_x + body.offset_x;
  footprint.body_center_z = footprint.center_z + body.offset_z;
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

  announce_region_dirty(center_x, center_z, width, depth);
  announce_obstruction_released_at(center_x, center_z);
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

  float const body_offset_x = m_buildings[index].body_center_x - old_x;
  float const body_offset_z = m_buildings[index].body_center_z - old_z;

  remove_from_spatial_index(m_buildings[index]);
  m_buildings[index].center_x = center_x;
  m_buildings[index].center_z = center_z;
  m_buildings[index].body_center_x = center_x + body_offset_x;
  m_buildings[index].body_center_z = center_z + body_offset_z;
  add_to_spatial_index(m_buildings[index]);

  announce_region_dirty(old_x, old_z, width, depth);
  announce_region_dirty(center_x, center_z, width, depth);
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

  announce_region_dirty(center_x, center_z, old_width, old_depth);
  announce_region_dirty(center_x, center_z, size.width, size.depth);
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

  announce_region_dirty(
      footprint.center_x, footprint.center_z, footprint.width, footprint.depth);
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

namespace {

[[nodiscard]] auto
point_inside_footprint(const BuildingFootprint& footprint, float x, float z) -> bool {
  float const half_width = footprint.width / 2.0F;
  float const half_depth = footprint.depth / 2.0F;
  return x >= footprint.center_x - half_width && x <= footprint.center_x + half_width &&
         z >= footprint.center_z - half_depth && z <= footprint.center_z + half_depth;
}

[[nodiscard]] auto
footprint_penetration(const BuildingFootprint& footprint, float x, float z) -> float {
  if (!point_inside_footprint(footprint, x, z)) {
    return 0.0F;
  }
  float const half_width = footprint.width / 2.0F;
  float const half_depth = footprint.depth / 2.0F;
  return std::min(half_width - std::abs(x - footprint.center_x),
                  half_depth - std::abs(z - footprint.center_z));
}

[[nodiscard]] auto footprint_overlaps_rect(const BuildingFootprint& footprint,
                                           float min_x,
                                           float max_x,
                                           float min_z,
                                           float max_z) -> bool {
  float const half_width = footprint.width / 2.0F;
  float const half_depth = footprint.depth / 2.0F;
  return footprint.center_x - half_width < max_x &&
         footprint.center_x + half_width > min_x &&
         footprint.center_z - half_depth < max_z &&
         footprint.center_z + half_depth > min_z;
}

[[nodiscard]] auto slab_overlap(float start,
                                float delta,
                                float min_bound,
                                float max_bound,
                                float& t_enter,
                                float& t_exit) -> bool {
  constexpr float k_epsilon = 1.0e-5F;
  if (std::abs(delta) <= k_epsilon) {
    return start >= min_bound && start <= max_bound;
  }

  float t0 = (min_bound - start) / delta;
  float t1 = (max_bound - start) / delta;
  if (t0 > t1) {
    std::swap(t0, t1);
  }
  t_enter = std::max(t_enter, t0);
  t_exit = std::min(t_exit, t1);
  return t_enter <= t_exit;
}

[[nodiscard]] auto segment_hits_footprint(const BuildingFootprint& footprint,
                                          float start_x,
                                          float start_z,
                                          float delta_x,
                                          float delta_z) -> bool {
  float const half_width = footprint.width / 2.0F;
  float const half_depth = footprint.depth / 2.0F;
  float t_enter = 0.0F;
  float t_exit = 1.0F;
  return slab_overlap(start_x,
                      delta_x,
                      footprint.center_x - half_width,
                      footprint.center_x + half_width,
                      t_enter,
                      t_exit) &&
         slab_overlap(start_z,
                      delta_z,
                      footprint.center_z - half_depth,
                      footprint.center_z + half_depth,
                      t_enter,
                      t_exit);
}

} // namespace

auto BuildingCollisionRegistry::is_point_in_blocking_building(float x,
                                                              float z) const -> bool {
  bool blocked = false;
  for_each_building_in_region(x, x, z, z, [&](const BuildingFootprint& footprint) {
    if (blocked || !footprint.blocks_navigation) {
      return;
    }
    blocked = point_inside_footprint(footprint, x, z);
  });
  if (blocked) {
    return true;
  }

  return std::any_of(m_authored_obstacles.begin(),
                     m_authored_obstacles.end(),
                     [x, z](const BuildingFootprint& obstacle) {
                       return point_inside_footprint(obstacle, x, z);
                     });
}

auto BuildingCollisionRegistry::is_rect_overlapping_blocking_building(
    float min_x,
    float max_x,
    float min_z,
    float max_z,
    Engine::Core::EntityID ignore_entity_id) const -> bool {
  bool overlapping = false;
  for_each_building_in_region(
      min_x, max_x, min_z, max_z, [&](const BuildingFootprint& footprint) {
        if (overlapping || !footprint.blocks_navigation ||
            (ignore_entity_id != 0 && footprint.entity_id == ignore_entity_id)) {
          return;
        }
        overlapping = footprint_overlaps_rect(footprint, min_x, max_x, min_z, max_z);
      });
  if (overlapping) {
    return true;
  }

  return std::any_of(m_authored_obstacles.begin(),
                     m_authored_obstacles.end(),
                     [&](const BuildingFootprint& obstacle) {
                       return footprint_overlaps_rect(
                           obstacle, min_x, max_x, min_z, max_z);
                     });
}

auto BuildingCollisionRegistry::blocking_penetration_depth(float x,
                                                           float z) const -> float {
  float depth = 0.0F;
  for_each_building_in_region(x, x, z, z, [&](const BuildingFootprint& footprint) {
    if (!footprint.blocks_navigation) {
      return;
    }
    depth = std::max(depth, footprint_penetration(footprint, x, z));
  });
  for (const auto& obstacle : m_authored_obstacles) {
    depth = std::max(depth, footprint_penetration(obstacle, x, z));
  }
  return depth;
}

auto BuildingCollisionRegistry::segment_crosses_blocking_building(
    float start_x, float start_z, float end_x, float end_z) const -> bool {
  float const delta_x = end_x - start_x;
  float const delta_z = end_z - start_z;

  auto separates = [&](const BuildingFootprint& footprint) {
    return !point_inside_footprint(footprint, start_x, start_z) &&
           !point_inside_footprint(footprint, end_x, end_z) &&
           segment_hits_footprint(footprint, start_x, start_z, delta_x, delta_z);
  };

  bool crossed = false;
  for_each_building_in_region(std::min(start_x, end_x),
                              std::max(start_x, end_x),
                              std::min(start_z, end_z),
                              std::max(start_z, end_z),
                              [&](const BuildingFootprint& footprint) {
                                if (crossed || !footprint.blocks_navigation) {
                                  return;
                                }
                                crossed = separates(footprint);
                              });
  if (crossed) {
    return true;
  }

  return std::any_of(
      m_authored_obstacles.begin(), m_authored_obstacles.end(), separates);
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

void BuildingCollisionRegistry::set_navigation_passages(
    std::vector<NavigationPassage> passages) {
  auto same_rect = [](const NavigationPassage& lhs, const NavigationPassage& rhs) {
    constexpr float k_epsilon = 1.0e-3F;
    return std::fabs(lhs.center_x - rhs.center_x) < k_epsilon &&
           std::fabs(lhs.center_z - rhs.center_z) < k_epsilon &&
           std::fabs(lhs.width - rhs.width) < k_epsilon &&
           std::fabs(lhs.depth - rhs.depth) < k_epsilon &&
           lhs.source_entity_id == rhs.source_entity_id;
  };

  if (m_navigation_passages.size() == passages.size() &&
      std::equal(m_navigation_passages.begin(),
                 m_navigation_passages.end(),
                 passages.begin(),
                 same_rect)) {
    return;
  }

  auto mark_dirty = [](const std::vector<NavigationPassage>& list) {
    for (const auto& passage : list) {
      announce_region_dirty(
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

  announce_grid_dirty();
  announce_obstruction_released_everywhere();
}

void BuildingCollisionRegistry::set_grid_padding(float padding) {
  s_grid_padding = padding;

  announce_grid_dirty();
}

auto BuildingCollisionRegistry::get_grid_padding() -> float {
  return s_grid_padding;
}

} // namespace Game::Systems
