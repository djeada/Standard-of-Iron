#include "posture_pose_manifest.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Animation {

namespace {

[[nodiscard]] auto hash_to_unit(std::uint32_t x) noexcept -> float {
  x ^= x >> 16U;
  x *= 0x7feb352dU;
  x ^= x >> 15U;
  x *= 0x846ca68bU;
  x ^= x >> 16U;
  return static_cast<float>(x & 0x7FFFFFU) / static_cast<float>(0x7FFFFFU);
}

[[nodiscard]] auto smoothstep(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] auto length_squared(PoseVec3 value) noexcept -> float {
  return value.x * value.x + value.y * value.y + value.z * value.z;
}

[[nodiscard]] auto scale(PoseVec3 value, float factor) noexcept -> PoseVec3 {
  return {value.x * factor, value.y * factor, value.z * factor};
}

[[nodiscard]] auto add(PoseVec3 lhs, PoseVec3 rhs) noexcept -> PoseVec3 {
  return {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
}

void add_axis_delta(MountedAxisDelta& target, MountedAxisDelta delta) noexcept {
  target.forward += delta.forward;
  target.right += delta.right;
  target.up += delta.up;
  target.world_y += delta.world_y;
}

[[nodiscard]] auto scale_axis(MountedAxisDelta delta,
                              float factor) noexcept -> MountedAxisDelta {
  return {delta.forward * factor,
          delta.right * factor,
          delta.up * factor,
          delta.world_y * factor};
}

[[nodiscard]] auto normalize_or(PoseVec3 value,
                                PoseVec3 fallback) noexcept -> PoseVec3 {
  float const len_sq = length_squared(value);
  if (len_sq <= 1.0e-6F) {
    return fallback;
  }
  return scale(value, 1.0F / std::sqrt(len_sq));
}

} // namespace

auto resolve_humanoid_micro_idle_pose(
    const HumanoidMicroIdlePoseInputs& inputs) noexcept -> HumanoidPostureDeltaSample {
  float const two_pi = 2.0F * std::numbers::pi_v<float>;
  float const breath_phase_off = hash_to_unit(inputs.seed ^ 0x1A2B3C4DU) * two_pi;
  float const sway_phase_off = hash_to_unit(inputs.seed ^ 0x5E6F7A8BU) * two_pi;
  float const head_phase_off = hash_to_unit(inputs.seed ^ 0x9CDE0F12U) * two_pi;

  float const breath_freq = 0.85F + hash_to_unit(inputs.seed ^ 0x33445566U) * 0.45F;
  float const sway_freq = 0.18F + hash_to_unit(inputs.seed ^ 0x778899AAU) * 0.10F;
  float const head_freq = 0.32F + hash_to_unit(inputs.seed ^ 0xBBCCDDEEU) * 0.18F;

  float const breath =
      std::sin(inputs.sample_time * breath_freq * two_pi + breath_phase_off);
  float const chest_lift = breath * 0.012F;
  float const breath_torso_z = breath * 0.005F;
  float const weight_shift =
      std::sin(inputs.sample_time * sway_freq * two_pi + sway_phase_off);
  float const lateral = weight_shift * 0.016F;
  float const weight_dip = -std::abs(weight_shift) * 0.005F;
  float const head_yaw =
      std::sin(inputs.sample_time * head_freq * two_pi + head_phase_off);
  float const head_nod =
      std::sin(inputs.sample_time * head_freq * 0.7F * two_pi + head_phase_off * 0.6F) *
      0.006F;
  float const arm_drift =
      std::sin(inputs.sample_time * (breath_freq * 0.55F) * two_pi + breath_phase_off);

  HumanoidPostureDeltaSample sample{};
  sample.shoulder_l_y_delta += chest_lift;
  sample.shoulder_r_y_delta += chest_lift;
  sample.neck_y_delta += chest_lift * 0.75F;
  sample.head_y_delta += chest_lift * 0.55F;
  sample.shoulder_l_z_delta -= breath_torso_z;
  sample.shoulder_r_z_delta -= breath_torso_z;
  sample.pelvis_x_delta += lateral;
  sample.shoulder_l_x_delta += lateral * 0.4F;
  sample.shoulder_r_x_delta += lateral * 0.4F;
  sample.head_x_delta += lateral * 0.25F;
  sample.pelvis_y_delta += weight_dip;
  sample.head_x_delta += head_yaw * 0.014F;
  sample.head_z_delta += head_nod;
  sample.hand_l_y_delta += arm_drift * 0.009F;
  sample.hand_r_y_delta += arm_drift * 0.009F * 0.85F;
  return sample;
}

auto resolve_humanoid_kneel_pose(const HumanoidKneelPoseInputs& inputs) noexcept
    -> HumanoidKneelPoseSample {
  float const depth = std::clamp(inputs.depth, 0.0F, 1.0F);
  if (depth < 1.0e-6F) {
    return {};
  }

  float const eased_depth = smoothstep(depth);
  float const kneel_offset = eased_depth * 0.40F;
  float const pelvis_y = inputs.waist_y - kneel_offset;
  float const stance_narrow = 0.11F;
  float const left_knee_y = inputs.ground_y + 0.07F * eased_depth;
  float const left_knee_z = -0.06F * eased_depth;
  float const right_knee_y = pelvis_y - 0.12F;
  float const right_foot_z = 0.28F * eased_depth;
  float const forward_lean = 0.03F * eased_depth;

  HumanoidKneelPoseSample sample{};
  sample.active = true;
  sample.pelvis = {0.0F, pelvis_y, 0.0F};
  sample.knee_l = {-stance_narrow, left_knee_y, left_knee_z};
  sample.foot_l = {-stance_narrow - 0.025F,
                   inputs.ground_y,
                   left_knee_z - inputs.lower_leg_len * 0.93F * eased_depth};
  sample.knee_r = {stance_narrow, right_knee_y, right_foot_z - 0.05F};
  sample.foot_r = {stance_narrow, inputs.ground_y + inputs.foot_y_offset, right_foot_z};
  sample.upper_body.shoulder_l_y_delta -= kneel_offset;
  sample.upper_body.shoulder_r_y_delta -= kneel_offset;
  sample.upper_body.neck_y_delta -= kneel_offset;
  sample.upper_body.head_y_delta -= kneel_offset;
  sample.upper_body.shoulder_l_z_delta += forward_lean;
  sample.upper_body.shoulder_r_z_delta += forward_lean;
  sample.upper_body.neck_z_delta += forward_lean * 0.8F;
  sample.upper_body.head_z_delta += forward_lean * 0.6F;
  return sample;
}

auto resolve_humanoid_kneel_transition_pose(
    const HumanoidKneelTransitionPoseInputs& inputs) noexcept
    -> HumanoidPostureDeltaSample {
  float const progress = std::clamp(inputs.progress, 0.0F, 1.0F);
  float const kneel_amount = inputs.standing_up ? (1.0F - progress) : progress;

  HumanoidPostureDeltaSample sample{};
  if (inputs.standing_up) {
    if (progress < 0.35F) {
      float const push_t = smoothstep(progress / 0.35F);
      float const momentum_lean = 0.06F * push_t;
      sample.foot_r_z_delta -= 0.08F * push_t;
      sample.knee_r_z_delta -= 0.05F * push_t;
      sample.shoulder_l_z_delta += momentum_lean;
      sample.shoulder_r_z_delta += momentum_lean;
      sample.neck_z_delta += momentum_lean * 0.9F;
      sample.head_z_delta += momentum_lean * 0.7F;
      sample.hand_l_z_delta += 0.04F * push_t;
      sample.hand_r_z_delta += 0.04F * push_t;
    } else if (progress < 0.70F) {
      float const rise_t = smoothstep((progress - 0.35F) / 0.35F);
      float const lift_boost = 0.02F * std::sin(rise_t * std::numbers::pi_v<float>);
      sample.pelvis_y_delta += lift_boost;
      sample.shoulder_l_y_delta += lift_boost;
      sample.shoulder_r_y_delta += lift_boost;
      sample.foot_l_z_delta += 0.15F * rise_t;
      sample.knee_l_z_delta += 0.10F * rise_t;
      sample.knee_l_y_delta += 0.20F * rise_t;
    } else {
      float const settle_t = smoothstep((progress - 0.70F) / 0.30F);
      float const correct_lean = -0.04F * settle_t * (1.0F - kneel_amount);
      sample.shoulder_l_z_delta += correct_lean;
      sample.shoulder_r_z_delta += correct_lean;
    }
  } else {
    if (progress < 0.30F) {
      float const prep_t = smoothstep(progress / 0.30F);
      sample.pelvis_z_delta -= 0.03F * prep_t;
      sample.hand_l_y_delta -= 0.02F * prep_t;
      sample.hand_r_y_delta -= 0.02F * prep_t;
    } else if (progress < 0.75F) {
      float const t = (progress - 0.30F) / 0.45F;
      float const controlled_lean = 0.04F * std::sin(t * std::numbers::pi_v<float>);
      sample.shoulder_l_z_delta += controlled_lean;
      sample.shoulder_r_z_delta += controlled_lean;
    } else {
      float const settle_t = smoothstep((progress - 0.75F) / 0.25F);
      sample.knee_l_y_delta -= 0.01F * settle_t;
    }
  }

  return sample;
}

auto resolve_humanoid_hit_flinch_pose(
    const HumanoidHitFlinchPoseInputs& inputs) noexcept -> HumanoidPostureDeltaSample {
  float const intensity = std::clamp(inputs.intensity, 0.0F, 1.0F);
  if (intensity < 0.01F) {
    return {};
  }

  float const flinch_back = intensity * 0.06F;
  float const flinch_down = intensity * 0.04F;
  float const shoulder_drop = intensity * 0.03F;

  HumanoidPostureDeltaSample sample{};
  sample.head_z_delta -= flinch_back;
  sample.head_y_delta -= flinch_down * 0.5F;
  sample.neck_z_delta -= flinch_back * 0.8F;
  sample.shoulder_l_y_delta -= shoulder_drop;
  sample.shoulder_r_y_delta -= shoulder_drop;
  sample.shoulder_l_z_delta -= flinch_back * 0.6F;
  sample.shoulder_r_z_delta -= flinch_back * 0.6F;
  sample.pelvis_y_delta -= flinch_down * 0.3F;
  return sample;
}

auto resolve_humanoid_lean_pose(const HumanoidLeanPoseInputs& inputs) noexcept
    -> HumanoidPostureDeltaSample {
  PoseVec3 const direction = normalize_or(inputs.direction, {0.0F, 0.0F, 1.0F});
  float const amount = std::clamp(inputs.amount, 0.0F, 1.0F);
  PoseVec3 const lean_offset = scale(direction, 0.12F * amount);

  HumanoidPostureDeltaSample sample{};
  sample.shoulder_l_x_delta += lean_offset.x;
  sample.shoulder_l_y_delta += lean_offset.y;
  sample.shoulder_l_z_delta += lean_offset.z;
  sample.shoulder_r_x_delta += lean_offset.x;
  sample.shoulder_r_y_delta += lean_offset.y;
  sample.shoulder_r_z_delta += lean_offset.z;
  sample.neck_x_delta += lean_offset.x * 0.85F;
  sample.neck_y_delta += lean_offset.y * 0.85F;
  sample.neck_z_delta += lean_offset.z * 0.85F;
  sample.head_x_delta += lean_offset.x * 0.75F;
  sample.head_y_delta += lean_offset.y * 0.75F;
  sample.head_z_delta += lean_offset.z * 0.75F;
  return sample;
}

auto resolve_humanoid_look_at_pose(const HumanoidLookAtPoseInputs& inputs) noexcept
    -> HumanoidPostureDeltaSample {
  PoseVec3 const head_to_target{inputs.target.x - inputs.head_position.x,
                                inputs.target.y - inputs.head_position.y,
                                inputs.target.z - inputs.head_position.z};
  if (length_squared(head_to_target) < 1.0e-6F) {
    return {};
  }

  PoseVec3 const direction = normalize_or(head_to_target, {0.0F, 0.0F, 1.0F});
  PoseVec3 const head_offset = scale(direction, 0.03F);

  HumanoidPostureDeltaSample sample{};
  sample.head_x_delta += head_offset.x;
  sample.head_z_delta += head_offset.z;
  sample.neck_x_delta += head_offset.x * 0.5F;
  sample.neck_z_delta += head_offset.z * 0.5F;
  return sample;
}

auto resolve_humanoid_torso_tilt_pose(
    const HumanoidTorsoTiltPoseInputs& inputs) noexcept -> HumanoidPostureDeltaSample {
  PoseVec3 const offset = add(scale(inputs.heading_right, inputs.side_tilt),
                              scale(inputs.heading_forward, inputs.forward_tilt));

  HumanoidPostureDeltaSample sample{};
  sample.shoulder_l_x_delta += offset.x;
  sample.shoulder_l_y_delta += offset.y;
  sample.shoulder_l_z_delta += offset.z;
  sample.shoulder_r_x_delta += offset.x;
  sample.shoulder_r_y_delta += offset.y;
  sample.shoulder_r_z_delta += offset.z;
  sample.neck_x_delta += offset.x * 1.2F;
  sample.neck_y_delta += offset.y * 1.2F;
  sample.neck_z_delta += offset.z * 1.2F;
  sample.head_x_delta += offset.x * 1.5F;
  sample.head_y_delta += offset.y * 1.5F;
  sample.head_z_delta += offset.z * 1.5F;
  sample.torso_frame_origin_delta = offset;
  sample.head_frame_origin_delta = scale(offset, 1.5F);
  return sample;
}

auto resolve_mounted_rider_lean_pose(const MountedRiderLeanPoseInputs& inputs) noexcept
    -> MountedRiderPostureSample {
  float const clamped_forward = std::clamp(inputs.forward_lean, -1.0F, 1.0F);
  float const clamped_side = std::clamp(inputs.side_lean, -1.0F, 1.0F);
  MountedAxisDelta const lean_offset{
      .forward = clamped_forward * 0.15F,
      .right = clamped_side * 0.10F,
  };

  MountedRiderPostureSample sample{};
  add_axis_delta(sample.shoulder_l_delta, lean_offset);
  add_axis_delta(sample.shoulder_r_delta, lean_offset);
  add_axis_delta(sample.neck_delta, scale_axis(lean_offset, 0.9F));
  sample.head_forward_tilt = clamped_forward * 0.4F;
  sample.head_side_tilt = clamped_side * 0.4F;
  return sample;
}

auto resolve_mounted_rider_charge_pose(
    const MountedRiderChargePoseInputs& inputs) noexcept -> MountedRiderPostureSample {
  float const intensity = std::clamp(inputs.intensity, 0.0F, 1.0F);
  MountedAxisDelta const charge_lean{.forward = 0.25F * intensity};
  float const crouch = 0.08F * intensity;

  MountedRiderPostureSample sample{};
  add_axis_delta(sample.shoulder_l_delta, charge_lean);
  add_axis_delta(sample.shoulder_r_delta, charge_lean);
  add_axis_delta(sample.neck_delta, scale_axis(charge_lean, 0.85F));
  sample.shoulder_l_delta.world_y -= crouch;
  sample.shoulder_r_delta.world_y -= crouch;
  sample.neck_delta.world_y -= crouch * 0.8F;
  return sample;
}

auto resolve_mounted_rider_reining_pose(
    const MountedRiderReiningPoseInputs& inputs) noexcept -> MountedRiderPostureSample {
  float const left_tension = std::clamp(inputs.left_tension, 0.0F, 1.0F);
  float const right_tension = std::clamp(inputs.right_tension, 0.0F, 1.0F);
  float const avg_tension = (left_tension + right_tension) * 0.5F;
  MountedAxisDelta const lean_back{.forward = -0.08F * avg_tension};

  MountedRiderPostureSample sample{};
  add_axis_delta(sample.shoulder_l_delta, lean_back);
  add_axis_delta(sample.shoulder_r_delta, lean_back);
  add_axis_delta(sample.neck_delta, scale_axis(lean_back, 0.9F));
  return sample;
}

auto resolve_mounted_rider_torso_sculpt_pose(
    const MountedRiderTorsoSculptPoseInputs& inputs) noexcept
    -> MountedRiderPostureSample {
  float const comp = std::clamp(inputs.compression, 0.0F, 1.0F);
  float const twist_amt = std::clamp(inputs.twist, -1.0F, 1.0F);
  float const dip_amt = std::clamp(inputs.shoulder_dip, -1.0F, 1.0F);

  MountedRiderPostureSample sample{};
  sample.active =
      comp >= 1.0e-3F || std::abs(twist_amt) >= 1.0e-3F || std::abs(dip_amt) >= 1.0e-3F;
  if (!sample.active) {
    return sample;
  }

  MountedAxisDelta const inward{.forward = -(0.035F + comp * 0.08F) * comp};
  MountedAxisDelta const chest_lift{.up = 0.012F * comp};
  MountedAxisDelta const narrow{.right = 0.022F * comp};
  MountedAxisDelta const twist_vec{.right = 0.0003F * twist_amt};
  MountedAxisDelta const dip_vec{.up = 0.03F * dip_amt};

  add_axis_delta(sample.shoulder_l_delta, inward);
  add_axis_delta(sample.shoulder_r_delta, inward);
  add_axis_delta(sample.neck_delta, scale_axis(inward, 0.55F));
  add_axis_delta(sample.head_delta, scale_axis(inward, 0.55F));

  add_axis_delta(sample.neck_delta, scale_axis(chest_lift, 0.8F));
  add_axis_delta(sample.head_delta, scale_axis(chest_lift, 0.8F));

  add_axis_delta(sample.shoulder_l_delta, scale_axis(narrow, -1.0F));
  add_axis_delta(sample.shoulder_r_delta, narrow);

  add_axis_delta(sample.shoulder_l_delta, twist_vec);
  add_axis_delta(sample.shoulder_r_delta, scale_axis(twist_vec, -1.0F));
  add_axis_delta(sample.neck_delta, scale_axis(twist_vec, 0.25F));

  add_axis_delta(sample.shoulder_l_delta, dip_vec);
  add_axis_delta(sample.shoulder_r_delta, scale_axis(dip_vec, -1.0F));
  return sample;
}

} // namespace Animation
