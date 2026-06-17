#include "gait.h"

namespace Render::Creature::Quadruped {

namespace {

[[nodiscard]] auto to_animation_leg(LegId leg) noexcept -> Animation::QuadrupedLegId {
  return static_cast<Animation::QuadrupedLegId>(leg);
}

[[nodiscard]] auto to_animation_dimensions(const Dimensions& dims) noexcept
    -> Animation::QuadrupedDimensions {
  return {
      .body_width = dims.body_width,
      .body_height = dims.body_height,
      .body_length = dims.body_length,
      .barrel_center_y = dims.barrel_center_y,
      .idle_bob_amplitude = dims.idle_bob_amplitude,
      .move_bob_amplitude = dims.move_bob_amplitude,
  };
}

[[nodiscard]] auto
to_animation_gait(const Gait& gait) noexcept -> Animation::QuadrupedGait {
  return {
      .cycle_time = gait.cycle_time,
      .stride_swing = gait.stride_swing,
      .stride_lift = gait.stride_lift,
      .front_leg_phase = gait.front_leg_phase,
      .rear_leg_phase = gait.rear_leg_phase,
      .phase_offset = gait.phase_offset,
  };
}

[[nodiscard]] auto
to_animation_vec(const QVector3D& v) noexcept -> Animation::PoseVec3 {
  return {.x = v.x(), .y = v.y(), .z = v.z()};
}

[[nodiscard]] auto to_qvector(const Animation::PoseVec3& v) noexcept -> QVector3D {
  return {v.x, v.y, v.z};
}

[[nodiscard]] auto to_render_sample(
    const Animation::QuadrupedMotionSample& sample) noexcept -> MotionSample {
  return {
      .phase = sample.phase,
      .bob = sample.bob,
      .locomotion_intensity = sample.locomotion_intensity,
      .body_sway = sample.body_sway,
      .is_moving = sample.is_moving,
      .is_fighting = sample.is_fighting,
      .barrel_center = to_qvector(sample.barrel_center),
  };
}

} // namespace

auto wrap_phase(float phase) noexcept -> float {
  return Animation::wrap_quadruped_phase(phase);
}

auto swing_ease(float t, bool clamp_input) noexcept -> float {
  return Animation::quadruped_swing_ease(t, clamp_input);
}

auto swing_arc(float t, bool clamp_input) noexcept -> float {
  return Animation::quadruped_swing_arc(t, clamp_input);
}

auto locomotion_intensity(bool is_moving,
                          bool is_running,
                          const Gait& gait,
                          const MotionConfig& config) noexcept -> float {
  return Animation::quadruped_locomotion_intensity(
      is_moving, is_running, to_animation_gait(gait), config);
}

auto default_foot_position(const Dimensions& dims,
                           LegId leg,
                           const QVector3D& barrel_center,
                           float lateral_scale,
                           float vertical_scale,
                           float fore_aft_scale) noexcept -> QVector3D {
  return to_qvector(
      Animation::quadruped_default_foot_position(to_animation_dimensions(dims),
                                                 to_animation_leg(leg),
                                                 to_animation_vec(barrel_center),
                                                 lateral_scale,
                                                 vertical_scale,
                                                 fore_aft_scale));
}

auto swing_target(const Dimensions& dims,
                  LegId leg,
                  const QVector3D& barrel_center,
                  float stride_length,
                  float lateral_scale,
                  float vertical_scale,
                  float fore_aft_scale,
                  bool mirror_stride_by_leg_side) noexcept -> QVector3D {
  return to_qvector(Animation::quadruped_swing_target(to_animation_dimensions(dims),
                                                      to_animation_leg(leg),
                                                      to_animation_vec(barrel_center),
                                                      stride_length,
                                                      lateral_scale,
                                                      vertical_scale,
                                                      fore_aft_scale,
                                                      mirror_stride_by_leg_side));
}

auto body_sway(bool is_moving,
               float phase,
               float time,
               float move_intensity,
               float stride_swing,
               const SwayConfig& config) noexcept -> float {
  return Animation::quadruped_body_sway(
      is_moving, phase, time, move_intensity, stride_swing, config);
}

auto evaluate_cycle_motion(const Dimensions& dims,
                           const Gait& gait,
                           float time,
                           bool is_moving,
                           bool is_running,
                           bool is_fighting,
                           const MotionConfig& motion,
                           const SwayConfig& sway) noexcept -> MotionSample {
  return to_render_sample(
      Animation::resolve_quadruped_cycle_motion(to_animation_dimensions(dims),
                                                to_animation_gait(gait),
                                                time,
                                                is_moving,
                                                is_running,
                                                is_fighting,
                                                motion,
                                                sway));
}

} // namespace Render::Creature::Quadruped
