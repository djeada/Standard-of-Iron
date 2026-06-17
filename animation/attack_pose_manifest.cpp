#include "attack_pose_manifest.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Animation {

namespace {

[[nodiscard]] auto add(PoseVec3 a, PoseVec3 b) noexcept -> PoseVec3 {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}

[[nodiscard]] auto add(MountedSeatOffset a,
                       MountedSeatOffset b) noexcept -> MountedSeatOffset {
  return {a.forward + b.forward, a.right + b.right, a.up + b.up};
}

[[nodiscard]] auto subtract(MountedSeatOffset a,
                            MountedSeatOffset b) noexcept -> MountedSeatOffset {
  return {a.forward - b.forward, a.right - b.right, a.up - b.up};
}

[[nodiscard]] auto scale(PoseVec3 v, float s) noexcept -> PoseVec3 {
  return {v.x * s, v.y * s, v.z * s};
}

[[nodiscard]] auto length_squared(PoseVec3 v) noexcept -> float {
  return v.x * v.x + v.y * v.y + v.z * v.z;
}

[[nodiscard]] auto normalize(PoseVec3 v) noexcept -> PoseVec3 {
  float const len_sq = length_squared(v);
  if (len_sq <= 1.0e-6F) {
    return {};
  }
  float const inv_len = 1.0F / std::sqrt(len_sq);
  return scale(v, inv_len);
}

[[nodiscard]] auto scale(MountedSeatOffset v, float s) noexcept -> MountedSeatOffset {
  return {v.forward * s, v.right * s, v.up * s};
}

[[nodiscard]] auto lerp(PoseVec3 a, PoseVec3 b, float t) noexcept -> PoseVec3 {
  return add(scale(a, 1.0F - t), scale(b, t));
}

[[nodiscard]] auto spear_direction_for_thrust(float attack_phase) noexcept -> PoseVec3 {
  PoseVec3 const rest = normalize({0.05F, 0.55F, 0.85F});
  PoseVec3 const attack = normalize({0.03F, -0.15F, 1.0F});
  if (attack_phase < 0.30F || attack_phase >= 0.50F) {
    return rest;
  }
  float const t = std::clamp((attack_phase - 0.30F) / 0.20F, 0.0F, 1.0F);
  return normalize(lerp(rest, attack, t));
}

[[nodiscard]] auto spear_braced_direction() noexcept -> PoseVec3 {
  return normalize({0.05F, 0.40F, 0.91F});
}

[[nodiscard]] auto
lerp(MountedSeatOffset a, MountedSeatOffset b, float t) noexcept -> MountedSeatOffset {
  return add(scale(a, 1.0F - t), scale(b, t));
}

[[nodiscard]] auto smoothstep(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] auto ease_out(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return 1.0F - (1.0F - t) * (1.0F - t);
}

[[nodiscard]] auto ease_in_out_cubic(float t) noexcept -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t < 0.5F ? 4.0F * t * t * t : 1.0F - std::pow(-2.0F * t + 2.0F, 3.0F) / 2.0F;
}

[[nodiscard]] auto clamp_attack_reach_scale(float reach_scale) noexcept -> float {
  return std::clamp(reach_scale, 0.70F, 1.35F);
}

void apply_sword_body_drive(HumanoidWeaponAttackPoseSample& sample,
                            float attack_phase,
                            float torso_twist,
                            float forward_lean,
                            float shoulder_rotation,
                            float weight_shift,
                            float strike_direction,
                            float amplified_weight,
                            float finisher_bonus,
                            bool combat_variant) noexcept {
  float const twist_scale = 1.0F + amplified_weight * (combat_variant ? 1.45F : 1.35F) +
                            finisher_bonus * (combat_variant ? 0.30F : 0.28F);
  float const lean_scale = 1.0F + amplified_weight * (combat_variant ? 1.60F : 1.55F) +
                           finisher_bonus * (combat_variant ? 0.34F : 0.30F);
  float const shoulder_scale = 1.0F +
                               amplified_weight * (combat_variant ? 1.00F : 0.95F) +
                               finisher_bonus * (combat_variant ? 0.20F : 0.18F);
  float const weight_scale = 1.0F +
                             amplified_weight * (combat_variant ? 1.28F : 1.25F) +
                             finisher_bonus * (combat_variant ? 0.25F : 0.24F);
  torso_twist *= twist_scale;
  forward_lean *= lean_scale;
  shoulder_rotation *= shoulder_scale;
  weight_shift *= weight_scale;

  sample.shoulder_r_z_delta += torso_twist;
  sample.shoulder_l_z_delta -= torso_twist * (combat_variant ? 0.62F : 0.60F);
  sample.shoulder_r_y_delta -= shoulder_rotation;
  sample.shoulder_l_y_delta += shoulder_rotation * (combat_variant ? 0.45F : 0.40F);

  if (forward_lean > 0.001F) {
    sample.shoulder_l_z_delta += forward_lean;
    sample.shoulder_r_z_delta += forward_lean;
    sample.neck_z_delta += forward_lean * (combat_variant ? 0.72F : 0.70F);
    sample.head_z_delta += forward_lean * (combat_variant ? 0.56F : 0.50F);
    if (combat_variant) {
      sample.pelvis_z_delta += forward_lean * 0.26F;
    }
  }

  float const swing_phase = combat_variant
                                ? std::clamp((attack_phase - 0.12F) / 0.76F, 0.0F, 1.0F)
                                : std::clamp(attack_phase, 0.0F, 1.0F);
  float const swing_drive = std::sin(swing_phase * std::numbers::pi_v<float>);
  float const pelvis_drop =
      swing_drive * ((combat_variant ? 0.018F : 0.012F) +
                     (combat_variant ? 0.028F : 0.024F) * amplified_weight +
                     (combat_variant ? 0.015F : 0.012F) * finisher_bonus);
  float const pelvis_drive =
      swing_drive * ((combat_variant ? 0.030F : 0.018F) +
                     (combat_variant ? 0.046F : 0.040F) * amplified_weight +
                     (combat_variant ? 0.020F : 0.018F) * finisher_bonus);
  float const head_drive =
      swing_drive * ((combat_variant ? 0.020F : 0.014F) +
                     (combat_variant ? 0.032F : 0.028F) * amplified_weight +
                     (combat_variant ? 0.018F : 0.015F) * finisher_bonus);

  sample.pelvis_y_delta -= pelvis_drop;
  sample.pelvis_z_delta += pelvis_drive;
  sample.neck_y_delta -= pelvis_drop * (combat_variant ? 0.20F : 0.18F);
  sample.neck_z_delta += head_drive * (combat_variant ? 0.42F : 0.40F);
  sample.head_y_delta -= pelvis_drop * (combat_variant ? 0.36F : 0.35F);
  sample.head_z_delta += head_drive;

  if (amplified_weight > 0.001F || finisher_bonus > 0.0F) {
    sample.foot_l_z_delta -= strike_direction * amplified_weight * 0.03F;
    sample.knee_l_z_delta -= strike_direction * amplified_weight * 0.02F;
  }

  sample.foot_r_z_delta += weight_shift;
  sample.knee_r_z_delta += weight_shift * (combat_variant ? 0.62F : 0.60F);
}

