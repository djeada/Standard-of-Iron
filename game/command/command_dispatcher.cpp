#include "command_dispatcher.h"

#include <QDebug>

#include <algorithm>
#include <optional>
#include <string>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "../formation/army_formation_planner.h"
#include "../formation/army_formation_service.h"
#include "../map/terrain_service.h"
#include "../session/session_context.h"
#include "../systems/build_site.h"
#include "../systems/builder_product_types.h"
#include "../systems/civilian_delivery_system.h"
#include "../systems/combat_rules.h"
#include "../systems/command_service.h"
#include "../systems/construction_cost_catalog.h"
#include "../systems/food_targets.h"
#include "../systems/gate_service.h"
#include "../systems/marketplace_system.h"
#include "../systems/order_service.h"
#include "../systems/player_feedback.h"
#include "../systems/player_resource_registry.h"
#include "../systems/production_service.h"
#include "../systems/squad_service.h"
#include "../systems/structure_placement_service.h"
#include "../systems/troop_count_registry.h"
#include "../systems/troop_profile_service.h"
#include "../systems/wall_plan_service.h"
#include "../units/spawn_type.h"
#include "../units/squad.h"
#include "../units/troop_type.h"

namespace Game::Command {

namespace {

using Engine::Core::Entity;
using Engine::Core::EntityID;
using Engine::Core::World;

template <typename Fn>
void for_each_subject(World& world, const std::vector<EntityID>& units, Fn&& fn) {
  for (const EntityID id : units) {
    if (auto* entity = world.get_entity(id)) {
      fn(*entity);
    }
  }
}

void apply_move(World& world, const Move& move) {
  std::vector<Game::Systems::CommandService::MoveIntent> intents;
  intents.reserve(move.units.size());
  for (std::size_t i = 0; i < move.units.size(); ++i) {
    intents.push_back({.unit_id = move.units[i],
                       .target = move.targets[i],
                       .facing_angle = i < move.facing_angles.size()
                                           ? std::optional<float>(move.facing_angles[i])
                                           : std::nullopt});
  }

  Game::Systems::CommandService::MoveOptions options;
  options.kind = move.kind;
  options.preserve_formation_mode = move.preserve_formation_mode;
  Game::Systems::CommandService::move_units(world, intents, options);
}

void apply_stop(World& world, const Stop& stop) {
  for_each_subject(world, stop.units, [](Entity& entity) {
    Game::Systems::OrderService::apply_stop(&entity);

    if (auto* formation =
            entity.get_component<Engine::Core::FormationModeComponent>()) {
      formation->active = false;
    }
  });
}

void apply_gate_mode(World& world, const SetGateMode& order) {
  for_each_subject(world, order.units, [&order](Entity& entity) {
    Game::Systems::GateService::set_manual_mode(entity, order.mode);
  });
}

void apply_hold(World& world, const SetHold& hold) {
  for_each_subject(world, hold.units, [&world, &hold](Entity& entity) {
    const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || !Game::Units::can_use_hold_mode(unit->spawn_type)) {
      return;
    }

    auto* hold_mode = entity.get_component<Engine::Core::HoldModeComponent>();
    if (!hold.active) {
      if (hold_mode != nullptr && hold_mode->active) {
        hold_mode->begin_exit();
      }
      return;
    }

    auto* attack = entity.get_component<Engine::Core::AttackComponent>();
    if (attack != nullptr && attack->in_melee_lock &&
        Game::Systems::CombatRules::participates_in_rts_melee_lock(&entity)) {
      auto* locked_target = world.get_entity(attack->melee_lock_target_id);
      const auto* locked_unit =
          locked_target != nullptr
              ? locked_target->get_component<Engine::Core::UnitComponent>()
              : nullptr;
      const bool locked_opponent_alive =
          locked_unit != nullptr && locked_unit->health > 0 &&
          !locked_target->has_component<Engine::Core::PendingRemovalComponent>();
      if (locked_opponent_alive) {
        return;
      }
      Game::Systems::CombatRules::clear_rts_melee_lock(&entity);
    }

    Game::Systems::OrderService::reset_movement(&entity);
    Game::Systems::OrderService::clear_attack_target(&entity);
    Game::Systems::OrderService::clear_player_order_intent(&entity);
    if (attack != nullptr) {
      Game::Systems::CombatRules::clear_rts_melee_lock(&entity);
    }
    Game::Systems::OrderService::clear_patrol(&entity);

    if (hold_mode == nullptr) {
      hold_mode = entity.add_component<Engine::Core::HoldModeComponent>();
    }
    hold_mode->active = true;
    hold_mode->exit_cooldown = 0.0F;
  });
}

void apply_guard(World& world, const SetGuard& guard) {
  for_each_subject(world, guard.units, [&guard](Entity& entity) {
    const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || !Game::Units::can_use_guard_mode(unit->spawn_type)) {
      return;
    }

    auto* guard_mode = entity.get_component<Engine::Core::GuardModeComponent>();
    if (!guard.active) {
      if (guard_mode != nullptr && guard_mode->active) {

        *guard_mode = Engine::Core::GuardModeComponent{};
        guard_mode->active = false;
      }
      return;
    }

    if (guard_mode == nullptr) {
      guard_mode = entity.add_component<Engine::Core::GuardModeComponent>();
    }
    guard_mode->active = true;
    guard_mode->returning_to_guard_position = false;
    guard_mode->guarded_entity_id = 0;

    if (guard.has_anchor) {

      guard_mode->guard_position_x = guard.anchor.x();
      guard_mode->guard_position_z = guard.anchor.z();
      guard_mode->has_guard_target = true;
      Game::Systems::OrderService::reset_movement(&entity);
      Game::Systems::OrderService::clear_attack_target(&entity);
      Game::Systems::OrderService::clear_player_order_intent(&entity);
    } else if (const auto* transform =
                   entity.get_component<Engine::Core::TransformComponent>()) {
      guard_mode->guard_position_x = transform->position.x;
      guard_mode->guard_position_z = transform->position.z;
      guard_mode->has_guard_target = true;
    }

    Game::Systems::OrderService::exit_hold_mode(&entity);
    Game::Systems::OrderService::clear_patrol(&entity);
  });
}

