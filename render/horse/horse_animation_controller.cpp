#include "horse_animation_controller.h"
#include "../humanoid/humanoid_renderer_base.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::GL {

namespace {
constexpr float k_pi = std::numbers::pi_v<float>;

constexpr float k_idle_breathing_primary_freq = 0.4F;
constexpr float k_idle_breathing_secondary_freq = 0.15F;
constexpr float k_idle_breathing_primary_weight = 0.6F;
constexpr float k_idle_breathing_secondary_weight = 0.4F;
constexpr float k_idle_phase_speed = 0.15F;

auto bob_scale_for_gait(GaitType gait) noexcept -> float {
  switch (gait) {
  case GaitType::IDLE:
    return 0.30F;
  case GaitType::WALK:
    return 0.50F;
  case GaitType::TROT:
    return 0.85F;
  case GaitType::CANTER:
    return 1.00F;
  case GaitType::GALLOP:
    return 1.12F;
  }
  return 0.30F;
}

} // namespace

HorseAnimationController::HorseAnimationController(
    const HorseProfile &profile, const AnimationInputs &anim,
    const HumanoidAnimationContext &rider_ctx)
    : m_profile(profile), m_anim(anim), m_rider_ctx(rider_ctx) {

  m_phase = 0.0F;
  m_bob = 0.0F;
  m_rein_slack = 0.05F;
  m_speed = 0.0F;
  m_turn_angle = 0.0F;
  m_banking = 0.0F;
  m_is_rearing = false;
  m_rear_height = 0.0F;
  m_is_kicking = false;
  m_kick_rear_legs = false;
  m_kick_power = 0.0F;
  m_is_bucking = false;
  m_buck_intensity = 0.0F;
  m_is_jumping = false;
  m_jump_height = 0.0F;
  m_jump_distance = 0.0F;
  m_target_gait = GaitType::IDLE;
  m_gait_transition_progress = 1.0F;
  m_transition_start_time = 0.0F;
  m_resolved_gait = gait_for_type(GaitType::IDLE, m_profile.gait);
}

void HorseAnimationController::set_gait(GaitType gait) {
  m_current_gait = gait;
  m_target_gait = gait;
  m_gait_transition_progress = 1.0F;
  m_idle_bob_intensity = 1.0F;

  switch (gait) {
  case GaitType::IDLE:
    m_speed = 0.0F;
    break;
  case GaitType::WALK:
    m_speed = 1.5F;
    break;
  case GaitType::TROT:
    m_speed = 4.0F;
    break;
  case GaitType::CANTER:
    m_speed = 6.5F;
    break;
  case GaitType::GALLOP:
    m_speed = 10.0F;
    break;
  }

  update_gait_parameters();
}

void HorseAnimationController::idle(float bob_intensity) {
  m_current_gait = GaitType::IDLE;
  m_target_gait = GaitType::IDLE;
  m_gait_transition_progress = 1.0F;
  m_speed = 0.0F;
  m_idle_bob_intensity = std::clamp(bob_intensity, 0.0F, 1.5F);

  update_gait_parameters();
}

void HorseAnimationController::accelerate(float speed_delta) {
  m_speed += speed_delta;
  m_speed = std::max(0.0F, m_speed);

  GaitType const new_gait = gait_from_speed(m_speed);

  if (m_current_gait != new_gait) {
    m_target_gait = new_gait;
    m_gait_transition_progress = 0.0F;
    m_transition_start_time = m_anim.time;
  }

  update_gait_parameters();
}

void HorseAnimationController::decelerate(float speed_delta) {
  accelerate(-speed_delta);
}

void HorseAnimationController::turn(float yaw_radians, float banking_amount) {
  m_turn_angle = yaw_radians;
  m_banking = std::clamp(banking_amount, -1.0F, 1.0F);
}

void HorseAnimationController::strafe_step(bool left, float distance) {

  float const direction = left ? -1.0F : 1.0F;
  m_phase = std::fmod(m_phase + direction * distance * 0.1F, 1.0F);
}

void HorseAnimationController::rear(float height_factor) {
  m_is_rearing = true;
  m_rear_height = std::clamp(height_factor, 0.0F, 1.0F);
}

void HorseAnimationController::kick(bool rear_legs, float power) {
  m_is_kicking = true;
  m_kick_rear_legs = rear_legs;
  m_kick_power = std::clamp(power, 0.0F, 1.0F);
}

void HorseAnimationController::buck(float intensity) {
  m_is_bucking = true;
  m_buck_intensity = std::clamp(intensity, 0.0F, 1.0F);
}

void HorseAnimationController::jump_obstacle(float height, float distance) {
  m_is_jumping = true;
  m_jump_height = std::max(0.0F, height);
  m_jump_distance = std::max(0.0F, distance);
}