[[nodiscard]] auto
resolve_sword_pose(const HumanoidWeaponAttackPoseInputs& inputs,
                   bool combat_variant) noexcept -> HumanoidWeaponAttackPoseSample {
  float const attack_phase = std::clamp(inputs.attack_phase, 0.0F, 1.0F);
  float const reach_scale = clamp_attack_reach_scale(inputs.reach_scale);
  float const shoulder_y = inputs.shoulder_y;
  float const waist_y = inputs.waist_y;
  constexpr float k_strike_right_to_left = 1.0F;
  constexpr float k_strike_left_to_right = -1.0F;

  PoseVec3 rest_pos = combat_variant ? PoseVec3{0.23F, shoulder_y + 0.02F, 0.20F}
                                     : PoseVec3{0.21F, shoulder_y + 0.03F, 0.16F};
  PoseVec3 chamber_pos = combat_variant ? PoseVec3{0.38F, shoulder_y + 0.18F, -0.16F}
                                        : PoseVec3{0.35F, shoulder_y + 0.18F, -0.10F};
  PoseVec3 apex_pos = combat_variant ? PoseVec3{0.44F, shoulder_y + 0.31F, -0.04F}
                                     : PoseVec3{0.39F, shoulder_y + 0.27F, 0.00F};
  PoseVec3 strike_pos = combat_variant ? PoseVec3{0.02F, shoulder_y - 0.32F, 1.02F}
                                       : PoseVec3{0.08F, shoulder_y - 0.30F, 0.90F};
  PoseVec3 followthrough_pos = combat_variant
                                   ? PoseVec3{-0.26F, waist_y - 0.12F, 0.84F}
                                   : PoseVec3{-0.12F, waist_y - 0.08F, 0.74F};
  PoseVec3 recover_pos =
      combat_variant ? PoseVec3{0.18F, shoulder_y - 0.01F, 0.30F} : rest_pos;

  float strike_direction = k_strike_right_to_left;
  switch (inputs.variant % 3U) {
  case 1U:
    chamber_pos = combat_variant ? PoseVec3{-0.22F, shoulder_y + 0.19F, -0.14F}
                                 : PoseVec3{-0.18F, shoulder_y + 0.20F, -0.10F};
    apex_pos = combat_variant ? PoseVec3{-0.18F, shoulder_y + 0.32F, -0.02F}
                              : PoseVec3{-0.16F, shoulder_y + 0.29F, 0.00F};
    strike_pos = combat_variant ? PoseVec3{0.50F, shoulder_y - 0.30F, 0.98F}
                                : PoseVec3{0.44F, shoulder_y - 0.28F, 0.86F};
    followthrough_pos = combat_variant ? PoseVec3{0.60F, waist_y - 0.10F, 0.82F}
                                       : PoseVec3{0.54F, waist_y - 0.06F, 0.72F};
    if (combat_variant) {
      recover_pos = {0.28F, shoulder_y - 0.02F, 0.32F};
    }
    strike_direction = k_strike_left_to_right;
    break;
  case 2U:
    chamber_pos = combat_variant ? PoseVec3{0.30F, shoulder_y + 0.29F, -0.18F}
                                 : PoseVec3{0.42F, shoulder_y + 0.10F, -0.12F};
    apex_pos = combat_variant ? PoseVec3{0.34F, shoulder_y + 0.36F, -0.04F}
                              : PoseVec3{0.46F, shoulder_y + 0.12F, -0.02F};
    strike_pos = combat_variant ? PoseVec3{-0.04F, waist_y - 0.14F, 1.08F}
                                : PoseVec3{-0.02F, shoulder_y - 0.24F, 0.94F};
    followthrough_pos = combat_variant ? PoseVec3{-0.20F, waist_y - 0.20F, 0.88F}
                                       : PoseVec3{-0.22F, shoulder_y - 0.30F, 0.80F};
    if (combat_variant) {
      recover_pos = {0.14F, shoulder_y - 0.04F, 0.34F};
    }
    break;
  default:
    break;
  }

  float const emphasis = std::max(1.0F, inputs.attack_emphasis);
  float const amplified_weight = emphasis - 1.0F;
  float const finisher_bonus = inputs.finisher_attack ? 1.0F : 0.0F;
  if (amplified_weight > 0.001F || finisher_bonus > 0.0F) {
    float const chamber_pull = (combat_variant ? 0.08F : 0.06F) +
                               (combat_variant ? 0.08F : 0.06F) * amplified_weight +
                               (combat_variant ? 0.05F : 0.04F) * finisher_bonus;
    float const apex_lift = (combat_variant ? 0.08F : 0.06F) +
                            (combat_variant ? 0.10F : 0.08F) * amplified_weight +
                            (combat_variant ? 0.06F : 0.05F) * finisher_bonus;
    float const strike_drive = (combat_variant ? 0.14F : 0.10F) +
                               (combat_variant ? 0.16F : 0.12F) * amplified_weight +
                               (combat_variant ? 0.12F : 0.10F) * finisher_bonus;
    float const followthrough_drive =
        (combat_variant ? 0.12F : 0.08F) +
        (combat_variant ? 0.14F : 0.12F) * amplified_weight +
        (combat_variant ? 0.12F : 0.10F) * finisher_bonus;

    rest_pos = add(rest_pos,
                   {0.00F, 0.00F, (combat_variant ? 0.03F : 0.02F) * amplified_weight});
    chamber_pos =
        add(chamber_pos,
            {0.10F * strike_direction * chamber_pull +
                 (combat_variant ? 0.02F * strike_direction * chamber_pull : 0.0F),
             (combat_variant ? 0.08F : 0.06F) * chamber_pull,
             (combat_variant ? -0.64F : -0.55F) * chamber_pull});
    apex_pos = add(apex_pos,
                   {(combat_variant ? 0.08F : 0.06F) * strike_direction * chamber_pull,
                    (combat_variant ? 0.10F : 0.08F) * apex_lift,
                    (combat_variant ? -0.22F : -0.18F) * chamber_pull});
    strike_pos =
        add(strike_pos,
            {(combat_variant ? -0.24F : -0.20F) * strike_direction * strike_drive,
             (combat_variant ? -0.14F : -0.12F) * strike_drive,
             (combat_variant ? 0.62F : 0.55F) * strike_drive});
    followthrough_pos = add(
        followthrough_pos,
        {(combat_variant ? -0.30F : -0.26F) * strike_direction * followthrough_drive,
         (combat_variant ? -0.12F : -0.08F) * followthrough_drive,
         (combat_variant ? 0.48F : 0.44F) * followthrough_drive});
  }

  strike_pos.z *= reach_scale;
  followthrough_pos.z *= reach_scale;
  if (combat_variant) {
    recover_pos.z = std::lerp(rest_pos.z, recover_pos.z, reach_scale);
  }

  HumanoidWeaponAttackPoseSample sample{};
  float torso_twist = 0.0F;
  float forward_lean = 0.0F;
  float shoulder_rotation = 0.0F;
  float weight_shift = 0.0F;

  if (combat_variant) {
    if (attack_phase < 0.22F) {
      float const t = ease_in_out_cubic(attack_phase / 0.22F);
      sample.right_hand = lerp(rest_pos, chamber_pos, t);
      sample.left_hand = {-0.22F - 0.03F * strike_direction * amplified_weight,
                          shoulder_y - 0.02F - 0.01F * amplified_weight,
                          0.18F + 0.04F * t + 0.05F * amplified_weight};
      torso_twist = strike_direction * (-0.10F * t);
      shoulder_rotation = 0.06F * t;
    } else if (attack_phase < 0.40F) {
      float const t = smoothstep((attack_phase - 0.22F) / 0.18F);
      sample.right_hand = lerp(chamber_pos, apex_pos, t);
      sample.left_hand = {-0.22F - 0.05F * strike_direction * amplified_weight,
                          shoulder_y - 0.05F - 0.02F * amplified_weight,
                          0.20F + 0.10F * amplified_weight};
      torso_twist = strike_direction * (-0.11F);
      shoulder_rotation = 0.06F + 0.04F * t;
      forward_lean = 0.03F * t;
      weight_shift = -0.04F * t;
    } else if (attack_phase < 0.63F) {
      float const t = (attack_phase - 0.40F) / 0.23F;
      float const power_t = t * t * t;
      sample.right_hand = lerp(apex_pos, strike_pos, power_t);
      sample.left_hand = {
          -0.22F + (0.16F + 0.07F * amplified_weight) * power_t,
          shoulder_y - 0.05F - (0.10F + 0.05F * amplified_weight) * power_t,
          0.20F +
              (0.36F + 0.16F * amplified_weight + 0.10F * finisher_bonus) * power_t};
      torso_twist = strike_direction * (-0.11F + 0.30F * power_t);
      forward_lean = 0.04F + 0.22F * power_t;
      shoulder_rotation = 0.10F - 0.22F * power_t;
      weight_shift = -0.04F + 0.20F * power_t;
    } else if (attack_phase < 0.82F) {
      float const t = ease_out((attack_phase - 0.63F) / 0.19F);
      sample.right_hand = lerp(strike_pos, followthrough_pos, t);
      sample.left_hand = {-0.10F + 0.05F * strike_direction * amplified_weight,
                          shoulder_y - 0.12F - 0.03F * amplified_weight,
                          0.54F + 0.14F * amplified_weight + 0.10F * finisher_bonus};
      torso_twist = strike_direction * (0.18F - 0.05F * t);
      forward_lean = 0.24F - 0.06F * t;
      weight_shift = 0.18F;
    } else {
      float const t = ease_out((attack_phase - 0.82F) / 0.18F);
      sample.right_hand =
          add(add(scale(followthrough_pos, 1.0F - t), scale(recover_pos, 0.55F * t)),
              scale(rest_pos, 0.45F * t));
      sample.left_hand = {
          -0.10F - (0.10F + 0.03F * amplified_weight) * t,
          shoulder_y - 0.12F * (1.0F - t) - 0.02F * amplified_weight * (1.0F - t),
          (0.54F + 0.14F * amplified_weight + 0.10F * finisher_bonus) * (1.0F - t) +
              0.16F * t};
      torso_twist = 0.12F * strike_direction * (1.0F - t);
      forward_lean = 0.16F * (1.0F - t);
      shoulder_rotation = -0.03F * (1.0F - t);
      weight_shift = 0.12F * (1.0F - t);
    }
  } else {
    if (attack_phase < 0.18F) {
      float const t = attack_phase / 0.18F;
      float const ease_t = t * t;
      sample.right_hand = lerp(rest_pos, chamber_pos, ease_t);
      sample.left_hand = {-0.20F - 0.04F * strike_direction * amplified_weight,
                          shoulder_y - 0.02F - 0.01F * amplified_weight,
                          0.15F + 0.05F * amplified_weight};
      torso_twist = strike_direction * (-0.08F * ease_t);
      shoulder_rotation = 0.05F * ease_t;
    } else if (attack_phase < 0.34F) {
      float const ease_t = smoothstep((attack_phase - 0.18F) / 0.16F);
      sample.right_hand = lerp(chamber_pos, apex_pos, ease_t);
      sample.left_hand = {-0.20F - 0.05F * strike_direction * amplified_weight,
                          shoulder_y - 0.04F - 0.02F * amplified_weight,
                          0.17F + 0.08F * amplified_weight};
      torso_twist = strike_direction * (-0.08F);
      shoulder_rotation = 0.05F + 0.03F * ease_t;
      weight_shift = -0.03F * ease_t;
    } else if (attack_phase < 0.58F) {
      float const t = (attack_phase - 0.34F) / 0.24F;
      float const power_t = t * t * t;
      sample.right_hand = lerp(apex_pos, strike_pos, power_t);
      sample.left_hand = {
          -0.20F + (0.12F + 0.06F * amplified_weight) * power_t,
          shoulder_y - 0.04F - (0.08F + 0.04F * amplified_weight) * power_t,
          0.17F +
              (0.30F + 0.14F * amplified_weight + 0.10F * finisher_bonus) * power_t};
      torso_twist = strike_direction * (-0.08F + 0.22F * power_t);
      forward_lean = 0.20F * power_t;
      shoulder_rotation = 0.08F - 0.16F * power_t;
      weight_shift = -0.03F + 0.16F * power_t;
    } else if (attack_phase < 0.76F) {
      float const t = (attack_phase - 0.58F) / 0.18F;
      float const ease_t = ease_out(t);
      sample.right_hand = lerp(strike_pos, followthrough_pos, ease_t);
      sample.left_hand = {-0.12F + 0.04F * strike_direction * amplified_weight,
                          shoulder_y - 0.10F - 0.03F * amplified_weight,
                          0.47F + 0.12F * amplified_weight + 0.10F * finisher_bonus};
      torso_twist = strike_direction * (0.14F - 0.05F * t);
      forward_lean = 0.18F - 0.04F * t;
      weight_shift = 0.14F;
    } else {
      float const ease_t = ease_out((attack_phase - 0.76F) / 0.24F);
      sample.right_hand = lerp(followthrough_pos, rest_pos, ease_t);
      sample.left_hand = {-0.12F - (0.08F + 0.03F * amplified_weight) * ease_t,
                          shoulder_y - 0.10F * (1.0F - ease_t) -
                              0.02F * amplified_weight * (1.0F - ease_t),
                          (0.47F + 0.12F * amplified_weight + 0.10F * finisher_bonus) *
                                  (1.0F - ease_t) +
                              0.15F * ease_t};
      torso_twist = 0.10F * strike_direction * (1.0F - ease_t);
      forward_lean = 0.12F * (1.0F - ease_t);
      weight_shift = 0.10F * (1.0F - ease_t);
    }
  }

  apply_sword_body_drive(sample,
                         attack_phase,
                         torso_twist,
                         forward_lean,
                         shoulder_rotation,
                         weight_shift,
                         strike_direction,
                         amplified_weight,
                         finisher_bonus,
                         combat_variant);
  return sample;
}

