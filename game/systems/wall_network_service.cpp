#include "wall_network_service.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string_view>
#include <vector>

#include "../../render/entity/building_render_common.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "command_service.h"
#include "pathfinding.h"

namespace Game::Systems {

namespace {

using Engine::Core::ConstructionPreviewComponent;
using Engine::Core::PendingRemovalComponent;
using Engine::Core::RenderableComponent;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Engine::Core::WallConstructionSiteComponent;
using Engine::Core::WallSegmentComponent;

constexpr std::string_view k_wall_variant_isolated = "wall_segment_isolated";
constexpr std::string_view k_wall_variant_end = "wall_segment_end";
constexpr std::string_view k_wall_variant_straight = "wall_segment_straight";
constexpr std::string_view k_wall_variant_corner = "wall_segment_corner";
constexpr std::string_view k_wall_variant_tee = "wall_segment_tee";
constexpr std::string_view k_wall_variant_cross = "wall_segment_cross";

auto is_live_wall_entity(Engine::Core::Entity* entity,
                         bool include_construction_sites) -> bool {
  if (entity == nullptr || entity->has_component<PendingRemovalComponent>() ||
      entity->has_component<ConstructionPreviewComponent>()) {
    return false;
  }

  auto* wall = entity->get_component<WallSegmentComponent>();
  if (wall == nullptr) {
    return false;
  }

  if (const auto* unit = entity->get_component<UnitComponent>()) {
    return unit->spawn_type == Game::Units::SpawnType::WallSegment && unit->health > 0;
  }

  return include_construction_sites &&
         entity->get_component<WallConstructionSiteComponent>() != nullptr;
}

auto canonical_variant_for_mask(std::uint8_t mask)
    -> std::pair<std::string_view, float> {
  using Service = WallNetworkService;

  auto has = [mask](std::uint8_t bit) {
    return (mask & bit) != 0U;
  };
  int const degree_count = static_cast<int>(has(Service::k_connection_north)) +
                           static_cast<int>(has(Service::k_connection_east)) +
                           static_cast<int>(has(Service::k_connection_south)) +
                           static_cast<int>(has(Service::k_connection_west));

  if (degree_count <= 0) {
    return {k_wall_variant_isolated, 0.0F};
  }

  if (degree_count == 4) {
    return {k_wall_variant_cross, 0.0F};
  }

  if (degree_count == 1) {
    if (has(Service::k_connection_east)) {
      return {k_wall_variant_end, 0.0F};
    }
    if (has(Service::k_connection_south)) {
      return {k_wall_variant_end, 90.0F};
    }
    if (has(Service::k_connection_west)) {
      return {k_wall_variant_end, 180.0F};
    }
    return {k_wall_variant_end, -90.0F};
  }

  if (degree_count == 2) {
    if (has(Service::k_connection_east) && has(Service::k_connection_west)) {
      return {k_wall_variant_straight, 0.0F};
    }
    if (has(Service::k_connection_north) && has(Service::k_connection_south)) {
      return {k_wall_variant_straight, 90.0F};
    }
    if (has(Service::k_connection_north) && has(Service::k_connection_east)) {
      return {k_wall_variant_corner, 0.0F};
    }
    if (has(Service::k_connection_east) && has(Service::k_connection_south)) {
      return {k_wall_variant_corner, 90.0F};
    }
    if (has(Service::k_connection_south) && has(Service::k_connection_west)) {
      return {k_wall_variant_corner, 180.0F};
    }
    return {k_wall_variant_corner, -90.0F};
  }

  if (!has(Service::k_connection_west)) {
    return {k_wall_variant_tee, 0.0F};
  }
  if (!has(Service::k_connection_north)) {
    return {k_wall_variant_tee, 90.0F};
  }
  if (!has(Service::k_connection_east)) {
    return {k_wall_variant_tee, 180.0F};
  }
  return {k_wall_variant_tee, -90.0F};
}

void update_wall_entity_visuals(Engine::Core::Entity* entity,
                                WallSegmentComponent* wall,
                                std::uint8_t connection_mask) {
  if (entity == nullptr || wall == nullptr) {
    return;
  }

  auto* transform = entity->get_component<TransformComponent>();
  auto* renderable = entity->get_component<RenderableComponent>();
  if (transform == nullptr || renderable == nullptr) {
    wall->connection_mask = connection_mask;
    return;
  }

  Game::Systems::NationID nation_id = Game::Systems::NationID::RomanRepublic;
  if (const auto* unit = entity->get_component<UnitComponent>()) {
    nation_id = unit->nation_id;
  } else if (const auto* site =
                 entity->get_component<WallConstructionSiteComponent>()) {
    nation_id = site->nation_id;
  }

  const auto appearance =
      WallNetworkService::resolve_appearance(nation_id, connection_mask);
  transform->rotation.y = appearance.rotation_y;
  renderable->renderer_id = appearance.renderer_id;
  wall->connection_mask = connection_mask;
}

} // namespace

auto WallNetworkService::snap_grid_coordinate(int grid_value) -> int {
  return static_cast<int>(std::round(static_cast<float>(grid_value) /
                                     static_cast<float>(k_segment_spacing))) *
         k_segment_spacing;
}

auto WallNetworkService::snap_world_position(float world_x,
                                             float world_z) -> WallGridPosition {
  const Point grid = CommandService::world_to_grid(world_x, world_z);
  return {.x = snap_grid_coordinate(grid.x), .z = snap_grid_coordinate(grid.y)};
}

auto WallNetworkService::build_axis_aligned_chain(const WallGridPosition& start,
                                                  const WallGridPosition& end)
    -> std::vector<WallGridPosition> {
  std::vector<WallGridPosition> chain;

  const int dx = end.x - start.x;
  const int dz = end.z - start.z;
  const bool horizontal = std::abs(dx) >= std::abs(dz);

  if (horizontal) {
    const int final_x = end.x;
    const int step = final_x >= start.x ? k_segment_spacing : -k_segment_spacing;
    for (int x = start.x;; x += step) {
      chain.push_back({.x = x, .z = start.z});
      if (x == final_x) {
        break;
      }
    }
    return chain;
  }

  const int final_z = end.z;
  const int step = final_z >= start.z ? k_segment_spacing : -k_segment_spacing;
  for (int z = start.z;; z += step) {
    chain.push_back({.x = start.x, .z = z});
    if (z == final_z) {
      break;
    }
  }
  return chain;
}

auto WallNetworkService::encode_key(int grid_x, int grid_z) -> std::uint64_t {
  return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(grid_x)) << 32U) |
         static_cast<std::uint32_t>(grid_z);
}

