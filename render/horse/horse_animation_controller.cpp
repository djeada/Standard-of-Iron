#include "horse_animation_controller.h"
#include "../humanoid/rig.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::GL {

namespace {
constexpr float k_pi = std::numbers::pi_v<float>;

struct GaitParameters {
  float cycle_time;
  float front_leg_phase;
  float rear_leg_phase;
  float stride_swing;
  float stride_lift;
  float bob_amplitude;
};

constexpr GaitParameters getGaitParams(GaitType gait) {
  switch (gait) {
  case GaitType::IDLE:
    return {1.0F, 0.0F, 0.0F, 0.02F, 0.01F, 0.005F};
  case GaitType::WALK:

    return {1.0F, 0.25F, 0.75F, 0.55F, 0.22F, 0.020F};
  case GaitType::TROT:

    return {0.60F, 0.0F, 0.5F, 0.70F, 0.35F, 0.030F};
  case GaitType::CANTER:

    return {0.50F, 0.33F, 0.66F, 0.85F, 0.45F, 0.040F};
  case GaitType::GALLOP:

    return {0.38F, 0.15F, 0.65F, 1.05F, 0.58F, 0.055F};
  }
  return {1.0F, 0.0F, 0.0F, 0.02F, 0.01F, 0.005F};
}

} // namespace

HorseAnimationController::HorseAnimationController(
    HorseProfile &profile, const AnimationInputs &anim,
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
}

void HorseAnimationController::setGait(GaitType gait) {

  m_current_gait = gait;
  m_target_gait = gait;
  m_gait_transition_progress = 1.0F;

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
  m_speed = 0.0F;

  float const phase = std::fmod(m_anim.time * 0.25F, 1.0F);
  m_phase = phase;
  m_bob = std::sin(phase * 2.0F * k_pi) * m_profile.dims.idle_bob_amplitude *
          bob_intensity;

  update_gait_parameters();
}

void HorseAnimationController::accelerate(float speed_delta) {
  m_speed += speed_delta;
  m_speed = std::max(0.0F, m_speed);

  GaitType new_gait;
  if (m_speed < 0.5F) {
    new_gait = GaitType::IDLE;
  } else if (m_speed < 3.0F) {
    new_gait = GaitType::WALK;
  } else if (m_speed < 5.5F) {
    new_gait = GaitType::TROT;
  } else if (m_speed < 8.0F) {
    new_gait = GaitType::CANTER;
  } else {
    new_gait = GaitType::GALLOP;
  }

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

void HorseAnimationController::strafeStep(bool left, float distance) {

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

void HorseAnimationController::jumpObstacle(float height, float distance) {
  m_is_jumping = true;
  m_jump_height = std::max(0.0F, height);
  m_jump_distance = std::max(0.0F, distance);
}

auto HorseAnimationController::get_current_phase() const -> float {
  return m_phase;
}

auto HorseAnimationController::get_current_bob() const -> float { return m_bob; }

auto HorseAnimationController::get_stride_cycle() const -> float {
  GaitParameters const params = getGaitParams(m_current_gait);
  return params.cycle_time;
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

  GaitParameters const current_params = getGaitParams(m_current_gait);
  GaitParameters target_params = current_params;

  if (m_gait_transition_progress < 1.0F) {
    target_params = getGaitParams(m_target_gait);
    float const t = m_gait_transition_progress;

    float const ease_t = t * t * (3.0F - 2.0F * t);

    m_profile.gait.cycle_time =
        current_params.cycle_time +
        ease_t * (target_params.cycle_time - current_params.cycle_time);
    m_profile.gait.front_leg_phase = current_params.front_leg_phase +
                                     ease_t * (target_params.front_leg_phase -
                                               current_params.front_leg_phase);
    m_profile.gait.rear_leg_phase =
        current_params.rear_leg_phase +
        ease_t * (target_params.rear_leg_phase - current_params.rear_leg_phase);
    m_profile.gait.stride_swing =
        current_params.stride_swing +
        ease_t * (target_params.stride_swing - current_params.stride_swing);
    m_profile.gait.stride_lift =
        current_params.stride_lift +
        ease_t * (target_params.stride_lift - current_params.stride_lift);
  } else {

    m_profile.gait.cycle_time = current_params.cycle_time;
    m_profile.gait.front_leg_phase = current_params.front_leg_phase;
    m_profile.gait.rear_leg_phase = current_params.rear_leg_phase;
    m_profile.gait.stride_swing = current_params.stride_swing;
    m_profile.gait.stride_lift = current_params.stride_lift;
  }

  bool const is_moving = m_current_gait != GaitType::IDLE;

  if (is_moving) {

    if (m_rider_ctx.gait.cycle_time > 0.0001F) {
      m_phase = m_rider_ctx.gait.cycle_phase;
    } else {
      m_phase = std::fmod(m_anim.time / m_profile.gait.cycle_time, 1.0F);
    }

    float const rider_intensity = m_rider_ctx.locomotion_normalized_speed();
    float const bob_amp = m_profile.dims.idle_bob_amplitude +
                          rider_intensity * (m_profile.dims.move_bob_amplitude -
                                             m_profile.dims.idle_bob_amplitude);

    float const variation = std::sin(m_anim.time * 0.7F) * 0.05F + 1.0F;
    m_bob = std::sin(m_phase * 2.0F * k_pi) * bob_amp * variation;
  } else {

    m_phase = std::fmod(m_anim.time * 0.25F, 1.0F);
    m_bob = std::sin(m_phase * 2.0F * k_pi) * m_profile.dims.idle_bob_amplitude;
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
