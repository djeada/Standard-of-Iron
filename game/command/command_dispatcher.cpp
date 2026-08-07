#include "command_dispatcher.h"

#include <string>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "../systems/builder_product_types.h"
#include "../systems/combat_rules.h"
#include "../systems/command_service.h"
#include "../systems/defense_formation_service.h"
#include "../systems/gate_service.h"
#include "../systems/order_service.h"
#include "../systems/production_service.h"
#include "../systems/troop_profile_service.h"
#include "../units/spawn_type.h"
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

  for (std::size_t i = 0; i < move.units.size() && i < move.facing_angles.size(); ++i) {
    auto* entity = world.get_entity(move.units[i]);
    if (entity == nullptr) {
      continue;
    }
    const auto* formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if (formation_mode == nullptr || !formation_mode->active) {
      continue;
    }
    if (auto* transform = entity->get_component<Engine::Core::TransformComponent>()) {
      transform->desired_yaw = move.facing_angles[i];
      transform->has_desired_yaw = true;
    }
  }

  std::vector<Game::Systems::CommandService::MoveIntent> intents;
  intents.reserve(move.units.size());
  for (std::size_t i = 0; i < move.units.size(); ++i) {
    intents.push_back({.unit_id = move.units[i], .target = move.targets[i]});
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
    } else if (const auto* transform =
                   entity.get_component<Engine::Core::TransformComponent>()) {
      guard_mode->guard_position_x = transform->position.x;
      guard_mode->guard_position_z = transform->position.z;
      guard_mode->has_guard_target = true;
    }

    Game::Systems::OrderService::exit_hold_mode(&entity);
    Game::Systems::OrderService::clear_patrol(&entity);
  });

  if (guard.active) {
    Game::Systems::DefenseFormationService::begin(
        world, guard.units, guard.anchor, guard.has_anchor);
  } else {
    for_each_subject(world, guard.units, [](Entity& entity) {
      Game::Systems::DefenseFormationService::begin_break(&entity);
    });
  }
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
  for_each_subject(world, order.units, [&order](Entity& entity) {
    const auto* unit = entity.get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->spawn_type != Game::Units::SpawnType::Builder) {
      return;
    }

    auto* builder = entity.get_component<Engine::Core::BuilderProductionComponent>();
    if (builder == nullptr || builder->is_placement_preview) {
      return;
    }

    if (!order.active) {
      builder->clear_auto_gather();
      return;
    }

    builder->auto_gather = true;
    builder->auto_gather_priority =
        Game::Systems::is_harvest_builder_product(order.priority_product_type)
            ? order.priority_product_type
            : std::string{};

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
        }
      },
      command.payload);
}

} // namespace Game::Command
