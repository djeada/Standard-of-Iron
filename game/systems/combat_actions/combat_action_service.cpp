#include "combat_action_service.h"

#include <algorithm>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "combat_action_events.h"

namespace Game::Systems::CombatActions {

namespace {

[[nodiscard]] auto
select_rpg_sword_action_id(const Engine::Core::CommanderComponent* commander,
                           std::uint8_t attack_sequence,
                           int move_right_axis,
                           int move_forward_axis,
                           bool finisher_attack) -> CombatActionId {
  if (finisher_attack) {
    return CombatActionId::RpgSwordFinisher;
  }

  if (move_forward_axis > 0) {
    return CombatActionId::RpgSwordThrust;
  }

  if (move_forward_axis < 0 || (move_right_axis == 0 && commander != nullptr &&
                                commander->power_strike_active)) {
    return CombatActionId::RpgSwordOverhead;
  }

  switch (attack_sequence % 5U) {
  case 0:
    return CombatActionId::RpgSwordSlashLeft;
  case 1:
    return CombatActionId::RpgSwordSlashRight;
  case 2:
    return (move_right_axis > 0) ? CombatActionId::RpgSwordSlashRight
                                 : CombatActionId::RpgSwordSlashLeft;
  case 3:
    return (move_right_axis < 0) ? CombatActionId::RpgSwordSlashLeft
                                 : CombatActionId::RpgSwordSlashRight;
  case 4:
    return CombatActionId::RpgSwordOverhead;
  default:
    return CombatActionId::RpgSwordSlashLeft;
  }
}

[[nodiscard]] auto select_rpg_spear_action_id(int move_right_axis,
                                              int move_forward_axis,
                                              bool finisher_attack) -> CombatActionId {
  if (finisher_attack || move_forward_axis < 0 || move_right_axis != 0) {
    return CombatActionId::RpgSpearSweep;
  }
  return CombatActionId::RpgSpearThrust;
}

[[nodiscard]] auto
should_request_ranged_action(const Engine::Core::AttackComponent* attack) -> bool {
  if (attack == nullptr || !attack->can_ranged) {
    return false;
  }
  return !attack->can_melee ||
         attack->preferred_mode == Engine::Core::AttackComponent::CombatMode::Ranged;
}

[[nodiscard]] auto is_mounted_unit(const Engine::Core::UnitComponent* unit) -> bool {
  return unit != nullptr && Game::Units::is_cavalry(unit->spawn_type);
}

} // namespace

auto CombatActionService::request_attack(
    Engine::Core::World& world, const AttackRequest& request) -> AttackRequestResult {
  AttackRequestResult result;
  auto* attacker = world.get_entity(request.attacker_id);
  if (attacker == nullptr) {
    return result;
  }

  auto* combat_state = attacker->get_component<Engine::Core::CombatStateComponent>();
  auto* active_action =
      attacker->get_component<Engine::Core::RpgCommanderActionComponent>();
  if (active_action != nullptr && active_action->action_running) {
    if (active_action->cancel_window_active) {
      active_action->input_buffered = true;
      if (combat_state != nullptr) {
        combat_state->input_buffered = true;
      }
      result.buffered = true;
    }
    result.accepted = true;
    return result;
  }
  if (combat_state != nullptr &&
      combat_state->animation_state != Engine::Core::CombatAnimationState::Idle) {
    result.accepted = true;
    return result;
  }

  auto* unit = attacker->get_component<Engine::Core::UnitComponent>();
  auto* attack = attacker->get_component<Engine::Core::AttackComponent>();
  bool const request_ranged_action = should_request_ranged_action(attack);
  if (attack != nullptr && request_ranged_action) {
    attack->current_mode = Engine::Core::AttackComponent::CombatMode::Ranged;
  } else if (attack != nullptr && attack->can_melee) {
    attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  }

  if (combat_state == nullptr) {
    combat_state = attacker->add_component<Engine::Core::CombatStateComponent>();
  }

  auto* commander = attacker->get_component<Engine::Core::CommanderComponent>();
  bool const finisher_attack = commander != nullptr && commander->combo_step >= 3;

  Engine::Core::CombatAttackFamily attack_family =
      Engine::Core::CombatAttackFamily::None;
  if (unit != nullptr && attack != nullptr) {
    attack_family = Engine::Core::resolve_combat_attack_family(unit->spawn_type,
                                                               attack->current_mode);
  }

  CombatActionId action_id = CombatActionId::None;
  const CombatActionDefinition* definition = nullptr;
  if (commander != nullptr &&
      attack_family == Engine::Core::CombatAttackFamily::Sword) {
    if (is_mounted_unit(unit)) {
      action_id = CombatActionId::MountedSwordSlash;
    } else {
      auto* action =
          attacker->get_component<Engine::Core::RpgCommanderActionComponent>();
      action_id = select_rpg_sword_action_id(
          commander,
          action != nullptr ? action->melee_attack_sequence : 0U,
          request.move_right_axis,
          request.move_forward_axis,
          finisher_attack);
    }
    definition = find_combat_action_definition(action_id);
  } else if (commander != nullptr &&
             attack_family == Engine::Core::CombatAttackFamily::Spear) {
    action_id = is_mounted_unit(unit)
                    ? CombatActionId::MountedSpearThrust
                    : select_rpg_spear_action_id(request.move_right_axis,
                                                 request.move_forward_axis,
                                                 finisher_attack);
    definition = find_combat_action_definition(action_id);
  } else if (commander != nullptr &&
             attack_family == Engine::Core::CombatAttackFamily::Bow) {
    action_id = CombatActionId::RpgBowShot;
    definition = find_combat_action_definition(action_id);
  }

  if (combat_state != nullptr) {
    combat_state->animation_state = Engine::Core::CombatAnimationState::Advance;
    combat_state->state_time = 0.0F;
    combat_state->state_duration =
        Engine::Core::CombatStateComponent::k_advance_duration *
        (finisher_attack ? 1.70F : 1.35F);
    combat_state->attack_family = attack_family;
    combat_state->finisher_attack = finisher_attack;

    static std::uint8_t s_fpv_attack_seed = 0;
    combat_state->attack_offset = static_cast<float>(s_fpv_attack_seed % 7) * 0.022F;
    combat_state->attack_variant =
        s_fpv_attack_seed %
        Engine::Core::CombatStateComponent::k_attack_variant_seed_slots;
    ++s_fpv_attack_seed;
  }

  if (commander != nullptr) {
    if (auto* action = Engine::Core::get_or_add_component<
            Engine::Core::RpgCommanderActionComponent>(attacker)) {
      action->phase = Engine::Core::RpgCommanderActionPhase::Strike;
      action->combat_action_id = static_cast<std::uint8_t>(action_id);
      action->active_target_id = request.target_hint_id;
      action->action_duration =
          definition != nullptr ? definition->duration_seconds : 0.0F;
      reset_combat_action_event_runtime(*action);

      if (combat_state != nullptr && definition != nullptr) {
        combat_state->attack_variant = 0U;
        combat_state->attack_direction = definition->attack_direction;
        action->melee_attack_sequence =
            static_cast<std::uint8_t>((action->melee_attack_sequence + 1U) % 5U);
      }
    }

    if (request.primary_held_duration >= 0.4F) {
      commander->power_strike_active = true;
    }
  }

  if (auto* stamina = attacker->get_component<Engine::Core::StaminaComponent>()) {
    bool const heavy = commander != nullptr && commander->power_strike_active;
    float const cost =
        definition != nullptr
            ? (heavy ? definition->heavy_stamina_cost : definition->light_stamina_cost)
            : (heavy ? Engine::Core::CombatStateComponent::k_stamina_cost_heavy_attack
                     : Engine::Core::CombatStateComponent::k_stamina_cost_light_attack);
    stamina->stamina = std::max(0.0F, stamina->stamina - cost);
  }

  if (combat_state != nullptr) {
    combat_state->damage_dealt_this_swing = false;
  }

  result.accepted = true;
  result.action_id = action_id;
  result.definition = definition;
  return result;
}

} // namespace Game::Systems::CombatActions
