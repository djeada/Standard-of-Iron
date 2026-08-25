#include "combat_action_service.h"

#include <algorithm>

#include "../../audio/cue_ids.h"
#include "../../core/component.h"
#include "../../core/event_manager.h"
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

namespace {

[[nodiscard]] auto running_definition_of(const Engine::Core::Entity& attacker)
    -> const CombatActionDefinition* {
  auto const* action =
      attacker.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action == nullptr || !action->action_running || action->combat_action_id == 0U) {
    return nullptr;
  }
  return find_combat_action_definition(
      static_cast<CombatActionId>(action->combat_action_id));
}

void record_outcome(Engine::Core::Entity& attacker,
                    Engine::Core::CombatIntentOutcome outcome) {
  if (auto* queue =
          attacker.get_component<Engine::Core::CombatIntentQueueComponent>()) {
    queue->record(outcome);
  }
}

} // namespace

void expire_stale_intents(Engine::Core::CombatIntentQueueComponent& queue,
                          float delta_time) {
  queue.clock += delta_time;
  queue.last_outcome_age += delta_time;
  std::uint8_t kept = 0;
  for (std::uint8_t i = 0; i < queue.count; ++i) {
    if (queue.clock - queue.entries[i].pressed_at <=
        Engine::Core::CombatIntentQueueComponent::k_intent_lifetime) {
      queue.entries[kept++] = queue.entries[i];
    }
  }
  if (kept < queue.count) {
    queue.count = kept;
    queue.record(Engine::Core::CombatIntentOutcome::Expired);
    return;
  }
  queue.count = kept;
}