void apply_run_mode(World& world, const SetRunMode& run) {
  for_each_subject(world, run.units, [&run](Entity& entity) {
    const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || !Game::Units::can_use_run_mode(unit->spawn_type)) {
      return;
    }

    auto* stamina = entity.get_component<Engine::Core::StaminaComponent>();
    if (!run.active) {
      if (stamina != nullptr) {
        stamina->run_requested = false;
        stamina->is_running = false;
      }
      return;
    }

    if (stamina == nullptr) {
      stamina = entity.add_component<Engine::Core::StaminaComponent>();
      const auto troop_type = Game::Units::spawn_typeToTroopType(unit->spawn_type);
      if (troop_type.has_value()) {
        const auto profile = Game::Systems::TroopProfileService::instance().get_profile(
            unit->nation_id, *troop_type);
        stamina->initialize_from_stats(profile.combat.max_stamina,
                                       profile.combat.stamina_regen_rate,
                                       profile.combat.stamina_depletion_rate);
      }
    }
    stamina->run_requested = true;
  });
}

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

    if (Game::Systems::is_gather_builder_product(builder->product_type) &&
        !builder->in_progress) {

      Game::Systems::OrderService::clear_builder_task(world, &entity);
    }

    builder->clear_gather_order();
    builder->clear_fault();
  });
}

void apply_patrol(World& world, const Patrol& patrol) {
  for_each_subject(world, patrol.units, [&patrol](Entity& entity) {
    auto* component = entity.get_component<Engine::Core::PatrolComponent>();
    if (component == nullptr) {
      component = entity.add_component<Engine::Core::PatrolComponent>();
    }
    if (component == nullptr) {
      return;
    }

    component->waypoints.clear();
    component->waypoints.emplace_back(patrol.first_waypoint.x(),
                                      patrol.first_waypoint.z());
    component->waypoints.emplace_back(patrol.second_waypoint.x(),
                                      patrol.second_waypoint.z());
    component->current_waypoint = 0;
    component->patrolling = true;

    Game::Systems::OrderService::reset_movement(&entity);
    Game::Systems::OrderService::clear_attack_target(&entity);
    Game::Systems::OrderService::clear_player_order_intent(&entity);
  });
}

void apply_trade(World& world, int owner_id, const Trade& trade) {
  auto& marketplace = Game::Session::session_for(world).marketplace();
  if (trade.direction == TradeDirection::Buy) {
    static_cast<void>(marketplace.buy_resource(world, owner_id, trade.resource));
  } else {
    static_cast<void>(marketplace.sell_resource(world, owner_id, trade.resource));
  }
}

void apply_commander_ability(World& world, const UseCommanderAbility& order) {
  auto* entity = world.get_entity(order.commander);
  auto* commander = entity != nullptr
                        ? entity->get_component<Engine::Core::CommanderComponent>()
                        : nullptr;
  if (commander == nullptr) {
    return;
  }
  switch (order.ability) {
  case CommanderAbility::Aura:
    static_cast<void>(commander->request_aura_ability());
    return;
  case CommanderAbility::Rally:
    commander->rally_requested = true;
    return;
  case CommanderAbility::FlagRally: {
    if (commander->flag_rally_in_progress) {
      return;
    }
    const std::vector<EntityID> subject{order.commander};
    auto const plan =
        Game::Systems::CommandService::plan_ground_move(world, subject, order.target);
    if (!plan.fully_placeable_for(subject)) {
      return;
    }
    commander->begin_flag_rally(
        plan.resolved_target.x(), plan.resolved_target.z(), false);
    if (auto* stamina = entity->get_component<Engine::Core::StaminaComponent>()) {
      stamina->run_requested = false;
      stamina->is_running = false;
    }
    Game::Systems::CommandService::issue_ground_move(world, subject, plan);
    return;
  }
  }
}

