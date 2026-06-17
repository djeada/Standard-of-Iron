#include "quadruped_gait_manifest.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Animation {

namespace {

constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;

}

auto wrap_quadruped_phase(float phase) noexcept -> float {
  phase = std::fmod(phase, 1.0F);
  if (phase < 0.0F) {
    phase += 1.0F;
  }
  return phase;
}

auto quadruped_swing_ease(float t, bool clamp_input) noexcept -> float {
  if (clamp_input) {
    t = std::clamp(t, 0.0F, 1.0F);
  }
  return t * t * (3.0F - 2.0F * t);
}

auto quadruped_swing_arc(float t, bool clamp_input) noexcept -> float {
  if (clamp_input) {
    t = std::clamp(t, 0.0F, 1.0F);
  }
  return 4.0F * t * (1.0F - t);
}

auto quadruped_locomotion_intensity(bool is_moving,
                                    bool is_running,
                                    const QuadrupedGait& gait,
                                    const QuadrupedMotionConfig& config) noexcept
    -> float {
  if (!is_moving) {
    return 0.0F;
  }
  float const stride_swing = std::clamp(gait.stride_swing, 0.0F, 1.0F);
  float const stride_lift =
      std::clamp(gait.stride_lift * config.stride_lift_intensity_scale, 0.0F, 1.0F);
  float const running_bonus = is_running ? config.running_intensity_bonus : 0.0F;
  return std::clamp(config.base_intensity +
                        stride_swing * config.swing_intensity_scale +
                        stride_lift * config.lift_intensity_scale + running_bonus,
                    0.0F,
                    1.0F);
}

auto quadruped_default_foot_position(const QuadrupedDimensions& dims,
                                     QuadrupedLegId leg,
                                     const PoseVec3& barrel_center,
                                     float lateral_scale,
                                     float vertical_scale,
                                     float fore_aft_scale) noexcept -> PoseVec3 {
  PoseVec3 const hip{
      .x = barrel_center.x +
           quadruped_leg_lateral_sign(leg) * dims.body_width * lateral_scale,
      .y = barrel_center.y - dims.body_height * vertical_scale,
      .z = barrel_center.z +
           quadruped_leg_forward_sign(leg) * dims.body_length * fore_aft_scale,
  };
  return {.x = hip.x, .y = 0.0F, .z = hip.z};
}

auto quadruped_swing_target(const QuadrupedDimensions& dims,
                            QuadrupedLegId leg,
                            const PoseVec3& barrel_center,
                            float stride_length,
                            float lateral_scale,
                            float vertical_scale,
                            float fore_aft_scale,
                            bool mirror_stride_by_leg_side) noexcept -> PoseVec3 {
  PoseVec3 target = quadruped_default_foot_position(
      dims, leg, barrel_center, lateral_scale, vertical_scale, fore_aft_scale);
  float const direction =
      mirror_stride_by_leg_side ? quadruped_leg_forward_sign(leg) : 1.0F;
  target.z += stride_length * direction;
  return target;
}

auto quadruped_body_sway(bool is_moving,
                         float phase,
                         float time,
                         float move_intensity,
                         float stride_swing,
                         const QuadrupedSwayConfig& config) noexcept -> float {
  if (!is_moving) {
    return std::sin(time * config.idle_frequency) * config.idle_amplitude;
  }
  float const primary = std::sin(phase * k_two_pi);
  float const secondary =
      std::sin((phase + config.moving_secondary_phase) * 2.0F * k_two_pi) *
      config.moving_secondary_weight;
  float const amplitude = config.base_amplitude +
                          std::clamp(stride_swing, 0.0F, 1.0F) * config.stride_scale;
  return (primary + secondary) * amplitude * (0.8F + move_intensity);
}

auto resolve_quadruped_cycle_motion(const QuadrupedDimensions& dims,
                                    const QuadrupedGait& gait,
                                    float time,
                                    bool is_moving,
                                    bool is_running,
                                    bool is_fighting,
                                    const QuadrupedMotionConfig& motion,
                                    const QuadrupedSwayConfig& sway) noexcept
    -> QuadrupedMotionSample {
  QuadrupedMotionSample sample{};
  sample.is_moving = is_moving;
  sample.is_fighting = is_fighting;
  sample.locomotion_intensity =
      quadruped_locomotion_intensity(is_moving, is_running, gait, motion);

  if (is_moving) {
    float const cycle_time = std::max(gait.cycle_time, motion.cycle_time_floor);
    sample.phase = wrap_quadruped_phase(time / cycle_time + gait.phase_offset);
    float const primary = std::sin(sample.phase * k_two_pi);
    float const secondary = std::sin((sample.phase + motion.moving_secondary_phase) *
                                     motion.moving_secondary_frequency * k_two_pi);
    float const tertiary = std::sin((sample.phase + motion.moving_tertiary_phase) *
                                    motion.moving_tertiary_frequency * k_two_pi);
    float const bob_scale =
        (motion.moving_bob_base_scale +
         sample.locomotion_intensity * motion.moving_bob_intensity_scale) *
        (is_running ? motion.running_bob_scale : 1.0F);
    sample.bob = (primary * motion.moving_primary_weight +
                  secondary * motion.moving_secondary_weight +
                  tertiary * motion.moving_tertiary_weight) *
                 dims.move_bob_amplitude * bob_scale;
  } else {
    sample.phase =
        wrap_quadruped_phase(time * motion.idle_phase_speed + gait.phase_offset);
    sample.bob = std::sin(time * motion.idle_bob_frequency) * dims.idle_bob_amplitude;
  }

  sample.body_sway = quadruped_body_sway(is_moving,
                                         sample.phase,
                                         time,
                                         sample.locomotion_intensity,
                                         gait.stride_swing,
                                         sway);
  sample.barrel_center = {
      .x = sample.body_sway,
      .y = dims.barrel_center_y + sample.bob,
      .z = 0.0F,
  };
  return sample;
}

} // namespace Animation
