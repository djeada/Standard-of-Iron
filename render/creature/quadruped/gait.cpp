#include "gait.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Render::Creature::Quadruped {

namespace {

constexpr float kTwoPi = 2.0F * std::numbers::pi_v<float>;

}

auto wrap_phase(float phase) noexcept -> float {
  phase = std::fmod(phase, 1.0F);
  if (phase < 0.0F) {
    phase += 1.0F;
  }
  return phase;
}

auto swing_ease(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

auto swing_arc(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return 4.0F * t * (1.0F - t);
}

auto locomotion_intensity(bool is_moving, bool is_running, const Gait &gait,
                          const MotionConfig &config) noexcept -> float {
  if (!is_moving) {
    return 0.0F;
  }
  float const stride_swing = std::clamp(gait.stride_swing, 0.0F, 1.0F);
  float const stride_lift = std::clamp(
      gait.stride_lift * config.stride_lift_intensity_scale, 0.0F, 1.0F);
  float const running_bonus =
      is_running ? config.running_intensity_bonus : 0.0F;
  return std::clamp(
      config.base_intensity + stride_swing * config.swing_intensity_scale +
          stride_lift * config.lift_intensity_scale + running_bonus,
      0.0F, 1.0F);
}

auto default_foot_position(const Dimensions &dims, LegId leg,
                           const QVector3D &barrel_center, float lateral_scale,
                           float vertical_scale,
                           float fore_aft_scale) noexcept -> QVector3D {
  QVector3D const hip =
      barrel_center +
      QVector3D(leg_lateral_sign(leg) * dims.body_width * lateral_scale,
                -dims.body_height * vertical_scale,
                leg_forward_sign(leg) * dims.body_length * fore_aft_scale);
  return {hip.x(), 0.0F, hip.z()};
}

auto swing_target(const Dimensions &dims, LegId leg,
                  const QVector3D &barrel_center, float stride_length,
                  float lateral_scale, float vertical_scale,
                  float fore_aft_scale) noexcept -> QVector3D {
  QVector3D target = default_foot_position(
      dims, leg, barrel_center, lateral_scale, vertical_scale, fore_aft_scale);
  target += QVector3D(0.0F, 0.0F, stride_length * leg_forward_sign(leg));
  return target;
}

auto body_sway(bool is_moving, float phase, float time, float move_intensity,
               float stride_swing, const SwayConfig &config) noexcept -> float {
  if (!is_moving) {
    return std::sin(time * config.idle_frequency) * config.idle_amplitude;
  }
  float const primary = std::sin(phase * kTwoPi);
  float const secondary =
      std::sin((phase + config.moving_secondary_phase) * 2.0F * kTwoPi) *
      config.moving_secondary_weight;
  float const amplitude =
      config.base_amplitude +
      std::clamp(stride_swing, 0.0F, 1.0F) * config.stride_scale;
  return (primary + secondary) * amplitude * (0.8F + move_intensity);
}

auto evaluate_cycle_motion(const Dimensions &dims, const Gait &gait, float time,
                           bool is_moving, bool is_running, bool is_fighting,
                           const MotionConfig &motion,
                           const SwayConfig &sway) noexcept -> MotionSample {
  MotionSample sample{};
  sample.is_moving = is_moving;
  sample.is_fighting = is_fighting;
  sample.locomotion_intensity =
      locomotion_intensity(is_moving, is_running, gait, motion);

  if (is_moving) {
    float const cycle_time = std::max(gait.cycle_time, 0.001F);
    sample.phase = wrap_phase(time / cycle_time + gait.phase_offset);
    float const primary = std::sin(sample.phase * kTwoPi);
    float const secondary = std::sin(
        (sample.phase + motion.moving_secondary_phase) * 2.0F * kTwoPi);
    float const bob_scale = is_running ? motion.running_bob_scale : 1.0F;
    sample.bob =
        (primary * 0.70F + secondary * motion.moving_secondary_weight) *
        dims.move_bob_amplitude * bob_scale;
  } else {
    sample.phase =
        wrap_phase(time * motion.idle_phase_speed + gait.phase_offset);
    sample.bob =
        std::sin(time * motion.idle_bob_frequency) * dims.idle_bob_amplitude;
  }

  sample.body_sway =
      body_sway(is_moving, sample.phase, time, sample.locomotion_intensity,
                gait.stride_swing, sway);
  sample.barrel_center =
      QVector3D(sample.body_sway, dims.barrel_center_y + sample.bob, 0.0F);
  return sample;
}

} // namespace Render::Creature::Quadruped