void apply_formation_mode(World& world, const SetFormationMode& order) {
  for_each_subject(world, order.units, [&order](Entity& entity) {
    const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || !Game::Units::is_troop_spawn(unit->spawn_type)) {
      return;
    }
    auto* formation_mode = entity.get_component<Engine::Core::FormationModeComponent>();
    if (!order.active) {
      if (formation_mode != nullptr) {
        formation_mode->active = false;
      }
      return;
    }
    if (formation_mode == nullptr) {
      formation_mode = entity.add_component<Engine::Core::FormationModeComponent>();
    }
    formation_mode->active = true;
    Game::Systems::OrderService::exit_hold_mode(&entity);
    if (auto* guard = entity.get_component<Engine::Core::GuardModeComponent>();
        guard != nullptr && guard->active) {
      guard->active = false;
    }
    Game::Systems::OrderService::clear_patrol(&entity);
  });
}

void apply_deploy_formation(World& world, const DeployFormation& order) {
  Game::Formation::ArmyFormationRequest request;
  request.members = order.units;
  request.anchor = order.anchor;
  request.facing = order.facing;
  request.frontage = order.frontage;
  request.intent = order.intent;
  request.doctrine = order.doctrine;
  request.options = order.options;
  request.spacing = order.spacing;

  auto const result = Game::Formation::ArmyFormationService::commit(world, request);

  for (std::size_t i = 0; i < order.units.size(); ++i) {
    auto* entity = world.get_entity(order.units[i]);
    if (entity == nullptr) {
      continue;
    }
    if (auto* transform = entity->get_component<Engine::Core::TransformComponent>()) {
      transform->desired_yaw =
          i < result.facing_angles.size() ? result.facing_angles[i] : 0.0F;
      transform->has_desired_yaw = true;
    }
    auto* formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if (formation_mode != nullptr && i < result.stable_slot_ids.size()) {
      formation_mode->formation_id = result.group_id;
      formation_mode->stable_slot_id = result.stable_slot_ids[i];
      formation_mode->stable_rank = result.stable_ranks[i];
      formation_mode->stable_file = result.stable_files[i];
      formation_mode->stable_slot_x = result.positions[i].x();
      formation_mode->stable_slot_z = result.positions[i].z();
    }
    Game::Systems::OrderService::clear_patrol(entity);
  }

  if (result.positions.size() != order.units.size()) {
    return;
  }
  Move march;
  march.units = order.units;
  march.targets = result.positions;
  march.kind = Game::Systems::MoveOrderKind::FormationMove;
  march.preserve_formation_mode = result.used_army_formation;
  apply_move(world, march);
}

void apply_release_formation(World& world, const ReleaseFormation& order) {
  for_each_subject(world, order.units, [](Entity& entity) {
    if (auto* formation_mode =
            entity.get_component<Engine::Core::FormationModeComponent>()) {
      formation_mode->active = false;
    }
  });
  Game::Formation::ArmyFormationService::release(world, order.units);
}

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
    if (qEnvironmentVariableIsSet("SOI_BUILD_TRACE")) {
      qWarning() << "BUILDTRACE p" << owner_id << "cannot pay for"
                 << order.construction_type.c_str();
    }
    return;
  }

  const auto verdict = Game::Systems::assess_ground(world,
                                                    order.construction_type,
                                                    order.site.x(),
                                                    order.site.z(),
                                                    0,
                                                    order.rotation_y,
                                                    order.units);
  if (verdict != Game::Systems::GroundVerdict::Clear) {
    if (qEnvironmentVariableIsSet("SOI_BUILD_TRACE")) {
      qWarning() << "BUILDTRACE p" << owner_id << "ground refused"
                 << order.construction_type.c_str() << "at" << order.site.x()
                 << order.site.z() << "yaw" << order.rotation_y << "verdict"
                 << static_cast<int>(verdict);
    }
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
  if (qEnvironmentVariableIsSet("SOI_BUILD_TRACE")) {
    qWarning() << "BUILDTRACE p" << owner_id << "assigned" << assigned_any
               << order.construction_type.c_str() << "units" << order.units.size()
               << "at" << order.site.x() << order.site.z() << "yaw" << order.rotation_y;
  }
  if (assigned_any) {
    Game::Systems::spend_resources_at(
        owner_id, order.site.x(), order.site.y(), order.site.z(), costs);
  }
}