auto CombatActionService::request_attack(
    Engine::Core::World& world, const AttackRequest& request) -> AttackRequestResult {
  AttackRequestResult result;
  auto* attacker = world.get_entity(request.attacker_id);
  if (attacker == nullptr) {
    result.outcome = Engine::Core::CombatIntentOutcome::NoFighter;
    return result;
  }

  auto* combat_state = attacker->get_component<Engine::Core::CombatStateComponent>();
  auto* active_action =
      attacker->get_component<Engine::Core::RpgCommanderActionComponent>();
  auto* commander = attacker->get_component<Engine::Core::CommanderComponent>();

  MeleeInterruption interruption;
  if (auto const* running = running_definition_of(*attacker); running != nullptr) {
    interruption =
        melee_interruption_at(*running, active_action->normalized_action_time);
  }

  if (active_action != nullptr && active_action->action_running &&
      !interruption.accepts_attack) {
    result.buffered = true;
    result.accepted = true;
    switch (interruption.phase) {
    case MeleePhase::CommittedStrike:
      result.outcome = Engine::Core::CombatIntentOutcome::Committed;
      break;
    case MeleePhase::EarlyStrike:
    case MeleePhase::FollowThrough:
      result.outcome = Engine::Core::CombatIntentOutcome::Recovering;
      break;
    default:
      result.outcome = Engine::Core::CombatIntentOutcome::Buffered;
      break;
    }
    record_outcome(*attacker, result.outcome);
    return result;
  }
  if (combat_state != nullptr && !interruption.accepts_attack &&
      combat_state->animation_state != Engine::Core::CombatAnimationState::Idle) {
    result.accepted = true;
    result.buffered = true;
    result.outcome = Engine::Core::CombatIntentOutcome::Recovering;
    record_outcome(*attacker, result.outcome);
    return result;
  }

  if (auto const* guard =
          attacker->get_component<Engine::Core::CommanderGuardComponent>();
      guard != nullptr && guard->guard_break_remaining > 0.0F) {
    result.outcome = Engine::Core::CombatIntentOutcome::GuardBroken;
    record_outcome(*attacker, result.outcome);
    return result;
  }
  if (attacker->has_component<Engine::Core::StaggerComponent>()) {
    result.outcome = Engine::Core::CombatIntentOutcome::Staggered;
    record_outcome(*attacker, result.outcome);
    return result;
  }

  auto* unit = attacker->get_component<Engine::Core::UnitComponent>();
  auto* attack = attacker->get_component<Engine::Core::AttackComponent>();
  auto* aim = attacker->get_component<Engine::Core::RpgCommanderAimComponent>();
  bool const request_ranged_action = should_request_ranged_action(attack, aim);
  if (attack != nullptr && request_ranged_action) {
    attack->current_mode = Engine::Core::AttackComponent::CombatMode::Ranged;
  } else if (attack != nullptr && attack->can_melee) {
    attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  }

  if (combat_state == nullptr) {
    combat_state = attacker->add_component<Engine::Core::CombatStateComponent>();
  }

  auto* body = attacker->get_component<Engine::Core::CommanderBodyControlComponent>();
  if (commander != nullptr && body != nullptr) {
    using Body = Engine::Core::CommanderBodyControlComponent;
    auto const line = Engine::Core::normalized_melee_intent(body->steered_intent);
    float const along = (line.strike_dir_x * body->last_strike_dir_x) +
                        (line.strike_dir_y * body->last_strike_dir_y);
    bool const carries_on = body->chain_window_remaining > 0.0F &&
                            commander->just_struck_enemy &&
                            along < Body::k_chain_new_line_dot;
    commander->combo_step = carries_on ? std::min(commander->combo_step + 1, 3) : 0;
    body->chain_window_remaining = Body::k_chain_window_seconds;
    commander->just_struck_enemy = false;
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
    swing = request.has_swing ? request.swing
            : body != nullptr
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

  auto* stamina = attacker->get_component<Engine::Core::StaminaComponent>();
  bool const heavy_request =
      request.primary_held_duration >= 0.4F && action_id != CombatActionId::RpgBowShot;
  float const stamina_cost =
      definition != nullptr
          ? (heavy_request ? definition->heavy_stamina_cost
                           : definition->light_stamina_cost)
          : (heavy_request
                 ? Engine::Core::CombatStateComponent::k_stamina_cost_heavy_attack
                 : Engine::Core::CombatStateComponent::k_stamina_cost_light_attack);
  bool tired_swing = false;
  if (stamina != nullptr) {
    if (stamina->stamina < stamina_cost) {

      auto const* queue =
          attacker->get_component<Engine::Core::CombatIntentQueueComponent>();
      bool const already_refused =
          queue != nullptr &&
          queue->last_outcome == Engine::Core::CombatIntentOutcome::InsufficientStamina;
      result.outcome = Engine::Core::CombatIntentOutcome::InsufficientStamina;
      record_outcome(*attacker, result.outcome);
      if (!already_refused) {
        Engine::Core::EventManager::instance().publish(
            Engine::Core::AudioCueEvent(Game::Audio::Cue::k_combat_ability_refused));
      }
      return result;
    }
    tired_swing =
        stamina->stamina <
        Engine::Core::CombatStateComponent::k_low_stamina_threshold + stamina_cost;
  }

  if (tired_swing && swing_resolved) {

    swing.swing_speed = std::max(0.45F, swing.swing_speed * 0.72F);
    swing.follow_through = std::max(0.0F, swing.follow_through * 0.55F);
    Engine::Core::complete_melee_intent(
        swing,
        attack != nullptr ? attack->melee_range : Engine::Core::k_melee_default_reach);
  }

  if (combat_state != nullptr) {
    combat_state->tired_swing = tired_swing;
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

      float const swing_speed =
          swing_resolved ? std::clamp(swing.swing_speed, 0.55F, 1.85F) : 1.0F;
      action->action_duration =
          definition != nullptr ? definition->duration_seconds / swing_speed : 0.0F;
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

  if (stamina != nullptr) {
    bool const heavy = commander != nullptr && commander->power_strike_active;
    float const cost =
        definition != nullptr
            ? (heavy ? definition->heavy_stamina_cost : definition->light_stamina_cost)
            : (heavy ? Engine::Core::CombatStateComponent::k_stamina_cost_heavy_attack
                     : Engine::Core::CombatStateComponent::k_stamina_cost_light_attack);
    stamina->spend(cost);
  }

  if (combat_state != nullptr) {
    combat_state->damage_dealt_this_swing = false;
  }

  result.accepted = true;
  result.outcome = Engine::Core::CombatIntentOutcome::Accepted;
  record_outcome(*attacker, result.outcome);
  result.action_id = action_id;
  result.definition = definition;
  return result;
}

} // namespace Game::Systems::CombatActions
