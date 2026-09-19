#include "production_system.h"

#include <QDebug>
#include <qvectornd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iterator>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "../core/ambient_session.h"
#include "../core/component_economy.h"
#include "../core/component_presentation.h"
#include "../core/death_sequence.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../game_config.h"
#include "../map/map_transformer.h"
#include "../map/terrain_service.h"
#include "../units/factory.h"
#include "../units/squad.h"
#include "../units/troop_config.h"
#include "build_site.h"
#include "builder_product_types.h"
#include "building_collision_registry.h"
#include "command_service.h"
#include "construction_cost_catalog.h"
#include "food_targets.h"
#include "harvest_yields.h"
#include "nation_registry.h"
#include "nav_grid.h"
#include "owner_queries.h"
#include "pathfinding.h"
#include "player_feedback.h"
#include "player_resource_registry.h"
#include "production_service.h"
#include "troop_profile_service.h"
#include "units/spawn_type.h"
#include "units/unit.h"
#include "wall_network_service.h"

namespace Game::Systems {

namespace {

void start_completion_effect(Engine::Core::World& world,
                             const Game::Units::Unit& unit,
                             Game::Units::SpawnType spawn_type) {
  auto* entity = world.get_entity(unit.id());
  if (entity == nullptr) {
    return;
  }
  auto* effect = entity->add_component<Engine::Core::ProductionCompletionComponent>();
  effect->radius = std::max(
      1.0F, Game::Units::TroopConfig::instance().get_selection_ring_size(spawn_type));
  if (entity->has_component<Engine::Core::BuildingComponent>()) {
    const auto size = BuildingCollisionRegistry::get_building_size(spawn_type);
    effect->radius =
        std::max(effect->radius, 0.6F * std::hypot(size.width, size.depth));
  }
}

void face_work_target(Engine::Core::TransformComponent& transform,
                      const Engine::Core::BuilderProductionComponent& builder) {
  float target_x = 0.0F;
  float target_z = 0.0F;
  if (builder.has_task_target || builder.structure_task_entity_id != 0) {
    target_x = builder.task_target_x;
    target_z = builder.task_target_z;
  } else if (builder.has_construction_site) {
    target_x = builder.construction_site_x;
    target_z = builder.construction_site_z;
  } else {
    return;
  }

  float const dx = target_x - transform.position.x;
  float const dz = target_z - transform.position.z;
  if ((dx * dx) + (dz * dz) < 0.01F) {
    return;
  }

  transform.desired_yaw = std::atan2(dx, dz) * 180.0F / std::numbers::pi_v<float>;
  transform.has_desired_yaw = true;
}

constexpr auto k_collect_stone_product_type = k_builder_product_collect_stone;
constexpr auto k_collect_iron_ore_product_type = k_builder_product_collect_iron_ore;

void apply_production_profile(Engine::Core::ProductionComponent* prod,
                              Game::Systems::NationID nation_id,
                              Game::Units::TroopType troop_type) {
  if (prod == nullptr) {
    return;
  }
  const auto profile =
      TroopProfileService::instance().get_profile(nation_id, troop_type);
  prod->build_time = profile.production.build_time;
  prod->villager_cost = profile.production.cost;
}

auto resolve_nation_id(const Engine::Core::World& world,
                       int owner_id) -> Game::Systems::NationID {
  auto& registry = *Game::Session::services_for(world).nations;
  if (const auto* nation = registry.get_nation_for_player(owner_id)) {
    return nation->id;
  }
  return registry.default_nation_id();
}

auto production_count_increment(const Engine::Core::UnitComponent* unit_comp,
                                int production_cost) -> int {
  if (unit_comp != nullptr && unit_comp->spawn_type == Game::Units::SpawnType::Home) {
    return 1;
  }
  return production_cost;
}

auto distance_to_site_edge(const Engine::Core::BuilderProductionComponent& builder,
                           float x,
                           float z) -> float {
  auto const size = BuildingCollisionRegistry::get_building_size(builder.product_type);
  float const yaw =
      builder.construction_site_rotation_y * std::numbers::pi_v<float> / 180.0F;
  float const cosine = std::cos(yaw);
  float const sine = std::sin(yaw);
  float const dx = x - builder.construction_site_x;
  float const dz = z - builder.construction_site_z;
  float const local_x = std::fabs((dx * cosine) - (dz * sine));
  float const local_z = std::fabs((dx * sine) + (dz * cosine));
  float const outside_x = std::max(0.0F, local_x - (size.width * 0.5F));
  float const outside_z = std::max(0.0F, local_z - (size.depth * 0.5F));
  return std::hypot(outside_x, outside_z);
}

auto find_guaranteed_valid_exit(float exit_x,
                                float exit_z,
                                float unit_radius) -> QVector3D {
  Point const exit_grid = NavGrid::world_to_grid(exit_x, exit_z);

  (void)unit_radius;
  if (NavGrid::is_grid_walkable(exit_grid)) {
    return {exit_x, 0.0F, exit_z};
  }

  constexpr int k_max_search_radius = 50;
  auto const safe_grid =
      NavGrid::find_nearest_walkable_grid(exit_grid, k_max_search_radius);
  if (safe_grid.has_value()) {
    return NavGrid::grid_to_world(*safe_grid);
  }

  return NavGrid::grid_to_world(exit_grid);
}

auto builder_exit_position(const Engine::Core::BuilderProductionComponent& builder,
                           const Engine::Core::MovementComponent& movement,
                           float unit_radius) -> QVector3D {
  float const center_x = builder.construction_site_x;
  float const center_z = builder.construction_site_z;
  auto const size = BuildingCollisionRegistry::get_building_size(builder.product_type);
  float const half_width = size.width * 0.5F;
  float const half_depth = size.depth * 0.5F;
  float const clearance = unit_radius + 0.25F;

  auto const exit_along = [&](float dir_x, float dir_z) {
    float const abs_x = std::fabs(dir_x);
    float const abs_z = std::fabs(dir_z);
    float const sx = abs_x > 0.0001F ? (half_width + clearance) / abs_x
                                     : std::numeric_limits<float>::infinity();
    float const sz = abs_z > 0.0001F ? (half_depth + clearance) / abs_z
                                     : std::numeric_limits<float>::infinity();
    float const scale = std::min(sx, sz);
    return QVector3D(center_x + dir_x * scale, 0.0F, center_z + dir_z * scale);
  };

  std::vector<QVector3D> candidates;
  if (builder.has_site_approach) {
    float dir_x = builder.site_approach_x - center_x;
    float dir_z = builder.site_approach_z - center_z;
    float const len = std::hypot(dir_x, dir_z);
    if (len > 0.0001F) {
      candidates.push_back(exit_along(dir_x / len, dir_z / len));
    }
  }
  for (auto const [dir_x, dir_z] : {std::pair{1.0F, 0.0F},
                                    std::pair{-1.0F, 0.0F},
                                    std::pair{0.0F, 1.0F},
                                    std::pair{0.0F, -1.0F}}) {
    candidates.push_back(exit_along(dir_x, dir_z));
  }

  auto* pathfinder = NavGrid::get_pathfinder();
  auto const passability = movement.get_can_enter_forest()
                               ? Pathfinding::Passability::Light
                               : Pathfinding::Passability::Heavy;
  std::uint32_t const home_region =
      pathfinder != nullptr && builder.has_site_approach
          ? pathfinder->region_of(NavGrid::world_to_grid(builder.site_approach_x,
                                                         builder.site_approach_z),
                                  passability)
          : Pathfinding::k_unreachable_region;
  if (home_region != Pathfinding::k_unreachable_region) {
    for (auto const& candidate : candidates) {
      Point const cell = NavGrid::world_to_grid(candidate.x(), candidate.z());
      if (pathfinder->region_of(cell, passability) == home_region) {
        return candidate;
      }
    }
  }
  for (auto const& candidate : candidates) {
    if (NavGrid::is_grid_walkable(
            NavGrid::world_to_grid(candidate.x(), candidate.z()))) {
      return candidate;
    }
  }
  return find_guaranteed_valid_exit(
      candidates.front().x(), candidates.front().z(), unit_radius);
}

constexpr float k_site_bypass_reach = 2.5F;
constexpr float k_site_route_goal_tolerance_sq = 0.25F;

auto site_bypass_radius_sq(const Engine::Core::BuilderProductionComponent& builder,
                           const Engine::Core::MovementComponent* movement) -> float {
  float radius = k_site_bypass_reach;
  if (movement != nullptr && is_gather_builder_product(builder.product_type)) {
    radius += std::max(0.0F, movement->get_navigation_clearance());
  }
  return radius * radius;
}

void reset_site_approach(Engine::Core::BuilderProductionComponent& builder) {
  builder.site_approach_seconds = 0.0F;
  builder.site_closest_approach = 0.0F;
}

void activate_bypass_movement(Engine::Core::BuilderProductionComponent* builder,
                              float target_x,
                              float target_z) {
  if (builder == nullptr) {
    return;
  }
  builder->bypass_movement_active = true;
  builder->bypass_target_x = target_x;
  builder->bypass_target_z = target_z;
}

auto walking_to_site(const Engine::Core::BuilderProductionComponent& builder,
                     const Engine::Core::MovementComponent& movement) -> bool {
  if (!movement.get_has_target()) {
    return false;
  }
  const float goal_x = movement.get_has_requested_goal()
                           ? movement.get_requested_goal_x()
                           : movement.get_goal_x();
  const float goal_z = movement.get_has_requested_goal()
                           ? movement.get_requested_goal_z()
                           : movement.get_goal_y();
  const float dx = goal_x - builder.construction_site_x;
  const float dz = goal_z - builder.construction_site_z;

  float const tolerance_sq = is_gather_builder_product(builder.product_type)
                                 ? site_bypass_radius_sq(builder, &movement)
                                 : k_site_route_goal_tolerance_sq;
  return (dx * dx + dz * dz) <= tolerance_sq;
}

void abandon_site_route(const Engine::Core::BuilderProductionComponent& builder,
                        Engine::Core::MovementComponent* movement) {
  if (movement != nullptr && walking_to_site(builder, *movement)) {
    movement->stop();
  }
}

auto needs_site_route(const Engine::Core::BuilderProductionComponent& builder,
                      const Engine::Core::MovementComponent* movement) -> bool {
  return movement != nullptr && !walking_to_site(builder, *movement);
}

void load_onto_hauler(
    Engine::Core::Entity* worker,
    ResourceType resource_type,
    int amount,
    Engine::Core::CarriedFoodForm food_form = Engine::Core::CarriedFoodForm::Grain) {
  if (worker == nullptr || amount <= 0) {
    return;
  }
  auto* carry =
      Engine::Core::get_or_add_component<Engine::Core::ResourceCarryComponent>(worker);
  if (carry == nullptr) {
    return;
  }

  if (resource_type == ResourceType::Food &&
      carry->amounts.get(ResourceType::Food) <= 0) {
    carry->food_form = food_form;
  }
  carry->amounts.add(resource_type, amount);
}

void clear_builder_task_target(Engine::Core::World& world,
                               Engine::Core::BuilderProductionComponent* builder,
                               bool release_tree = true) {
  if (builder == nullptr) {
    return;
  }
  if (release_tree && builder->task_target_reserved) {
    Game::Session::services_for(world).terrain->release_world_prop(
        builder->task_target_id);
  }
  builder->has_task_target = false;
  builder->task_target_id = 0;
  builder->task_target_x = 0.0F;
  builder->task_target_z = 0.0F;
  builder->task_target_reserved = false;
}

auto assign_next_wall_site(Engine::Core::World* world,
                           Engine::Core::Entity* builder_entity,
                           Engine::Core::BuilderProductionComponent* builder) -> bool {
  if (world == nullptr || builder_entity == nullptr || builder == nullptr) {
    return false;
  }

  while (!builder->queued_construction_site_ids.empty()) {
    const auto site_id = builder->queued_construction_site_ids.front();
    builder->queued_construction_site_ids.erase(
        builder->queued_construction_site_ids.begin());
    auto* site_entity = world->get_entity(site_id);
    auto* site_transform =
        site_entity != nullptr
            ? world->try_get<Engine::Core::TransformComponent>(site_entity->get_id())
            : nullptr;
    auto* site = site_entity != nullptr
                     ? world->try_get<Engine::Core::WallConstructionSiteComponent>(
                           site_entity->get_id())
                     : nullptr;
    if (site_transform == nullptr || site == nullptr) {
      continue;
    }

    builder->construction_site_entity_id = site_id;
    builder->has_construction_site = true;
    builder->construction_site_x = site_transform->position.x;
    builder->construction_site_z = site_transform->position.z;
    builder->at_construction_site = false;
    builder->in_progress = false;
    builder->build_time = site->build_time;
    builder->time_remaining = site->build_time;
    builder->construction_complete = false;
    builder->bypass_movement_active = false;

    if (auto* movement =
            world->try_get<Engine::Core::MovementComponent>(builder_entity->get_id())) {
      movement->set_rest_position(builder->construction_site_x,
                                  builder->construction_site_z);
    }
    return true;
  }

  builder->construction_site_entity_id = 0;
  builder->has_construction_site = false;
  builder->at_construction_site = false;
  builder->in_progress = false;
  builder->time_remaining = 0.0F;
  return false;
}

auto skip_invalid_wall_site(Engine::Core::World* world,
                            Engine::Core::Entity* builder_entity,
                            Engine::Core::BuilderProductionComponent* builder) -> bool {
  if (world == nullptr || builder_entity == nullptr || builder == nullptr ||
      builder->construction_site_entity_id == 0) {
    return false;
  }

  auto* site_entity = world->get_entity(builder->construction_site_entity_id);
  auto* site = site_entity != nullptr
                   ? world->try_get<Engine::Core::WallConstructionSiteComponent>(
                         site_entity->get_id())
                   : nullptr;
  auto* transform =
      site_entity != nullptr
          ? world->try_get<Engine::Core::TransformComponent>(site_entity->get_id())
          : nullptr;
  auto* wall =
      site_entity != nullptr
          ? world->try_get<Engine::Core::WallSegmentComponent>(site_entity->get_id())
          : nullptr;
  if (site_entity == nullptr || site == nullptr || transform == nullptr) {
    return false;
  }

  Game::Systems::WallGridPosition const position =
      wall != nullptr ? Game::Systems::WallGridPosition{wall->grid_x, wall->grid_z}
                      : Game::Systems::WallNetworkService::snap_world_position(
                            transform->position.x, transform->position.z);
  const auto validation =
      Game::Systems::WallNetworkService::validate_wall_segment_placement(
          *world,
          position,
          Game::Systems::wall_ground_probe(*world),
          true,
          site_entity->get_id());
  if (validation.valid) {
    return false;
  }

  const auto refund =
      construction_cost_info(Game::Units::spawn_typeToString(site->product_type))
          .resource_costs;
  grant_resources_at(site->owner_id,
                     transform->position.x,
                     transform->position.y,
                     transform->position.z,
                     refund);
  world->destroy_entity(site_entity->get_id());
  builder->construction_site_entity_id = 0;
  builder->has_construction_site = false;
  builder->at_construction_site = false;
  builder->in_progress = false;
  builder->time_remaining = 0.0F;
  builder->construction_complete = false;
  builder->bypass_movement_active = false;
  clear_builder_task_target(*world, builder, false);
  builder->report_fault(Engine::Core::BuilderTaskFault::TargetLost);
  WallNetworkService::refresh_world(*world);
  assign_next_wall_site(world, builder_entity, builder);
  return true;
}

auto is_wall_network_product(const std::string& product_type) -> bool {
  return is_wall_builder_product(product_type);
}

constexpr float k_repair_fraction_per_tick = 0.06F;
constexpr int k_repair_minimum_per_tick = 8;

auto repair_target_of(Engine::Core::World* world,
                      const Engine::Core::BuilderProductionComponent* builder)
    -> Engine::Core::Entity* {
  if (world == nullptr || builder == nullptr ||
      builder->structure_task_entity_id == 0) {
    return nullptr;
  }
  return world->get_entity(builder->structure_task_entity_id);
}

auto structure_needs_repair(const Engine::Core::Entity* structure) -> bool {
  if (structure == nullptr) {
    return false;
  }
  const auto* unit = structure->get_component<Engine::Core::UnitComponent>();
  return unit != nullptr && unit->health > 0 && unit->health < unit->max_health;
}

auto apply_structure_repair_tick(Engine::Core::World* world,
                                 Engine::Core::BuilderProductionComponent* builder)
    -> bool {
  auto* structure = repair_target_of(world, builder);
  if (!structure_needs_repair(structure)) {
    return false;
  }

  auto* unit = structure->get_component<Engine::Core::UnitComponent>();
  const int per_tick = std::max(k_repair_minimum_per_tick,
                                static_cast<int>(static_cast<float>(unit->max_health) *
                                                 k_repair_fraction_per_tick));
  restore_health(
      unit->owner_id, structure->get_id(), *unit, per_tick, unit->max_health);

  if (unit->health >= unit->max_health) {
    if (auto* fire = structure->get_component<Engine::Core::StructureFireComponent>()) {
      fire->remaining_duration = 0.0F;
      fire->ignition_progress = 0.0F;
      fire->tick_accumulator = 0.0F;
    }
    return false;
  }
  return true;
}

auto food_target_position(Engine::Core::World* world,
                          const Engine::Core::BuilderProductionComponent* builder)
    -> std::optional<QVector3D> {
  if (world == nullptr || builder == nullptr ||
      builder->structure_task_entity_id == 0) {
    return std::nullopt;
  }
  auto* target = world->get_entity(builder->structure_task_entity_id);
  const auto* transform =
      target != nullptr
          ? world->try_get<Engine::Core::TransformComponent>(target->get_id())
          : nullptr;
  if (transform == nullptr) {
    return std::nullopt;
  }
  return QVector3D(transform->position.x, 0.0F, transform->position.z);
}

auto food_target_still_valid(Engine::Core::World* world,
                             const Engine::Core::Entity* worker,
                             const Engine::Core::BuilderProductionComponent* builder)
    -> bool {
  if (world == nullptr || worker == nullptr || builder == nullptr) {
    return false;
  }
  auto* target = world->get_entity(builder->structure_task_entity_id);
  if (target == nullptr) {
    return false;
  }
  if (builder->product_type == k_builder_product_harvest_grain) {
    const auto* unit = world->try_get<Engine::Core::UnitComponent>(worker->get_id());
    return unit != nullptr && farm_is_harvestable(*target, unit->owner_id);
  }
  return sheep_is_slaughterable(*target);
}

void abandon_food_task(Engine::Core::World& world,
                       Engine::Core::BuilderProductionComponent* builder,
                       Engine::Core::BuilderTaskFault fault) {
  builder->in_progress = false;
  builder->time_remaining = 0.0F;
  builder->construction_complete = false;
  builder->has_construction_site = false;
  builder->at_construction_site = false;
  builder->bypass_movement_active = false;
  builder->structure_task_entity_id = 0;
  clear_builder_task_target(world, builder, false);
  builder->report_fault(fault);
}

void hold_sheep_still(Engine::Core::World* world,
                      const Engine::Core::BuilderProductionComponent* builder) {
  auto* sheep = world->get_entity(builder->structure_task_entity_id);
  if (sheep == nullptr) {
    return;
  }
  if (auto* wildlife =
          world->try_get<Engine::Core::WildlifeComponent>(sheep->get_id())) {
    wildlife->held_timer = std::max(wildlife->held_timer, 0.75F);
  }
  if (auto* movement =
          world->try_get<Engine::Core::MovementComponent>(sheep->get_id())) {
    if (movement->get_has_target()) {
      movement->stop();
    }
  }
}

void slaughter_sheep(Engine::Core::World* world,
                     Engine::Core::Entity* worker,
                     Engine::Core::EntityID sheep_id) {
  auto* sheep = world->get_entity(sheep_id);
  auto* unit = sheep != nullptr
                   ? world->try_get<Engine::Core::UnitComponent>(sheep->get_id())
                   : nullptr;
  if (unit == nullptr) {
    return;
  }
  unit->health = 0;
  if (auto* movement =
          world->try_get<Engine::Core::MovementComponent>(sheep->get_id())) {
    movement->stop();
  }
  Engine::Core::begin_death_sequence(*sheep, 0U);
  const auto* worker_unit =
      worker != nullptr ? world->try_get<Engine::Core::UnitComponent>(worker->get_id())
                        : nullptr;
  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitDiedEvent(sheep_id,
                                  unit->owner_id,
                                  unit->spawn_type,
                                  worker != nullptr ? worker->get_id() : 0,
                                  worker_unit != nullptr ? worker_unit->owner_id : 0));
}

auto complete_food_harvest(Engine::Core::World* world,
                           Engine::Core::Entity* worker,
                           Engine::Core::BuilderProductionComponent* builder) -> bool {
  if (!food_target_still_valid(world, worker, builder)) {
    return false;
  }
  auto* target = world->get_entity(builder->structure_task_entity_id);
  int reward = 0;
  auto form = Engine::Core::CarriedFoodForm::Grain;
  if (builder->product_type == k_builder_product_harvest_grain) {
    auto* crop = world->try_get<Engine::Core::FarmComponent>(target->get_id());
    if (crop == nullptr) {
      return false;
    }
    crop->reset_after_harvest();
    reward = k_harvest_grain_food_reward;
  } else {
    slaughter_sheep(world, worker, target->get_id());
    reward = k_slaughter_sheep_food_reward;

    form = Engine::Core::CarriedFoodForm::Meat;
  }
  load_onto_hauler(worker, ResourceType::Food, reward, form);
  return true;
}

// A building order names every crew that will raise it. Those crews share one
// site: its progress, the hands that speed it up and the single building it
// ends in. Walls keep their own site entities and gathering is per crew.
auto raises_shared_site(const Engine::Core::BuilderProductionComponent& builder)
    -> bool {
  return builder.has_construction_site && builder.construction_site_entity_id == 0 &&
         !builder.product_type.empty() &&
         !is_wall_builder_product(builder.product_type) &&
         !is_gather_builder_product(builder.product_type) &&
         builder.product_type != k_builder_product_repair &&
         builder.product_type != k_builder_product_dismantle;
}

struct SharedSiteKey {
  int owner_id = 0;
  std::string product_type;
  float x = 0.0F;
  float z = 0.0F;
  float rotation_y = 0.0F;

