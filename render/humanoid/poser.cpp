

#include "humanoid_renderer_base.h"

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

namespace {

struct LocomotionProfile {
  float planted_fraction{0.5F};
  float stride_length{0.0F};
  float step_height{0.0F};
  float vertical_bob{0.0F};
  float hip_sway{0.0F};
  float shoulder_counter_sway{0.0F};
  float shoulder_twist{0.0F};
  float forward_lean{0.0F};
  float lateral_foot_shift{0.0F};
  float arm_swing{0.0F};
  float max_arm_displacement{0.0F};
  float arm_lift_scale{0.0F};
  float arm_counter_shift{0.0F};
};

auto profile_for_gait(const HumanoidGaitDescriptor &gait,
                      const VariationParams &variation) -> LocomotionProfile {
  LocomotionProfile profile{};
  float const speed_factor =
      std::clamp(gait.normalized_speed > 0.0F ? gait.normalized_speed : 1.0F,
                 0.75F, 1.20F);

  if (gait.state == HumanoidMotionState::Run) {
    profile.planted_fraction = 0.44F;
    profile.stride_length =
        0.28F * variation.walk_speed_mult * (0.92F + 0.12F * speed_factor);
    profile.step_height = 0.072F * (0.85F + 0.20F * speed_factor);
    profile.vertical_bob = 0.010F * speed_factor;
    profile.hip_sway = 0.017F * variation.stance_width;
    profile.shoulder_counter_sway = 0.015F;
    profile.shoulder_twist = 0.014F;
    profile.forward_lean = 0.060F + (speed_factor - 1.0F) * 0.015F;
    profile.lateral_foot_shift = 0.010F;
    profile.arm_swing = 0.145F * variation.arm_swing_amp;
    profile.max_arm_displacement = 0.145F;
    profile.arm_lift_scale = 0.13F;
    profile.arm_counter_shift = 0.12F;
    return profile;
  }

  profile.planted_fraction = 0.58F;
  profile.stride_length =
      0.20F * variation.walk_speed_mult * (0.92F + 0.10F * speed_factor);
  profile.step_height = 0.048F * (0.90F + 0.12F * speed_factor);
  profile.vertical_bob = 0.006F * speed_factor;
  profile.hip_sway = 0.012F * variation.stance_width;
  profile.shoulder_counter_sway = 0.014F;
  profile.shoulder_twist = 0.009F;
  profile.forward_lean = 0.020F + (speed_factor - 1.0F) * 0.010F;
  profile.lateral_foot_shift = 0.006F;
  profile.arm_swing = 0.125F * variation.arm_swing_amp;
  profile.max_arm_displacement = 0.125F;
  profile.arm_lift_scale = 0.12F;
  profile.arm_counter_shift = 0.11F;
  return profile;
}

} // namespace