[[nodiscard]] auto
resolve_spear_pose(const HumanoidWeaponAttackPoseInputs& inputs) noexcept
    -> HumanoidWeaponAttackPoseSample {
  float const attack_phase = std::clamp(inputs.attack_phase, 0.0F, 1.0F);
  float const shoulder_y = inputs.shoulder_y;
  float const waist_y = inputs.waist_y;
  constexpr float k_thrust_high = 1.0F;
  constexpr float k_thrust_middle = 0.0F;
  constexpr float k_thrust_low = -1.0F;

  PoseVec3 const guard_pos{0.30F, shoulder_y + 0.08F, 0.30F};
  PoseVec3 chamber_pos{0.34F, shoulder_y + 0.08F, 0.00F};
  PoseVec3 thrust_pos{0.28F, shoulder_y - 0.10F, 1.04F};
  PoseVec3 extended_pos{0.24F, shoulder_y - 0.16F, 1.18F};
  PoseVec3 recover_pos{0.30F, shoulder_y + 0.04F, 0.48F};
  float thrust_height = k_thrust_middle;
  float crouch_amount = 0.0F;

  switch (inputs.variant % 3U) {
  case 1U:
    chamber_pos = {0.30F, shoulder_y + 0.10F, 0.0F};
    thrust_pos = {0.28F, waist_y + 0.03F, 1.06F};
    extended_pos = {0.24F, waist_y - 0.03F, 1.20F};
    recover_pos = {0.28F, shoulder_y - 0.08F, 0.42F};
    thrust_height = k_thrust_low;
    crouch_amount = 0.08F;
    break;
  case 2U:
    chamber_pos = {0.35F, shoulder_y + 0.02F, 0.08F};
    thrust_pos = {0.30F, shoulder_y - 0.02F, 1.00F};
    extended_pos = {0.28F, shoulder_y - 0.06F, 1.12F};
    thrust_height = k_thrust_high;
    break;
  default:
    break;
  }

  HumanoidWeaponAttackPoseSample sample{};
  float forward_lean = 0.0F;
  float torso_twist = 0.0F;
  float shoulder_drop = 0.0F;
  float step_forward = 0.0F;
  float hip_rotation = 0.0F;
  float crouch_factor = 0.0F;

  if (attack_phase < 0.18F) {
    float const t = ease_in_out_cubic(attack_phase / 0.18F);
    sample.right_hand = lerp(guard_pos, chamber_pos, t);
    sample.left_hand = {-0.08F, shoulder_y - 0.04F, 0.22F * (1.0F - t) + 0.06F * t};
    torso_twist = -0.06F * t;
    hip_rotation = -0.04F * t;
    forward_lean = -0.03F * t;
    crouch_factor = crouch_amount * t;
  } else if (attack_phase < 0.28F) {
    sample.right_hand = chamber_pos;
    sample.left_hand = {-0.08F, shoulder_y - 0.04F, 0.06F};
    torso_twist = -0.06F;
    hip_rotation = -0.04F;
    forward_lean = -0.03F;
    crouch_factor = crouch_amount;
  } else if (attack_phase < 0.48F) {
    float const t = (attack_phase - 0.28F) / 0.20F;
    float const power_t = t * t * t;
    sample.right_hand = lerp(chamber_pos, thrust_pos, power_t);
    sample.left_hand = {-0.08F + 0.06F * power_t,
                        shoulder_y - 0.04F + 0.02F * power_t,
                        0.06F + 0.50F * power_t};
    torso_twist = -0.06F + 0.14F * power_t;
    hip_rotation = -0.04F + 0.10F * power_t;
    forward_lean = -0.05F + 0.26F * power_t;
    shoulder_drop = 0.08F * power_t;
    step_forward = 0.18F * power_t;
    crouch_factor = crouch_amount * (1.0F - power_t * 0.3F);
    if (thrust_height < 0.0F) {
      crouch_factor += 0.06F * power_t;
    } else if (thrust_height > 0.0F) {
      crouch_factor += 0.02F * power_t;
    }
  } else if (attack_phase < 0.60F) {
    float const t = smoothstep((attack_phase - 0.48F) / 0.12F);
    sample.right_hand = lerp(thrust_pos, extended_pos, t);
    sample.left_hand = {-0.02F, shoulder_y - 0.02F, 0.56F + 0.10F * t};
    torso_twist = 0.08F;
    hip_rotation = 0.06F;
    forward_lean = 0.21F + 0.06F * t;
    shoulder_drop = 0.08F + 0.03F * t;
    step_forward = 0.18F + 0.05F * t;
    crouch_factor = crouch_amount * 0.7F;
  } else if (attack_phase < 0.78F) {
    float const t = ease_in_out_cubic((attack_phase - 0.60F) / 0.18F);
    sample.right_hand = lerp(extended_pos, recover_pos, t);
    sample.left_hand = {-0.02F * (1.0F - t) - 0.08F * t,
                        shoulder_y - 0.02F * (1.0F - t) - 0.05F * t,
                        0.66F * (1.0F - t) + 0.38F * t};
    torso_twist = 0.08F * (1.0F - t);
    hip_rotation = 0.06F * (1.0F - t);
    forward_lean = 0.25F * (1.0F - t) + 0.04F * t;
    shoulder_drop = 0.10F * (1.0F - t);
    step_forward = 0.22F * (1.0F - t * 0.5F);
    crouch_factor = crouch_amount * 0.7F * (1.0F - t);
  } else {
    float const t = ease_out((attack_phase - 0.78F) / 0.22F);
    sample.right_hand = lerp(recover_pos, guard_pos, t);
    sample.left_hand = {-0.08F,
                        shoulder_y - 0.05F * (1.0F - t) - 0.02F * t,
                        0.38F * (1.0F - t) + 0.22F * t};
    forward_lean = 0.04F * (1.0F - t);
    step_forward = 0.08F * (1.0F - t);
  }

  sample.shoulder_r_z_delta += torso_twist;
  sample.shoulder_l_z_delta -= torso_twist * 0.4F;
  sample.pelvis_z_delta += hip_rotation * 0.5F;
  sample.shoulder_l_z_delta += forward_lean;
  sample.shoulder_r_z_delta += forward_lean;
  sample.neck_z_delta += forward_lean * 0.85F;
  sample.head_z_delta += forward_lean * 0.7F;
  sample.shoulder_r_y_delta -= shoulder_drop;
  sample.shoulder_l_y_delta -= shoulder_drop * 0.3F;
  sample.foot_r_z_delta += step_forward;
  sample.knee_r_z_delta += step_forward * 0.6F;
  sample.foot_l_z_delta -= step_forward * 0.15F;

  if (crouch_factor > 0.001F) {
    sample.pelvis_y_delta -= crouch_factor;
    sample.shoulder_l_y_delta -= crouch_factor * 0.6F;
    sample.shoulder_r_y_delta -= crouch_factor * 0.6F;
    sample.neck_y_delta -= crouch_factor * 0.5F;
    sample.head_y_delta -= crouch_factor * 0.4F;
  }

  float const thrust_drive =
      std::sin(std::clamp((attack_phase - 0.22F) / 0.64F, 0.0F, 1.0F) *
               std::numbers::pi_v<float>);
  float const height_abs = std::abs(thrust_height);
  float const pelvis_drop =
      thrust_drive * (0.010F + 0.014F * height_abs + 0.020F * crouch_amount);
  float const pelvis_drive =
      thrust_drive * (0.020F + 0.028F * height_abs + 0.030F * crouch_amount);
  float const head_drive =
      thrust_drive * (0.014F + 0.020F * height_abs + 0.016F * crouch_amount);
  sample.pelvis_y_delta -= pelvis_drop;
  sample.pelvis_z_delta += pelvis_drive;
  sample.neck_y_delta -= pelvis_drop * 0.18F;
  sample.neck_z_delta += head_drive * 0.42F;
  sample.head_y_delta -= pelvis_drop * 0.28F;
  sample.head_z_delta += head_drive;

  float const thrust_extent = std::clamp((attack_phase - 0.18F) / 0.60F, 0.0F, 1.0F);
  sample.use_offhand_spear_grip = true;
  sample.offhand_spear_direction = resolve_humanoid_spear_direction({
      .is_attacking = true,
      .is_melee = true,
      .attack_phase = attack_phase,
  });
  sample.offhand_along_offset = -0.08F + 0.04F * thrust_extent;
  sample.offhand_y_drop = 0.05F + 0.03F * thrust_extent;
  sample.offhand_lateral_offset = -0.05F;
  return sample;
}

