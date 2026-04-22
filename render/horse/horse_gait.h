#pragma once

#include <algorithm>

namespace Render::GL {

enum class GaitType { IDLE, WALK, TROT, CANTER, GALLOP };

struct HorseGait {
  float cycle_time{};
  float front_leg_phase{};
  float rear_leg_phase{};
  float stride_swing{};
  float stride_lift{};

  float phase_offset{};
  float stride_jitter{};
  float head_height_jitter{};
  float lateral_lead_front{0.5F};
  float lateral_lead_rear{0.5F};
  float ear_pin{0.0F};
};

[[nodiscard]] inline auto gait_for_type(GaitType gait,
                                        const HorseGait &base) noexcept
    -> HorseGait {
  HorseGait resolved = base;
  switch (gait) {
  case GaitType::IDLE:
    resolved.cycle_time = 2.5F;
    resolved.front_leg_phase = 0.0F;
    resolved.rear_leg_phase = 0.0F;
    resolved.stride_swing = 0.015F;
    resolved.stride_lift = 0.008F;
    resolved.lateral_lead_front = 0.50F;
    resolved.lateral_lead_rear = 0.50F;
    resolved.ear_pin = 0.0F;
    break;
  case GaitType::WALK:
    resolved.cycle_time = 1.1F;
    resolved.front_leg_phase = 0.25F;
    resolved.rear_leg_phase = 0.0F;
    resolved.stride_swing = 0.24F;
    resolved.stride_lift = 0.07F;
    resolved.lateral_lead_front = 0.48F;
    resolved.lateral_lead_rear = 0.48F;
    resolved.ear_pin = 0.03F;
    break;
  case GaitType::TROT:
    resolved.cycle_time = 0.55F;
    resolved.front_leg_phase = 0.0F;
    resolved.rear_leg_phase = 0.50F;
    resolved.stride_swing = 0.62F;
    resolved.stride_lift = 0.24F;
    resolved.lateral_lead_front = 0.52F;
    resolved.lateral_lead_rear = 0.52F;
    resolved.ear_pin = 0.16F;
    break;
  case GaitType::CANTER:
    resolved.cycle_time = 0.48F;
    resolved.front_leg_phase = 0.58F;
    resolved.rear_leg_phase = 0.30F;
    resolved.stride_swing = 0.76F;
    resolved.stride_lift = 0.34F;
    resolved.lateral_lead_front = 0.30F;
    resolved.lateral_lead_rear = 0.42F;
    resolved.ear_pin = 0.40F;
    break;
  case GaitType::GALLOP:
    resolved.cycle_time = 0.38F;
    resolved.front_leg_phase = 0.42F;
    resolved.rear_leg_phase = 0.10F;
    resolved.stride_swing = 0.90F;
    resolved.stride_lift = 0.46F;
    resolved.lateral_lead_front = 0.18F;
    resolved.lateral_lead_rear = 0.28F;
    resolved.ear_pin = 0.72F;
    break;
  }
  return resolved;
}

[[nodiscard]] inline auto blend_gaits(const HorseGait &from, const HorseGait &to,
                                      float t) noexcept -> HorseGait {
  t = std::clamp(t, 0.0F, 1.0F);
  HorseGait out = from;
  auto lerp = [t](float a, float b) { return a + (b - a) * t; };
  out.cycle_time = lerp(from.cycle_time, to.cycle_time);
  out.front_leg_phase = lerp(from.front_leg_phase, to.front_leg_phase);
  out.rear_leg_phase = lerp(from.rear_leg_phase, to.rear_leg_phase);
  out.stride_swing = lerp(from.stride_swing, to.stride_swing);
  out.stride_lift = lerp(from.stride_lift, to.stride_lift);
  out.lateral_lead_front = lerp(from.lateral_lead_front, to.lateral_lead_front);
  out.lateral_lead_rear = lerp(from.lateral_lead_rear, to.lateral_lead_rear);
  out.ear_pin = lerp(from.ear_pin, to.ear_pin);
  return out;
}

[[nodiscard]] inline auto gait_from_speed(float speed) noexcept -> GaitType {
  if (speed < 0.5F) {
    return GaitType::IDLE;
  }
  if (speed < 3.0F) {
    return GaitType::WALK;
  }
  if (speed < 5.5F) {
    return GaitType::TROT;
  }
  if (speed < 8.0F) {
    return GaitType::CANTER;
  }
  return GaitType::GALLOP;
}

} // namespace Render::GL
