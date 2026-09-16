#include "target_assignment.h"

#include <cmath>

#include "../../core/component_combat.h"
#include "../../core/component_core.h"
#include "../../core/component_gameplay.h"
#include "../../units/combat_role.h"
#include "../defensive_unit_layout_service.h"
#include "combat_utils.h"

namespace Game::Systems::Combat {

namespace {

constexpr float k_brawl_leash = 3.0F;

auto free_to_pursue(const Engine::Core::Entity* unit) -> bool {
  auto const* hold_mode = unit->get_component<Engine::Core::HoldModeComponent>();
  if ((hold_mode != nullptr) && hold_mode->active) {
    return false;
  }
  return !Game::Systems::DefensiveUnitLayoutService::holds_position(*unit);
}

auto steps_into_a_brawl(const Engine::Core::Entity* unit) -> bool {
  return combat_role_of(unit) == Game::Units::CombatRole::Noncombatant;
}

auto sets_player_command_flag(TargetSource source) -> bool {
  return source == TargetSource::Opportunity || source == TargetSource::Answering ||
         source == TargetSource::Patrol;
}

} // namespace

auto chases_target(const Engine::Core::Entity* unit,
                   TargetSource source,
                   bool player_command) -> bool {
  if (unit == nullptr) {
    return false;
  }
  switch (source) {
  case TargetSource::Opportunity:
    return pursues_targets(unit) && !opens_fire_without_closing(unit) &&
           free_to_pursue(unit);
  case TargetSource::Answering:
    return (pursues_targets(unit) || steps_into_a_brawl(unit)) && free_to_pursue(unit);
  case TargetSource::InReach:
  case TargetSource::Patrol:
    return false;
  case TargetSource::MeleeLock:
    return player_command || pursues_targets(unit);
  case TargetSource::Commitment:
    return true;
  }
  return false;
}

void set_attack_target(Engine::Core::AttackTargetComponent& attack_target,
                       const Engine::Core::Entity* unit,
                       Engine::Core::EntityID target_id,
                       TargetSource source) {
  attack_target.target_id = target_id;
  if (sets_player_command_flag(source)) {
    attack_target.is_player_command = false;
  }
  attack_target.should_chase =
      chases_target(unit, source, attack_target.is_player_command);
}

auto assign_attack_target(Engine::Core::Entity* unit,
                          Engine::Core::EntityID target_id,
                          TargetSource source) -> Engine::Core::AttackTargetComponent* {
  auto* attack_target =
      Engine::Core::get_or_add_component<Engine::Core::AttackTargetComponent>(unit);
  if (attack_target != nullptr) {
    set_attack_target(*attack_target, unit, target_id, source);
  }
  return attack_target;
}

auto keeps_pursuing(Engine::Core::Entity* attacker,
                    Engine::Core::Entity* target) -> bool {
  auto const* attack_target =
      attacker->get_component<Engine::Core::AttackTargetComponent>();
  if ((attack_target == nullptr) || !attack_target->should_chase) {
    return false;
  }

  if (!pursues_targets(attacker) && !attack_target->is_player_command) {
    auto const* from = attacker->get_component<Engine::Core::TransformComponent>();
    auto const* to = target->get_component<Engine::Core::TransformComponent>();
    if ((from != nullptr) && (to != nullptr)) {
      float const leash =
          combat_radius(attacker) + combat_radius(target) + k_brawl_leash;
      if (std::hypot(to->position.x - from->position.x,
                     to->position.z - from->position.z) > leash) {
        return false;
      }
    }
  }

  return free_to_pursue(attacker) &&
         within_guard_reach(attacker, target, GuardReachRule::AnswersFire);
}

} // namespace Game::Systems::Combat
