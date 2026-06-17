#include "elephant_motion.h"

#include <QMatrix4x4>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>

#include "../creature/animation_state_components.h"
#include "../creature/movement_animation.h"
#include "../creature/quadruped/gait.h"
#include "../gl/humanoid/animation/animation_inputs.h"
#include "animation/elephant_gait_manifest.h"
#include "dimensions.h"
#include "elephant_spec.h"

namespace Render::GL {

namespace {

constexpr float k_pi = std::numbers::pi_v<float>;
constexpr float k_two_pi = 2.0F * k_pi;

namespace Quadruped = Render::Creature::Quadruped;

[[nodiscard]] auto
to_quadruped_dimensions(const ElephantDimensions& d) noexcept -> Quadruped::Dimensions {
  Quadruped::Dimensions dims{};
  dims.body_width = d.body_width;
  dims.body_height = d.body_height;
  dims.body_length = d.body_length;
  dims.barrel_center_y = d.barrel_center_y;
  dims.leg_length = d.leg_length;
  dims.leg_radius = d.leg_radius;
  dims.neck_length = d.neck_length;
  dims.head_width = d.head_width;
  dims.head_height = d.head_height;
  dims.head_length = d.head_length;
  dims.appendage_length = d.trunk_length;
  dims.appendage_base_radius = d.trunk_base_radius;
  dims.appendage_tip_radius = d.trunk_tip_radius;
  dims.idle_bob_amplitude = d.idle_bob_amplitude;
  dims.move_bob_amplitude = d.move_bob_amplitude;
  return dims;
}

[[nodiscard]] auto
to_animation_vec(const QVector3D& v) noexcept -> Animation::PoseVec3 {
  return {.x = v.x(), .y = v.y(), .z = v.z()};
}

[[nodiscard]] auto to_qvector(const Animation::PoseVec3& v) noexcept -> QVector3D {
  return {v.x, v.y, v.z};
}

[[nodiscard]] constexpr auto to_animation_dimensions(
    const ElephantDimensions& d) noexcept -> Animation::QuadrupedDimensions {
  return {
      .body_width = d.body_width,
      .body_height = d.body_height,
      .body_length = d.body_length,
      .barrel_center_y = d.barrel_center_y,
      .idle_bob_amplitude = d.idle_bob_amplitude,
      .move_bob_amplitude = d.move_bob_amplitude,
  };
}

[[nodiscard]] constexpr auto
to_animation_gait(const ElephantGait& g) noexcept -> Animation::QuadrupedGait {
  return {
      .cycle_time = g.cycle_time,
      .stride_swing = g.stride_swing,
      .stride_lift = g.stride_lift,
      .front_leg_phase = g.front_leg_phase,
      .rear_leg_phase = g.rear_leg_phase,
      .phase_offset = g.phase_offset,
  };
}

[[nodiscard]] auto to_animation_elephant_gait_state(
    const ElephantGaitState& state) noexcept -> Animation::ElephantGaitState {
  Animation::ElephantGaitState core{};
  for (int i = 0; i < 4; ++i) {
    auto& core_leg = core.legs[static_cast<std::size_t>(i)];
    const ElephantLegState& render_leg = state.legs[i];
    core_leg.planted_foot = to_animation_vec(render_leg.planted_foot);
    core_leg.swing_start = to_animation_vec(render_leg.swing_start);
    core_leg.swing_target = to_animation_vec(render_leg.swing_target);
    core_leg.swing_progress = render_leg.swing_progress;
    core_leg.in_swing = render_leg.in_swing;
  }
  core.cycle_phase = state.cycle_phase;
  core.weight_shift_x = state.weight_shift_x;
  core.weight_shift_z = state.weight_shift_z;
  core.shoulder_lag = state.shoulder_lag;
  core.hip_lag = state.hip_lag;
  core.initialized = state.initialized;
  return core;
}

[[nodiscard]] auto to_render_elephant_gait_state(
    const Animation::ElephantGaitState& core) noexcept -> ElephantGaitState {
  ElephantGaitState state{};
  for (int i = 0; i < 4; ++i) {
    const auto& core_leg = core.legs[static_cast<std::size_t>(i)];
    ElephantLegState& render_leg = state.legs[i];
    render_leg.planted_foot = to_qvector(core_leg.planted_foot);
    render_leg.swing_start = to_qvector(core_leg.swing_start);
    render_leg.swing_target = to_qvector(core_leg.swing_target);
    render_leg.swing_progress = core_leg.swing_progress;
    render_leg.in_swing = core_leg.in_swing;
  }
  state.cycle_phase = core.cycle_phase;
  state.weight_shift_x = core.weight_shift_x;
  state.weight_shift_z = core.weight_shift_z;
  state.shoulder_lag = core.shoulder_lag;
  state.hip_lag = core.hip_lag;
  state.initialized = core.initialized;
  return state;
}

[[nodiscard]] constexpr auto
elephant_motion_config() noexcept -> Quadruped::MotionConfig {
  Quadruped::MotionConfig config{};
  config.moving_bob_base_scale = 0.88F;
  config.moving_bob_intensity_scale = 0.18F;
  config.running_bob_scale = 1.0F;
  return config;
}

} // namespace

auto compute_howdah_frame(const ElephantProfile& profile) -> HowdahAttachmentFrame {
  using namespace HowdahFrameConstants;
  const ElephantDimensions& d = profile.dims;
  HowdahAttachmentFrame frame{};

  frame.seat_forward = QVector3D(0.0F, 0.0F, 1.0F);
  frame.seat_right = QVector3D(1.0F, 0.0F, 0.0F);
  frame.seat_up = QVector3D(0.0F, 1.0F, 0.0F);

  frame.ground_offset = QVector3D(
      0.0F, -d.barrel_center_y + d.leg_length * k_leg_reveal_lift_scale, 0.0F);

  frame.howdah_center =
      QVector3D(0.0F,
                d.barrel_center_y + d.body_height * k_howdah_body_height_offset,
                d.body_length * k_howdah_body_length_offset);

  frame.seat_position = frame.howdah_center +
                        QVector3D(0.0F, d.howdah_height * k_seat_height_offset, 0.0F);

  return frame;
}

auto evaluate_elephant_motion(const ElephantProfile& profile,
                              const AnimationInputs& anim,
                              Render::Creature::ElephantAnimationStateComponent*
                                  io_state) -> ElephantMotionSample {
  (void)io_state;
  ElephantMotionSample sample{};
  const ElephantGait& g = profile.gait;
  const ElephantDimensions& d = profile.dims;
  sample.gait = g;

  Render::Creature::MovementAnimationState const movement_animation =
      anim.movement_state;
  sample.movement_state = movement_animation;
  sample.is_moving = Render::Creature::is_moving_animation(movement_animation);
  sample.is_fighting =
      anim.is_attacking || (anim.combat_phase != Render::GL::CombatAnimPhase::Idle);
  Quadruped::MotionSample const quadruped_motion = Quadruped::evaluate_cycle_motion(
      to_quadruped_dimensions(d),
      g,
      anim.time,
      sample.is_moving,
      Render::Creature::is_running_animation(movement_animation),
      sample.is_fighting,
      elephant_motion_config());
  float const locomotion_intensity = quadruped_motion.locomotion_intensity;
  sample.phase = quadruped_motion.phase;
  sample.bob = quadruped_motion.bob;

  float const trunk_primary = sample.is_moving
                                  ? std::sin((sample.phase + 0.11F) * k_two_pi) *
                                        (0.10F + locomotion_intensity * 0.05F)
                                  : std::sin(anim.time * 0.8F) * 0.09F;
  float const trunk_secondary =
      sample.is_moving ? std::sin((sample.phase + 0.39F) * 2.0F * k_two_pi) * 0.03F
                       : std::sin(anim.time * 1.3F + 0.5F) * 0.05F;
  sample.trunk_swing = trunk_primary + trunk_secondary;

  float const ear_base = sample.is_moving
                             ? std::sin((sample.phase + 0.33F) * 2.0F * k_two_pi)
                             : std::sin(anim.time * 0.6F);
  sample.ear_flap = sample.is_moving ? ear_base * (0.16F + locomotion_intensity * 0.08F)
                                     : ear_base * 0.10F;

  sample.body_sway = quadruped_motion.body_sway;

  float const weight_transfer = std::sin((sample.phase + 0.20F) * k_two_pi);
  float const shoulder_settle =
      sample.is_moving ? std::max(0.0F, weight_transfer) * d.move_bob_amplitude *
                             (0.50F + locomotion_intensity * 0.10F)
                       : 0.0F;
  float const hip_settle = sample.is_moving ? std::max(0.0F, -weight_transfer) *
                                                  d.move_bob_amplitude *
                                                  (0.42F + locomotion_intensity * 0.10F)
                                            : 0.0F;
  float const spine_wave =
      sample.is_moving
          ? std::sin((sample.phase + 0.47F) * k_two_pi) * d.move_bob_amplitude * 0.18F
          : 0.0F;
  float const head_nod = sample.trunk_swing * 0.16F;

  sample.barrel_center =
      QVector3D(sample.body_sway, d.barrel_center_y + sample.bob, 0.0F);
  sample.chest_center = sample.barrel_center +
                        QVector3D(0.0F,
                                  d.body_height * 0.12F - shoulder_settle + spine_wave,
                                  d.body_length * 0.30F);
  sample.rump_center =
      sample.barrel_center +
      QVector3D(0.0F,
                d.body_height * 0.04F - hip_settle - spine_wave * 0.35F,
                -d.body_length * 0.32F);
  sample.neck_base =
      sample.chest_center +
      QVector3D(0.0F, d.body_height * 0.23F + head_nod * 0.35F, d.body_length * 0.14F);
  sample.neck_top =
      sample.neck_base + QVector3D(0.0F, d.neck_length * 0.60F, d.neck_length * 0.46F);
  sample.head_center =
      sample.neck_top + QVector3D(0.0F,
                                  d.head_height * 0.18F + head_nod,
                                  d.head_length * (0.35F + sample.trunk_swing * 0.04F));

  sample.howdah = compute_howdah_frame(profile);
  apply_howdah_vertical_offset(sample.howdah, sample.bob);

  return sample;
}

auto build_elephant_pose_motion(const ElephantMotionSample& motion,
                                const AnimationInputs& anim)
    -> Render::Elephant::ElephantPoseMotion {
  return Render::Elephant::ElephantPoseMotion{
      .phase = motion.phase,
      .bob = motion.bob,
      .is_moving = motion.is_moving,
      .is_fighting = motion.is_fighting,
      .anim_time = anim.time,
      .combat_phase = anim.combat_phase,
  };
}

void apply_howdah_vertical_offset(HowdahAttachmentFrame& frame, float bob) {
  QVector3D const offset(0.0F, bob, 0.0F);
  frame.howdah_center += offset;
  frame.seat_position += offset;
}

auto evaluate_swing_position(const ElephantLegState& leg,
                             float lift_height) -> QVector3D {
  float const t = leg.swing_progress;
  float const eased_t = Quadruped::swing_ease(t, false);

  QVector3D const horizontal =
      leg.swing_start * (1.0F - eased_t) + leg.swing_target * eased_t;

  float const lift = Quadruped::swing_arc(t, false) * lift_height;

  return {horizontal.x(), horizontal.y() + lift, horizontal.z()};
}

auto solve_elephant_leg_ik(const QVector3D& hip,
                           const QVector3D& foot_target,
                           float upper_len,
                           float lower_len,
                           float lateral_sign) -> ElephantLegPose {
  ElephantLegPose pose{};
  pose.hip = hip;

  QVector3D const to_foot = foot_target - hip;
  float const reach = to_foot.length();

  float const max_reach = upper_len + lower_len - 0.01F;
  float const min_reach = std::abs(upper_len - lower_len) + 0.01F;
  float const clamped_reach = std::clamp(reach, min_reach, max_reach);

  QVector3D const reach_dir =
      reach > 0.001F ? to_foot / reach : QVector3D(0.0F, -1.0F, 0.0F);

  QVector3D const actual_foot = hip + reach_dir * clamped_reach;
  pose.foot = actual_foot;
  pose.ankle = actual_foot + QVector3D(0.0F, lower_len * 0.08F, 0.0F);

  float const a2 = upper_len * upper_len;
  float const b2 = lower_len * lower_len;
  float const c2 = clamped_reach * clamped_reach;

  float const cos_hip_angle = (a2 + c2 - b2) / (2.0F * upper_len * clamped_reach);
  float const hip_angle = std::acos(std::clamp(cos_hip_angle, -1.0F, 1.0F));

  QVector3D const up(0.0F, 1.0F, 0.0F);
  QVector3D bend_axis = QVector3D::crossProduct(reach_dir, up);
  if (bend_axis.lengthSquared() < 0.001F) {

    bend_axis = QVector3D(lateral_sign, 0.0F, 0.0F);
  }
  bend_axis.normalize();

  QMatrix4x4 rot;
  rot.setToIdentity();
  rot.rotate(hip_angle * 180.0F / std::numbers::pi_v<float>, bend_axis);

  QVector3D const knee_dir = rot.map(reach_dir);
  pose.knee = hip + knee_dir * upper_len;

  return pose;
}

auto get_leg_phase_offset(int leg_index) -> float {
  return Animation::elephant_leg_phase_offset(leg_index);
}

auto is_leg_in_swing(float cycle_phase, int leg_index) -> bool {
  return Animation::elephant_leg_is_in_swing(cycle_phase, leg_index);
}

auto get_swing_progress(float cycle_phase, int leg_index) -> float {
  return Animation::elephant_leg_swing_progress(cycle_phase, leg_index);
}

void update_elephant_gait(ElephantGaitState& state,
                          const ElephantProfile& profile,
                          const AnimationInputs& anim,
                          const QVector3D& body_world_pos,
                          float body_forward_z) {
  Render::Creature::MovementAnimationState const movement_animation =
      anim.movement_state;
  bool const is_moving = Render::Creature::is_moving_animation(movement_animation);
  bool const is_running = Render::Creature::is_running_animation(movement_animation);
  Animation::ElephantGaitUpdateInputs const inputs{
      .previous = to_animation_elephant_gait_state(state),
      .dimensions = to_animation_dimensions(profile.dims),
      .gait = to_animation_gait(profile.gait),
      .sample_time = anim.time,
      .body_world_x = body_world_pos.x(),
      .body_world_z = body_world_pos.z(),
      .body_forward_z = body_forward_z,
      .is_moving = is_moving,
      .is_running = is_running,
  };
  state = to_render_elephant_gait_state(Animation::resolve_elephant_gait_state(inputs));
}

} // namespace Render::GL