  [[nodiscard]] auto operator==(const SharedSiteKey& other) const -> bool = default;
};

struct SharedSite {
  SharedSiteKey key;
  std::vector<Engine::Core::EntityID> crews;
};

auto crew_hands(const Engine::Core::World& world, Engine::Core::EntityID id) -> float {
  const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
  return unit != nullptr ? std::max(0.05F, Game::Units::squad_fraction(*unit)) : 1.0F;
}

auto site_progress(const Engine::Core::BuilderProductionComponent& builder) -> float {
  if (!builder.in_progress || builder.build_time <= 0.0F) {
    return 0.0F;
  }
  return std::clamp(1.0F - (builder.time_remaining / builder.build_time), 0.0F, 1.0F);
}

auto collect_shared_sites(Engine::Core::World& world) -> std::vector<SharedSite> {
  std::vector<SharedSite> sites;
  for (auto [entity_ref, builder] :
       world.entity_view<Engine::Core::BuilderProductionComponent>()) {
    if (!raises_shared_site(builder)) {
      continue;
    }
    const Engine::Core::EntityID id = entity_ref.get_id();
    const auto* unit = world.try_get<Engine::Core::UnitComponent>(id);
    SharedSiteKey key{.owner_id = unit != nullptr ? unit->owner_id : 0,
                      .product_type = builder.product_type,
                      .x = builder.construction_site_x,
                      .z = builder.construction_site_z,
                      .rotation_y = builder.construction_site_rotation_y};
    auto site = std::find_if(sites.begin(), sites.end(), [&key](const SharedSite& s) {
      return s.key == key;
    });
    if (site == sites.end()) {
      sites.push_back(SharedSite{.key = std::move(key), .crews = {}});
      site = std::prev(sites.end());
    }
    site->crews.push_back(id);
  }
  for (auto& site : sites) {
    std::sort(site.crews.begin(), site.crews.end());
  }
  return sites;
}

void release_helper_crew(Engine::Core::World& world, Engine::Core::EntityID crew) {
  auto* builder = world.try_get<Engine::Core::BuilderProductionComponent>(crew);
  auto* movement = world.try_get<Engine::Core::MovementComponent>(crew);
  if (builder == nullptr) {
    return;
  }
  if (movement != nullptr && builder->at_construction_site) {
    const QVector3D exit = builder_exit_position(
        *builder, *movement, CommandService::get_unit_radius(world, crew));
    activate_bypass_movement(builder, exit.x(), exit.z());
    movement->set_rest_position(exit.x(), exit.z());
  } else if (movement != nullptr) {
    abandon_site_route(*builder, movement);
  }
  builder->in_progress = false;
  builder->time_remaining = 0.0F;
  builder->construction_complete = true;
  builder->has_construction_site = false;
  builder->at_construction_site = false;
  reset_site_approach(*builder);
  clear_builder_task_target(world, builder, false);
}

using FinishedSites =
    std::vector<std::pair<Engine::Core::EntityID, std::vector<Engine::Core::EntityID>>>;

// Advances every shared site by the hands working on it and hands the
// finished site to exactly one crew. Returns, per finishing crew, everyone
// who stood on that site so the building may rise around them.
auto advance_shared_sites(Engine::Core::World& world,
                          float delta_time) -> FinishedSites {
  FinishedSites finishing;
  for (auto& site : collect_shared_sites(world)) {
    const float work = construction_build_time(site.key.product_type);
    if (work <= 0.0F) {
      continue;
    }

    float progress = 0.0F;
    float hands = 0.0F;
    Engine::Core::EntityID lead = 0;
    for (const auto crew : site.crews) {
      const auto& builder =
          *world.try_get<Engine::Core::BuilderProductionComponent>(crew);
      progress = std::max(progress, site_progress(builder));
      if (builder.at_construction_site && builder.in_progress) {
        hands += crew_hands(world, crew);
        if (lead == 0) {
          lead = crew;
        }
      }
    }
    if (lead == 0) {
      continue;
    }

    progress = std::min(1.0F, progress + (std::max(0.0F, delta_time) * hands / work));
    const float seconds_at_this_pace = work / hands;
    for (const auto crew : site.crews) {
      auto& builder = *world.try_get<Engine::Core::BuilderProductionComponent>(crew);
      builder.build_time = seconds_at_this_pace;
      builder.time_remaining = (1.0F - progress) * seconds_at_this_pace;
    }
    if (progress < 1.0F) {
      continue;
    }

    for (const auto crew : site.crews) {
      if (crew != lead) {
        release_helper_crew(world, crew);
      }
    }
    world.try_get<Engine::Core::BuilderProductionComponent>(lead)->time_remaining =
        0.0F;
    finishing.emplace_back(lead, site.crews);
  }
  return finishing;
}

} // namespace

void ProductionSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  for (auto* entity :
       world->collect_entities_with<Engine::Core::ProductionCompletionComponent>()) {
    auto* effect = entity->get_component<Engine::Core::ProductionCompletionComponent>();
    effect->remaining -= std::max(delta_time, 0.0F);
    if (effect->remaining <= 0.0F) {
      entity->remove_component<Engine::Core::ProductionCompletionComponent>();
    }
  }

  for (auto [entity_ref, prod_ref] :
       world->entity_view<Engine::Core::ProductionComponent>()) {
    Engine::Core::Entity* e = &entity_ref;
    auto* prod = &prod_ref;

    if (world->has<Engine::Core::DismantleSiteComponent>(e->get_id())) {
      continue;
    }

    auto* unit_comp = world->try_get<Engine::Core::UnitComponent>(e->get_id());
    if ((unit_comp != nullptr) && Game::Core::is_neutral_owner(unit_comp->owner_id)) {
      continue;
    }

    prod->reserve_short = unit_comp != nullptr && ProductionService::reserve_is_short(
                                                      *prod, unit_comp->spawn_type);

    if (!prod->in_progress) {
      continue;
    }

    const int owner_id = (unit_comp != nullptr) ? unit_comp->owner_id : -1;
    const auto nation_id = resolve_nation_id(*world, owner_id);
    const auto current_profile =
        TroopProfileService::instance().get_profile(nation_id, prod->product_type);
    int const production_cost = current_profile.production.cost;
    int const capacity_increment =
        production_count_increment(unit_comp, production_cost);

    if ((unit_comp != nullptr) &&
        (unit_comp->spawn_type == Game::Units::SpawnType::Home) &&
        (prod->produced_count + capacity_increment > prod->max_units)) {
      prod->in_progress = false;
      continue;
    }
    prod->time_remaining -= delta_time;
    if (prod->time_remaining <= 0.0F) {

      auto* t = world->try_get<Engine::Core::TransformComponent>(e->get_id());
      auto* u = world->try_get<Engine::Core::UnitComponent>(e->get_id());
      if ((t != nullptr) && (u != nullptr)) {

        int const current_troops = Game::Systems::troop_count_for(*world, u->owner_id);
        int const max_troops = Game::GameConfig::instance().get_max_troops_per_player();
        if (current_troops + std::max(1, current_profile.individuals_per_unit) >
            max_troops) {
          prod->in_progress = false;
          prod->time_remaining = 0.0F;
          continue;
        }

        float const exit_offset = 2.5F + 0.2F * float(prod->produced_count % 5);
        float const exit_angle = 0.5F * float(prod->produced_count % 8);
        QVector3D const raw_exit_pos =
            QVector3D(t->position.x + exit_offset * std::cos(exit_angle),
                      0.0F,
                      t->position.z + exit_offset * std::sin(exit_angle));

        auto reg = Game::Map::MapTransformer::get_factory_registry();
        if (reg) {
          Game::Units::SpawnParams sp;
          sp.player_id = u->owner_id;
          sp.spawn_type = Game::Units::spawn_typeFromTroopType(prod->product_type);
          sp.ai_controlled =
              world->has<Engine::Core::AIControlledComponent>(e->get_id());
          sp.nation_id = nation_id;
          sp.is_initial_spawn = false;

          float const unit_radius =
              Game::Units::TroopConfig::instance().get_selection_ring_size(
                  sp.spawn_type);
          QVector3D const safe_exit = find_guaranteed_valid_exit(
              raw_exit_pos.x(), raw_exit_pos.z(), unit_radius);
          sp.position = safe_exit;

          auto unit = reg->create(sp.spawn_type, *world, sp);

          if (unit && prod->rally_set) {
            CommandService::MoveOptions rally;
            rally.kind = MoveOrderKind::ScriptedMove;
            CommandService::move_unit(*world,
                                      unit->id(),
                                      QVector3D(prod->rally_x, 0.0F, prod->rally_z),
                                      rally);
          }
          if (unit) {
            start_completion_effect(*world, *unit, sp.spawn_type);
            Engine::Core::EventManager::instance().publish(
                Engine::Core::AudioCueEvent::for_owner(u->owner_id,
                                                       "build.unit_ready"));
          }
        }

        prod->produced_count += capacity_increment;
      }

      prod->in_progress = false;
      prod->time_remaining = 0.0F;

      if (!prod->production_queue.empty()) {
        prod->product_type = prod->production_queue.front();
        prod->production_queue.erase(prod->production_queue.begin());
        apply_production_profile(prod, nation_id, prod->product_type);
        prod->time_remaining = prod->build_time;
        prod->in_progress = true;
      }
    }
  }

  constexpr float k_site_arrival_distance_sq = 1.0F * 1.0F;

  constexpr float k_site_approach_limit_seconds = 30.0F;

  constexpr float k_orphaned_task_limit_seconds = 8.0F;
  constexpr float MAX_CONSTRUCTION_DISTANCE_SQ = 9.0F;

  const auto finishing_sites = advance_shared_sites(*world, delta_time);

  for (auto [entity_ref, builder_prod_ref] :
       world->entity_view<Engine::Core::BuilderProductionComponent>()) {
    Engine::Core::Entity* e = &entity_ref;
    auto* builder_prod = &builder_prod_ref;

    if (builder_prod->fault_display_remaining > 0.0F) {
      builder_prod->fault_display_remaining -= delta_time;
      if (builder_prod->fault_display_remaining <= 0.0F) {
        builder_prod->clear_fault();
      }
    }

    auto* transform = world->try_get<Engine::Core::TransformComponent>(e->get_id());
    auto* movement = world->try_get<Engine::Core::MovementComponent>(e->get_id());

    const auto* builder_unit = world->try_get<Engine::Core::UnitComponent>(e->get_id());
    const int builder_owner_id = (builder_unit != nullptr) ? builder_unit->owner_id : 0;

    if (is_wall_network_product(builder_prod->product_type) &&
        builder_prod->construction_site_entity_id != 0 &&
        world->get_entity(builder_prod->construction_site_entity_id) == nullptr) {
      builder_prod->construction_site_entity_id = 0;
      builder_prod->has_construction_site = false;
      builder_prod->at_construction_site = false;
      builder_prod->in_progress = false;
      builder_prod->time_remaining = 0.0F;
      builder_prod->report_fault(Engine::Core::BuilderTaskFault::TargetLost);
    }

    if (is_wall_network_product(builder_prod->product_type) &&
        !builder_prod->has_construction_site && !builder_prod->in_progress &&
        !builder_prod->queued_construction_site_ids.empty()) {
      assign_next_wall_site(world, e, builder_prod);
    }

    if (is_food_builder_product(builder_prod->product_type) &&
        builder_prod->structure_task_entity_id != 0 &&
        (builder_prod->has_construction_site || builder_prod->in_progress)) {
      if (!food_target_still_valid(world, e, builder_prod)) {
        abandon_food_task(
            *world, builder_prod, Engine::Core::BuilderTaskFault::TargetLost);
        continue;
      }
      if (builder_prod->product_type == k_builder_product_slaughter_sheep &&
          transform != nullptr) {
        auto const sheep_position = food_target_position(world, builder_prod);
        if (sheep_position.has_value()) {
          float const dx = sheep_position->x() - transform->position.x;
          float const dz = sheep_position->z() - transform->position.z;
          float const dist_sq = dx * dx + dz * dz;
          if (dist_sq <= k_sheep_work_reach * k_sheep_work_reach) {

            hold_sheep_still(world, builder_prod);
            if (!builder_prod->at_construction_site) {
              builder_prod->construction_site_x = sheep_position->x();
              builder_prod->construction_site_z = sheep_position->z();
              builder_prod->task_target_x = sheep_position->x();
              builder_prod->task_target_z = sheep_position->z();
            }
          } else {
            QVector3D const work_position = food_work_position(
                *world,
                e->get_id(),
                QVector3D(transform->position.x, 0.0F, transform->position.z),
                FoodTarget{.id = builder_prod->structure_task_entity_id,
                           .product_type = k_builder_product_slaughter_sheep,
                           .x = sheep_position->x(),
                           .z = sheep_position->z()});
            builder_prod->construction_site_x = work_position.x();
            builder_prod->construction_site_z = work_position.z();
            builder_prod->task_target_x = sheep_position->x();
            builder_prod->task_target_z = sheep_position->z();
            if (builder_prod->at_construction_site) {
              builder_prod->at_construction_site = false;
              builder_prod->in_progress = false;
              builder_prod->has_construction_site = true;
            }
            builder_prod->bypass_movement_active = false;
            if (movement != nullptr) {
              movement->set_rest_position(work_position.x(), work_position.z());
            }
          }
        }
      }
    }

    if (builder_prod->has_construction_site && !builder_prod->at_construction_site) {
      if (transform != nullptr) {
        float const dx = builder_prod->construction_site_x - transform->position.x;
        float const dz = builder_prod->construction_site_z - transform->position.z;
        float const dist_sq = dx * dx + dz * dz;

        const float arrival_sq = k_site_arrival_distance_sq;
        float const edge = distance_to_site_edge(
            *builder_prod, transform->position.x, transform->position.z);
        if (dist_sq < arrival_sq || (edge * edge) < arrival_sq) {

          builder_prod->at_construction_site = true;
          builder_prod->in_progress = true;
          builder_prod->bypass_movement_active = false;
          builder_prod->clear_fault();
          builder_prod->has_site_approach = true;
          builder_prod->site_approach_x = transform->position.x;
          builder_prod->site_approach_z = transform->position.z;
          Engine::Core::EventManager::instance().publish(
              Engine::Core::AudioCueEvent::for_owner(builder_owner_id,
                                                     "build.construction_started"));

          transform->position.x = builder_prod->construction_site_x;
          transform->position.z = builder_prod->construction_site_z;

          if (movement != nullptr) {
            movement->set_rest_position(builder_prod->construction_site_x,
                                        builder_prod->construction_site_z);
            movement->stop();
          }

          face_work_target(*transform, *builder_prod);
          reset_site_approach(*builder_prod);
        } else {

          constexpr float k_site_progress_epsilon = 0.75F;
          float const distance = std::sqrt(dist_sq);

          auto const* facts =
              world->try_get<Engine::Core::MovementFactsComponent>(e->get_id());
          bool const on_route = movement != nullptr && movement->get_has_target() &&
                                facts != nullptr &&
                                facts->progress.remaining_arclength > 0.0F;
          float const approach =
              on_route ? facts->progress.remaining_arclength : distance;
          if (builder_prod->site_closest_approach <= 0.0F ||
              approach <
                  builder_prod->site_closest_approach - k_site_progress_epsilon) {
            builder_prod->site_closest_approach = approach;
            builder_prod->site_approach_seconds = 0.0F;
          }
          builder_prod->site_approach_seconds += delta_time;

          if (dist_sq > site_bypass_radius_sq(*builder_prod, movement)) {
            builder_prod->bypass_movement_active = false;
            if (needs_site_route(*builder_prod, movement)) {
              CommandService::move_unit(
                  *world,
                  e->get_id(),
                  QVector3D(builder_prod->construction_site_x,
                            0.0F,
                            builder_prod->construction_site_z),
                  CommandService::MoveOptions{.kind = MoveOrderKind::RecoveryMove,
                                              .preserve_formation_mode = true});
            }
          } else if (!builder_prod->bypass_movement_active) {
            activate_bypass_movement(builder_prod,
                                     builder_prod->construction_site_x,
                                     builder_prod->construction_site_z);
          }

          if (builder_prod->site_approach_seconds > k_site_approach_limit_seconds) {
            if (qEnvironmentVariableIsSet("SOI_BUILD_TRACE")) {
              qWarning() << "BUILDTRACE p" << builder_owner_id << "gave up reaching"
                         << builder_prod->product_type.c_str() << "site at"
                         << builder_prod->construction_site_x
                         << builder_prod->construction_site_z << "from"
                         << transform->position.x << transform->position.z << "still"
                         << std::sqrt(dist_sq) << "m out bypass"
                         << builder_prod->bypass_movement_active << "routed"
                         << (movement != nullptr && movement->get_has_target());
            }
            abandon_site_route(*builder_prod, movement);
            builder_prod->has_construction_site = false;
            builder_prod->at_construction_site = false;
            builder_prod->in_progress = false;
            builder_prod->bypass_movement_active = false;
            reset_site_approach(*builder_prod);
            clear_builder_task_target(*world, builder_prod);
            builder_prod->report_fault(Engine::Core::BuilderTaskFault::Unreachable);
          }
        }
      }
      continue;
    }

    if (!builder_prod->in_progress) {

      if (builder_prod->has_task_target && !builder_prod->has_construction_site) {
        builder_prod->site_approach_seconds += delta_time;
        if (builder_prod->site_approach_seconds > k_orphaned_task_limit_seconds) {
          clear_builder_task_target(*world, builder_prod);
          reset_site_approach(*builder_prod);
          builder_prod->report_fault(Engine::Core::BuilderTaskFault::TargetLost);
        }
      } else if (!builder_prod->has_construction_site) {
        reset_site_approach(*builder_prod);
      }
      continue;
    }

    if (builder_prod->at_construction_site && transform != nullptr) {
      float const dx = builder_prod->construction_site_x - transform->position.x;
      float const dz = builder_prod->construction_site_z - transform->position.z;
      float const dist_sq = dx * dx + dz * dz;

      if (dist_sq > MAX_CONSTRUCTION_DISTANCE_SQ) {
        builder_prod->has_construction_site = false;
        builder_prod->at_construction_site = false;
        builder_prod->in_progress = false;
        builder_prod->construction_complete = false;
        builder_prod->time_remaining = 0.0F;
        clear_builder_task_target(*world, builder_prod);
        builder_prod->report_fault(Engine::Core::BuilderTaskFault::Interrupted);
        continue;
      }
    }

    if (!raises_shared_site(*builder_prod)) {
      builder_prod->time_remaining -= delta_time;
    }
    if (is_wall_network_product(builder_prod->product_type) &&
        builder_prod->construction_site_entity_id != 0) {
      if (auto* site_entity =
              world->get_entity(builder_prod->construction_site_entity_id)) {
        if (auto* site =
                site_entity
                    ->get_component<Engine::Core::WallConstructionSiteComponent>()) {
          const float duration = std::max(builder_prod->build_time, 0.001F);
          site->progress =
              std::clamp(1.0F - builder_prod->time_remaining / duration, 0.0F, 1.0F);
        }
      }
    }
    if (builder_prod->product_type == k_builder_product_dismantle) {
      if (builder_prod->structure_task_entity_id != 0 &&
          world->get_entity(builder_prod->structure_task_entity_id) != nullptr) {
        builder_prod->time_remaining = builder_prod->build_time;
        continue;
      }

      builder_prod->in_progress = false;
      builder_prod->time_remaining = 0.0F;
      builder_prod->construction_complete = true;
      builder_prod->has_construction_site = false;
      builder_prod->at_construction_site = false;
      builder_prod->structure_task_entity_id = 0;
      clear_builder_task_target(*world, builder_prod, false);
      continue;
    }

    if (builder_prod->time_remaining <= 0.0F &&
        builder_prod->product_type == k_builder_product_repair) {
      if (apply_structure_repair_tick(world, builder_prod)) {
        builder_prod->time_remaining = builder_prod->build_time;
        continue;
      }

      if (repair_target_of(world, builder_prod) == nullptr) {
        builder_prod->report_fault(Engine::Core::BuilderTaskFault::TargetLost);
      } else {
        Engine::Core::EventManager::instance().publish(
            Engine::Core::AudioCueEvent::for_owner(builder_owner_id,
                                                   "build.construction_complete"));
      }
      builder_prod->in_progress = false;
      builder_prod->time_remaining = 0.0F;
      builder_prod->construction_complete = true;
      builder_prod->has_construction_site = false;
      builder_prod->at_construction_site = false;
      builder_prod->structure_task_entity_id = 0;
      clear_builder_task_target(*world, builder_prod, false);
      continue;
    }

    if (builder_prod->time_remaining <= 0.0F) {

      auto* t = world->try_get<Engine::Core::TransformComponent>(e->get_id());
      auto* u = world->try_get<Engine::Core::UnitComponent>(e->get_id());
      if ((t != nullptr) && (u != nullptr)) {
        if (is_food_builder_product(builder_prod->product_type)) {
          float const anchor_x = builder_prod->task_target_x;
          float const anchor_z = builder_prod->task_target_z;
          if (complete_food_harvest(world, e, builder_prod)) {
            if (!world->has<Engine::Core::AIControlledComponent>(e->get_id())) {
              builder_prod->has_gather_order = true;
              builder_prod->gather_product_type = builder_prod->product_type;
              builder_prod->gather_anchor_x = anchor_x;
              builder_prod->gather_anchor_z = anchor_z;
            }
          } else {
            builder_prod->report_fault(Engine::Core::BuilderTaskFault::TargetLost);
          }
          builder_prod->structure_task_entity_id = 0;
        } else if (is_harvest_builder_product(builder_prod->product_type)) {
          bool const harvested =
              builder_prod->has_task_target &&
              Game::Session::services_for(*world).terrain->harvest_world_prop(
                  builder_prod->task_target_id);
          if (harvested) {
            ResourceType resource_type = ResourceType::Wood;
            int reward_amount = k_cut_tree_wood_reward;
            if (builder_prod->product_type == k_collect_stone_product_type) {
              resource_type = ResourceType::Stone;
              reward_amount = k_collect_stone_reward;
            } else if (builder_prod->product_type == k_collect_iron_ore_product_type) {
              resource_type = ResourceType::Iron;
              reward_amount = k_collect_iron_ore_reward;
            }
            load_onto_hauler(e, resource_type, reward_amount);

            if (!world->has<Engine::Core::AIControlledComponent>(e->get_id())) {
              builder_prod->has_gather_order = true;
              builder_prod->gather_product_type = builder_prod->product_type;
              builder_prod->gather_anchor_x = builder_prod->task_target_x;
              builder_prod->gather_anchor_z = builder_prod->task_target_z;
            }
            if (auto* pathfinder = NavGrid::get_pathfinder()) {
              Point const tree_grid = NavGrid::world_to_grid(
                  builder_prod->task_target_x, builder_prod->task_target_z);
              pathfinder->mark_region_dirty(
                  tree_grid.x - 1, tree_grid.x + 1, tree_grid.y - 1, tree_grid.y + 1);
            }
          } else {
            clear_builder_task_target(*world, builder_prod);
            builder_prod->report_fault(Engine::Core::BuilderTaskFault::TargetLost);
          }
        } else {
          auto reg = Game::Map::MapTransformer::get_factory_registry();
          if (reg) {
            Game::Units::SpawnParams sp;

            if (builder_prod->has_construction_site) {
              sp.position = QVector3D(builder_prod->construction_site_x,
                                      t->position.y,
                                      builder_prod->construction_site_z);
            } else {
              sp.position = QVector3D(t->position.x, t->position.y, t->position.z);
            }
            float construction_rotation_y = builder_prod->construction_site_rotation_y;
            Engine::Core::Entity* wall_site_entity = nullptr;
            if (is_wall_network_product(builder_prod->product_type) &&
                builder_prod->construction_site_entity_id != 0) {
              wall_site_entity =
                  world->get_entity(builder_prod->construction_site_entity_id);
              if (skip_invalid_wall_site(world, e, builder_prod)) {
                continue;
              }
              wall_site_entity =
                  world->get_entity(builder_prod->construction_site_entity_id);
              if (auto* site_transform =
                      wall_site_entity != nullptr
                          ? wall_site_entity
                                ->get_component<Engine::Core::TransformComponent>()
                          : nullptr) {
                sp.position = QVector3D(site_transform->position.x,
                                        site_transform->position.y,
                                        site_transform->position.z);
                construction_rotation_y = site_transform->rotation.y;
              }
            }
            sp.player_id = u->owner_id;
            sp.ai_controlled =
                world->has<Engine::Core::AIControlledComponent>(e->get_id());
            sp.nation_id = u->nation_id;
            sp.is_initial_spawn = false;
            sp.rotation_y = construction_rotation_y;

            if (builder_prod->product_type == "catapult") {
              sp.spawn_type = Game::Units::SpawnType::Catapult;
            } else if (builder_prod->product_type == "ballista") {
              sp.spawn_type = Game::Units::SpawnType::Ballista;
            } else if (builder_prod->product_type == "barracks") {
              sp.spawn_type = Game::Units::SpawnType::Barracks;
            } else if (builder_prod->product_type == "defense_tower") {
              sp.spawn_type = Game::Units::SpawnType::DefenseTower;
            } else if (builder_prod->product_type == "wall_gate") {
              sp.spawn_type = Game::Units::SpawnType::WallGate;
            } else if (is_wall_network_product(builder_prod->product_type)) {
              sp.spawn_type = Game::Units::SpawnType::WallSegment;
            } else if (builder_prod->product_type == "home") {
              sp.spawn_type = Game::Units::SpawnType::Home;
            } else if (builder_prod->product_type == "marketplace") {
              sp.spawn_type = Game::Units::SpawnType::Marketplace;
            } else if (builder_prod->product_type == "farm") {
              sp.spawn_type = Game::Units::SpawnType::Farm;
            } else if (builder_prod->product_type == "temple") {
              sp.spawn_type = Game::Units::SpawnType::Temple;
            } else {
              builder_prod->in_progress = false;
              builder_prod->time_remaining = 0.0F;
              builder_prod->has_construction_site = false;
              builder_prod->at_construction_site = false;
              clear_builder_task_target(*world, builder_prod, false);
              continue;
            }

            const bool free_standing_wall =
                is_wall_network_product(builder_prod->product_type) &&
                wall_site_entity == nullptr;
            if (!is_wall_network_product(builder_prod->product_type) ||
                free_standing_wall) {

              constexpr float k_finished_site_nudge = 5.0F;
              std::vector<Engine::Core::EntityID> finishing_crew{e->get_id()};
              for (const auto& [lead_id, crew] : finishing_sites) {
                if (lead_id == e->get_id()) {
                  finishing_crew = crew;
                }
              }
              const auto clear_site =
                  find_clear_site(*world,
                                  builder_prod->product_type,
                                  sp.position,
                                  free_standing_wall ? 0.0F : k_finished_site_nudge,
                                  construction_rotation_y,
                                  finishing_crew);
              if (!clear_site.has_value()) {
                if (qEnvironmentVariableIsSet("SOI_BUILD_TRACE")) {
                  qWarning() << "BUILDTRACE p" << u->owner_id << "finished site refused"
                             << builder_prod->product_type.c_str() << "at"
                             << sp.position.x() << sp.position.z() << "yaw"
                             << construction_rotation_y << "verdict"
                             << static_cast<int>(
                                    assess_ground(*world,
                                                  builder_prod->product_type,
                                                  sp.position.x(),
                                                  sp.position.z(),
                                                  0,
                                                  construction_rotation_y,
                                                  finishing_crew));
                }

                grant_resources_at(
                    u->owner_id,
                    sp.position.x(),
                    sp.position.y(),
                    sp.position.z(),
                    construction_cost_info(builder_prod->product_type).resource_costs);
                builder_prod->in_progress = false;
                builder_prod->time_remaining = 0.0F;
                builder_prod->has_construction_site = false;
                builder_prod->at_construction_site = false;
                clear_builder_task_target(*world, builder_prod, false);
                builder_prod->report_fault(Engine::Core::BuilderTaskFault::Unreachable);
                continue;
              }
              sp.position =
                  QVector3D(clear_site->x(), sp.position.y(), clear_site->z());
            }

            if (qEnvironmentVariableIsSet("SOI_BUILD_TRACE")) {
              qWarning() << "BUILDTRACE p" << u->owner_id << "raised"
                         << builder_prod->product_type.c_str() << "at"
                         << sp.position.x() << sp.position.z() << "yaw"
                         << sp.rotation_y;
            }
            if (auto completed = reg->create(sp.spawn_type, *world, sp)) {
              start_completion_effect(*world, *completed, sp.spawn_type);
            }

            if (is_wall_network_product(builder_prod->product_type) &&
                builder_prod->construction_site_entity_id != 0) {
              world->destroy_entity(builder_prod->construction_site_entity_id);
              builder_prod->construction_site_entity_id = 0;
              WallNetworkService::refresh_world(*world);
            }

            if (builder_prod->has_construction_site && movement != nullptr &&
                t != nullptr) {
              QVector3D const safe_exit = builder_exit_position(
                  *builder_prod,
                  *movement,
                  CommandService::get_unit_radius(*world, e->get_id()));

              activate_bypass_movement(builder_prod, safe_exit.x(), safe_exit.z());

              movement->set_rest_position(safe_exit.x(), safe_exit.z());
            }
          }
        }
      }

      builder_prod->in_progress = false;
      builder_prod->time_remaining = 0.0F;
      builder_prod->construction_complete = true;
      Engine::Core::EventManager::instance().publish(
          Engine::Core::AudioCueEvent::for_owner(builder_owner_id,
                                                 "build.construction_complete"));
      builder_prod->has_construction_site = false;
      builder_prod->at_construction_site = false;
      builder_prod->construction_site_entity_id = 0;
      clear_builder_task_target(*world, builder_prod, false);
    }
  }
}

} // namespace Game::Systems