[[nodiscard]] auto
resolve_classic_spear_pose(const HumanoidWeaponAttackPoseInputs& inputs,
                           bool from_hold) noexcept -> HumanoidWeaponAttackPoseSample {
  float const attack_phase = std::clamp(inputs.attack_phase, 0.0F, 1.0F);
  float const hold_depth = std::clamp(inputs.hold_depth, 0.0F, 1.0F);
  float const shoulder_y = inputs.shoulder_y;
  float const height_offset = from_hold ? -hold_depth * 0.35F : 0.0F;

  PoseVec3 const guard_pos =
      from_hold ? PoseVec3{0.22F, shoulder_y + height_offset + 0.05F, 0.32F}
                : PoseVec3{0.30F, shoulder_y + 0.08F, 0.30F};
  PoseVec3 const chamber_pos =
      from_hold ? PoseVec3{0.28F, shoulder_y + height_offset + 0.10F, 0.08F}
                : PoseVec3{0.34F, shoulder_y + 0.12F, 0.04F};
  PoseVec3 const thrust_pos =
      from_hold ? PoseVec3{0.24F, shoulder_y + height_offset - 0.08F, 0.90F}
                : PoseVec3{0.28F, shoulder_y + 0.05F, 0.95F};
  PoseVec3 const extended_pos =
      from_hold ? PoseVec3{0.22F, shoulder_y + height_offset - 0.12F, 1.00F}
                : PoseVec3{0.25F, shoulder_y + 0.02F, 1.05F};
  PoseVec3 const recover_pos =
      from_hold ? PoseVec3{0.24F, shoulder_y + height_offset + 0.02F, 0.48F}
                : PoseVec3{0.30F, shoulder_y + 0.06F, 0.48F};

  HumanoidWeaponAttackPoseSample sample{};
  float forward_lean = 0.0F;
  float torso_twist = 0.0F;
  float shoulder_drop = 0.0F;
  float shoulder_extension = 0.0F;
  float step_forward = 0.0F;
  float hip_rotation = 0.0F;

  if (from_hold) {
    if (attack_phase < 0.15F) {
      float const t = attack_phase / 0.15F;
      float const ease_t = t * t;
      sample.right_hand = lerp(guard_pos, chamber_pos, ease_t);
      sample.left_hand = {-0.06F,
                          shoulder_y + height_offset - 0.03F,
                          0.28F * (1.0F - ease_t) + 0.10F * ease_t};
      torso_twist = -0.04F * ease_t;
    } else if (attack_phase < 0.22F) {
      sample.right_hand = chamber_pos;
      sample.left_hand = {-0.06F, shoulder_y + height_offset - 0.03F, 0.10F};
      torso_twist = -0.04F;
    } else if (attack_phase < 0.42F) {
      float const t = (attack_phase - 0.22F) / 0.20F;
      float const power_t = t * t * t;
      sample.right_hand = lerp(chamber_pos, thrust_pos, power_t);
      sample.left_hand = {-0.06F + 0.05F * power_t,
                          shoulder_y + height_offset - 0.03F + 0.01F * power_t,
                          0.10F + 0.48F * power_t};
      torso_twist = -0.04F + 0.10F * power_t;
      forward_lean = 0.12F * power_t;
      shoulder_extension = 0.06F * power_t;
    } else if (attack_phase < 0.55F) {
      float const t = smoothstep((attack_phase - 0.42F) / 0.13F);
      sample.right_hand = lerp(thrust_pos, extended_pos, t);
      sample.left_hand = {
          -0.01F, shoulder_y + height_offset - 0.02F, 0.58F + 0.08F * t};
      torso_twist = 0.06F;
      forward_lean = 0.12F + 0.04F * t;
      shoulder_extension = 0.06F + 0.03F * t;
    } else if (attack_phase < 0.75F) {
      float const t = smoothstep((attack_phase - 0.55F) / 0.20F);
      sample.right_hand = lerp(extended_pos, recover_pos, t);
      sample.left_hand = {-0.01F * (1.0F - t) - 0.05F * t,
                          shoulder_y + height_offset - 0.02F * (1.0F - t) - 0.04F * t,
                          0.66F * (1.0F - t) + 0.40F * t};
      torso_twist = 0.06F * (1.0F - t);
      forward_lean = 0.16F * (1.0F - t) + 0.03F * t;
      shoulder_extension = 0.09F * (1.0F - t);
    } else {
      float const t = ease_out((attack_phase - 0.75F) / 0.25F);
      sample.right_hand = lerp(recover_pos, guard_pos, t);
      sample.left_hand = {-0.05F - 0.01F * t,
                          shoulder_y + height_offset - 0.04F * (1.0F - t) - 0.03F * t,
                          0.40F * (1.0F - t) + 0.28F * t};
      forward_lean = 0.03F * (1.0F - t);
    }

    sample.shoulder_r_z_delta += torso_twist + shoulder_extension;
    sample.shoulder_l_z_delta -= torso_twist * 0.3F;
    sample.shoulder_r_y_delta -= shoulder_extension * 0.3F;
    sample.shoulder_l_z_delta += forward_lean;
    sample.shoulder_r_z_delta += forward_lean;
    sample.neck_z_delta += forward_lean * 0.9F;
    sample.head_z_delta += forward_lean * 0.75F;

    float const thrust_extent = std::clamp((attack_phase - 0.15F) / 0.55F, 0.0F, 1.0F);
    sample.offhand_along_offset = -0.06F + 0.03F * thrust_extent;
    sample.offhand_y_drop = 0.06F + 0.02F * thrust_extent;
  } else {
    if (attack_phase < 0.18F) {
      float const t = ease_in_out_cubic(attack_phase / 0.18F);
      sample.right_hand = lerp(guard_pos, chamber_pos, t);
      sample.left_hand = {-0.08F, shoulder_y - 0.04F, 0.22F * (1.0F - t) + 0.06F * t};
      torso_twist = -0.06F * t;
      hip_rotation = -0.04F * t;
      forward_lean = -0.03F * t;
    } else if (attack_phase < 0.28F) {
      float const t = (attack_phase - 0.18F) / 0.10F;
      sample.right_hand = chamber_pos;
      sample.left_hand = {-0.08F, shoulder_y - 0.04F, 0.06F};
      torso_twist = -0.06F;
      hip_rotation = -0.04F;
      forward_lean = -0.03F - 0.02F * t;
    } else if (attack_phase < 0.48F) {
      float const t = (attack_phase - 0.28F) / 0.20F;
      float const power_t = t * t * t;
      sample.right_hand = lerp(chamber_pos, thrust_pos, power_t);
      sample.left_hand = {-0.08F + 0.06F * power_t,
                          shoulder_y - 0.04F + 0.02F * power_t,
                          0.06F + 0.50F * power_t};
      torso_twist = -0.06F + 0.14F * power_t;
      hip_rotation = -0.04F + 0.10F * power_t;
      forward_lean = -0.05F + 0.18F * power_t;
      shoulder_drop = 0.05F * power_t;
      step_forward = 0.10F * power_t;
    } else if (attack_phase < 0.60F) {
      float const t = ease_out((attack_phase - 0.48F) / 0.12F);
      sample.right_hand = lerp(thrust_pos, extended_pos, t);
      sample.left_hand = {-0.02F, shoulder_y - 0.02F, 0.56F + 0.10F * t};
      torso_twist = 0.08F;
      hip_rotation = 0.06F;
      forward_lean = 0.13F + 0.05F * t;
      shoulder_drop = 0.05F + 0.02F * t;
      step_forward = 0.10F + 0.04F * t;
    } else if (attack_phase < 0.78F) {
      float const t = ease_out((attack_phase - 0.60F) / 0.18F);
      sample.right_hand = lerp(extended_pos, recover_pos, t);
      sample.left_hand = {-0.02F * (1.0F - t) - 0.08F * t,
                          shoulder_y - 0.02F * (1.0F - t) - 0.05F * t,
                          0.66F * (1.0F - t) + 0.38F * t};
      torso_twist = 0.08F * (1.0F - t);
      hip_rotation = 0.06F * (1.0F - t);
      forward_lean = 0.18F * (1.0F - t) + 0.04F * t;
      shoulder_drop = 0.07F * (1.0F - t);
      step_forward = 0.14F * (1.0F - t * 0.5F);
    } else {
      float const t = ease_out((attack_phase - 0.78F) / 0.22F);
      sample.right_hand = lerp(recover_pos, guard_pos, t);
      sample.left_hand = {-0.08F,
                          shoulder_y - 0.05F * (1.0F - t) - 0.02F * t,
                          0.38F * (1.0F - t) + 0.22F * t};
      forward_lean = 0.04F * (1.0F - t);
      step_forward = 0.07F * (1.0F - t);
    }

    sample.shoulder_r_z_delta += torso_twist;
    sample.shoulder_l_z_delta -= torso_twist * 0.4F;
    sample.pelvis_z_delta += hip_rotation * 0.5F;
    sample.shoulder_l_z_delta += forward_lean;
    sample.shoulder_r_z_delta += forward_lean;
    sample.neck_z_delta += forward_lean * 0.85F;
    sample.head_z_delta += forward_lean * 0.7F;
    sample.shoulder_r_y_delta -= shoulder_drop;
    sample.shoulder_l_y_delta -= shoulder_drop * 0.3F;
    sample.foot_r_z_delta += step_forward;
    sample.knee_r_z_delta += step_forward * 0.6F;
    sample.foot_l_z_delta -= step_forward * 0.15F;

    float const thrust_extent = std::clamp((attack_phase - 0.18F) / 0.60F, 0.0F, 1.0F);
    sample.offhand_along_offset = -0.08F + 0.04F * thrust_extent;
    sample.offhand_y_drop = 0.05F + 0.03F * thrust_extent;
  }

  sample.use_offhand_spear_grip = true;
  sample.offhand_spear_direction = resolve_humanoid_spear_direction({
      .is_attacking = true,
      .is_melee = true,
      .attack_phase = attack_phase,
  });
  sample.offhand_lateral_offset = -0.05F;
  return sample;
}

