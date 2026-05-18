

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

#include "../gl/humanoid/animation/animation_inputs.h"
#include "../gl/humanoid/humanoid_constants.h"
#include "humanoid_math.h"
#include "humanoid_renderer_base.h"
#include "humanoid_specs.h"
#include "pose_controller.h"

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

auto blend_profile(const LocomotionProfile& a,
                   const LocomotionProfile& b,
                   float blend) -> LocomotionProfile {
  auto const mix = [blend](float from, float to) {
    return from + (to - from) * blend;
  };

  LocomotionProfile out{};
  out.planted_fraction = mix(a.planted_fraction, b.planted_fraction);
  out.stride_length = mix(a.stride_length, b.stride_length);
  out.step_height = mix(a.step_height, b.step_height);
  out.vertical_bob = mix(a.vertical_bob, b.vertical_bob);
  out.hip_sway = mix(a.hip_sway, b.hip_sway);
  out.shoulder_counter_sway = mix(a.shoulder_counter_sway, b.shoulder_counter_sway);
  out.shoulder_twist = mix(a.shoulder_twist, b.shoulder_twist);
  out.forward_lean = mix(a.forward_lean, b.forward_lean);
  out.lateral_foot_shift = mix(a.lateral_foot_shift, b.lateral_foot_shift);
  out.arm_swing = mix(a.arm_swing, b.arm_swing);
  out.max_arm_displacement = mix(a.max_arm_displacement, b.max_arm_displacement);
  out.arm_lift_scale = mix(a.arm_lift_scale, b.arm_lift_scale);
  out.arm_counter_shift = mix(a.arm_counter_shift, b.arm_counter_shift);
  return out;
}

auto walk_profile(float speed_factor,
                  const VariationParams& variation) -> LocomotionProfile {
  LocomotionProfile profile{};
  profile.planted_fraction = 0.58F;
  profile.stride_length =
      0.20F * variation.walk_speed_mult * (0.92F + 0.10F * speed_factor);
  profile.step_height = 0.048F * (0.90F + 0.12F * speed_factor);
  profile.vertical_bob = 0.006F * speed_factor;
  profile.hip_sway = 0.014F * variation.stance_width;
  profile.shoulder_counter_sway = 0.016F;
  profile.shoulder_twist = 0.012F;
  profile.forward_lean = 0.020F + (speed_factor - 1.0F) * 0.010F;
  profile.lateral_foot_shift = 0.006F;
  profile.arm_swing = 0.125F * variation.arm_swing_amp;
  profile.max_arm_displacement = 0.125F;
  profile.arm_lift_scale = 0.12F;
  profile.arm_counter_shift = 0.11F;
  return profile;
}

auto run_profile(float speed_factor,
                 const VariationParams& variation) -> LocomotionProfile {
  LocomotionProfile profile{};
  profile.planted_fraction = 0.44F;
  profile.stride_length =
      0.28F * variation.walk_speed_mult * (0.92F + 0.12F * speed_factor);
  profile.step_height = 0.072F * (0.85F + 0.20F * speed_factor);
  profile.vertical_bob = 0.012F * speed_factor;
  profile.hip_sway = 0.017F * variation.stance_width;
  profile.shoulder_counter_sway = 0.020F;
  profile.shoulder_twist = 0.018F;
  profile.forward_lean = 0.060F + (speed_factor - 1.0F) * 0.015F;
  profile.lateral_foot_shift = 0.010F;
  profile.arm_swing = 0.155F * variation.arm_swing_amp;
  profile.max_arm_displacement = 0.155F;
  profile.arm_lift_scale = 0.15F;
  profile.arm_counter_shift = 0.12F;
  return profile;
}