void HumanoidRendererBase::compute_locomotion_pose(
    uint32_t seed, float time, const HumanoidGaitDescriptor &gait,
    const VariationParams &variation, HumanoidPose &pose) {
  bool const is_moving =
      gait.state == HumanoidMotionState::Walk ||
      gait.state == HumanoidMotionState::Run;

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

  auto ease_in_out = [](float t) {
    t = std::clamp(t, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
  };

  float const h_scale = variation.height_scale;
  float const b_scale = variation.bulk_scale;
  float const s_width = variation.stance_width;
  float const shoulder_span =
      HP::SHOULDER_WIDTH * (1.00F + (b_scale - 1.0F) * 0.10F);
  float const half_shoulder_span = 0.5F * shoulder_span;
  float const chest_forward = 0.014F + variation.posture_slump * 0.08F;
  float const pelvis_setback = -0.010F * h_scale;

  pose.head_pos = QVector3D(0.0F, HP::HEAD_CENTER_Y * h_scale,
                            chest_forward * 0.55F);
  pose.head_r = HP::HEAD_RADIUS * h_scale;
  pose.neck_base =
      QVector3D(0.0F, HP::NECK_BASE_Y * h_scale, chest_forward * 0.35F);

  pose.shoulder_l = QVector3D(-half_shoulder_span, HP::SHOULDER_Y * h_scale,
                              chest_forward);
  pose.shoulder_r = QVector3D(half_shoulder_span, HP::SHOULDER_Y * h_scale,
                              chest_forward);

  pose.pelvis_pos =
      QVector3D(0.0F, HP::WAIST_Y * h_scale - 0.018F * h_scale, pelvis_setback);

  float const rest_stride = 0.018F + (s_width - 1.0F) * 0.024F;
  float const foot_x_span = shoulder_span * 0.40F * s_width;
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
  pose.neck_base.setZ(pose.neck_base.z() + slouch_offset * 0.85F);
  pose.head_pos.setZ(pose.head_pos.z() + slouch_offset * 0.65F);

  float const foot_inward_jitter = (hash_01(seed ^ 0x5678U) - 0.5F) * 0.008F;
  float const foot_forward_jitter = (hash_01(seed ^ 0x9ABCU) - 0.5F) * 0.016F;

  pose.foot_l.setX(pose.foot_l.x() + foot_inward_jitter);
  pose.foot_r.setX(pose.foot_r.x() - foot_inward_jitter);
  pose.foot_l.setZ(pose.foot_l.z() + foot_forward_jitter);
  pose.foot_r.setZ(pose.foot_r.z() - foot_forward_jitter);

  float const arm_height_jitter = (hash_01(seed ^ 0xABCDU) - 0.5F) * 0.016F;
  float const arm_asymmetry = (hash_01(seed ^ 0xDEF0U) - 0.5F) * 0.018F;
  float const relaxed_hand_y =
      HP::CHEST_Y * h_scale - 0.120F + arm_height_jitter;
  float const hand_forward = 0.030F + variation.posture_slump * 0.04F;

  pose.hand_l =
      QVector3D(-half_shoulder_span * 0.62F - 0.022F + arm_asymmetry,
                relaxed_hand_y, hand_forward + 0.010F);
  pose.hand_r = QVector3D(half_shoulder_span * 0.62F + 0.022F -
                               arm_asymmetry * 0.60F,
                           relaxed_hand_y + 0.010F, hand_forward - 0.010F);

  if (is_moving) {
    LocomotionProfile const profile = profile_for_gait(resolved_gait, variation);
    float const walk_phase = resolved_gait.cycle_phase;
    float const left_phase = walk_phase;
    float const right_phase = std::fmod(walk_phase + 0.5F, 1.0F);

    const float ground_y = HP::GROUND_Y;
    float const cycle_radians = walk_phase * 2.0F * std::numbers::pi_v<float>;
    float const vertical_bob =
        -std::abs(std::sin(cycle_radians)) * profile.vertical_bob;
    float const sway_raw = std::sin(cycle_radians);
    float const hip_sway = sway_raw * profile.hip_sway;
    float const shoulder_counter_sway =
        -sway_raw * profile.shoulder_counter_sway;
    float const shoulder_twist = sway_raw * profile.shoulder_twist;
    float const forward_lean = profile.forward_lean;

    auto animate_foot = [&](QVector3D &foot, float base_x, float phase,
                            float lateral_sign) {
      float z_pos = 0.0F;
      if (phase < profile.planted_fraction) {
        float const t = ease_in_out(phase / profile.planted_fraction);
        z_pos =
            profile.stride_length * 0.50F + (-profile.stride_length * 1.00F) * t;
        foot.setY(ground_y + pose.foot_y_offset -
                  std::sin(t * std::numbers::pi_v<float>) * 0.002F);
      } else {
        float const t =
            ease_in_out((phase - profile.planted_fraction) /
                        (1.0F - profile.planted_fraction));
        z_pos = -profile.stride_length * 0.50F +
                (profile.stride_length * 1.00F) * t;
        foot.setY(ground_y + pose.foot_y_offset +
                  std::sin(t * std::numbers::pi_v<float>) * profile.step_height);
      }

      foot.setZ(z_pos);
      foot.setX(base_x - lateral_sign * std::abs(std::sin(phase * 2.0F *
                                                          std::numbers::pi_v<float>)) *
                             profile.lateral_foot_shift);
    };

    float const base_foot_l_x = pose.foot_l.x();
    float const base_foot_r_x = pose.foot_r.x();
    animate_foot(pose.foot_l, base_foot_l_x, left_phase, -1.0F);
    animate_foot(pose.foot_r, base_foot_r_x, right_phase, 1.0F);

    pose.pelvis_pos.setY(pose.pelvis_pos.y() + vertical_bob);
    pose.shoulder_l.setY(pose.shoulder_l.y() + vertical_bob * 0.45F);
    pose.shoulder_r.setY(pose.shoulder_r.y() + vertical_bob * 0.45F);
    pose.neck_base.setY(pose.neck_base.y() + vertical_bob * 0.28F);
    pose.head_pos.setY(pose.head_pos.y() + vertical_bob * 0.18F);

    pose.pelvis_pos.setX(pose.pelvis_pos.x() + hip_sway);
    pose.shoulder_l.setX(pose.shoulder_l.x() + shoulder_counter_sway);
    pose.shoulder_r.setX(pose.shoulder_r.x() + shoulder_counter_sway);
    pose.neck_base.setX(pose.neck_base.x() + shoulder_counter_sway * 0.75F);
    pose.head_pos.setX(pose.head_pos.x() + shoulder_counter_sway * 0.55F);

    pose.pelvis_pos.setZ(pose.pelvis_pos.z() + forward_lean * 0.20F);
    pose.shoulder_l.setZ(pose.shoulder_l.z() + forward_lean + shoulder_twist);
    pose.shoulder_r.setZ(pose.shoulder_r.z() + forward_lean - shoulder_twist);
    pose.neck_base.setZ(pose.neck_base.z() + forward_lean * 0.78F);
    pose.head_pos.setZ(pose.head_pos.z() + forward_lean * 0.68F);

    float const left_swing_raw =
        std::sin(left_phase * 2.0F * std::numbers::pi_v<float>);
    float const left_arm_swing =
        std::clamp(left_swing_raw * profile.arm_swing,
                   -profile.max_arm_displacement, profile.max_arm_displacement);
    pose.hand_l.setZ(pose.hand_l.z() - left_arm_swing);
    pose.hand_l.setY(pose.hand_l.y() +
                     std::abs(left_arm_swing) * profile.arm_lift_scale);
    pose.hand_l.setX(pose.hand_l.x() -
                     left_arm_swing * profile.arm_counter_shift);

    float const right_swing_raw =
        std::sin(right_phase * 2.0F * std::numbers::pi_v<float>);
    float const right_arm_swing =
        std::clamp(right_swing_raw * profile.arm_swing,
                   -profile.max_arm_displacement, profile.max_arm_displacement);
    pose.hand_r.setZ(pose.hand_r.z() - right_arm_swing);
    pose.hand_r.setY(pose.hand_r.y() +
                     std::abs(right_arm_swing) * profile.arm_lift_scale);
    pose.hand_r.setX(pose.hand_r.x() -
                     right_arm_swing * profile.arm_counter_shift);

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

  float const elbow_along_bias = (variation.bulk_scale - 1.0F) * 0.05F;
  pose.elbow_l = elbow_bend_torso(pose.shoulder_l, pose.hand_l, outward_l,
                                  0.45F - elbow_along_bias, 0.10F, -0.03F,
                                  +1.0F);
  pose.elbow_r = elbow_bend_torso(pose.shoulder_r, pose.hand_r, outward_r,
                                  0.45F - elbow_along_bias, 0.08F, 0.0F,
                                  +1.0F);
}

void HumanoidRendererBase::compute_locomotion_pose(
    uint32_t seed, float time, bool is_moving, const VariationParams &variation,
    HumanoidPose &pose) {
  HumanoidGaitDescriptor gait{};
  gait.state =
      is_moving ? HumanoidMotionState::Walk : HumanoidMotionState::Idle;
  gait.normalized_speed = is_moving ? 1.0F : 0.0F;
  if (is_moving) {
    gait.cycle_time = 0.92F / std::max(0.1F, variation.walk_speed_mult);
    gait.cycle_phase =
        std::fmod(time / std::max(0.001F, gait.cycle_time), 1.0F);
  }
  compute_locomotion_pose(seed, time, gait, variation, pose);
}

} // namespace Render::GL
