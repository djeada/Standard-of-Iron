#include "horse_animation_controller.h"
#include "../humanoid/rig.h"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::GL {

namespace {
constexpr float k_pi = std::numbers::pi_v<float>;

struct GaitParameters {
  float cycleTime;
  float frontLegPhase;
  float rearLegPhase;
  float strideSwing;
  float strideLift;
  float bobAmplitude;
};

constexpr GaitParameters getGaitParams(GaitType gait) {
  switch (gait) {
  case GaitType::IDLE:
    return {1.0F, 0.0F, 0.0F, 0.02F, 0.01F, 0.005F};
  case GaitType::WALK:
    return {0.80F, 0.25F, 0.75F, 0.28F, 0.12F, 0.015F};
  case GaitType::TROT:
    return {0.55F, 0.0F, 0.5F, 0.32F, 0.18F, 0.025F};
  case GaitType::CANTER:
    return {0.45F, 0.15F, 0.60F, 0.38F, 0.24F, 0.035F};
  case GaitType::GALLOP:
    return {0.35F, 0.10F, 0.55F, 0.45F, 0.32F, 0.045F};
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
}

void HorseAnimationController::setGait(GaitType gait) {
  m_current_gait = gait;

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

  updateGaitParameters();
}

void HorseAnimationController::idle(float bob_intensity) {
  m_current_gait = GaitType::IDLE;
  m_speed = 0.0F;

  float const phase = std::fmod(m_anim.time * 0.25F, 1.0F);
  m_phase = phase;
  m_bob = std::sin(phase * 2.0F * k_pi) * m_profile.dims.idleBobAmplitude *
          bob_intensity;

  updateGaitParameters();
}

void HorseAnimationController::accelerate(float speed_delta) {
  m_speed += speed_delta;
  m_speed = std::max(0.0F, m_speed);

  if (m_speed < 0.5F) {
    m_current_gait = GaitType::IDLE;
  } else if (m_speed < 3.0F) {
    m_current_gait = GaitType::WALK;
  } else if (m_speed < 5.5F) {
    m_current_gait = GaitType::TROT;
  } else if (m_speed < 8.0F) {
    m_current_gait = GaitType::CANTER;
  } else {
    m_current_gait = GaitType::GALLOP;
  }

  updateGaitParameters();
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

auto HorseAnimationController::getCurrentPhase() const -> float {
  return m_phase;
}

auto HorseAnimationController::getCurrentBob() const -> float { return m_bob; }

auto HorseAnimationController::getStrideCycle() const -> float {
  GaitParameters const params = getGaitParams(m_current_gait);
  return params.cycleTime;
}

void HorseAnimationController::updateGaitParameters() {
  GaitParameters const params = getGaitParams(m_current_gait);

  m_profile.gait.cycleTime = params.cycleTime;
  m_profile.gait.frontLegPhase = params.frontLegPhase;
  m_profile.gait.rearLegPhase = params.rearLegPhase;
  m_profile.gait.strideSwing = params.strideSwing;
  m_profile.gait.strideLift = params.strideLift;

  bool const is_moving = m_current_gait != GaitType::IDLE;

  if (is_moving) {

    if (m_rider_ctx.gait.cycle_time > 0.0001F) {
      m_phase = m_rider_ctx.gait.cycle_phase;
    } else {
      m_phase = std::fmod(m_anim.time / params.cycleTime, 1.0F);
    }

    float const rider_intensity = m_rider_ctx.locomotion_normalized_speed();
    float const bob_amp = m_profile.dims.idleBobAmplitude +
                          rider_intensity * (m_profile.dims.moveBobAmplitude -
                                             m_profile.dims.idleBobAmplitude);
    m_bob = std::sin(m_phase * 2.0F * k_pi) * bob_amp;
  } else {

    m_phase = std::fmod(m_anim.time * 0.25F, 1.0F);
    m_bob = std::sin(m_phase * 2.0F * k_pi) * m_profile.dims.idleBobAmplitude;
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
