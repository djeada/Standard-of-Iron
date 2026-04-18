// Stage 2 split — pose-generation layer of the humanoid rig pipeline.
//
// Responsibility: given a deterministic seed, time, a "moving" flag and a set
// of per-unit variation parameters, fill a HumanoidPose with joint positions
// (shoulders, pelvis, hands, elbows, knees, feet, head). No draw calls, no
// materials, no world transform.
//
// This file was split out of rig.cpp as part of the Stage 2 skeleton/pose/
// skin separation. It implements HumanoidRendererBase::compute_locomotion_pose
// which was previously at rig.cpp:452-676.
//
// Kept strictly free of anonymous-namespace statics from rig.cpp so this
// layer has no hidden coupling to the render loop.

#include "rig.h"

#include "../gl/humanoid/animation/animation_inputs.h"
#include "../gl/humanoid/humanoid_constants.h"
#include "humanoid_math.h"
#include "humanoid_specs.h"
#include "pose_controller.h"

#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace Render::GL {

void HumanoidRendererBase::compute_locomotion_pose(
    uint32_t seed, float time, bool is_moving, const VariationParams &variation,
    HumanoidPose &pose) {
  using HP = HumanProportions;

  float const h_scale = variation.height_scale;

  pose.head_pos = QVector3D(0.0F, HP::HEAD_CENTER_Y * h_scale, 0.0F);
  pose.head_r = HP::HEAD_RADIUS * h_scale;
  pose.neck_base = QVector3D(0.0F, HP::NECK_BASE_Y * h_scale, 0.0F);

  float const b_scale = variation.bulk_scale;
  float const s_width = variation.stance_width;

  float const half_shoulder_span = 0.5F * HP::SHOULDER_WIDTH * b_scale;
  pose.shoulder_l =
      QVector3D(-half_shoulder_span, HP::SHOULDER_Y * h_scale, 0.0F);
  pose.shoulder_r =
      QVector3D(half_shoulder_span, HP::SHOULDER_Y * h_scale, 0.0F);

  pose.pelvis_pos = QVector3D(0.0F, HP::WAIST_Y * h_scale, 0.0F);

  float const rest_stride = 0.06F + (variation.arm_swing_amp - 1.0F) * 0.045F;
  float const foot_x_span = HP::SHOULDER_WIDTH * 0.62F * s_width;
  pose.foot_y_offset = HP::FOOT_Y_OFFSET_DEFAULT;
  pose.foot_l =
      QVector3D(-foot_x_span, HP::GROUND_Y + pose.foot_y_offset, rest_stride);
  pose.foot_r =
      QVector3D(foot_x_span, HP::GROUND_Y + pose.foot_y_offset, -rest_stride);

  pose.shoulder_l.setY(pose.shoulder_l.y() + variation.shoulder_tilt);
  pose.shoulder_r.setY(pose.shoulder_r.y() - variation.shoulder_tilt);

  float const slouch_offset = variation.posture_slump * 0.15F;
  pose.shoulder_l.setZ(pose.shoulder_l.z() + slouch_offset);
  pose.shoulder_r.setZ(pose.shoulder_r.z() + slouch_offset);

  float const foot_inward_jitter = (hash_01(seed ^ 0x5678U) - 0.5F) * 0.02F;
  float const foot_forward_jitter = (hash_01(seed ^ 0x9ABCU) - 0.5F) * 0.035F;

  pose.foot_l.setX(pose.foot_l.x() + foot_inward_jitter);
  pose.foot_r.setX(pose.foot_r.x() - foot_inward_jitter);
  pose.foot_l.setZ(pose.foot_l.z() + foot_forward_jitter);
  pose.foot_r.setZ(pose.foot_r.z() - foot_forward_jitter);

  float const arm_height_jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.03F;
  float const arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.04F;

  pose.hand_l =
      QVector3D(-0.05F + arm_asymmetry,
                HP::SHOULDER_Y * h_scale + 0.05F + arm_height_jitter, 0.55F);
  pose.hand_r = QVector3D(
      0.15F - arm_asymmetry * 0.5F,
      HP::SHOULDER_Y * h_scale + 0.15F + arm_height_jitter * 0.8F, 0.20F);

  if (is_moving) {

    float const walk_cycle_time = 0.8F / variation.walk_speed_mult;
    float const walk_phase = std::fmod(time * (1.0F / walk_cycle_time), 1.0F);
    float const left_phase = walk_phase;
    float const right_phase = std::fmod(walk_phase + 0.5F, 1.0F);

    const float ground_y = HP::GROUND_Y;

    const float stride_length = 0.35F * variation.arm_swing_amp;

    float const bob_phase = walk_phase * 2.0F;
    float const vertical_bob =
        std::sin(bob_phase * std::numbers::pi_v<float>) * 0.018F;

    float const hip_sway_amount = 0.002F;
    float const sway_raw =
        std::sin(walk_phase * 2.0F * std::numbers::pi_v<float>);
    float const hip_sway = sway_raw * hip_sway_amount;

    float const torso_sway_z = 0.0F;

    auto animate_foot = [ground_y, &pose, stride_length](QVector3D &foot,
                                                         float phase) {
      float const lift_raw = std::sin(phase * 2.0F * std::numbers::pi_v<float>);
      float lift = 0.0F;
      if (lift_raw > 0.0F) {

        float const lift_phase =
            phase < 0.5F ? phase * 2.0F : (1.0F - phase) * 2.0F;
        float const ease_t =
            lift_phase * lift_phase * (3.0F - 2.0F * lift_phase);
        lift = lift_raw * ease_t;
        foot.setY(ground_y + pose.foot_y_offset + lift * 0.15F);
      } else {
        foot.setY(ground_y + pose.foot_y_offset);
      }

      float const stride_phase =
          (phase - 0.25F) * 2.0F * std::numbers::pi_v<float>;
      foot.setZ(foot.z() + std::sin(stride_phase) * stride_length);
    };

    animate_foot(pose.foot_l, left_phase);
    animate_foot(pose.foot_r, right_phase);

    pose.pelvis_pos.setY(pose.pelvis_pos.y() + vertical_bob);
    pose.shoulder_l.setY(pose.shoulder_l.y() + vertical_bob);
    pose.shoulder_r.setY(pose.shoulder_r.y() + vertical_bob);
    pose.neck_base.setY(pose.neck_base.y() + vertical_bob);
    pose.head_pos.setY(pose.head_pos.y() + vertical_bob);

    pose.pelvis_pos.setX(pose.pelvis_pos.x() + hip_sway);

    pose.shoulder_l.setZ(pose.shoulder_l.z() + torso_sway_z);
    pose.shoulder_r.setZ(pose.shoulder_r.z() + torso_sway_z);
    pose.neck_base.setZ(pose.neck_base.z() + torso_sway_z * 0.7F);
    pose.head_pos.setZ(pose.head_pos.z() + torso_sway_z * 0.5F);

    float const arm_swing_amp = 0.04F * variation.arm_swing_amp;
    float const arm_phase_offset = 0.15F;
    constexpr float max_arm_displacement = 0.06F;

    float const left_swing_raw = std::sin((left_phase + arm_phase_offset) *
                                          2.0F * std::numbers::pi_v<float>);
    float const left_arm_swing =
        std::clamp(left_swing_raw * arm_swing_amp, -max_arm_displacement,
                   max_arm_displacement);
    pose.hand_l.setZ(pose.hand_l.z() - left_arm_swing);

    float const right_swing_raw = std::sin((right_phase + arm_phase_offset) *
                                           2.0F * std::numbers::pi_v<float>);
    float const right_arm_swing =
        std::clamp(right_swing_raw * arm_swing_amp, -max_arm_displacement,
                   max_arm_displacement);
    pose.hand_r.setZ(pose.hand_r.z() - right_arm_swing);

    auto clamp_hand_reach = [&](const QVector3D &shoulder, QVector3D &hand) {
      float const max_reach =
          (HP::UPPER_ARM_LEN + HP::FORE_ARM_LEN) * h_scale * 0.98F;
      QVector3D diff = hand - shoulder;
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

  auto solve_leg = [&](const QVector3D &hip, const QVector3D &foot,
                       bool is_left) -> QVector3D {
    QVector3D hip_to_foot = foot - hip;
    float const distance = hip_to_foot.length();
    if (distance < HP::EPSILON_SMALL) {
      return hip;
    }

    float const upper_len = HP::UPPER_LEG_LEN * h_scale;
    float const lower_len = HP::LOWER_LEG_LEN * h_scale;
    float const reach = upper_len + lower_len;
    float const min_reach =
        std::max(std::abs(upper_len - lower_len) + 1e-4F, 1e-3F);
    float const max_reach = std::max(reach - 1e-4F, min_reach + 1e-4F);
    float const clamped_dist = std::clamp(distance, min_reach, max_reach);

    QVector3D const dir = hip_to_foot / distance;

    float cos_theta = (upper_len * upper_len + clamped_dist * clamped_dist -
                       lower_len * lower_len) /
                      (2.0F * upper_len * clamped_dist);
    cos_theta = std::clamp(cos_theta, -1.0F, 1.0F);
    float const sin_theta =
        std::sqrt(std::max(0.0F, 1.0F - cos_theta * cos_theta));

    QVector3D bend_pref = is_left ? QVector3D(-0.24F, 0.0F, 0.95F)
                                  : QVector3D(0.24F, 0.0F, 0.95F);
    bend_pref.normalize();

    QVector3D bend_axis =
        bend_pref - dir * QVector3D::dotProduct(dir, bend_pref);
    if (bend_axis.lengthSquared() < 1e-6F) {
      bend_axis = QVector3D::crossProduct(dir, QVector3D(0.0F, 1.0F, 0.0F));
      if (bend_axis.lengthSquared() < 1e-6F) {
        bend_axis = QVector3D::crossProduct(dir, QVector3D(1.0F, 0.0F, 0.0F));
      }
    }
    bend_axis.normalize();

    QVector3D const knee = hip + dir * (cos_theta * upper_len) +
                           bend_axis * (sin_theta * upper_len);

    float const knee_floor = HP::GROUND_Y + pose.foot_y_offset * 0.5F;
    if (knee.y() < knee_floor) {
      QVector3D adjusted = knee;
      adjusted.setY(knee_floor);
      return adjusted;
    }

    return knee;
  };

  pose.knee_l = solve_leg(hip_l, pose.foot_l, true);
  pose.knee_r = solve_leg(hip_r, pose.foot_r, false);

  QVector3D right_axis = pose.shoulder_r - pose.shoulder_l;
  right_axis.setY(0.0F);
  if (right_axis.lengthSquared() < 1e-8F) {
    right_axis = QVector3D(1, 0, 0);
  }
  right_axis.normalize();

  if (right_axis.x() < 0.0F) {
    right_axis = -right_axis;
  }
  QVector3D const outward_l = -right_axis;
  QVector3D const outward_r = right_axis;

  pose.elbow_l = elbow_bend_torso(pose.shoulder_l, pose.hand_l, outward_l,
                                  0.45F, 0.15F, -0.08F, +1.0F);
  pose.elbow_r = elbow_bend_torso(pose.shoulder_r, pose.hand_r, outward_r,
                                  0.48F, 0.12F, 0.02F, +1.0F);
}

} // namespace Render::GL
