

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../gl/humanoid/animation/animation_inputs.h"
#include "../gl/humanoid/humanoid_constants.h"
#include "animation/locomotion_manifest.h"
#include "humanoid_math.h"
#include "humanoid_renderer_base.h"
#include "humanoid_specs.h"
#include "pose_controller.h"
#include "pose_primitives.h"

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

  if (is_moving) {
    auto const locomotion_pose = Animation::resolve_humanoid_locomotion_pose({
        .state = static_cast<Animation::HumanoidMotionState>(resolved_gait.state),
        .normalized_speed = resolved_gait.normalized_speed,
        .cycle_phase = resolved_gait.cycle_phase,
        .stride_distance = resolved_gait.stride_distance,
        .locomotion_blend = resolved_gait.locomotion_blend,
        .run_blend = resolved_gait.run_blend,
        .turn_amount = resolved_gait.turn_amount,
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
    });
    if (locomotion_pose.active) {
      pose.foot_l = to_qvec(locomotion_pose.foot_l);
      pose.foot_r = to_qvec(locomotion_pose.foot_r);
      pose.pelvis_pos += to_qvec(locomotion_pose.pelvis_delta);
      pose.shoulder_l += to_qvec(locomotion_pose.shoulder_l_delta);
      pose.shoulder_r += to_qvec(locomotion_pose.shoulder_r_delta);
      pose.neck_base += to_qvec(locomotion_pose.neck_delta);
      pose.head_pos += to_qvec(locomotion_pose.head_delta);
      pose.hand_l += to_qvec(locomotion_pose.hand_l_delta);
      pose.hand_r += to_qvec(locomotion_pose.hand_r_delta);
    }

    auto clamp_hand_reach = [&](const QVector3D& shoulder, QVector3D& hand) {
      float const max_reach = (HP::UPPER_ARM_LEN + HP::FORE_ARM_LEN) * h_scale * 0.98F;
      QVector3D const diff = hand - shoulder;
      float const len = diff.length();
      if (len > max_reach && len > 1e-6F) {
        hand = shoulder + diff * (max_reach / len);
      }
    };
    clamp_hand_reach(pose.shoulder_l, pose.hand_l);
    clamp_hand_reach(pose.shoulder_r, pose.hand_r);
  }

  QVector3D const hip_l =
      pose.pelvis_pos +
      QVector3D(-HP::HIP_LATERAL_OFFSET, HP::HIP_VERTICAL_OFFSET, 0.0F);
  QVector3D const hip_r =
      pose.pelvis_pos +
      QVector3D(HP::HIP_LATERAL_OFFSET, HP::HIP_VERTICAL_OFFSET, 0.0F);

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
  QVector3D const outward_l = -right_axis;
  QVector3D const outward_r = right_axis;

  float const elbow_along_bias = (variation.bulk_scale - 1.0F) * 0.05F;
  pose.elbow_l = elbow_bend_torso(pose.shoulder_l,
                                  pose.hand_l,
                                  outward_l,
                                  0.45F - elbow_along_bias,
                                  0.10F,
                                  -0.03F,
                                  +1.0F);
  pose.elbow_r = elbow_bend_torso(pose.shoulder_r,
                                  pose.hand_r,
                                  outward_r,
                                  0.45F - elbow_along_bias,
                                  0.08F,
                                  0.0F,
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