[[nodiscard]] auto
resolve_basic_melee_pose(const HumanoidWeaponAttackPoseInputs& inputs) noexcept
    -> HumanoidWeaponAttackPoseSample {
  float const strike_phase = std::clamp(inputs.attack_phase, 0.0F, 1.0F);
  float const shoulder_y = inputs.shoulder_y;

  PoseVec3 const rest_pos{0.22F, shoulder_y + 0.02F, 0.18F};
  PoseVec3 const chamber_pos{0.30F, shoulder_y + 0.08F, 0.05F};
  PoseVec3 const strike_pos{0.28F, shoulder_y - 0.05F, 0.65F};
  PoseVec3 const followthrough_pos{0.10F, shoulder_y - 0.12F, 0.55F};

  HumanoidWeaponAttackPoseSample sample{};
  float torso_twist = 0.0F;
  float forward_lean = 0.0F;
  float shoulder_dip = 0.0F;
  float step_forward = 0.0F;

  if (strike_phase < 0.20F) {
    float const t = strike_phase / 0.20F;
    float const ease_t = t * t;
    sample.right_hand = lerp(rest_pos, chamber_pos, ease_t);
    sample.left_hand = {-0.18F, shoulder_y + 0.02F, 0.22F - 0.08F * t};
    torso_twist = -0.04F * ease_t;
    shoulder_dip = -0.02F * ease_t;
  } else if (strike_phase < 0.28F) {
    sample.right_hand = chamber_pos;
    sample.left_hand = {-0.18F, shoulder_y + 0.02F, 0.14F};
    torso_twist = -0.04F;
    shoulder_dip = -0.02F;
  } else if (strike_phase < 0.48F) {
    float const t = (strike_phase - 0.28F) / 0.20F;
    float const power_t = smoothstep(t);
    sample.right_hand = lerp(chamber_pos, strike_pos, power_t);
    sample.left_hand = {-0.18F + 0.06F * power_t,
                        shoulder_y + 0.02F - 0.08F * power_t,
                        0.14F + 0.20F * power_t};
    torso_twist = -0.04F + 0.10F * power_t;
    forward_lean = 0.08F * power_t;
    shoulder_dip = -0.02F + 0.05F * power_t;
    step_forward = 0.06F * power_t;
  } else if (strike_phase < 0.65F) {
    float const t = (strike_phase - 0.48F) / 0.17F;
    float const ease_t = t * t;
    sample.right_hand = lerp(strike_pos, followthrough_pos, ease_t);
    sample.left_hand = {-0.12F, shoulder_y - 0.06F, 0.34F};
    torso_twist = 0.06F - 0.02F * t;
    forward_lean = 0.08F - 0.03F * t;
    shoulder_dip = 0.03F;
    step_forward = 0.06F;
  } else {
    float const t = (strike_phase - 0.65F) / 0.35F;
    float const ease_t = ease_out(t);
    sample.right_hand = lerp(followthrough_pos, rest_pos, ease_t);
    sample.left_hand = {-0.12F + (-0.18F + 0.12F) * ease_t,
                        shoulder_y - 0.06F * (1.0F - ease_t) + 0.02F * ease_t,
                        0.34F * (1.0F - ease_t) + 0.22F * ease_t};
    torso_twist = 0.04F * (1.0F - ease_t);
    forward_lean = 0.05F * (1.0F - ease_t);
    shoulder_dip = 0.03F * (1.0F - ease_t);
    step_forward = 0.06F * (1.0F - ease_t);
  }

  if (std::abs(torso_twist) > 0.001F) {
    sample.shoulder_r_z_delta += torso_twist;
    sample.shoulder_l_z_delta -= torso_twist * 0.5F;
  }

  if (forward_lean > 0.001F) {
    sample.shoulder_l_z_delta += forward_lean;
    sample.shoulder_r_z_delta += forward_lean;
    sample.neck_z_delta += forward_lean * 0.8F;
    sample.head_z_delta += forward_lean * 0.6F;
  }

  if (std::abs(shoulder_dip) > 0.001F) {
    sample.shoulder_r_y_delta += shoulder_dip;
  }

  if (step_forward > 0.001F) {
    sample.foot_r_z_delta += step_forward;
    sample.knee_r_z_delta += step_forward * 0.5F;
  }

  return sample;
}

} // namespace

