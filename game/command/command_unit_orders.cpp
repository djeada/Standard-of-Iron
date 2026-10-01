#include <optional>
#include <vector>

#include "../core/component_gameplay.h"
#include "../formation/army_formation_planner.h"
#include "../formation/army_formation_registry.h"
#include "../formation/army_formation_service.h"
#include "../session/session_context.h"
#include "../systems/combat_rules.h"
#include "../systems/movement/command_service.h"
#include "../systems/movement/order_service.h"
#include "../systems/navigation/gate_service.h"
#include "../systems/rockfall_system.h"
#include "../systems/squad_service.h"
#include "../systems/troop_count_registry.h"
#include "../systems/troop_profile_service.h"
#include "../units/spawn_type.h"
#include "../units/troop_type.h"
#include "command_handlers.h"

namespace Game::Command::handlers {

using Engine::Core::Entity;
using Engine::Core::EntityID;
using Engine::Core::World;

namespace {

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

} // namespace

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

  if (Game::Systems::OrderService::move_ends_builder_gather_job(move.kind)) {
    for_each_subject(world, move.units, [&world](Entity& entity) {
      Game::Systems::OrderService::clear_builder_gather_job(world, &entity);
    });
  }

  if (move.kind == Game::Systems::MoveOrderKind::PlayerMove ||
      move.kind == Game::Systems::MoveOrderKind::AttackMove) {
    Game::Formation::ArmyFormationService::release(world, move.units);
  }

  Game::Systems::CommandService::MoveOptions options;
  options.kind = move.kind;
  options.preserve_formation_mode = move.preserve_formation_mode;
  options.prefer_own_routes = move.kind == Game::Systems::MoveOrderKind::PlayerMove ||
                              move.kind == Game::Systems::MoveOrderKind::FormationMove;
  Game::Systems::CommandService::move_units(world, intents, options);
}
void apply_stop(World& world, const Stop& stop) {
  Game::Formation::ArmyFormationService::release(world, stop.units);
  for_each_subject(world, stop.units, [&world](Entity& entity) {
    Game::Systems::OrderService::clear_builder_gather_job(world, &entity);
    Game::Systems::OrderService::apply_stop(&entity);

    if (auto* formation =
            entity.get_component<Engine::Core::FormationModeComponent>()) {
      formation->active = false;
    }
  });
}
void apply_roll_stones(World& world, const RollStones& order) {
  auto* rockfall = world.get_system<Game::Systems::RockfallSystem>();
  if (rockfall == nullptr) {
    return;
  }
  for (auto const unit : order.units) {
    (void)rockfall->order_release(world, unit);
  }
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
        const auto& profile =
            Game::Systems::TroopProfileService::instance().get_profile_ref(
                unit->nation_id, *troop_type);
        stamina->initialize_from_stats(profile.combat.max_stamina,
                                       profile.combat.stamina_regen_rate,
                                       profile.combat.stamina_depletion_rate);
      }
    }
    stamina->run_requested = true;
  });
}
void apply_patrol(World& world, const Patrol& patrol) {
  Game::Formation::ArmyFormationService::release(world, patrol.units);
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
  if (!result.valid) {
    return;
  }

  for (auto const id : order.units) {
    if (auto* entity = world.get_entity(id)) {
      Game::Systems::OrderService::clear_patrol(entity);
    }
  }
  Game::Systems::CommandService::march_into_formation(world, order.units, result);
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

} // namespace Game::Command::handlers
