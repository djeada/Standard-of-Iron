#include "combat_action_predicates.h"

#include <algorithm>

#include "../../core/world.h"
#include "../combat_rules.h"
#include "combat_utils.h"

namespace Game::Systems::Combat {

auto is_rts_melee_action(Game::Systems::CombatActions::CombatActionId id) -> bool {
  return id == Game::Systems::CombatActions::CombatActionId::RtsSwordStrike ||
         id == Game::Systems::CombatActions::CombatActionId::RtsHeavyOverhead ||
         id == Game::Systems::CombatActions::CombatActionId::RtsSpearThrust ||
         id == Game::Systems::CombatActions::CombatActionId::RtsElephantStomp ||
         id == Game::Systems::CombatActions::CombatActionId::RtsCommanderThrust ||
         id == Game::Systems::CombatActions::CombatActionId::RtsCommanderCut;
}

auto is_rts_attack_action(Game::Systems::CombatActions::CombatActionId id) -> bool {
  return is_rts_melee_action(id) ||
         id == Game::Systems::CombatActions::CombatActionId::RtsBowShot ||
         id == Game::Systems::CombatActions::CombatActionId::RtsCommanderShot;
}

auto action_swings_a_traced_weapon(
    const Game::Systems::CombatActions::CombatActionDefinition& definition) -> bool {
  return definition.weapon_family ==
             Game::Systems::CombatActions::WeaponFamily::Sword ||
         definition.weapon_family == Game::Systems::CombatActions::WeaponFamily::Spear;
}

auto is_advanced_rts_commander_melee(
    const Engine::Core::Entity& attacker,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) -> bool {
  auto const* commander = attacker.get_component<Engine::Core::CommanderComponent>();
  return commander != nullptr && !commander->fpv_controlled &&
         commander->advanced_combat_enabled && definition.commander_only &&
         action_swings_a_traced_weapon(definition);
}

auto commander_swings_under_player_control(
    const Engine::Core::CommanderComponent* commander,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) -> bool {
  return commander != nullptr && commander->fpv_controlled &&
         definition.commander_only && action_swings_a_traced_weapon(definition);
}

auto target_uses_rpg_combat(Engine::Core::World& world,
                            Engine::Core::EntityID target_id) -> bool {
  auto* target = world.get_entity(target_id);
  return target != nullptr && Game::Systems::CombatRules::uses_rpg_combat_rules(target);
}

auto rts_melee_reach(
    const Engine::Core::AttackComponent* attack,
    const Engine::Core::CommanderComponent* commander,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    bool advanced_commander_melee) -> float {
  float const weapon_reach =
      advanced_commander_melee
          ? std::max(attack != nullptr ? attack->melee_range : 0.0F,
                     definition.hit_shape.reach)
          : (attack != nullptr ? attack->melee_range : definition.hit_shape.reach);
  bool const signature_strike =
      commander != nullptr && commander->signature_strike_active;
  return weapon_reach +
         (signature_strike ? std::max(0.0F, commander->signature_bonus_reach) : 0.0F);
}

void halt_action(Engine::Core::RpgCommanderActionComponent& action) {
  action.action_running = false;
  action.action_completed = true;
  action.action_active = false;
  action.weapon_trace_active = false;
}

auto rts_melee_target_still_stands(
    Engine::Core::World& world,
    const Engine::Core::Entity& attacker,
    const Engine::Core::RpgCommanderActionComponent& action) -> bool {
  auto const* attacker_unit = attacker.get_component<Engine::Core::UnitComponent>();
  auto* target = world.get_entity(action.active_target_id);
  auto const* target_unit = target != nullptr
                                ? target->get_component<Engine::Core::UnitComponent>()
                                : nullptr;
  return attacker_unit != nullptr && attacker_unit->health > 0 && target != nullptr &&
         target_unit != nullptr && target_unit->health > 0 &&
         target_unit->owner_id != attacker_unit->owner_id &&
         target->get_component<Engine::Core::TransformComponent>() != nullptr &&
         !target->has_component<Engine::Core::PendingRemovalComponent>();
}

auto melee_contact_comes_from_the_sweep(
    Engine::Core::World& world,
    const Engine::Core::Entity& attacker,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    const Engine::Core::RpgCommanderActionComponent& action) -> bool {
  if (!action_swings_a_traced_weapon(definition)) {
    return false;
  }

  auto* target = Game::Systems::CombatRules::is_player_driven(&attacker)
                     ? nullptr
                     : world.get_entity(action.active_target_id);
  if (target != nullptr && !Game::Systems::CombatRules::is_player_driven(target)) {
    return false;
  }
  return !is_building(world.get_entity(action.active_target_id));
}

} // namespace Game::Systems::Combat
