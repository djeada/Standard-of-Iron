#include "combat_action_service.h"

#include <algorithm>

#include "../../core/component.h"
#include "../../core/world.h"
#include "../../units/spawn_type.h"
#include "combat_action_events.h"
#include "melee_intent_solver.h"

namespace Game::Systems::CombatActions {

namespace {

[[nodiscard]] auto should_request_ranged_action(
    const Engine::Core::AttackComponent* attack,
    const Engine::Core::RpgCommanderAimComponent* aim) -> bool {
  if (attack == nullptr || !attack->can_ranged) {
    return false;
  }

  if (aim != nullptr) {
    return aim->stance == Engine::Core::FpvWeaponStance::Bow;
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
  auto* commander = attacker->get_component<Engine::Core::CommanderComponent>();
  bool const player_driven = commander != nullptr && commander->fpv_controlled;

  bool const cancels_into_next_action =
      player_driven && active_action != nullptr && active_action->cancel_window_active;

  if (active_action != nullptr && active_action->action_running &&
      !cancels_into_next_action) {
    if (active_action->cancel_window_active) {
      active_action->input_buffered = true;

      if (combat_state != nullptr && !player_driven) {
        combat_state->input_buffered = true;
      }
      result.buffered = true;
    }
    result.accepted = true;
    return result;
  }
  if (combat_state != nullptr && !cancels_into_next_action &&
      combat_state->animation_state != Engine::Core::CombatAnimationState::Idle) {
    result.accepted = true;
    return result;
  }

  auto* unit = attacker->get_component<Engine::Core::UnitComponent>();
  auto* attack = attacker->get_component<Engine::Core::AttackComponent>();
  auto* aim = player_driven
                  ? attacker->get_component<Engine::Core::RpgCommanderAimComponent>()
                  : nullptr;
  bool const request_ranged_action = should_request_ranged_action(attack, aim);
  if (attack != nullptr && request_ranged_action) {
    attack->current_mode = Engine::Core::AttackComponent::CombatMode::Ranged;
  } else if (attack != nullptr && attack->can_melee) {
    attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  }

  if (combat_state == nullptr) {
    combat_state = attacker->add_component<Engine::Core::CombatStateComponent>();
  }

  bool const finisher_attack = commander != nullptr && commander->combo_step >= 3;

  Engine::Core::CombatAttackFamily attack_family =
      Engine::Core::CombatAttackFamily::None;
  if (unit != nullptr && attack != nullptr) {
    attack_family = Engine::Core::resolve_combat_attack_family(unit->spawn_type,
                                                               attack->current_mode);
  }

  CombatActionId action_id = CombatActionId::None;
  const CombatActionDefinition* definition = nullptr;
  Engine::Core::MeleeIntent swing{};
  bool swing_resolved = false;

  if (commander != nullptr && attack_family != Engine::Core::CombatAttackFamily::None &&
      attack_family != Engine::Core::CombatAttackFamily::Bow) {

    auto const* body =
        attacker->get_component<Engine::Core::CommanderBodyControlComponent>();
    swing = body != nullptr
                ? body->steered_intent
                : resolve_melee_intent({
                      .move_right_axis = request.move_right_axis,
                      .move_forward_axis = request.move_forward_axis,
                      .held_duration = request.primary_held_duration,
                      .reach = attack != nullptr ? attack->melee_range
                                                 : Engine::Core::k_melee_default_reach,
                      .prefer_thrust =
                          attack_family == Engine::Core::CombatAttackFamily::Spear,
                  });
    swing_resolved = true;

    action_id = select_melee_action(
        swing, attack_family, is_mounted_unit(unit), finisher_attack);
    definition = find_combat_action_definition(action_id);
  } else if (commander != nullptr &&
             attack_family == Engine::Core::CombatAttackFamily::Bow) {
    action_id = CombatActionId::RpgBowShot;
    definition = find_combat_action_definition(action_id);
    if (aim != nullptr) {
      aim->draw_stage = Engine::Core::BowDrawStage::Drawing;
      aim->draw_progress = 0.0F;
      aim->full_draw_hold = 0.0F;
      aim->shot_power = 0.0F;
      aim->relaxed_from_overhold = false;
    }
  }

  if (combat_state != nullptr) {
    combat_state->animation_state = Engine::Core::CombatAnimationState::Advance;
    combat_state->state_time = 0.0F;
    combat_state->state_duration =
        definition != nullptr
            ? authored_phase_duration(*definition,
                                      Engine::Core::CombatAnimationState::Advance)
            : Engine::Core::CombatStateComponent::k_advance_duration *
                  (finisher_attack ? 1.70F : 1.35F);
    combat_state->attack_family = attack_family;
    combat_state->finisher_attack = finisher_attack;
    if (swing_resolved) {
      combat_state->intent = swing;
    }

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
      action->active_target_soldier_slot = request.target_soldier_slot;
      action->action_duration =
          definition != nullptr ? definition->duration_seconds : 0.0F;
      reset_combat_action_event_runtime(*action);

      if (combat_state != nullptr && definition != nullptr) {
        combat_state->attack_variant = 0U;
        action->melee_attack_sequence =
            static_cast<std::uint8_t>((action->melee_attack_sequence + 1U) % 5U);
      }
    }

    if (request.primary_held_duration >= 0.4F &&
        action_id != CombatActionId::RpgBowShot) {
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