auto resolve_humanoid_weapon_attack_pose(
    const HumanoidWeaponAttackPoseInputs& inputs) noexcept
    -> HumanoidWeaponAttackPoseSample {
  switch (inputs.kind) {
  case HumanoidWeaponAttackKind::CombatSwordSlash:
    return resolve_sword_pose(inputs, true);
  case HumanoidWeaponAttackKind::SpearThrustClassic:
    return resolve_classic_spear_pose(inputs, false);
  case HumanoidWeaponAttackKind::SpearThrustFromHold:
    return resolve_classic_spear_pose(inputs, true);
  case HumanoidWeaponAttackKind::SpearThrust:
    return resolve_spear_pose(inputs);
  case HumanoidWeaponAttackKind::BasicMeleeStrike:
    return resolve_basic_melee_pose(inputs);
  case HumanoidWeaponAttackKind::SwordSlash:
    break;
  }
  return resolve_sword_pose(inputs, false);
}

auto resolve_humanoid_spear_direction(
    const HumanoidSpearDirectionInputs& inputs) noexcept -> PoseVec3 {
  PoseVec3 const rest = normalize({0.05F, 0.55F, 0.85F});
  float const hold_blend = std::clamp(inputs.hold_blend, 0.0F, 1.0F);
  if (hold_blend > 0.0F) {
    return normalize(lerp(rest, spear_braced_direction(), hold_blend));
  }
  if (inputs.is_attacking && inputs.is_melee) {
    return spear_direction_for_thrust(inputs.attack_phase);
  }
  return rest;
}

auto resolve_humanoid_bow_draw_pose(const HumanoidBowDrawPoseInputs& inputs) noexcept
    -> HumanoidBowDrawPoseSample {
  float const draw_phase = std::clamp(inputs.draw_phase, 0.0F, 1.0F);
  float const shoulder_y = inputs.shoulder_y;

  PoseVec3 const aim_pos{-0.02F, shoulder_y + 0.18F, 0.42F};
  PoseVec3 const draw_pos{0.03F, shoulder_y + 0.04F, 0.22F};
  PoseVec3 const release_pos{-0.02F, shoulder_y + 0.20F, 0.34F};
  PoseVec3 const full_draw_pos = add(draw_pos, {0.0F, -0.015F, -0.090F});

  HumanoidBowDrawPoseSample sample{};
  sample.shoulder_r_z_delta = 0.10F;

  float shoulder_twist = 0.0F;
  float head_recoil = 0.0F;
  float forward_lean = 0.0F;
  float exhale_dip = 0.0F;
  float draw_sway_x = 0.0F;
  float draw_sway_y = 0.0F;
  float draw_tension = 0.0F;
  float bow_arm_push = 0.0F;
  float pelvis_setback = 0.0F;

  float const two_pi = 2.0F * std::numbers::pi_v<float>;
  float const jitter_phase = inputs.jitter_seed * two_pi;

  if (draw_phase < 0.20F) {
    float const t = smoothstep(draw_phase / 0.20F);
    sample.left_hand = lerp(aim_pos, draw_pos, t);
    shoulder_twist = t * 0.08F;
    forward_lean = t * 0.025F;
    draw_tension = t * 0.025F;
    bow_arm_push = t * 0.015F;
  } else if (draw_phase < 0.50F) {
    float const tension_t = smoothstep((draw_phase - 0.20F) / 0.30F);
    sample.left_hand = lerp(draw_pos, full_draw_pos, tension_t);
    shoulder_twist = 0.08F + 0.02F * tension_t;
    forward_lean = 0.025F + 0.018F * tension_t;
    draw_tension = 0.025F + 0.055F * tension_t;
    bow_arm_push = 0.015F + 0.040F * tension_t;
    pelvis_setback = -0.006F - 0.010F * tension_t;

    float const sway_freq_a = 5.0F;
    float const sway_freq_b = 7.3F;
    float const sway_amp = 0.004F + tension_t * 0.004F;
    draw_sway_x = std::sin(draw_phase * sway_freq_a * two_pi + jitter_phase) * sway_amp;
    draw_sway_y = std::cos(draw_phase * sway_freq_b * two_pi + jitter_phase * 1.7F) *
                  sway_amp * 0.7F;
  } else if (draw_phase < 0.58F) {
    float const t = ease_out((draw_phase - 0.50F) / 0.08F);
    sample.left_hand = lerp(full_draw_pos, release_pos, t);
    shoulder_twist = 0.10F * (1.0F - t * 0.55F);
    head_recoil = t * 0.045F;
    forward_lean = 0.043F * (1.0F - t);
    exhale_dip = t * 0.014F;
    draw_tension = 0.080F * (1.0F - t);
    bow_arm_push = 0.055F * (1.0F - t * 0.6F);
    pelvis_setback = -0.016F * (1.0F - t);
  } else {
    float const t = smoothstep((draw_phase - 0.58F) / 0.42F);
    sample.left_hand = lerp(release_pos, aim_pos, t);
    shoulder_twist = 0.045F * (1.0F - t);
    head_recoil = 0.045F * (1.0F - t);
    exhale_dip = 0.014F * (1.0F - t);
    bow_arm_push = 0.022F * (1.0F - t);
  }

  sample.left_hand.x += draw_sway_x;
  sample.left_hand.y += draw_sway_y;
  sample.right_hand = {0.03F + draw_sway_x * 0.3F,
                       shoulder_y + 0.08F + draw_sway_y * 0.3F,
                       0.62F + bow_arm_push};

  if (shoulder_twist > 0.01F) {
    sample.shoulder_l_y_delta += shoulder_twist;
    sample.shoulder_r_y_delta -= shoulder_twist * 0.5F;
  }

  if (forward_lean > 0.001F) {
    sample.shoulder_l_z_delta += forward_lean;
    sample.shoulder_r_z_delta += forward_lean;
    sample.neck_z_delta += forward_lean * 0.7F;
    sample.head_z_delta += forward_lean * 0.5F;
  }

  if (draw_tension > 0.001F) {
    sample.shoulder_l_z_delta += draw_tension * 0.55F;
    sample.shoulder_r_z_delta += draw_tension;
    sample.shoulder_r_y_delta -= draw_tension * 0.30F;
    sample.neck_z_delta += draw_tension * 0.55F;
    sample.head_z_delta += draw_tension * 0.42F;
  }

  if (std::abs(pelvis_setback) > 0.001F) {
    sample.pelvis_z_delta += pelvis_setback;
  }

  if (exhale_dip > 0.001F) {
    sample.shoulder_l_y_delta -= exhale_dip;
    sample.shoulder_r_y_delta -= exhale_dip;
    sample.neck_y_delta -= exhale_dip * 0.7F;
  }

  if (head_recoil > 0.01F) {
    sample.head_z_delta -= head_recoil;
    sample.neck_z_delta -= head_recoil * 0.45F;
  }

  return sample;
}

