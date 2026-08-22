#include "melee_intent_solver.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "../../core/component.h"

namespace Game::Systems::CombatActions {

namespace {

[[nodiscard]] auto smoothstep(float value) noexcept -> float {
  float const t = std::clamp(value, 0.0F, 1.0F);
  return t * t * (3.0F - (2.0F * t));
}

[[nodiscard]] auto resolve_chamber(const MeleeIntentInputs& inputs) noexcept
    -> Engine::Core::MeleeRestDirection {
  Engine::Core::MeleeRestDirection const guard{0.80F, 0.60F};
  if (!inputs.has_rest) {
    return guard;
  }

  constexpr float k_guard_recovery = 0.34F;
  Engine::Core::MeleeRestDirection chamber{
      std::lerp(inputs.rest_dir_x, guard.x, k_guard_recovery),
      std::lerp(inputs.rest_dir_y, guard.y, k_guard_recovery)};
  float const length = std::sqrt((chamber.x * chamber.x) + (chamber.y * chamber.y));
  if (length < Engine::Core::k_melee_intent_min_axis) {
    return guard;
  }
  chamber.x /= length;
  chamber.y /= length;
  return chamber;
}

[[nodiscard]] auto melee_family_of(const Engine::Core::Entity& fighter) noexcept
    -> Engine::Core::CombatAttackFamily {
  auto const* unit = fighter.get_component<Engine::Core::UnitComponent>();
  auto const* attack = fighter.get_component<Engine::Core::AttackComponent>();
  if (unit == nullptr || attack == nullptr) {
    return Engine::Core::CombatAttackFamily::None;
  }
  return Engine::Core::resolve_combat_attack_family(unit->spawn_type,
                                                    attack->current_mode);
}

[[nodiscard]] auto
melee_reach_of(const Engine::Core::Entity& fighter) noexcept -> float {
  auto const* attack = fighter.get_component<Engine::Core::AttackComponent>();
  return attack != nullptr ? attack->melee_range : Engine::Core::k_melee_default_reach;
}

} // namespace

auto resolve_melee_intent(const MeleeIntentInputs& inputs) noexcept
    -> Engine::Core::MeleeIntent {
  Engine::Core::MeleeIntent intent{};

  auto const chamber = resolve_chamber(inputs);
  intent.windup_dir_x = chamber.x;
  intent.windup_dir_y = chamber.y;

  float const drag_length = std::sqrt((inputs.aim_delta_x * inputs.aim_delta_x) +
                                      (inputs.aim_delta_y * inputs.aim_delta_y));
  float const sweep = smoothstep(drag_length / k_melee_full_sweep_drag);

  if (drag_length > Engine::Core::k_melee_intent_min_axis) {

    intent.strike_dir_x = inputs.aim_delta_x / drag_length;
    intent.strike_dir_y = inputs.aim_delta_y / drag_length;
  } else {

    intent.strike_dir_x =
        -chamber.x + (static_cast<float>(inputs.move_right_axis) * 0.45F);
    intent.strike_dir_y = -chamber.y;
  }

  float thrust = 0.0F;
  if (inputs.move_forward_axis > 0) {
    thrust += 0.60F;
  }
  if (inputs.move_forward_axis < 0) {
    thrust -= 0.35F;
  }
  if (inputs.prefer_thrust) {
    thrust += 0.75F;
  }

  intent.thrust_amount = std::clamp(thrust * (1.0F - (0.85F * sweep)), 0.0F, 1.0F);

  intent.charge =
      std::clamp(inputs.held_duration / k_melee_full_charge_seconds, 0.0F, 1.0F);

  constexpr float k_rate_gain = 0.55F;
  intent.swing_speed =
      std::clamp(0.80F + (inputs.aim_rate * k_rate_gain), 0.45F, 2.10F);

  intent.follow_through =
      std::clamp(0.32F + (0.46F * sweep) + (0.22F * intent.charge), 0.0F, 1.0F);

  constexpr float k_pitch_to_metres = 1.0F / 55.0F;
  float const strike_rise =
      intent.strike_dir_y /
      std::max(std::sqrt((intent.strike_dir_x * intent.strike_dir_x) +
                         (intent.strike_dir_y * intent.strike_dir_y)),
               Engine::Core::k_melee_intent_min_axis);
  intent.elevation =
      (inputs.view_pitch_degrees * k_pitch_to_metres) + (strike_rise * 0.38F * sweep);

  Engine::Core::complete_melee_intent(intent, inputs.reach);
  return intent;
}

auto steer_melee_intent(const Engine::Core::MeleeIntent& current,
                        const Engine::Core::MeleeIntent& desired,
                        float authority,
                        float max_delta) noexcept -> Engine::Core::MeleeIntent {
  float weight = std::clamp(authority, 0.0F, 1.0F);
  if (weight <= 0.0F) {
    return current;
  }

  float const angular_gap = Engine::Core::melee_intent_strike_delta(current, desired);
  if (angular_gap * weight > max_delta && angular_gap > 0.0F) {
    weight = std::min(weight, max_delta / angular_gap);
  }

  Engine::Core::MeleeIntent result =
      Engine::Core::blend_melee_intent(current, desired, weight);

  result.charge = std::max(current.charge, desired.charge);
  return result;
}

auto select_melee_action(const Engine::Core::MeleeIntent& intent,
                         Engine::Core::CombatAttackFamily family,
                         bool mounted,
                         bool finisher) noexcept -> CombatActionId {
  auto const direction = Engine::Core::classify_attack_direction(intent);

  switch (family) {
  case Engine::Core::CombatAttackFamily::Sword:
    if (mounted) {
      return CombatActionId::MountedSwordSlash;
    }
    if (finisher) {
      return CombatActionId::RpgSwordFinisher;
    }
    switch (direction) {
    case Engine::Core::AttackDirection::Thrust:
      return CombatActionId::RpgSwordThrust;
    case Engine::Core::AttackDirection::Overhead:
    case Engine::Core::AttackDirection::HeavyOverhead:
      return CombatActionId::RpgSwordOverhead;
    case Engine::Core::AttackDirection::RightSlash:
      return CombatActionId::RpgSwordSlashRight;
    case Engine::Core::AttackDirection::LeftSlash:
      break;
    }
    return CombatActionId::RpgSwordSlashLeft;

  case Engine::Core::CombatAttackFamily::Spear:
    if (mounted) {
      return CombatActionId::MountedSpearThrust;
    }
    if (finisher) {
      return CombatActionId::RpgSpearFinisher;
    }
    return direction == Engine::Core::AttackDirection::Thrust
               ? CombatActionId::RpgSpearThrust
               : CombatActionId::RpgSpearSweep;

  case Engine::Core::CombatAttackFamily::Bow:
    return CombatActionId::RpgBowShot;

  case Engine::Core::CombatAttackFamily::None:
    break;
  }
  return CombatActionId::None;
}

auto resolve_guard_direction(const MeleeControlTick& tick) noexcept
    -> Engine::Core::MeleeRestDirection {
  float const dt = std::max(tick.delta_time, 0.0F);

  constexpr float k_settled_guard_x = 0.0F;
  constexpr float k_settled_guard_y = 1.0F;
  if (!tick.guard_held) {
    return {k_settled_guard_x, k_settled_guard_y};
  }

  constexpr float k_full_lean_turn_rate = 240.0F;
  float const turn_rate = dt > 0.0F ? tick.aim_delta_x / dt : 0.0F;
  float const lean =
      (std::clamp(turn_rate / k_full_lean_turn_rate, -1.0F, 1.0F) * 0.75F) +
      (static_cast<float>(tick.move_right_axis) * 0.35F);

  constexpr float k_pitch_to_height = 1.0F / 60.0F;
  float const height =
      std::clamp(0.90F + (tick.view_pitch_degrees * k_pitch_to_height), 0.18F, 1.0F);

  Engine::Core::MeleeRestDirection guard{std::clamp(lean, -1.0F, 1.0F), height};
  float const length = std::sqrt((guard.x * guard.x) + (guard.y * guard.y));
  if (length < Engine::Core::k_melee_intent_min_axis) {
    return {k_settled_guard_x, k_settled_guard_y};
  }
  guard.x /= length;
  guard.y /= length;
  return guard;
}

namespace {

void advance_guard_direction(Engine::Core::CommanderGuardComponent& guard,
                             const MeleeControlTick& tick) noexcept {
  float const dt = std::max(tick.delta_time, 0.0F);
  auto const desired = resolve_guard_direction(tick);

  float const current_angle = std::atan2(guard.guard_dir_y, guard.guard_dir_x);
  float const desired_angle = std::atan2(desired.y, desired.x);
  float gap = desired_angle - current_angle;
  constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;
  gap -= k_two_pi * std::round(gap / k_two_pi);

  float const step = std::clamp(gap, -k_guard_slew_rate * dt, k_guard_slew_rate * dt);
  float const angle = current_angle + step;
  guard.guard_dir_x = std::cos(angle);
  guard.guard_dir_y = std::sin(angle);
  guard.guard_turn_rate = dt > 0.0F ? std::abs(step) / dt : 0.0F;
}

void latch_melee_rest(Engine::Core::Entity& fighter) noexcept {
  auto const* combat = fighter.get_component<Engine::Core::CombatStateComponent>();
  auto* body = fighter.get_component<Engine::Core::CommanderBodyControlComponent>();
  if (combat == nullptr || body == nullptr) {
    return;
  }
  auto const rest = Engine::Core::melee_intent_resting_direction(combat->intent);
  body->rest_dir_x = rest.x;
  body->rest_dir_y = rest.y;
  body->rest_valid = true;
  body->steer_x = 0.0F;
  body->steer_y = 0.0F;

  auto const finished = Engine::Core::normalized_melee_intent(combat->intent);
  body->last_strike_dir_x = finished.strike_dir_x;
  body->last_strike_dir_y = finished.strike_dir_y;
  body->chain_window_remaining =
      Engine::Core::CommanderBodyControlComponent::k_chain_window_seconds;
}

} // namespace

void advance_melee_control(Engine::Core::Entity& fighter,
                           const MeleeControlTick& tick) noexcept {
  using Body = Engine::Core::CommanderBodyControlComponent;
  auto* body = Engine::Core::get_or_add_component<Body>(&fighter);
  if (body == nullptr) {
    return;
  }

  if (auto* guard = fighter.get_component<Engine::Core::CommanderGuardComponent>()) {
    advance_guard_direction(*guard, tick);
  }

  auto* combat = fighter.get_component<Engine::Core::CombatStateComponent>();

  auto const* action =
      fighter.get_component<Engine::Core::RpgCommanderActionComponent>();
  bool const swinging =
      action != nullptr && action->action_running && action->combat_action_id != 0U;
  bool const idle = !swinging;

  float redirect_authority = 1.0F;
  if (swinging) {
    if (auto const* definition = find_combat_action_definition(
            static_cast<CombatActionId>(action->combat_action_id));
        definition != nullptr) {
      redirect_authority =
          melee_interruption_at(*definition, action->normalized_action_time)
              .redirect_authority;
    }
  }
  float const dt = std::max(tick.delta_time, 0.0F);

  constexpr float k_filter_hz = 12.0F;
  float const smoothing = dt > 0.0F ? 1.0F - std::exp(-dt * k_filter_hz) : 1.0F;

  if (idle && body->swing_in_flight) {
    latch_melee_rest(fighter);
  }
  body->swing_in_flight = !idle;

  if (idle) {
    body->chain_window_remaining = std::max(0.0F, body->chain_window_remaining - dt);
  }

  constexpr float k_sweep_scale = 1.0F / Body::k_sweep_degrees;
  if (tick.primary_held) {
    body->steer_x += tick.aim_delta_x * k_sweep_scale;
    body->steer_y += tick.aim_delta_y * k_sweep_scale;
  } else if (idle) {
    body->steer_x = 0.0F;
    body->steer_y = 0.0F;
  }

  float const tick_motion = std::sqrt((tick.aim_delta_x * tick.aim_delta_x) +
                                      (tick.aim_delta_y * tick.aim_delta_y)) *
                            k_sweep_scale;
  float const instantaneous_rate = dt > 0.0F ? tick_motion / dt : 0.0F;
  body->steer_rate = std::lerp(body->steer_rate, instantaneous_rate, smoothing);

  body->steered_intent = resolve_melee_intent({
      .aim_delta_x = body->steer_x,
      .aim_delta_y = body->steer_y,
      .aim_rate = body->steer_rate,
      .move_right_axis = tick.move_right_axis,
      .move_forward_axis = tick.move_forward_axis,
      .held_duration = tick.held_duration,
      .view_pitch_degrees = tick.view_pitch_degrees,
      .reach = melee_reach_of(fighter),
      .has_rest = body->rest_valid,
      .rest_dir_x = body->rest_dir_x,
      .rest_dir_y = body->rest_dir_y,
      .prefer_thrust =
          melee_family_of(fighter) == Engine::Core::CombatAttackFamily::Spear,
  });

  float const authority = redirect_authority;
  if (combat != nullptr) {
    if (idle) {

      combat->intent = body->steered_intent;
    } else if (authority > 0.0F) {
      combat->intent = steer_melee_intent(combat->intent,
                                          body->steered_intent,
                                          authority * smoothing,
                                          Body::k_max_steer_rate * dt);
    }
  }
}

} // namespace Game::Systems::CombatActions