auto worker_position_or(const Entity& worker, const QVector3D& fallback) -> QVector3D;

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

      builder->has_construction_site = false;
      builder->product_type.clear();
      release_task_target(terrain, *builder);
      continue;
    }
    release_task_target(terrain, *builder);

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

auto worker_position_or(const Entity& worker, const QVector3D& fallback) -> QVector3D {
  const auto* transform = worker.get_component<Engine::Core::TransformComponent>();
  return transform != nullptr
             ? QVector3D(transform->position.x, 0.0F, transform->position.z)
             : fallback;
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

auto owned_subjects(World& world,
                    int owner_id,
                    const std::vector<Engine::Core::EntityID>& units)
    -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> owned;
  owned.reserve(units.size());
  for (const auto id : units) {
    auto* entity = world.get_entity(id);
    const auto* unit = entity != nullptr
                           ? entity->get_component<Engine::Core::UnitComponent>()
                           : nullptr;
    if (unit != nullptr && unit->owner_id == owner_id && unit->health > 0) {
      owned.push_back(id);
    }
  }
  return owned;
}

void apply_divide_squads(World& world, int owner_id, const DivideSquads& order) {
  const auto divisions = Game::Systems::SquadService::divide_all(
      world, owned_subjects(world, owner_id, order.units));
  if (!divisions.empty()) {

    Game::Session::session_for(world).troop_counts().rebuild_from_world(world);
  }
}

void apply_merge_squads(World& world, int owner_id, const MergeSquads& order) {
  const auto merges = Game::Systems::SquadService::merge_all(
      world, owned_subjects(world, owner_id, order.units));
  if (!merges.empty()) {
    Game::Session::session_for(world).troop_counts().rebuild_from_world(world);
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

} // namespace

void dispatch(World& world, const Command& command) {
  std::visit(
      [&](const auto& payload) {
        using T = std::decay_t<decltype(payload)>;

        if constexpr (std::is_same_v<T, Move>) {
          apply_move(world, payload);
        } else if constexpr (std::is_same_v<T, AttackTarget>) {
          Game::Systems::CommandService::attack_target(
              world, payload.units, payload.target, payload.should_chase);
        } else if constexpr (std::is_same_v<T, Stop>) {
          apply_stop(world, payload);
        } else if constexpr (std::is_same_v<T, SetHold>) {
          apply_hold(world, payload);
        } else if constexpr (std::is_same_v<T, SetGuard>) {
          apply_guard(world, payload);
        } else if constexpr (std::is_same_v<T, SetRunMode>) {
          apply_run_mode(world, payload);
        } else if constexpr (std::is_same_v<T, Patrol>) {
          apply_patrol(world, payload);
        } else if constexpr (std::is_same_v<T, SetRallyPoint>) {
          Game::Systems::ProductionService::set_rally_point(
              world, payload.building, payload.position.x(), payload.position.z());
        } else if constexpr (std::is_same_v<T, SetGateMode>) {
          apply_gate_mode(world, payload);
        } else if constexpr (std::is_same_v<T, SetAutoGather>) {
          apply_auto_gather(world, payload);
        } else if constexpr (std::is_same_v<T, Produce>) {
          Game::Systems::ProductionService::start_production(
              world, payload.building, payload.product);
        } else if constexpr (std::is_same_v<T, Trade>) {
          apply_trade(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, UseCommanderAbility>) {
          apply_commander_ability(world, payload);
        } else if constexpr (std::is_same_v<T, SetFormationMode>) {
          apply_formation_mode(world, payload);
        } else if constexpr (std::is_same_v<T, DeployFormation>) {
          apply_deploy_formation(world, payload);
        } else if constexpr (std::is_same_v<T, ReleaseFormation>) {
          apply_release_formation(world, payload);
        } else if constexpr (std::is_same_v<T, StartConstruction>) {
          apply_start_construction(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, StartHarvest>) {
          apply_start_harvest(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, DeliverCivilians>) {
          apply_deliver_civilians(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, DivideSquads>) {
          apply_divide_squads(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, MergeSquads>) {
          apply_merge_squads(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, RepairStructure>) {
          apply_repair_structure(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, DismantleStructure>) {
          apply_dismantle_structure(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, PlaceWallPlan>) {
          apply_place_wall_plan(world, command.owner_id, payload);
        } else if constexpr (std::is_same_v<T, PlaceBuilding>) {
          apply_place_building(world, command.owner_id, payload);
        }
      },
      command.payload);
}

} // namespace Game::Command