auto resolve_humanoid_construction_pose(
    const HumanoidConstructionPoseInputs& inputs) noexcept
    -> HumanoidConstructionPoseSample {
  float const work_phase = std::clamp(inputs.work_phase, 0.0F, 1.0F);
  float const shoulder_y = inputs.shoulder_y;

  HumanoidConstructionPoseSample sample{};

  if (inputs.kind == HumanoidConstructionPoseKind::Saw) {
    float const two_pi = 2.0F * std::numbers::pi_v<float>;
    float const cycle = work_phase * two_pi;
    float const jitter_phase = inputs.jitter_seed * two_pi;
    float const stroke = std::sin(cycle);
    float const abs_stroke = std::abs(stroke);
    float const body_sway = std::cos(cycle + jitter_phase * 0.7F);

    sample.use_two_handed_grip = true;
    sample.grip_center = {0.04F + body_sway * 0.012F,
                          shoulder_y - 0.12F + abs_stroke * 0.010F,
                          0.46F + stroke * 0.18F};
    sample.hand_separation = 0.22F + abs_stroke * 0.03F;

    float const lean = 0.065F + abs_stroke * 0.030F;
    sample.shoulder_l_z_delta += lean;
    sample.shoulder_r_z_delta += lean;
    sample.neck_z_delta += lean * 0.75F;
    sample.head_z_delta += lean * 0.55F;

    float const shoulder_drop = 0.030F + abs_stroke * 0.010F;
    sample.shoulder_l_y_delta -= shoulder_drop;
    sample.shoulder_r_y_delta -= shoulder_drop;
    sample.head_y_delta -= shoulder_drop * 0.30F;

    float const lateral_shift = stroke * 0.018F;
    sample.pelvis_x_delta += lateral_shift;
    sample.shoulder_l_x_delta += lateral_shift * 0.50F;
    sample.shoulder_r_x_delta += lateral_shift * 0.50F;

    sample.foot_l_z_delta -= 0.030F;
    sample.foot_r_z_delta += 0.045F;
    sample.knee_l_z_delta -= 0.015F;
    sample.knee_r_z_delta += 0.020F;
    return sample;
  }

  bool const kneeling = inputs.kind == HumanoidConstructionPoseKind::KneelingChisel;
  PoseVec3 const brace_pos{
      -0.05F, shoulder_y + (kneeling ? -0.19F : -0.14F), kneeling ? 0.54F : 0.48F};
  PoseVec3 const raised_pos{
      0.15F, shoulder_y + (kneeling ? -0.05F : 0.03F), kneeling ? 0.30F : 0.24F};
  PoseVec3 const strike_pos{
      0.09F, shoulder_y + (kneeling ? -0.16F : -0.10F), kneeling ? 0.58F : 0.52F};
  PoseVec3 const recover_pos{
      0.12F, shoulder_y + (kneeling ? -0.10F : -0.04F), kneeling ? 0.42F : 0.38F};

  sample.left_hand = brace_pos;
  float torso_lean = kneeling ? 0.08F : 0.05F;
  float shoulder_drop = 0.0F;

  if (work_phase < 0.24F) {
    float const t = work_phase / 0.24F;
    float const ease_t = t * t;
    sample.right_hand = lerp(recover_pos, raised_pos, ease_t);
    shoulder_drop = -0.01F * ease_t;
  } else if (work_phase < 0.36F) {
    sample.right_hand = raised_pos;
    shoulder_drop = -0.01F;
  } else if (work_phase < 0.58F) {
    float const t = (work_phase - 0.36F) / 0.22F;
    float const ease_t = smoothstep(t);
    sample.right_hand = lerp(raised_pos, strike_pos, ease_t);
    shoulder_drop = 0.04F * ease_t;
    torso_lean += 0.04F * ease_t;
  } else if (work_phase < 0.70F) {
    sample.right_hand = strike_pos;
    shoulder_drop = 0.04F;
    torso_lean += 0.04F;
  } else {
    float const t = (work_phase - 0.70F) / 0.30F;
    float const ease_t = ease_out(t);
    sample.right_hand = lerp(strike_pos, recover_pos, ease_t);
    shoulder_drop = 0.04F * (1.0F - ease_t);
    torso_lean += 0.04F * (1.0F - ease_t);
  }

  sample.shoulder_l_z_delta += torso_lean * 0.75F;
  sample.shoulder_r_z_delta += torso_lean;
  sample.neck_z_delta += torso_lean * 0.70F;
  sample.head_z_delta += torso_lean * 0.55F;
  sample.shoulder_r_y_delta += shoulder_drop;

  if (kneeling) {
    sample.shoulder_l_y_delta -= 0.02F;
    sample.shoulder_r_y_delta -= 0.02F;
  } else {
    sample.foot_r_z_delta += 0.03F;
    sample.knee_r_z_delta += 0.02F;
  }

  return sample;
}

auto resolve_mounted_sword_strike_pose(
    const MountedSwordStrikePoseInputs& inputs) noexcept
    -> MountedSwordStrikePoseSample {
  float const attack_phase = std::clamp(inputs.attack_phase, 0.0F, 1.0F);

  constexpr MountedSeatOffset rest_pos{0.08F, 0.24F, 0.12F};
  constexpr MountedSeatOffset chamber_pos{-0.09F, 0.34F, 0.44F};
  constexpr MountedSeatOffset apex_pos{-0.04F, 0.44F, 0.52F};
  constexpr MountedSeatOffset strike_pos{0.78F, 0.58F, -0.22F};
  constexpr MountedSeatOffset followthrough_pos{1.00F, 0.72F, -0.32F};

  MountedSwordStrikePoseSample sample{};

  if (attack_phase < 0.22F) {
    float const t = attack_phase / 0.22F;
    float const ease_t = t * t;
    sample.right_hand = lerp(rest_pos, chamber_pos, ease_t);
    sample.torso_twist = -0.05F * ease_t;
    sample.torso_commit = -0.02F * ease_t;
    sample.shoulder_dip = 0.04F * ease_t;
    sample.debug_label = "sword_chamber";
  } else if (attack_phase < 0.42F) {
    float const t = (attack_phase - 0.22F) / 0.20F;
    float const ease_t = smoothstep(t);
    sample.right_hand = lerp(chamber_pos, apex_pos, ease_t);
    sample.torso_twist = -0.05F;
    sample.forward_lean = 0.02F * ease_t;
    sample.torso_commit = 0.04F * ease_t;
    sample.counter_lift = 0.02F * ease_t;
    sample.shoulder_dip = 0.04F + 0.03F * ease_t;
    sample.debug_label = "sword_apex";
  } else if (attack_phase < 0.68F) {
    float const t = (attack_phase - 0.42F) / 0.26F;
    float const ease_t = smoothstep(t);
    sample.right_hand = lerp(apex_pos, strike_pos, ease_t);
    sample.torso_twist = -0.03F + 0.15F * ease_t;
    sample.side_lean = 0.03F + 0.08F * ease_t;
    sample.forward_lean = 0.03F + 0.07F * ease_t;
    sample.torso_commit = 0.05F + 0.11F * ease_t;
    sample.counter_lift = 0.03F + 0.05F * ease_t;
    sample.shoulder_dip = 0.06F - 0.11F * ease_t;
    sample.left_hand_up_offset = -0.02F - 0.05F * ease_t;
    sample.head_forward_tilt = 0.40F * ease_t;
    sample.head_side_tilt = 0.16F * ease_t;
    sample.debug_label = "sword_strike";
  } else if (attack_phase < 0.84F) {
    float const t = (attack_phase - 0.68F) / 0.16F;
    float const ease_t = smoothstep(t);
    sample.right_hand = lerp(strike_pos, followthrough_pos, ease_t);
    sample.torso_twist = 0.12F - 0.03F * ease_t;
    sample.side_lean = 0.10F - 0.03F * ease_t;
    sample.forward_lean = 0.10F - 0.03F * ease_t;
    sample.torso_commit = 0.16F - 0.04F * ease_t;
    sample.counter_lift = 0.08F - 0.02F * ease_t;
    sample.shoulder_dip = -0.05F;
    sample.head_forward_tilt = 0.22F;
    sample.head_side_tilt = 0.12F;
    sample.debug_label = "sword_followthrough";
  } else {
    float const t = (attack_phase - 0.84F) / 0.16F;
    float const ease_t = ease_out(t);
    sample.right_hand = lerp(followthrough_pos, rest_pos, ease_t);
    sample.torso_twist = 0.09F * (1.0F - ease_t);
    sample.side_lean = 0.05F * (1.0F - ease_t);
    sample.forward_lean = 0.07F * (1.0F - ease_t);
    sample.torso_commit = 0.12F * (1.0F - ease_t);
    sample.counter_lift = 0.06F * (1.0F - ease_t);
    sample.shoulder_dip = -0.05F * (1.0F - ease_t);
    sample.debug_label = "sword_recover";
  }

  return sample;
}