void WallNetworkService::add_world_occupancy(Engine::Core::World& world,
                                             OccupancySet& out,
                                             bool include_construction_sites) {
  auto entities = world.get_entities_with<WallSegmentComponent>();
  for (auto* entity : entities) {
    if (!is_live_wall_entity(entity, include_construction_sites)) {
      continue;
    }

    auto* transform = entity->get_component<TransformComponent>();
    auto* wall = entity->get_component<WallSegmentComponent>();
    if (transform == nullptr || wall == nullptr) {
      continue;
    }

    const auto snapped =
        snap_world_position(transform->position.x, transform->position.z);
    wall->grid_x = snapped.x;
    wall->grid_z = snapped.z;
    out.insert(encode_key(snapped.x, snapped.z));
  }
}

auto WallNetworkService::compute_connection_mask(const OccupancySet& occupancy,
                                                 int grid_x,
                                                 int grid_z) -> std::uint8_t {
  std::uint8_t mask = 0;
  if (occupancy.find(encode_key(grid_x, grid_z - k_segment_spacing)) !=
      occupancy.end()) {
    mask |= k_connection_north;
  }
  if (occupancy.find(encode_key(grid_x + k_segment_spacing, grid_z)) !=
      occupancy.end()) {
    mask |= k_connection_east;
  }
  if (occupancy.find(encode_key(grid_x, grid_z + k_segment_spacing)) !=
      occupancy.end()) {
    mask |= k_connection_south;
  }
  if (occupancy.find(encode_key(grid_x - k_segment_spacing, grid_z)) !=
      occupancy.end()) {
    mask |= k_connection_west;
  }
  return mask;
}

auto WallNetworkService::resolve_appearance(Game::Systems::NationID nation_id,
                                            std::uint8_t mask) -> WallAppearance {
  const auto [variant_name, rotation_y] = canonical_variant_for_mask(mask);
  return {
      .renderer_id = Render::GL::building_renderer_key(nation_id, variant_name),
      .rotation_y = rotation_y,
      .connection_mask = mask,
  };
}

void WallNetworkService::refresh_world(Engine::Core::World& world) {
  OccupancySet occupancy;
  add_world_occupancy(world, occupancy, true);

  auto entities = world.get_entities_with<WallSegmentComponent>();
  for (auto* entity : entities) {
    if (!is_live_wall_entity(entity, true)) {
      continue;
    }

    auto* wall = entity->get_component<WallSegmentComponent>();
    if (wall == nullptr) {
      continue;
    }

    const auto mask = compute_connection_mask(occupancy, wall->grid_x, wall->grid_z);
    update_wall_entity_visuals(entity, wall, mask);
  }
}

} // namespace Game::Systems
