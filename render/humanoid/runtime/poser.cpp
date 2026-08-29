

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "animation/locomotion_manifest.h"
#include "animation/rig/humanoid_proportions.h"
#include "render/gl/humanoid/animation/animation_inputs.h"
#include "render/gl/humanoid/humanoid_constants.h"
#include "render/humanoid/runtime/humanoid_math.h"
#include "render/humanoid/runtime/humanoid_renderer.h"
#include "render/humanoid/runtime/pose_controller.h"
#include "render/humanoid/runtime/pose_primitives.h"

namespace Render::GL {

namespace {

auto to_qvec(const Animation::PoseVec3& value) -> QVector3D {
  return {value.x, value.y, value.z};
}

} // namespace

void HumanoidRendererBase::compute_locomotion_pose(uint32_t seed,
                                                   float time,
                                                   const HumanoidGaitDescriptor& gait,
                                                   const VariationParams& variation,
                                                   HumanoidPose& pose) {
  bool const is_moving =
      gait.state == HumanoidMotionState::Walk || gait.state == HumanoidMotionState::Run;

  auto resolved_gait = gait;
  if (is_moving && resolved_gait.cycle_time <= 0.0001F) {
    resolved_gait.cycle_time =
        ((resolved_gait.state == HumanoidMotionState::Run) ? 0.56F : 0.92F) /
        std::max(0.1F, variation.walk_speed_mult);
  }
  if (is_moving && resolved_gait.cycle_phase <= 0.0F &&
      resolved_gait.cycle_time > 0.0001F) {
    resolved_gait.cycle_phase =
        std::fmod(time / std::max(0.001F, resolved_gait.cycle_time), 1.0F);
  }

  using HP = HumanProportions;

  float const h_scale = variation.height_scale;
  auto const base_pose = Animation::resolve_humanoid_base_pose({
      .seed = seed,
      .height_scale = variation.height_scale,
      .bulk_scale = variation.bulk_scale,
      .stance_width = variation.stance_width,
      .posture_slump = variation.posture_slump,
      .shoulder_tilt = variation.shoulder_tilt,
      .proportions =
          {
              .chest_y = HP::CHEST_Y,
              .ground_y = HP::GROUND_Y,
              .head_center_y = HP::HEAD_CENTER_Y,
              .head_radius = HP::HEAD_RADIUS,
              .neck_base_y = HP::NECK_BASE_Y,
              .shoulder_y = HP::SHOULDER_Y,
              .shoulder_width = HP::SHOULDER_WIDTH,
              .waist_y = HP::WAIST_Y,
              .foot_y_offset_default = HP::FOOT_Y_OFFSET_DEFAULT,
          },
  });
  pose.head_pos = to_qvec(base_pose.head_pos);
  pose.head_r = base_pose.head_radius;
  pose.neck_base = to_qvec(base_pose.neck_base);
  pose.shoulder_l = to_qvec(base_pose.shoulder_l);
  pose.shoulder_r = to_qvec(base_pose.shoulder_r);
  pose.pelvis_pos = to_qvec(base_pose.pelvis);
  pose.foot_y_offset = base_pose.foot_y_offset;
  pose.foot_l = to_qvec(base_pose.foot_l);
  pose.foot_r = to_qvec(base_pose.foot_r);
  pose.hand_l = to_qvec(base_pose.hand_l);
  pose.hand_r = to_qvec(base_pose.hand_r);
  pose.foot_pitch_l = 0.0F;
  pose.foot_pitch_r = 0.0F;

  QVector3D hip_delta_l;
  QVector3D hip_delta_r;

  if (is_moving) {
    float const rest_arm_length = 0.5F * ((pose.hand_l - pose.shoulder_l).length() +
                                          (pose.hand_r - pose.shoulder_r).length());
    auto const locomotion_pose = Animation::resolve_humanoid_locomotion_pose({
        .state = static_cast<Animation::HumanoidMotionState>(resolved_gait.state),
        .normalized_speed = resolved_gait.normalized_speed,
        .cycle_phase = resolved_gait.cycle_phase,
        .stride_distance = resolved_gait.stride_distance,
        .locomotion_blend = resolved_gait.locomotion_blend,
        .run_blend = resolved_gait.run_blend,
        .turn_amount = resolved_gait.turn_amount,
        .travel_alignment = resolved_gait.travel_alignment,
        .acceleration = resolved_gait.acceleration,
        .walk_speed_multiplier = variation.walk_speed_mult,
        .stance_width = variation.stance_width,
        .arm_swing_amplitude = variation.arm_swing_amp,
        .reference_walk_speed = k_reference_walk_speed,
        .reference_run_speed = k_reference_run_speed,
        .ground_y = HP::GROUND_Y,
        .foot_y_offset = pose.foot_y_offset,
        .base_foot_l = base_pose.foot_l,
        .base_foot_r = base_pose.foot_r,
        .arm_pendulum_length = rest_arm_length,
        .pelvis_y = pose.pelvis_pos.y(),
        .hip_lateral_offset = HP::HIP_LATERAL_OFFSET,
        .hip_vertical_offset = HP::HIP_VERTICAL_OFFSET,
        .leg_length = (HP::UPPER_LEG_LEN + HP::LOWER_LEG_LEN) * h_scale,
    });
    if (locomotion_pose.active) {
      pose.foot_l = to_qvec(locomotion_pose.foot_l);
      pose.foot_r = to_qvec(locomotion_pose.foot_r);
      pose.foot_pitch_l = locomotion_pose.foot_pitch_l;
      pose.foot_pitch_r = locomotion_pose.foot_pitch_r;
      pose.pelvis_pos += to_qvec(locomotion_pose.pelvis_delta);
      pose.shoulder_l += to_qvec(locomotion_pose.shoulder_l_delta);
      pose.shoulder_r += to_qvec(locomotion_pose.shoulder_r_delta);
      pose.neck_base += to_qvec(locomotion_pose.neck_delta);
      pose.head_pos += to_qvec(locomotion_pose.head_delta);
      pose.hand_l += to_qvec(locomotion_pose.hand_l_delta);
      pose.hand_r += to_qvec(locomotion_pose.hand_r_delta);
      hip_delta_l = to_qvec(locomotion_pose.hip_l_delta);
      hip_delta_r = to_qvec(locomotion_pose.hip_r_delta);
    }

    float const max_reach =
        Render::Humanoid::PosePrimitives::humanoid_arm_reach_limit(h_scale);
    pose.hand_l = Render::Humanoid::PosePrimitives::clamp_to_reach(
        pose.shoulder_l, pose.hand_l, max_reach);
    pose.hand_r = Render::Humanoid::PosePrimitives::clamp_to_reach(
        pose.shoulder_r, pose.hand_r, max_reach);
  }

  QVector3D const hip_l =
      pose.pelvis_pos +
      QVector3D(-HP::HIP_LATERAL_OFFSET, HP::HIP_VERTICAL_OFFSET, 0.0F) + hip_delta_l;
  QVector3D const hip_r =
      pose.pelvis_pos +
      QVector3D(HP::HIP_LATERAL_OFFSET, HP::HIP_VERTICAL_OFFSET, 0.0F) + hip_delta_r;

  auto solve_leg =
      [&](const QVector3D& hip, const QVector3D& foot, Side side) -> QVector3D {
    return Render::Humanoid::PosePrimitives::solve_knee_ik(
        hip,
        foot,
        {.upper_leg_len = HP::UPPER_LEG_LEN * h_scale,
         .lower_leg_len = HP::LOWER_LEG_LEN * h_scale,
         .knee_floor = HP::GROUND_Y + pose.foot_y_offset * 0.5F,
         .bend_preference = (side == Side::Left) ? QVector3D(-0.24F, 0.0F, 0.95F)
                                                 : QVector3D(0.24F, 0.0F, 0.95F)});
  };

  pose.knee_l = solve_leg(hip_l, pose.foot_l, Side::Left);
  pose.knee_r = solve_leg(hip_r, pose.foot_r, Side::Right);

  QVector3D right_axis = Render::Humanoid::PosePrimitives::compute_right_axis(pose);

  if (right_axis.x() < 0.0F) {
    right_axis = -right_axis;
  }

  bool const is_running =
      resolved_gait.state == HumanoidMotionState::Run || resolved_gait.run_blend > 0.5F;
  constexpr float k_walking_elbow_backset = 0.45F;
  constexpr float k_running_elbow_backset = 0.88F;
  float const elbow_backset_amount =
      is_running ? k_running_elbow_backset : k_walking_elbow_backset;
  QVector3D const elbow_backset =
      is_moving ? QVector3D(0.0F, 0.0F, -1.0F) * elbow_backset_amount : QVector3D();
  float const elbow_outward_amount = is_moving ? (is_running ? 0.18F : 0.65F) : 1.0F;
  QVector3D const outward_l = (-right_axis * elbow_outward_amount) + elbow_backset;
  QVector3D const outward_r = (right_axis * elbow_outward_amount) + elbow_backset;

  float const elbow_along_bias = (variation.bulk_scale - 1.0F) * 0.05F;
  float const elbow_lateral_l = is_running ? 0.035F : 0.10F;
  float const elbow_lateral_r = is_running ? 0.030F : 0.08F;
  float const elbow_height_l = is_running ? 0.04F : -0.03F;
  float const elbow_height_r = is_running ? 0.06F : 0.0F;
  pose.elbow_l =
      Render::Humanoid::PosePrimitives::solve_elbow_ik(pose.shoulder_l,
                                                       pose.hand_l,
                                                       outward_l,
                                                       0.45F - elbow_along_bias,
                                                       elbow_lateral_l,
                                                       elbow_height_l,
                                                       +1.0F);
  pose.elbow_r =
      Render::Humanoid::PosePrimitives::solve_elbow_ik(pose.shoulder_r,
                                                       pose.hand_r,
                                                       outward_r,
                                                       0.45F - elbow_along_bias,
                                                       elbow_lateral_r,
                                                       elbow_height_r,
                                                       +1.0F);
}

void HumanoidRendererBase::compute_locomotion_pose(uint32_t seed,
                                                   float time,
                                                   bool is_moving,
                                                   const VariationParams& variation,
                                                   HumanoidPose& pose) {
  HumanoidGaitDescriptor gait{};
  gait.state = is_moving ? HumanoidMotionState::Walk : HumanoidMotionState::Idle;
  gait.normalized_speed = is_moving ? 1.0F : 0.0F;
  if (is_moving) {
    gait.cycle_time = 0.92F / std::max(0.1F, variation.walk_speed_mult);
    gait.cycle_phase = std::fmod(time / std::max(0.001F, gait.cycle_time), 1.0F);
  }
  compute_locomotion_pose(seed, time, gait, variation, pose);
}

} // namespace Render::GL
