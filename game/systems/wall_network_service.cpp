#include "wall_network_service.h"

#include <QCoreApplication>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <string_view>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "../map/terrain_service.h"
#include "../units/spawn_type.h"
#include "../visuals/building_asset_key.h"
#include "building_collision_registry.h"
#include "gate_service.h"
#include "nav_grid.h"
#include "pathfinding.h"

namespace Game::Systems {

namespace {

using Engine::Core::BuildingComponent;
using Engine::Core::ConstructionPreviewComponent;
using Engine::Core::GateComponent;
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
constexpr std::string_view k_wall_gate_variant = "wall_gate";

auto is_excluded_from_wall_network(const Engine::Core::Entity* entity) -> bool {
  return entity == nullptr || entity->has_component<PendingRemovalComponent>() ||
         entity->has_component<ConstructionPreviewComponent>();
}

auto is_live_wall_entity(Engine::Core::Entity* entity,
                         bool include_construction_sites) -> bool {
  if (is_excluded_from_wall_network(entity)) {
    return false;
  }

  auto* wall = entity->get_component<WallSegmentComponent>();
  if (wall == nullptr) {
    return false;
  }

  if (const auto* unit = entity->get_component<UnitComponent>()) {
    return Game::Units::is_wall_network_spawn(unit->spawn_type) && unit->health > 0;
  }

  return include_construction_sites &&
         entity->get_component<WallConstructionSiteComponent>() != nullptr;
}

auto is_live_tower_socket_entity(Engine::Core::Entity* entity) -> bool {
  if (is_excluded_from_wall_network(entity) ||
      !entity->has_component<BuildingComponent>()) {
    return false;
  }

  const auto* unit = entity->get_component<UnitComponent>();
  return unit != nullptr && unit->spawn_type == Game::Units::SpawnType::DefenseTower &&
         unit->health > 0;
}

auto resolve_wall_network_owner_id(const Engine::Core::Entity* entity)
    -> std::optional<int> {
  if (entity == nullptr) {
    return std::nullopt;
  }

  if (const auto* unit = entity->get_component<UnitComponent>()) {
    return unit->owner_id;
  }
  if (const auto* site = entity->get_component<WallConstructionSiteComponent>()) {
    return site->owner_id;
  }
  return std::nullopt;
}

auto wall_network_cells(const Engine::Core::Entity* entity,
                        const WallGridPosition& center)
    -> std::vector<WallGridPosition> {
  const auto* transform =
      entity != nullptr ? entity->get_component<TransformComponent>() : nullptr;
  const bool is_gate =
      entity != nullptr &&
      (entity->has_component<GateComponent>() ||
       (entity->get_component<WallConstructionSiteComponent>() != nullptr &&
        entity->get_component<WallConstructionSiteComponent>()->product_type ==
            Game::Units::SpawnType::WallGate));
  if (!is_gate || transform == nullptr) {
    return {center};
  }

  constexpr int k_spacing = WallNetworkService::k_segment_spacing;
  if (GateComponent::spans_x_axis(transform->rotation.y)) {
    return {{center.x - k_spacing, center.z}, center, {center.x + k_spacing, center.z}};
  }
  return {{center.x, center.z - k_spacing}, center, {center.x, center.z + k_spacing}};
}

auto decode_key(std::uint64_t key) -> WallGridPosition {
  return {
      .x = static_cast<int>(static_cast<std::int32_t>(key >> 32U)),
      .z = static_cast<int>(static_cast<std::int32_t>(key & 0xFFFFFFFFU)),
  };
}

void add_owner_occupancy(WallNetworkService::OwnerOccupancyMap& out,
                         int owner_id,
                         int grid_x,
                         int grid_z) {
  if (owner_id <= 0) {
    return;
  }
  out[owner_id].insert(WallNetworkService::encode_key(grid_x, grid_z));
}

auto circle_overlaps_footprint(const BuildingFootprint& building,
                               float x,
                               float z,
                               float radius,
                               float* distance_sq_out = nullptr) -> bool {
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
  float const distance_sq = dx * dx + dz * dz;
  if (distance_sq_out != nullptr) {
    *distance_sq_out = distance_sq;
  }
  return distance_sq <= radius * radius;
}

auto entity_allows_wall_boundary_touch(const Engine::Core::Entity* entity) -> bool {
  if (entity == nullptr) {
    return false;
  }

  if (entity->get_component<WallConstructionSiteComponent>() != nullptr) {
    return true;
  }

  const auto* unit = entity->get_component<UnitComponent>();
  if (unit == nullptr) {
    return false;
  }

  return Game::Units::is_wall_network_spawn(unit->spawn_type) ||
         unit->spawn_type == Game::Units::SpawnType::DefenseTower;
}

auto placement_can_touch_wall_network_structures(const std::string& building_type)
    -> bool {
  return building_type == "wall_segment" || building_type == "wall_gate" ||
         building_type == "defense_tower";
}

auto collides_with_registered_building(Engine::Core::World& world,
                                       float pos_x,
                                       float pos_z,
                                       float radius,
                                       const std::string& building_type,
                                       Engine::Core::EntityID ignore_entity_id = 0)
    -> bool {
  auto& collision_registry = BuildingCollisionRegistry::instance();
  if (!placement_can_touch_wall_network_structures(building_type)) {
    return collision_registry.is_circle_overlapping_building(
        pos_x, pos_z, radius, ignore_entity_id);
  }

  static constexpr float k_touch_epsilon = 1.0e-4F;
  for (const auto& building : collision_registry.get_all_buildings()) {
    if (ignore_entity_id != 0 && building.entity_id == ignore_entity_id) {
      continue;
    }

    float distance_sq = 0.0F;
    if (!circle_overlaps_footprint(building, pos_x, pos_z, radius, &distance_sq)) {
      continue;
    }

    Engine::Core::Entity* entity = world.get_entity(building.entity_id);
    if (entity_allows_wall_boundary_touch(entity) &&
        distance_sq + k_touch_epsilon >= radius * radius) {
      continue;
    }
    return true;
  }

  return false;
}

auto is_buildable_world_position(float pos_x,
                                 float pos_z,
                                 Engine::Core::World& world,
                                 const std::string& building_type,
                                 Engine::Core::EntityID ignore_entity_id = 0) -> bool {
  auto size =
      Game::Systems::BuildingCollisionRegistry::get_building_size(building_type);

  if (collides_with_registered_building(world,
                                        pos_x,
                                        pos_z,
                                        std::max(size.width, size.depth) * 0.5F,
                                        building_type,
                                        ignore_entity_id)) {
    return false;
  }

  if (placement_can_touch_wall_network_structures(building_type)) {
    if (auto& terrain_service = Game::Map::TerrainService::instance();
        terrain_service.is_initialized()) {
      Game::Systems::Point const grid =
          Game::Systems::NavGrid::world_to_grid(pos_x, pos_z);
      return terrain_service.is_walkable(grid.x, grid.y);
    }
    return true;
  }

  Game::Systems::Pathfinding* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  if (pathfinder != nullptr) {
    Game::Systems::Point const grid =
        Game::Systems::NavGrid::world_to_grid(pos_x, pos_z);
    if (!pathfinder->is_walkable(grid.x, grid.y)) {
      return false;
    }
    if (auto& terrain_service = Game::Map::TerrainService::instance();
        terrain_service.is_initialized() &&
        !terrain_service.is_walkable(grid.x, grid.y)) {
      return false;
    }
  }

  return true;
}

auto is_wall_key_occupied(Engine::Core::World& world,
                          int grid_x,
                          int grid_z,
                          bool include_construction_sites,
                          Engine::Core::EntityID ignore_entity_id = 0) -> bool {
  auto entities = world.get_entities_with<WallSegmentComponent>();
  for (auto* entity : entities) {
    if (entity == nullptr || entity->get_id() == ignore_entity_id ||
        !is_live_wall_entity(entity, include_construction_sites)) {
      continue;
    }

    auto* transform = entity->get_component<TransformComponent>();
    auto* wall = entity->get_component<WallSegmentComponent>();
    if (transform == nullptr || wall == nullptr) {
      continue;
    }

    const auto snapped = WallNetworkService::snap_world_position(transform->position.x,
                                                                 transform->position.z);
    wall->grid_x = snapped.x;
    wall->grid_z = snapped.z;
    if (snapped.x == grid_x && snapped.z == grid_z) {
      return true;
    }
  }
  return false;
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
    if (has(Service::k_connection_north)) {
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
    if (has(Service::k_connection_west) && has(Service::k_connection_north)) {
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
  if (!has(Service::k_connection_south)) {
    return {k_wall_variant_tee, 90.0F};
  }
  if (!has(Service::k_connection_east)) {
    return {k_wall_variant_tee, 180.0F};
  }
  return {k_wall_variant_tee, -90.0F};
}

auto normalize_rotation_degrees(float angle) -> float {
  while (angle < 0.0F) {
    angle += 360.0F;
  }
  while (angle >= 360.0F) {
    angle -= 360.0F;
  }
  return angle;
}

auto preserved_isolated_rotation(float current_rotation_y) -> float {
  int const quarter_turns = static_cast<int>(
      std::round(normalize_rotation_degrees(current_rotation_y) / 90.0F));
  return (quarter_turns % 2) == 0 ? 0.0F : 90.0F;
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
  bool is_gate = entity->has_component<GateComponent>();
  if (const auto* unit = entity->get_component<UnitComponent>()) {
    nation_id = unit->nation_id;
  } else if (const auto* site =
                 entity->get_component<WallConstructionSiteComponent>()) {
    nation_id = site->nation_id;

    is_gate = site->product_type == Game::Units::SpawnType::WallGate;
  }

  const auto appearance =
      is_gate ? WallNetworkService::resolve_gate_appearance(
                    nation_id, connection_mask, transform->rotation.y)
              : WallNetworkService::resolve_appearance(nation_id, connection_mask);
  transform->rotation.y = (connection_mask == 0U && !is_gate)
                              ? preserved_isolated_rotation(transform->rotation.y)
                              : appearance.rotation_y;
  renderable->renderer_id = appearance.renderer_id;
  wall->connection_mask = connection_mask;

  if (is_gate && entity->has_component<GateComponent>()) {
    GateService::sync_gate_footprint(entity->get_id(), transform->rotation.y);
  }
}

} // namespace

auto WallNetworkService::snap_grid_coordinate(int grid_value) -> int {
  return static_cast<int>(std::round(static_cast<float>(grid_value) /
                                     static_cast<float>(k_segment_spacing))) *
         k_segment_spacing;
}

auto WallNetworkService::snap_world_position(float world_x,
                                             float world_z) -> WallGridPosition {
  const Point grid = NavGrid::world_to_grid(world_x, world_z);
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
    for (const auto& cell : wall_network_cells(entity, snapped)) {
      out.insert(encode_key(cell.x, cell.z));
    }
  }
}

void WallNetworkService::build_connection_occupancy(Engine::Core::World& world,
                                                    OwnerOccupancyMap& out,
                                                    bool include_construction_sites,
                                                    bool include_towers) {
  out.clear();

  auto wall_entities = world.get_entities_with<WallSegmentComponent>();
  for (auto* entity : wall_entities) {
    if (!is_live_wall_entity(entity, include_construction_sites)) {
      continue;
    }

    auto owner_id = resolve_wall_network_owner_id(entity);
    auto* transform = entity->get_component<TransformComponent>();
    auto* wall = entity->get_component<WallSegmentComponent>();
    if (!owner_id.has_value() || transform == nullptr || wall == nullptr) {
      continue;
    }

    const auto snapped =
        snap_world_position(transform->position.x, transform->position.z);
    wall->grid_x = snapped.x;
    wall->grid_z = snapped.z;
    for (const auto& cell : wall_network_cells(entity, snapped)) {
      add_owner_occupancy(out, *owner_id, cell.x, cell.z);
    }
  }

  if (!include_towers) {
    return;
  }

  auto unit_entities = world.get_entities_with<UnitComponent>();
  for (auto* entity : unit_entities) {
    if (!is_live_tower_socket_entity(entity)) {
      continue;
    }

    const auto* unit = entity->get_component<UnitComponent>();
    const auto* transform = entity->get_component<TransformComponent>();
    if (unit == nullptr || transform == nullptr) {
      continue;
    }

    const auto snapped =
        snap_world_position(transform->position.x, transform->position.z);
    add_owner_occupancy(out, unit->owner_id, snapped.x, snapped.z);
  }
}

auto WallNetworkService::compute_connection_mask(const OccupancySet& occupancy,
                                                 int grid_x,
                                                 int grid_z) -> std::uint8_t {
  static const OccupancySet k_nothing_ignored;
  return compute_connection_mask(occupancy, grid_x, grid_z, k_nothing_ignored);
}

auto WallNetworkService::compute_connection_mask(const OccupancySet& occupancy,
                                                 int grid_x,
                                                 int grid_z,
                                                 const OccupancySet& ignored)
    -> std::uint8_t {
  const auto joined = [&](int x, int z) {
    const auto key = encode_key(x, z);
    return ignored.find(key) == ignored.end() && occupancy.find(key) != occupancy.end();
  };

  std::uint8_t mask = 0;
  if (joined(grid_x, grid_z - k_segment_spacing)) {
    mask |= k_connection_north;
  }
  if (joined(grid_x + k_segment_spacing, grid_z)) {
    mask |= k_connection_east;
  }
  if (joined(grid_x, grid_z + k_segment_spacing)) {
    mask |= k_connection_south;
  }
  if (joined(grid_x - k_segment_spacing, grid_z)) {
    mask |= k_connection_west;
  }
  return mask;
}

auto WallNetworkService::validate_wall_segment_placement(
    Engine::Core::World& world,
    const WallGridPosition& position,
    bool include_construction_sites,
    Engine::Core::EntityID ignore_entity_id) -> WallPlacementValidation {
  if (is_wall_key_occupied(world,
                           position.x,
                           position.z,
                           include_construction_sites,
                           ignore_entity_id)) {
    return {.valid = false,
            .failure_reason = QCoreApplication::translate(
                                  "WallNetworkService", "Blocked by an existing wall.")
                                  .toStdString()};
  }

  const auto world_position = NavGrid::grid_to_world(Point{position.x, position.z});
  if (!is_buildable_world_position(world_position.x(),
                                   world_position.z(),
                                   world,
                                   "wall_segment",
                                   ignore_entity_id)) {
    return {.valid = false,
            .failure_reason =
                QCoreApplication::translate("WallNetworkService", "Cannot build there.")
                    .toStdString()};
  }

  return {.valid = true};
}

auto WallNetworkService::find_tower_snap_socket(Engine::Core::World& world,
                                                int owner_id,
                                                float world_x,
                                                float world_z,
                                                float max_snap_distance)
    -> std::optional<WallGridPosition> {
  if (owner_id <= 0 || max_snap_distance <= 0.0F) {
    return std::nullopt;
  }

  OwnerOccupancyMap connection_occupancy;
  build_connection_occupancy(world, connection_occupancy, true, true);

  const auto owner_it = connection_occupancy.find(owner_id);
  if (owner_it == connection_occupancy.end() || owner_it->second.empty()) {
    return std::nullopt;
  }

  OccupancySet candidate_keys;
  for (const auto key : owner_it->second) {
    const auto base = decode_key(key);
    candidate_keys.insert(encode_key(base.x, base.z - k_segment_spacing));
    candidate_keys.insert(encode_key(base.x + k_segment_spacing, base.z));
    candidate_keys.insert(encode_key(base.x, base.z + k_segment_spacing));
    candidate_keys.insert(encode_key(base.x - k_segment_spacing, base.z));
  }

  const float max_distance_sq = max_snap_distance * max_snap_distance;
  float best_distance_sq = max_distance_sq;
  std::optional<WallGridPosition> best_position;

  for (const auto key : candidate_keys) {
    if (owner_it->second.find(key) != owner_it->second.end()) {
      continue;
    }

    const auto candidate = decode_key(key);
    if (is_wall_key_occupied(world, candidate.x, candidate.z, true, 0)) {
      continue;
    }

    const auto candidate_world =
        NavGrid::grid_to_world(Point{candidate.x, candidate.z});
    if (!is_buildable_world_position(
            candidate_world.x(), candidate_world.z(), world, "defense_tower")) {
      continue;
    }

    const float dx = candidate_world.x() - world_x;
    const float dz = candidate_world.z() - world_z;
    const float distance_sq = dx * dx + dz * dz;
    if (distance_sq <= best_distance_sq) {
      best_distance_sq = distance_sq;
      best_position = candidate;
    }
  }

  return best_position;
}

auto WallNetworkService::resolve_gate_appearance(Game::Systems::NationID nation_id,
                                                 std::uint8_t mask,
                                                 float current_rotation_y)
    -> WallAppearance {
  const bool spans_east_west = (mask & (k_connection_east | k_connection_west)) != 0U;
  const bool spans_north_south =
      (mask & (k_connection_north | k_connection_south)) != 0U;

  float rotation_y = preserved_isolated_rotation(current_rotation_y);
  if (spans_east_west && !spans_north_south) {
    rotation_y = 0.0F;
  } else if (spans_north_south && !spans_east_west) {
    rotation_y = 90.0F;
  }

  return {
      .renderer_id = Game::Visuals::building_asset_key(nation_id, k_wall_gate_variant),
      .rotation_y = rotation_y,
      .connection_mask = mask,
  };
}

auto WallNetworkService::resolve_appearance(Game::Systems::NationID nation_id,
                                            std::uint8_t mask) -> WallAppearance {
  const auto [variant_name, rotation_y] = canonical_variant_for_mask(mask);
  return {
      .renderer_id = Game::Visuals::building_asset_key(nation_id, variant_name),
      .rotation_y = rotation_y,
      .connection_mask = mask,
  };
}

namespace {

auto blocking_building_covers(float world_x, float world_z) -> bool {
  for (const auto& building :
       BuildingCollisionRegistry::instance().get_all_buildings()) {
    if (!building.blocks_navigation) {
      continue;
    }
    const float half_width = building.width / 2.0F;
    const float half_depth = building.depth / 2.0F;
    if (world_x >= building.center_x - half_width &&
        world_x <= building.center_x + half_width &&
        world_z >= building.center_z - half_depth &&
        world_z <= building.center_z + half_depth) {
      return true;
    }
  }
  return false;
}

auto collect_navigation_passages(
    Engine::Core::World& world,
    const WallNetworkService::OwnerOccupancyMap& connection_occupancy)
    -> std::vector<NavigationPassage> {
  std::vector<NavigationPassage> passages;
  WallNetworkService::OccupancySet emitted;

  const float crossing_depth =
      2.0F + (2.0F * BuildingCollisionRegistry::get_grid_padding());

  const auto emit_passage = [&passages, &emitted, crossing_depth](
                                int grid_x, int grid_z, std::uint8_t mask) {
    if (!emitted.insert(WallNetworkService::encode_key(grid_x, grid_z)).second) {
      return;
    }

    const bool spans_east_west = (mask & (WallNetworkService::k_connection_east |
                                          WallNetworkService::k_connection_west)) != 0U;
    const bool spans_north_south =
        (mask & (WallNetworkService::k_connection_north |
                 WallNetworkService::k_connection_south)) != 0U;

    const auto center = NavGrid::grid_to_world(Point{grid_x, grid_z});
    passages.push_back(
        NavigationPassage{.center_x = center.x(),
                          .center_z = center.z(),
                          .width = spans_north_south ? crossing_depth : 2.0F,
                          .depth = spans_east_west ? crossing_depth : 2.0F});
  };

  for (auto* entity : world.get_entities_with<GateComponent>()) {
    if (!is_live_wall_entity(entity, false)) {
      continue;
    }
    const auto* wall = entity->get_component<WallSegmentComponent>();
    const auto* transform = entity->get_component<TransformComponent>();
    if (wall == nullptr || transform == nullptr) {
      continue;
    }

    if (!emitted.insert(WallNetworkService::encode_key(wall->grid_x, wall->grid_z))
             .second) {
      continue;
    }
    const auto lane =
        GateService::lane_center(transform->position.x, transform->position.z);
    const auto extent = GateService::lane_extent(transform->rotation.y);

    passages.push_back(NavigationPassage{.center_x = lane.x(),
                                         .center_z = lane.z(),
                                         .width = extent.half_x * 2.0F,
                                         .depth = extent.half_z * 2.0F,
                                         .source_entity_id = entity->get_id()});
  }

  constexpr int k_spacing = WallNetworkService::k_segment_spacing;
  const std::array<WallGridPosition, 4> k_offsets{WallGridPosition{0, -k_spacing},
                                                  WallGridPosition{k_spacing, 0},
                                                  WallGridPosition{0, k_spacing},
                                                  WallGridPosition{-k_spacing, 0}};

  for (const auto& [owner_id, occupancy] : connection_occupancy) {
    for (const auto key : occupancy) {
      const auto occupied = decode_key(key);
      for (const auto& offset : k_offsets) {
        const int gap_x = occupied.x + offset.x;
        const int gap_z = occupied.z + offset.z;
        if (occupancy.find(WallNetworkService::encode_key(gap_x, gap_z)) !=
            occupancy.end()) {
          continue;
        }

        const auto mask =
            WallNetworkService::compute_connection_mask(occupancy, gap_x, gap_z);
        const bool spans_east_west =
            (mask & WallNetworkService::k_connection_east) != 0U &&
            (mask & WallNetworkService::k_connection_west) != 0U;
        const bool spans_north_south =
            (mask & WallNetworkService::k_connection_north) != 0U &&
            (mask & WallNetworkService::k_connection_south) != 0U;
        if (!spans_east_west && !spans_north_south) {
          continue;
        }

        const auto center = NavGrid::grid_to_world(Point{gap_x, gap_z});
        if (blocking_building_covers(center.x(), center.z())) {
          continue;
        }
        emit_passage(gap_x, gap_z, mask);
      }
    }
  }

  return passages;
}

} // namespace

void WallNetworkService::refresh_world(Engine::Core::World& world) {
  OwnerOccupancyMap connection_occupancy;
  build_connection_occupancy(world, connection_occupancy, true, true);
  OccupancySet const empty_occupancy;

  auto entities = world.get_entities_with<WallSegmentComponent>();
  for (auto* entity : entities) {
    if (!is_live_wall_entity(entity, true)) {
      continue;
    }

    auto owner_id = resolve_wall_network_owner_id(entity);
    auto* wall = entity->get_component<WallSegmentComponent>();
    if (wall == nullptr) {
      continue;
    }

    const auto occupancy_it = owner_id.has_value()
                                  ? connection_occupancy.find(*owner_id)
                                  : connection_occupancy.end();
    const auto& occupancy = occupancy_it != connection_occupancy.end()
                                ? occupancy_it->second
                                : empty_occupancy;
    OccupancySet self_cells;
    for (const auto& cell :
         wall_network_cells(entity, {.x = wall->grid_x, .z = wall->grid_z})) {
      self_cells.insert(encode_key(cell.x, cell.z));
    }
    const auto mask =
        compute_connection_mask(occupancy, wall->grid_x, wall->grid_z, self_cells);
    update_wall_entity_visuals(entity, wall, mask);
  }

  BuildingCollisionRegistry::instance().set_navigation_passages(
      collect_navigation_passages(world, connection_occupancy));
}

} // namespace Game::Systems
