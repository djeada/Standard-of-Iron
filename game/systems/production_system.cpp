#include "production_system.h"

#include <QDebug>
#include <qvectornd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

#include "../core/ambient_session.h"
#include "../core/component.h"
#include "../core/event_manager.h"
#include "../core/ownership_constants.h"
#include "../core/world.h"
#include "../game_config.h"
#include "../map/map_transformer.h"
#include "../map/terrain_service.h"
#include "../units/factory.h"
#include "../units/troop_config.h"
#include "build_site.h"
#include "builder_product_types.h"
#include "building_collision_registry.h"
#include "command_service.h"
#include "construction_cost_catalog.h"
#include "economy_feedback.h"
#include "food_targets.h"
#include "harvest_yields.h"
#include "nation_registry.h"
#include "nav_grid.h"
#include "owner_queries.h"
#include "pathfinding.h"
#include "player_resource_registry.h"
#include "troop_profile_service.h"
#include "units/spawn_type.h"
#include "units/unit.h"
#include "wall_network_service.h"

namespace Game::Systems {

namespace {

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

auto compute_builder_exit_position(float center_x,
                                   float center_z,
                                   const QVector3D& builder_pos,
                                   float unit_radius,
                                   const std::string& building_type) -> QVector3D {
  auto const size = BuildingCollisionRegistry::get_building_size(building_type);
  float const half_width = size.width * 0.5F;
  float const half_depth = size.depth * 0.5F;
  float const clearance = unit_radius + 0.25F;

  float dir_x = builder_pos.x() - center_x;
  float dir_z = builder_pos.z() - center_z;
  float const len_sq = dir_x * dir_x + dir_z * dir_z;
  if (len_sq < 0.0001F) {
    dir_x = 1.0F;
    dir_z = 0.0F;
  } else {
    float const len = std::sqrt(len_sq);
    dir_x /= len;
    dir_z /= len;
  }

  float const abs_x = std::fabs(dir_x);
  float const abs_z = std::fabs(dir_z);
  float const sx = (abs_x > 0.0001F) ? (half_width + clearance) / abs_x
                                     : std::numeric_limits<float>::infinity();
  float const sz = (abs_z > 0.0001F) ? (half_depth + clearance) / abs_z
                                     : std::numeric_limits<float>::infinity();
  float const scale = std::min(sx, sz);
  float const fallback_scale = std::max(half_width, half_depth) + clearance;
  float const final_scale =
      std::isfinite(scale) && scale > 0.0F ? scale : fallback_scale;

  return {
      center_x + dir_x * final_scale, builder_pos.y(), center_z + dir_z * final_scale};
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
  constexpr float k_route_goal_tolerance_sq = 0.25F;
  return (dx * dx + dz * dz) <= k_route_goal_tolerance_sq;
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
            ? site_entity->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    auto* site =
        site_entity != nullptr
            ? site_entity->get_component<Engine::Core::WallConstructionSiteComponent>()
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
            builder_entity->get_component<Engine::Core::MovementComponent>()) {
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
  auto* site =
      site_entity != nullptr
          ? site_entity->get_component<Engine::Core::WallConstructionSiteComponent>()
          : nullptr;
  auto* transform = site_entity != nullptr
                        ? site_entity->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
  auto* wall = site_entity != nullptr
                   ? site_entity->get_component<Engine::Core::WallSegmentComponent>()
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
  grant_resources(site->owner_id, refund);
  publish_resource_bundle_at(site->owner_id,
                             transform->position.x,
                             transform->position.y,
                             transform->position.z,
                             refund,
                             1);
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
  const int restored = std::max(k_repair_minimum_per_tick,
                                static_cast<int>(static_cast<float>(unit->max_health) *
                                                 k_repair_fraction_per_tick));
  unit->health = std::min(unit->max_health, unit->health + restored);

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
      target != nullptr ? target->get_component<Engine::Core::TransformComponent>()
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
    const auto* unit = worker->get_component<Engine::Core::UnitComponent>();
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
  if (auto* wildlife = sheep->get_component<Engine::Core::WildlifeComponent>()) {
    wildlife->held_timer = std::max(wildlife->held_timer, 0.75F);
  }
  if (auto* movement = sheep->get_component<Engine::Core::MovementComponent>()) {
    if (movement->get_has_target()) {
      movement->stop();
    }
  }
}

void slaughter_sheep(Engine::Core::World* world,
                     Engine::Core::Entity* worker,
                     Engine::Core::EntityID sheep_id) {
  auto* sheep = world->get_entity(sheep_id);
  auto* unit =
      sheep != nullptr ? sheep->get_component<Engine::Core::UnitComponent>() : nullptr;
  if (unit == nullptr) {
    return;
  }
  unit->health = 0;
  if (auto* movement = sheep->get_component<Engine::Core::MovementComponent>()) {
    movement->stop();
  }
  auto* death =
      Engine::Core::get_or_add_component<Engine::Core::DeathAnimationComponent>(*sheep);
  if (death != nullptr) {
    death->profile = Engine::Core::DeathSequenceProfile::Horse;
    death->state = Engine::Core::DeathSequenceState::Dying;
    death->state_time = 0.0F;
    death->state_duration = 1.2F;
    death->dead_hold_duration = 1.0F;
    death->sequence_variant = 0U;
  }
  const auto* worker_unit = worker != nullptr
                                ? worker->get_component<Engine::Core::UnitComponent>()
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
    auto* crop = target->get_component<Engine::Core::FarmComponent>();
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

} // namespace

void ProductionSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  for (auto [entity_ref, prod_ref] :
       world->entity_view<Engine::Core::ProductionComponent>()) {
    Engine::Core::Entity* e = &entity_ref;
    auto* prod = &prod_ref;

    if (e->has_component<Engine::Core::DismantleSiteComponent>()) {
      continue;
    }

    auto* unit_comp = e->get_component<Engine::Core::UnitComponent>();
    if ((unit_comp != nullptr) && Game::Core::is_neutral_owner(unit_comp->owner_id)) {
      continue;
    }

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

      auto* t = e->get_component<Engine::Core::TransformComponent>();
      auto* u = e->get_component<Engine::Core::UnitComponent>();
      if ((t != nullptr) && (u != nullptr)) {

        int const current_troops = Game::Systems::troop_count_for(*world, u->owner_id);
        int const max_troops = Game::GameConfig::instance().get_max_troops_per_player();
        if (current_troops + current_profile.production.population_cost() >
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
          sp.ai_controlled = e->has_component<Engine::Core::AIControlledComponent>();
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
            Engine::Core::EventManager::instance().publish(
                Engine::Core::AudioCueEvent("build.unit_ready"));
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

  constexpr float CONSTRUCTION_ARRIVAL_DISTANCE_SQ = 0.0225F;

  constexpr float k_site_approach_limit_seconds = 30.0F;

  constexpr float k_orphaned_task_limit_seconds = 8.0F;
  constexpr float MAX_CONSTRUCTION_DISTANCE_SQ = 9.0F;

  constexpr float k_site_bypass_radius_sq = 6.25F;

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

    auto* transform = e->get_component<Engine::Core::TransformComponent>();
    auto* movement = e->get_component<Engine::Core::MovementComponent>();

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
              builder_prod->construction_site_x = transform->position.x;
              builder_prod->construction_site_z = transform->position.z;
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

        if (dist_sq < CONSTRUCTION_ARRIVAL_DISTANCE_SQ) {

          builder_prod->at_construction_site = true;
          builder_prod->in_progress = true;
          builder_prod->bypass_movement_active = false;
          builder_prod->clear_fault();
          Engine::Core::EventManager::instance().publish(
              Engine::Core::AudioCueEvent("build.construction_started"));

          transform->position.x = builder_prod->construction_site_x;
          transform->position.z = builder_prod->construction_site_z;

          if (movement != nullptr) {
            movement->set_rest_position(builder_prod->construction_site_x,
                                        builder_prod->construction_site_z);
            movement->stop();
          }

          face_work_target(*transform, *builder_prod);
          builder_prod->site_approach_seconds = 0.0F;
        } else {
          builder_prod->site_approach_seconds += delta_time;

          if (dist_sq > k_site_bypass_radius_sq) {
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
            abandon_site_route(*builder_prod, movement);
            builder_prod->has_construction_site = false;
            builder_prod->at_construction_site = false;
            builder_prod->in_progress = false;
            builder_prod->bypass_movement_active = false;
            builder_prod->site_approach_seconds = 0.0F;
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
          builder_prod->site_approach_seconds = 0.0F;
          builder_prod->report_fault(Engine::Core::BuilderTaskFault::TargetLost);
        }
      } else if (!builder_prod->has_construction_site) {
        builder_prod->site_approach_seconds = 0.0F;
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

    builder_prod->time_remaining -= delta_time;
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
            Engine::Core::AudioCueEvent("build.construction_complete"));
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

      auto* t = e->get_component<Engine::Core::TransformComponent>();
      auto* u = e->get_component<Engine::Core::UnitComponent>();
      if ((t != nullptr) && (u != nullptr)) {
        if (is_food_builder_product(builder_prod->product_type)) {
          float const anchor_x = builder_prod->task_target_x;
          float const anchor_z = builder_prod->task_target_z;
          if (complete_food_harvest(world, e, builder_prod)) {
            if (!e->has_component<Engine::Core::AIControlledComponent>()) {
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

            if (!e->has_component<Engine::Core::AIControlledComponent>()) {
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
            sp.ai_controlled = e->has_component<Engine::Core::AIControlledComponent>();
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
              const std::array<Engine::Core::EntityID, 1> finishing_crew{e->get_id()};
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

                auto& treasury = *Game::Session::services_for(*world).economy;
                const auto refund =
                    construction_cost_info(builder_prod->product_type).resource_costs;
                for (const ResourceType type : k_all_resource_types) {
                  treasury.add(u->owner_id, type, refund.get(type));
                }
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
            reg->create(sp.spawn_type, *world, sp);

            if (is_wall_network_product(builder_prod->product_type) &&
                builder_prod->construction_site_entity_id != 0) {
              world->destroy_entity(builder_prod->construction_site_entity_id);
              builder_prod->construction_site_entity_id = 0;
              WallNetworkService::refresh_world(*world);
            }

            if (builder_prod->has_construction_site && movement != nullptr &&
                t != nullptr) {
              float const unit_radius =
                  CommandService::get_unit_radius(*world, e->get_id());
              QVector3D const preferred_exit = compute_builder_exit_position(
                  builder_prod->construction_site_x,
                  builder_prod->construction_site_z,
                  QVector3D(t->position.x, t->position.y, t->position.z),
                  unit_radius,
                  builder_prod->product_type);

              QVector3D const safe_exit = find_guaranteed_valid_exit(
                  preferred_exit.x(), preferred_exit.z(), unit_radius);

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
          Engine::Core::AudioCueEvent("build.construction_complete"));
      builder_prod->has_construction_site = false;
      builder_prod->at_construction_site = false;
      builder_prod->construction_site_entity_id = 0;
      clear_builder_task_target(*world, builder_prod, false);
    }
  }
}

} // namespace Game::Systems