auto profile_for_gait(const HumanoidGaitDescriptor& gait,
                      const VariationParams& variation) -> LocomotionProfile {
  float const speed_factor = std::clamp(
      gait.normalized_speed > 0.0F ? gait.normalized_speed : 1.0F, 0.75F, 1.20F);
  float const run_blend = std::clamp(
      (gait.run_blend > 0.0F) ? gait.run_blend
                              : (gait.state == HumanoidMotionState::Run ? 1.0F : 0.0F),
      0.0F,
      1.0F);
  return blend_profile(walk_profile(speed_factor, variation),
                       run_profile(speed_factor, variation),
                       run_blend);
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

  auto ease_in_out = [](float t) {
    t = std::clamp(t, 0.0F, 1.0F);
    return t * t * (3.0F - 2.0F * t);
  };

  float const h_scale = variation.height_scale;
  float const b_scale = variation.bulk_scale;
  float const s_width = variation.stance_width;
  float const shoulder_span = HP::SHOULDER_WIDTH * (1.00F + (b_scale - 1.0F) * 0.10F);
  float const half_shoulder_span = 0.5F * shoulder_span;
  float const chest_forward = 0.014F + variation.posture_slump * 0.08F;
  float const pelvis_setback = -0.010F * h_scale;

  pose.head_pos = QVector3D(0.0F, HP::HEAD_CENTER_Y * h_scale, chest_forward * 0.55F);
  pose.head_r = HP::HEAD_RADIUS * h_scale;
  pose.neck_base = QVector3D(0.0F, HP::NECK_BASE_Y * h_scale, chest_forward * 0.35F);

  pose.shoulder_l =
      QVector3D(-half_shoulder_span, HP::SHOULDER_Y * h_scale, chest_forward);
  pose.shoulder_r =
      QVector3D(half_shoulder_span, HP::SHOULDER_Y * h_scale, chest_forward);

  pose.pelvis_pos =
      QVector3D(0.0F, HP::WAIST_Y * h_scale - 0.018F * h_scale, pelvis_setback);

  float const rest_stride = 0.018F + (s_width - 1.0F) * 0.024F;
  float const foot_x_span = shoulder_span * 0.40F * s_width;
  pose.foot_y_offset = HP::FOOT_Y_OFFSET_DEFAULT;
  pose.foot_l = QVector3D(-foot_x_span, HP::GROUND_Y + pose.foot_y_offset, rest_stride);
  pose.foot_r = QVector3D(foot_x_span, HP::GROUND_Y + pose.foot_y_offset, -rest_stride);

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
  float const relaxed_hand_y = HP::CHEST_Y * h_scale - 0.120F + arm_height_jitter;
  float const hand_forward = 0.030F + variation.posture_slump * 0.04F;

  pose.hand_l = QVector3D(-half_shoulder_span * 0.62F - 0.022F + arm_asymmetry,
                          relaxed_hand_y,
                          hand_forward + 0.010F);
  pose.hand_r = QVector3D(half_shoulder_span * 0.62F + 0.022F - arm_asymmetry * 0.60F,
                          relaxed_hand_y + 0.010F,
                          hand_forward - 0.010F);

  if (is_moving) {
    LocomotionProfile const profile = profile_for_gait(resolved_gait, variation);
    float const locomotion_blend = std::clamp((resolved_gait.locomotion_blend > 0.0F)
                                                  ? resolved_gait.locomotion_blend
                                                  : resolved_gait.normalized_speed,
                                              0.0F,
                                              1.0F);
    float const walk_phase = resolved_gait.cycle_phase;
    float const left_phase = walk_phase;
    float const right_phase = std::fmod(walk_phase + 0.5F, 1.0F);

    const float ground_y = HP::GROUND_Y;
    float const cycle_radians = walk_phase * 2.0F * std::numbers::pi_v<float>;
    float const acceleration = std::clamp(resolved_gait.acceleration, -2.0F, 2.0F);
    float const acceleration_push = std::max(0.0F, acceleration);
    float const braking = std::max(0.0F, -acceleration);
    float const turn_amount = std::clamp(resolved_gait.turn_amount, -1.0F, 1.0F);
    float const turn_abs = std::abs(turn_amount);
    float const stride_scale = std::max(
        0.10F, locomotion_blend * (1.0F + acceleration_push * 0.12F - braking * 0.16F));
    float const step_scale =
        std::max(0.15F, locomotion_blend * (1.0F + acceleration_push * 0.08F));
    float const planted_fraction =
        std::clamp(profile.planted_fraction + braking * 0.06F -
                       acceleration_push * 0.04F + turn_abs * 0.04F,
                   0.38F,
                   0.74F);
    float const vertical_bob =
        -std::abs(std::sin(cycle_radians)) * profile.vertical_bob * stride_scale;
    float const sway_raw = std::sin(cycle_radians);
    float const hip_sway = sway_raw * profile.hip_sway * stride_scale;
    float const shoulder_counter_sway =
        -sway_raw * profile.shoulder_counter_sway * stride_scale;
    float const shoulder_twist = sway_raw * profile.shoulder_twist * stride_scale;
    float const acceleration_lean = acceleration * 0.006F;
    float const braking_sink = braking * 0.010F * locomotion_blend;
    float const forward_lean = profile.forward_lean * stride_scale + acceleration_lean;
    float const turn_lean = turn_amount * (0.010F + 0.012F * locomotion_blend);
    float const turn_twist = turn_amount * (0.008F + 0.010F * locomotion_blend);
    float const turn_step_bias = turn_amount * 0.018F * stride_scale;

    auto animate_foot = [&](QVector3D& foot,
                            float base_x,
                            float base_z,
                            float phase,
                            float lateral_sign) {
      float z_pos = 0.0F;
      float foot_y = ground_y + pose.foot_y_offset;
      float const foot_turn_scale = 1.0F - turn_amount * lateral_sign * 0.45F;
      float const foot_stride_length =
          profile.stride_length * stride_scale * std::max(0.60F, foot_turn_scale);
      float const foot_step_height =
          profile.step_height * step_scale * std::max(0.70F, 1.0F - turn_abs * 0.12F);
      if (phase < planted_fraction) {
        float const t = ease_in_out(phase / planted_fraction);
        z_pos = foot_stride_length * 0.50F + (-foot_stride_length * 1.00F) * t;
        foot_y -= std::sin(t * std::numbers::pi_v<float>) * 0.002F * stride_scale;
      } else {
        float const t =
            ease_in_out((phase - planted_fraction) / (1.0F - planted_fraction));
        z_pos = -foot_stride_length * 0.50F + (foot_stride_length * 1.00F) * t;
        foot_y += std::sin(t * std::numbers::pi_v<float>) * foot_step_height;
      }

      float const stance_turn_bias =
          turn_amount * lateral_sign * (0.014F + 0.006F * locomotion_blend);
      float const target_x =
          base_x -
          lateral_sign * std::abs(std::sin(phase * 2.0F * std::numbers::pi_v<float>)) *
              profile.lateral_foot_shift * stride_scale +
          turn_step_bias * lateral_sign + stance_turn_bias;
      float const target_z =
          z_pos * locomotion_blend + base_z * (1.0F - locomotion_blend);

      foot.setX(base_x + (target_x - base_x) * locomotion_blend);
      foot.setY(ground_y + pose.foot_y_offset +
                (foot_y - (ground_y + pose.foot_y_offset)) * locomotion_blend);
      foot.setZ(target_z);
    };

    float const base_foot_l_x = pose.foot_l.x();
    float const base_foot_r_x = pose.foot_r.x();
    float const base_foot_l_z = pose.foot_l.z();
    float const base_foot_r_z = pose.foot_r.z();
    animate_foot(pose.foot_l, base_foot_l_x, base_foot_l_z, left_phase, -1.0F);
    animate_foot(pose.foot_r, base_foot_r_x, base_foot_r_z, right_phase, 1.0F);

    pose.pelvis_pos.setY(pose.pelvis_pos.y() + vertical_bob);
    pose.shoulder_l.setY(pose.shoulder_l.y() + vertical_bob * 0.45F);
    pose.shoulder_r.setY(pose.shoulder_r.y() + vertical_bob * 0.45F);
    pose.neck_base.setY(pose.neck_base.y() + vertical_bob * 0.28F);
    pose.head_pos.setY(pose.head_pos.y() + vertical_bob * 0.18F);
    pose.pelvis_pos.setY(pose.pelvis_pos.y() - braking_sink);
    pose.shoulder_l.setY(pose.shoulder_l.y() - braking_sink * 0.20F);
    pose.shoulder_r.setY(pose.shoulder_r.y() - braking_sink * 0.20F);
    pose.head_pos.setY(pose.head_pos.y() +
                       acceleration_push * 0.003F * locomotion_blend);

    pose.pelvis_pos.setX(pose.pelvis_pos.x() + hip_sway);
    pose.shoulder_l.setX(pose.shoulder_l.x() + shoulder_counter_sway);
    pose.shoulder_r.setX(pose.shoulder_r.x() + shoulder_counter_sway);
    pose.neck_base.setX(pose.neck_base.x() + shoulder_counter_sway * 0.75F);
    pose.head_pos.setX(pose.head_pos.x() + shoulder_counter_sway * 0.55F);
    pose.pelvis_pos.setX(pose.pelvis_pos.x() + turn_lean * 0.55F);
    pose.shoulder_l.setX(pose.shoulder_l.x() + turn_lean);
    pose.shoulder_r.setX(pose.shoulder_r.x() + turn_lean);
    pose.head_pos.setX(pose.head_pos.x() + turn_lean * 0.40F);

    pose.pelvis_pos.setZ(pose.pelvis_pos.z() + forward_lean * 0.20F);
    pose.shoulder_l.setZ(pose.shoulder_l.z() + forward_lean + shoulder_twist +
                         turn_twist);
    pose.shoulder_r.setZ(pose.shoulder_r.z() + forward_lean - shoulder_twist -
                         turn_twist);
    pose.neck_base.setZ(pose.neck_base.z() + forward_lean * 0.78F);
    pose.head_pos.setZ(pose.head_pos.z() + forward_lean * 0.68F);
    pose.head_pos.setZ(pose.head_pos.z() - braking * 0.004F * locomotion_blend);

    float const left_swing_raw =
        std::sin(left_phase * 2.0F * std::numbers::pi_v<float>);
    float const left_arm_swing = std::clamp(left_swing_raw * profile.arm_swing,
                                            -profile.max_arm_displacement,
                                            profile.max_arm_displacement) *
                                 stride_scale;
    pose.hand_l.setZ(pose.hand_l.z() - left_arm_swing);
    pose.hand_l.setY(pose.hand_l.y() +
                     std::abs(left_arm_swing) * profile.arm_lift_scale);
    pose.hand_l.setX(pose.hand_l.x() - left_arm_swing * profile.arm_counter_shift);

    float const right_swing_raw =
        std::sin(right_phase * 2.0F * std::numbers::pi_v<float>);
    float const right_arm_swing = std::clamp(right_swing_raw * profile.arm_swing,
                                             -profile.max_arm_displacement,
                                             profile.max_arm_displacement) *
                                  stride_scale;
    pose.hand_r.setZ(pose.hand_r.z() - right_arm_swing);
    pose.hand_r.setY(pose.hand_r.y() +
                     std::abs(right_arm_swing) * profile.arm_lift_scale);
    pose.hand_r.setX(pose.hand_r.x() - right_arm_swing * profile.arm_counter_shift);
    pose.hand_l.setX(pose.hand_l.x() + turn_amount * 0.010F * locomotion_blend);
    pose.hand_r.setX(pose.hand_r.x() + turn_amount * 0.010F * locomotion_blend);

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
    QVector3D const hip_to_foot = foot - hip;
    float const distance = hip_to_foot.length();
    if (distance < HP::EPSILON_SMALL) {
      return hip;
    }

    float const upper_len = HP::UPPER_LEG_LEN * h_scale;
    float const lower_len = HP::LOWER_LEG_LEN * h_scale;
    float const reach = upper_len + lower_len;
    float const min_reach = std::max(std::abs(upper_len - lower_len) + 1e-4F, 1e-3F);
    float const max_reach = std::max(reach - 1e-4F, min_reach + 1e-4F);
    float const clamped_dist = std::clamp(distance, min_reach, max_reach);

    QVector3D const dir = hip_to_foot / distance;

    float cos_theta =
        (upper_len * upper_len + clamped_dist * clamped_dist - lower_len * lower_len) /
        (2.0F * upper_len * clamped_dist);
    cos_theta = std::clamp(cos_theta, -1.0F, 1.0F);
    float const sin_theta = std::sqrt(std::max(0.0F, 1.0F - cos_theta * cos_theta));

    QVector3D bend_pref = (side == Side::Left) ? QVector3D(-0.24F, 0.0F, 0.95F)
                                               : QVector3D(0.24F, 0.0F, 0.95F);
    bend_pref.normalize();

    QVector3D bend_axis = bend_pref - dir * QVector3D::dotProduct(dir, bend_pref);
    if (bend_axis.lengthSquared() < 1e-6F) {
      bend_axis = QVector3D::crossProduct(dir, QVector3D(0.0F, 1.0F, 0.0F));
      if (bend_axis.lengthSquared() < 1e-6F) {
        bend_axis = QVector3D::crossProduct(dir, QVector3D(1.0F, 0.0F, 0.0F));
      }
    }
    bend_axis.normalize();

    QVector3D const knee =
        hip + dir * (cos_theta * upper_len) + bend_axis * (sin_theta * upper_len);

    float const knee_floor = HP::GROUND_Y + pose.foot_y_offset * 0.5F;
    if (knee.y() < knee_floor) {
      QVector3D adjusted = knee;
      adjusted.setY(knee_floor);
      return adjusted;
    }

    return knee;
  };

  pose.knee_l = solve_leg(hip_l, pose.foot_l, Side::Left);
  pose.knee_r = solve_leg(hip_r, pose.foot_r, Side::Right);

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