auto HorseAnimationController::get_current_phase() const -> float {
  return m_phase;
}

auto HorseAnimationController::get_current_bob() const -> float {
  return m_bob;
}

auto HorseAnimationController::get_stride_cycle() const -> float {
  return m_resolved_gait.cycle_time;
}

auto HorseAnimationController::get_resolved_gait() const -> const HorseGait & {
  return m_resolved_gait;
}

void HorseAnimationController::update_gait_parameters() {
  constexpr float transition_duration = 0.3F;
  if (m_gait_transition_progress < 1.0F) {
    float const elapsed = m_anim.time - m_transition_start_time;
    m_gait_transition_progress = std::min(1.0F, elapsed / transition_duration);
    if (m_gait_transition_progress >= 1.0F) {
      m_current_gait = m_target_gait;
    }
  }

  HorseGait const current_gait = gait_for_type(m_current_gait, m_profile.gait);
  HorseGait target_gait = current_gait;

  if (m_gait_transition_progress < 1.0F) {
    target_gait = gait_for_type(m_target_gait, m_profile.gait);
    float const t = m_gait_transition_progress;
    float const ease_t = t * t * (3.0F - 2.0F * t);
    m_resolved_gait = blend_gaits(current_gait, target_gait, ease_t);
  } else {
    m_resolved_gait = current_gait;
  }

  bool const is_moving =
      m_current_gait != GaitType::IDLE || m_target_gait != GaitType::IDLE;
  float const phase_offset = m_resolved_gait.phase_offset;

  if (is_moving) {
    if (m_rider_ctx.gait.cycle_time > 0.0001F) {
      // Rider drives gait — keep horse synchronized so reins/saddle align.
      m_phase = m_rider_ctx.gait.cycle_phase;
    } else {
      m_phase = std::fmod(m_anim.time / m_resolved_gait.cycle_time + phase_offset,
                          1.0F);
    }

    float const rider_intensity = std::max(
        m_rider_ctx.locomotion_normalized_speed(),
        std::clamp(m_speed / 10.0F, 0.0F, 1.0F));
    float const base_bob_amp =
        m_profile.dims.idle_bob_amplitude +
        rider_intensity * (m_profile.dims.move_bob_amplitude -
                           m_profile.dims.idle_bob_amplitude);
    float const bob_amp =
        base_bob_amp * bob_scale_for_gait(m_target_gait);

    float const primary_bob = std::sin(m_phase * 2.0F * k_pi);
    float const secondary_bob = std::sin(m_phase * 4.0F * k_pi) * 0.25F;
    float const tertiary_variation =
        std::sin(m_anim.time * 0.7F + phase_offset * k_pi) * 0.08F + 1.0F;

    float bob_pattern = primary_bob;
    if (m_current_gait == GaitType::TROT) {
      // Diagonal-pair trot: stronger 2x harmonic.
      bob_pattern = (primary_bob + secondary_bob * 0.5F);
    } else if (m_current_gait == GaitType::CANTER) {
      // 3-beat lift pattern.
      bob_pattern = primary_bob + std::sin(m_phase * 3.0F * k_pi) * 0.15F;
    } else if (m_current_gait == GaitType::GALLOP) {
      // Suspension phase amplifies primary bob.
      bob_pattern = primary_bob * 1.2F + secondary_bob * 0.3F;
    }

    m_bob = bob_pattern * bob_amp * tertiary_variation;
  } else {
    // Slow horse breathing (~0.5 Hz primary) + low-frequency weight shift +
    // an irregular ear flick / muzzle twitch derived from per-unit offset.
    float const breathing =
        std::sin((m_anim.time + phase_offset * 2.0F) *
                 k_idle_breathing_primary_freq * 2.0F * k_pi) *
            k_idle_breathing_primary_weight +
        std::sin((m_anim.time + phase_offset) *
                 k_idle_breathing_secondary_freq * 2.0F * k_pi) *
            k_idle_breathing_secondary_weight;
    float const weight_shift =
        std::sin(m_anim.time * 0.18F + phase_offset * 3.0F) * 0.20F;
    m_phase = std::fmod(m_anim.time * k_idle_phase_speed + phase_offset, 1.0F);
    m_bob = (breathing + weight_shift) * m_profile.dims.idle_bob_amplitude *
            0.8F * m_idle_bob_intensity;
  }

  float rein_tension = m_rider_ctx.locomotion_normalized_speed();
  if (m_rider_ctx.gait.has_target) {
    rein_tension += 0.25F;
  }
  if (m_rider_ctx.is_attacking()) {
    rein_tension += 0.35F;
  }
  rein_tension = std::clamp(rein_tension, 0.0F, 1.0F);
  m_rein_slack = std::max(0.01F, m_rein_slack * (1.0F - rein_tension));
}

} // namespace Render::GL
