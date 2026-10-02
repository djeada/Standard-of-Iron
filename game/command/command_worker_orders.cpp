#include <QVector3D>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "../core/component_economy.h"
#include "../map/terrain_service.h"
#include "../session/session_context.h"
#include "../systems/builder_product_types.h"
#include "../systems/economy/build_site.h"
#include "../systems/economy/civilian_delivery_system.h"
#include "../systems/economy/construction_cost_catalog.h"
#include "../systems/economy/food_targets.h"
#include "../systems/movement/command_service.h"
#include "../systems/movement/order_service.h"
#include "../systems/player_feedback.h"
#include "../systems/player_resource_registry.h"
#include "../systems/structure_placement_service.h"
#include "../systems/wall_plan_service.h"
#include "../units/spawn_type.h"
#include "../units/squad.h"
#include "command_handlers.h"

namespace Game::Command::handlers {

using Engine::Core::Entity;
using Engine::Core::EntityID;
using Engine::Core::World;

namespace {

auto builder_of(World& world, EntityID id, int owner_id)
    -> std::pair<Entity*, Engine::Core::BuilderProductionComponent*> {
  auto* entity = world.get_entity(id);
  const auto* unit = entity != nullptr
                         ? entity->get_component<Engine::Core::UnitComponent>()
                         : nullptr;
  if (unit == nullptr || unit->owner_id != owner_id ||
      unit->spawn_type != Game::Units::SpawnType::Builder) {
    return {nullptr, nullptr};
  }
  return {entity, entity->get_component<Engine::Core::BuilderProductionComponent>()};
}
void release_task_target(Game::Map::TerrainService& terrain,
                         Engine::Core::BuilderProductionComponent& builder) {
  if (builder.task_target_reserved) {
    terrain.release_world_prop(builder.task_target_id);
  }
  builder.has_task_target = false;
  builder.task_target_id = 0;
  builder.task_target_x = 0.0F;
  builder.task_target_z = 0.0F;
  builder.task_target_reserved = false;
}
void begin_site_work(const Engine::Core::Entity& worker,
                     Engine::Core::BuilderProductionComponent& builder,
                     const std::string& construction_type,
                     const QVector3D& site,
                     float rotation_y) {
  builder.clear_auto_gather();
  builder.product_type = construction_type;

  const auto* unit = worker.get_component<Engine::Core::UnitComponent>();
  const float hands =
      unit != nullptr ? std::max(0.05F, Game::Units::squad_fraction(*unit)) : 1.0F;
  builder.build_time =
      Game::Systems::construction_build_time(construction_type) / hands;
  builder.time_remaining = builder.build_time;
  builder.has_construction_site = true;
  builder.construction_site_x = site.x();
  builder.construction_site_z = site.z();
  builder.construction_site_rotation_y = rotation_y;
  builder.at_construction_site = false;
  builder.in_progress = false;
  builder.construction_complete = false;
  builder.bypass_movement_active = false;
}
auto worker_position_or(const Entity& worker, const QVector3D& fallback) -> QVector3D {
  const auto* transform = worker.get_component<Engine::Core::TransformComponent>();
  return transform != nullptr
             ? QVector3D(transform->position.x, 0.0F, transform->position.z)
             : fallback;
}
void apply_start_food_harvest(World& world, int owner_id, const StartHarvest& order) {
  auto target =
      Game::Systems::resolve_food_target(world, order.resource_target, owner_id);
  if (!target.has_value() || target->product_type != order.construction_type) {
    return;
  }

  auto& terrain = Game::Session::session_for(world).terrain();
  bool assigned = false;
  for (const EntityID id : order.units) {
    auto [entity, builder] = builder_of(world, id, owner_id);
    if (builder == nullptr) {
      continue;
    }
    if (assigned) {
      builder->has_construction_site = false;
      builder->product_type.clear();
      release_task_target(terrain, *builder);
      continue;
    }
    if (Game::Systems::food_target_claimed(world, target->id, id)) {
      return;
    }
    Game::Systems::OrderService::clear_builder_task(world, entity);
    Game::Systems::OrderService::clear_builder_gather_order(entity);
    const QVector3D work_position = Game::Systems::food_work_position(
        world,
        id,
        worker_position_or(*entity, QVector3D(target->x, 0.0F, target->z)),
        *target);
    Game::Systems::assign_food_task(
        *builder,
        entity->get_component<Engine::Core::MovementComponent>(),
        *target,
        work_position);
    assigned = true;
  }
}

} // namespace

void apply_auto_gather(World& world, const SetAutoGather& order) {
  for_each_subject(world, order.units, [&world, &order](Entity& entity) {
    const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->spawn_type != Game::Units::SpawnType::Builder) {
      return;
    }

    auto* builder = entity.get_component<Engine::Core::BuilderProductionComponent>();
    if (builder == nullptr) {
      return;
    }

    if (!order.active) {
      builder->clear_auto_gather();
      return;
    }

    builder->auto_gather = true;
    builder->auto_gather_priority =
        Game::Systems::is_gather_builder_product(order.priority_product_type)
            ? order.priority_product_type
            : std::string{};

    bool const node_still_wanted =
        builder->auto_gather_priority.empty() ||
        builder->auto_gather_priority == builder->product_type;
    if (Game::Systems::is_gather_builder_product(builder->product_type) &&
        !builder->in_progress && !node_still_wanted) {
      if (auto* movement =
              world.try_get<Engine::Core::MovementComponent>(entity.get_id());
          movement != nullptr && builder->has_construction_site) {
        movement->stop();
      }
      Game::Systems::OrderService::clear_builder_task(world, &entity);
    }

    builder->clear_gather_order();
    builder->clear_fault();
  });
}
void apply_start_construction(World& world,
                              int owner_id,
                              const StartConstruction& order) {
  if (order.units.empty() || order.construction_type.empty()) {
    return;
  }
  const auto costs =
      Game::Systems::construction_cost_info(order.construction_type).resource_costs;
  auto& session = Game::Session::session_for(world);
  auto& resources = session.economy();
  if (!costs.empty() && !resources.has_at_least(owner_id, costs)) {
    return;
  }

  const auto verdict = Game::Systems::assess_ground(world,
                                                    order.construction_type,
                                                    order.site.x(),
                                                    order.site.z(),
                                                    0,
                                                    order.rotation_y,
                                                    order.units);
  if (verdict != Game::Systems::GroundVerdict::Clear ||
      Game::Systems::troops_stand_on(world,
                                     order.construction_type,
                                     order.site.x(),
                                     order.site.z(),
                                     order.rotation_y,
                                     order.units)) {
    return;
  }

  bool assigned_any = false;
  for (const EntityID id : order.units) {
    auto [entity, builder] = builder_of(world, id, owner_id);
    if (builder == nullptr) {
      continue;
    }
    release_task_target(session.terrain(), *builder);
    begin_site_work(
        *entity, *builder, order.construction_type, order.site, order.rotation_y);
    if (auto* movement = entity->get_component<Engine::Core::MovementComponent>()) {
      movement->set_rest_position(order.site.x(), order.site.z());
    }
    assigned_any = true;
  }
  if (assigned_any) {
    Game::Systems::spend_resources_at(
        owner_id, order.site.x(), order.site.y(), order.site.z(), costs);
  }
}
void apply_start_harvest(World& world, int owner_id, const StartHarvest& order) {
  if (order.units.empty() || order.construction_type.empty() ||
      order.resource_target == Engine::Core::NULL_ENTITY) {
    return;
  }
  if (Game::Systems::is_food_builder_product(order.construction_type)) {
    apply_start_food_harvest(world, owner_id, order);
    return;
  }
  auto& terrain = Game::Session::session_for(world).terrain();

  for (const EntityID id : order.units) {
    auto [entity, builder] = builder_of(world, id, owner_id);
    if (builder != nullptr && builder->task_target_reserved &&
        builder->task_target_id == order.resource_target) {
      release_task_target(terrain, *builder);
    }
  }
  if (!terrain.reserve_world_prop(order.resource_target)) {
    return;
  }

  bool assigned = false;
  for (const EntityID id : order.units) {
    auto [entity, builder] = builder_of(world, id, owner_id);
    if (builder == nullptr) {
      continue;
    }
    if (assigned) {

      Game::Systems::OrderService::clear_builder_task(world, entity);
      Game::Systems::OrderService::clear_builder_gather_order(entity);
      release_task_target(terrain, *builder);
      builder->auto_gather = true;
      builder->auto_gather_priority = order.construction_type;
      continue;
    }
    Game::Systems::OrderService::clear_builder_task(world, entity);
    Game::Systems::OrderService::clear_builder_gather_order(entity);

    const QVector3D work_position =
        Game::Systems::CommandService::world_prop_work_position(
            terrain, worker_position_or(*entity, order.site), order.resource_target);
    begin_site_work(*entity, *builder, order.construction_type, work_position, 0.0F);
    builder->has_task_target = true;
    builder->task_target_id = order.resource_target;
    builder->task_target_x = order.site.x();
    builder->task_target_z = order.site.z();
    builder->task_target_reserved = true;
    if (auto* movement = entity->get_component<Engine::Core::MovementComponent>()) {
      movement->set_rest_position(work_position.x(), work_position.z());
    }
    assigned = true;
  }
  if (!assigned) {
    terrain.release_world_prop(order.resource_target);
  }
}
void apply_deliver_civilians(World& world,
                             int owner_id,
                             const DeliverCivilians& order) {
  auto* barracks = world.get_entity(order.barracks);
  const auto* barracks_unit =
      barracks != nullptr ? barracks->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
  const auto* barracks_transform =
      barracks != nullptr ? barracks->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
  const auto* production =
      barracks != nullptr ? barracks->get_component<Engine::Core::ProductionComponent>()
                          : nullptr;
  if (barracks_unit == nullptr || barracks_transform == nullptr ||
      production == nullptr ||
      !Game::Units::is_recruitment_building(barracks_unit->spawn_type)) {
    return;
  }

  const int free_population =
      std::max(0, production->max_units - production->manpower_available);
  int remaining = free_population / Game::Systems::k_civilian_delivery_reserve_grant;
  if (remaining <= 0) {
    return;
  }

  const QVector3D barracks_position(
      barracks_transform->position.x, 0.0F, barracks_transform->position.z);
  Move move;
  move.kind = Game::Systems::MoveOrderKind::ScriptedMove;
  for (const EntityID id : order.units) {
    if (remaining <= 0) {
      break;
    }
    auto* entity = world.get_entity(id);
    const auto* unit = entity != nullptr
                           ? entity->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
    if (unit == nullptr || unit->owner_id != owner_id ||
        unit->spawn_type != Game::Units::SpawnType::Civilian) {
      continue;
    }
    auto* delivery = entity->get_component<Engine::Core::CivilianDeliveryComponent>();
    if (delivery == nullptr) {
      delivery = entity->add_component<Engine::Core::CivilianDeliveryComponent>();
    }
    if (delivery == nullptr) {
      continue;
    }
    delivery->target_barracks_id = order.barracks;

    move.units.push_back(id);
    move.targets.push_back(Game::Systems::CommandService::structure_work_position(
        worker_position_or(*entity, barracks_position),
        barracks_position,
        "barracks",
        Game::Systems::CommandService::get_unit_radius(world, id)));
    --remaining;
  }
  if (!move.units.empty()) {
    apply_move(world, move);
  }
}
void apply_repair_structure(World& world, int owner_id, const RepairStructure& order) {
  auto* structure = world.get_entity(order.structure);
  if (structure == nullptr ||
      !structure->has_component<Engine::Core::BuildingComponent>()) {
    return;
  }
  const auto* structure_unit = structure->get_component<Engine::Core::UnitComponent>();
  const auto* structure_transform =
      structure->get_component<Engine::Core::TransformComponent>();
  if (structure_unit == nullptr || structure_transform == nullptr ||
      structure_unit->health <= 0 ||
      structure_unit->health >= structure_unit->max_health) {
    return;
  }

  const std::string structure_key =
      Game::Units::spawn_typeToString(structure_unit->spawn_type);
  const QVector3D structure_position(
      structure_transform->position.x, 0.0F, structure_transform->position.z);

  Move move;
  move.kind = Game::Systems::MoveOrderKind::ScriptedMove;
  for (const EntityID id : order.units) {
    auto [entity, builder] = builder_of(world, id, owner_id);
    if (builder == nullptr) {
      continue;
    }
    const QVector3D work_position =
        Game::Systems::CommandService::structure_work_position(
            worker_position_or(*entity, structure_position),
            structure_position,
            structure_key,
            Game::Systems::CommandService::get_unit_radius(world, id));

    Game::Systems::OrderService::clear_builder_task(world, entity);
    Game::Systems::OrderService::clear_builder_gather_order(entity);
    builder->product_type = std::string(Game::Systems::k_builder_product_repair);
    builder->build_time = Game::Systems::k_builder_repair_tick_seconds;
    builder->time_remaining = Game::Systems::k_builder_repair_tick_seconds;
    builder->structure_task_entity_id = order.structure;
    builder->has_construction_site = true;
    builder->construction_site_x = work_position.x();
    builder->construction_site_z = work_position.z();
    builder->construction_site_rotation_y = 0.0F;
    builder->at_construction_site = false;
    builder->in_progress = false;
    builder->construction_complete = false;
    builder->bypass_movement_active = false;
    builder->clear_fault();

    move.units.push_back(id);
    move.targets.push_back(work_position);
  }
  if (!move.units.empty()) {
    apply_move(world, move);
  }
}
void apply_dismantle_structure(World& world,
                               int owner_id,
                               const DismantleStructure& order) {
  auto* structure = world.get_entity(order.structure);
  if (structure == nullptr ||
      !structure->has_component<Engine::Core::BuildingComponent>()) {
    return;
  }
  const auto* structure_unit = structure->get_component<Engine::Core::UnitComponent>();
  const auto* structure_transform =
      structure->get_component<Engine::Core::TransformComponent>();
  if (structure_unit == nullptr || structure_transform == nullptr ||
      structure_unit->health <= 0 || structure_unit->owner_id != owner_id) {
    return;
  }

  const std::string structure_key =
      Game::Units::spawn_typeToString(structure_unit->spawn_type);
  if (!Game::Systems::dismantle_info(structure_key).allowed) {
    return;
  }

  const QVector3D structure_position(
      structure_transform->position.x, 0.0F, structure_transform->position.z);

  Move move;
  move.kind = Game::Systems::MoveOrderKind::ScriptedMove;
  for (const EntityID id : order.units) {
    auto [entity, builder] = builder_of(world, id, owner_id);
    if (builder == nullptr) {
      continue;
    }
    const QVector3D work_position =
        Game::Systems::CommandService::structure_work_position(
            worker_position_or(*entity, structure_position),
            structure_position,
            structure_key,
            Game::Systems::CommandService::get_unit_radius(world, id));

    Game::Systems::OrderService::clear_builder_task(world, entity);
    Game::Systems::OrderService::clear_builder_gather_order(entity);
    builder->product_type = std::string(Game::Systems::k_builder_product_dismantle);
    builder->build_time = Game::Systems::dismantle_duration(structure_key);
    builder->time_remaining = builder->build_time;
    builder->structure_task_entity_id = order.structure;
    builder->has_construction_site = true;
    builder->construction_site_x = work_position.x();
    builder->construction_site_z = work_position.z();
    builder->construction_site_rotation_y = 0.0F;
    builder->at_construction_site = false;
    builder->in_progress = false;
    builder->construction_complete = false;
    builder->bypass_movement_active = false;
    builder->clear_fault();

    move.units.push_back(id);
    move.targets.push_back(work_position);
  }

  if (move.units.empty()) {
    return;
  }

  auto* site = structure->get_component<Engine::Core::DismantleSiteComponent>();
  if (site == nullptr) {
    site = structure->add_component<Engine::Core::DismantleSiteComponent>();
    site->duration = Game::Systems::dismantle_duration(structure_key);
    site->progress = 0.0F;
  }

  apply_move(world, move);
}
void apply_place_wall_plan(World& world, int owner_id, const PlaceWallPlan& order) {
  std::vector<EntityID> crew;
  for (const EntityID id : order.units) {
    if (builder_of(world, id, owner_id).second != nullptr) {
      crew.push_back(id);
    }
  }
  if (crew.empty()) {
    return;
  }
  const Game::Systems::WallPlanRequest request{
      .owner_id = owner_id,
      .gate = order.gate,
      .anchor = {order.anchor_x, order.anchor_z},
      .target = {order.target_x, order.target_z},
      .rotation_y = order.rotation_y};
  const auto plan = Game::Systems::WallPlanService::plan(world, request);
  Game::Systems::WallPlanService::commit(world, request, plan, crew);
}
void apply_place_building(World& world, int owner_id, const PlaceBuilding& order) {
  Game::Systems::StructurePlacementService::place(
      world, owner_id, order.building_type, order.position, order.rotation_y);
}

} // namespace Game::Command::handlers