auto resolve_mounted_spear_thrust_pose(
    const MountedSpearThrustPoseInputs& inputs) noexcept
    -> MountedSpearThrustPoseSample {
  float const attack_phase = std::clamp(inputs.attack_phase, 0.0F, 1.0F);

  constexpr MountedSeatOffset guard_pos{0.12F, 0.15F, 0.15F};
  constexpr MountedSeatOffset couch_pos{0.05F, 0.12F, 0.08F};
  constexpr MountedSeatOffset thrust_pos{0.95F, 0.08F, 0.18F};
  constexpr MountedSeatOffset extended_pos{1.05F, 0.05F, 0.15F};

  MountedSpearThrustPoseSample sample{};

  if (attack_phase < 0.20F) {
    float const t = attack_phase / 0.20F;
    float const ease_t = t * t;
    sample.right_hand = lerp(guard_pos, couch_pos, ease_t);
    sample.left_hand = add({guard_pos.forward, guard_pos.right - 0.25F, guard_pos.up},
                           scale(subtract(couch_pos, guard_pos), ease_t * 0.6F));
    sample.torso_compression = 0.03F * ease_t;
    sample.forward_lean = 0.04F * ease_t;
    sample.head_forward_tilt = 0.1F * ease_t;
    sample.debug_label = "spear_couch";
  } else if (attack_phase < 0.30F) {
    sample.right_hand = couch_pos;
    sample.left_hand = {couch_pos.forward, couch_pos.right - 0.22F, couch_pos.up};
    sample.torso_compression = 0.03F;
    sample.forward_lean = 0.04F;
    sample.head_forward_tilt = 0.1F;
    sample.debug_label = "spear_tension";
  } else if (attack_phase < 0.50F) {
    float const t = (attack_phase - 0.30F) / 0.20F;
    float const power_t = t * t * t;
    sample.right_hand = lerp(couch_pos, thrust_pos, power_t);
    sample.left_hand = lerp(
        MountedSeatOffset{couch_pos.forward, couch_pos.right - 0.22F, couch_pos.up},
        MountedSeatOffset{thrust_pos.forward, thrust_pos.right - 0.28F, thrust_pos.up},
        power_t);
    sample.forward_lean = 0.04F + 0.16F * power_t;
    sample.torso_twist = 0.05F * power_t;
    sample.shoulder_drop = 0.04F * power_t;
    sample.torso_compression = 0.03F * (1.0F - power_t * 0.5F);
    sample.head_forward_tilt = 0.5F * power_t;
    sample.debug_label = "spear_thrust";
  } else if (attack_phase < 0.65F) {
    float const t = (attack_phase - 0.50F) / 0.15F;
    float const ease_t = smoothstep(t);
    sample.right_hand = lerp(thrust_pos, extended_pos, ease_t);
    sample.left_hand = lerp(
        MountedSeatOffset{thrust_pos.forward, thrust_pos.right - 0.28F, thrust_pos.up},
        MountedSeatOffset{
            extended_pos.forward, extended_pos.right - 0.32F, extended_pos.up},
        ease_t);
    sample.forward_lean = 0.20F;
    sample.torso_twist = 0.05F;
    sample.shoulder_drop = 0.04F;
    sample.head_forward_tilt = 0.5F;
    sample.debug_label = "spear_extend";
  } else {
    float const t = (attack_phase - 0.65F) / 0.35F;
    float const ease_t = ease_out(t);
    sample.right_hand = lerp(extended_pos, guard_pos, ease_t);
    sample.left_hand = lerp(
        MountedSeatOffset{
            extended_pos.forward, extended_pos.right - 0.32F, extended_pos.up},
        MountedSeatOffset{guard_pos.forward, guard_pos.right - 0.25F, guard_pos.up},
        ease_t);
    sample.forward_lean = 0.20F * (1.0F - ease_t);
    sample.torso_twist = 0.05F * (1.0F - ease_t);
    sample.shoulder_drop = 0.04F * (1.0F - ease_t);
    sample.debug_label = "spear_recover";
  }

  return sample;
}

auto resolve_mounted_spear_guard_pose(
    const MountedSpearGuardPoseInputs& inputs) noexcept -> MountedSpearGuardPoseSample {
  MountedSpearGuardPoseSample sample{};

  switch (inputs.grip) {
  case MountedSpearGuardGrip::Couched:
    sample.right_hand = {-0.15F, 0.08F, 0.08F};
    sample.left_hand_uses_rein = true;
    sample.left_rein_slack = 0.35F;
    sample.left_rein_tension = 0.20F;
    break;
  case MountedSpearGuardGrip::TwoHanded:
    sample.right_hand = {0.15F, 0.15F, 0.12F};
    sample.left_hand = {0.15F, -0.10F, 0.12F};
    break;
  case MountedSpearGuardGrip::Overhand:
    sample.right_hand = {0.0F, 0.12F, 0.55F};
    sample.left_hand_uses_rein = true;
    sample.left_rein_slack = 0.30F;
    sample.left_rein_tension = 0.30F;
    break;
  }

  return sample;
}

auto resolve_mounted_bow_draw_pose(const MountedBowDrawPoseInputs& inputs) noexcept
    -> MountedBowDrawPoseSample {
  float const draw_phase = std::clamp(inputs.draw_phase, 0.0F, 1.0F);

  constexpr MountedSeatOffset bow_hold_pos{0.25F, -0.08F, 0.25F};
  constexpr MountedSeatOffset draw_start_pos{0.25F, 0.0F, 0.25F};
  constexpr MountedSeatOffset draw_end_pos{0.0F, 0.12F, 0.18F};

  MountedBowDrawPoseSample sample{};
  sample.left_hand = bow_hold_pos;

  if (draw_phase < 0.30F) {
    float const t = draw_phase / 0.30F;
    float const ease_t = t * t;
    sample.right_hand = lerp(draw_start_pos, draw_end_pos, ease_t);
    sample.right_hand_world_y_offset = -0.05F * (1.0F - ease_t);
  } else if (draw_phase < 0.65F) {
    sample.right_hand = draw_end_pos;
  } else {
    float const t = (draw_phase - 0.65F) / 0.35F;
    float const ease_t = t * t * t;
    sample.right_hand = lerp(draw_end_pos, draw_start_pos, ease_t);
    sample.right_hand_world_y_offset = -0.05F * ease_t;
  }

  return sample;
}

auto resolve_mounted_shield_defense_pose(
    const MountedShieldDefensePoseInputs& inputs) noexcept
    -> MountedShieldDefensePoseSample {
  if (inputs.raised) {
    return {
        .left_hand = {0.15F, -0.18F, 0.40F},
        .right_rein_slack = 0.15F,
        .right_rein_tension = 0.45F,
        .debug_label = "shield_defense",
    };
  }

  return {
      .left_hand = {0.05F, -0.16F, 0.22F},
      .right_rein_slack = 0.30F,
      .right_rein_tension = 0.25F,
      .debug_label = "shield_defense",
  };
}

} // namespace Animation
