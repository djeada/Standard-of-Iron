#include "melee_intent_solver.h"

#include <algorithm>
#include <cmath>

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

[[nodiscard]] auto
steer_authority_for(Engine::Core::CombatAnimationState phase) noexcept -> float {
  using Body = Engine::Core::CommanderBodyControlComponent;
  using CS = Engine::Core::CombatAnimationState;
  switch (phase) {
  case CS::Idle:
  case CS::Advance:
  case CS::WindUp:
    return Body::k_windup_steer_authority;
  case CS::Strike:
  case CS::Impact:
    return Body::k_strike_steer_authority;
  case CS::Recover:
  case CS::Reposition:
    break;
  }
  return Body::k_recover_steer_authority;
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

namespace {

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
}

} // namespace

void advance_melee_control(Engine::Core::Entity& fighter,
                           const MeleeControlTick& tick) noexcept {
  using Body = Engine::Core::CommanderBodyControlComponent;
  auto* body = Engine::Core::get_or_add_component<Body>(&fighter);
  if (body == nullptr) {
    return;
  }

  auto* combat = fighter.get_component<Engine::Core::CombatStateComponent>();
  auto const phase = combat != nullptr ? combat->animation_state
                                       : Engine::Core::CombatAnimationState::Idle;
  bool const idle = phase == Engine::Core::CombatAnimationState::Idle;
  float const dt = std::max(tick.delta_time, 0.0F);

  constexpr float k_filter_hz = 12.0F;
  float const smoothing = dt > 0.0F ? 1.0F - std::exp(-dt * k_filter_hz) : 1.0F;

  if (idle && body->swing_in_flight) {
    latch_melee_rest(fighter);
  }
  body->swing_in_flight = !idle;

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

  float const authority = steer_authority_for(phase);
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
