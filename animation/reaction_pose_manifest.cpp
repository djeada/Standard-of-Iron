#include "reaction_pose_manifest.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace Animation {

namespace {

[[nodiscard]] auto clamp01(float t) noexcept -> float {
  return std::clamp(t, 0.0F, 1.0F);
}

[[nodiscard]] auto smooth01(float t) noexcept -> float {
  t = clamp01(t);
  return t * t * (3.0F - 2.0F * t);
}

[[nodiscard]] auto ease_out(float t) noexcept -> float {
  t = clamp01(t);
  return 1.0F - (1.0F - t) * (1.0F - t);
}

[[nodiscard]] auto segment(float value, float start, float end) noexcept -> float {
  if (end <= start) {
    return value >= end ? 1.0F : 0.0F;
  }
  return clamp01((value - start) / (end - start));
}

[[nodiscard]] auto scaled(PoseVec3 v, float s) noexcept -> PoseVec3 {
  return {v.x * s, v.y * s, v.z * s};
}

constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;

} // namespace

auto reaction_out_and_back(float phase, float out_fraction) noexcept -> float {
  float const q = clamp01(phase);
  float const out = std::clamp(out_fraction, 0.05F, 0.95F);
  if (q < out) {
    return ease_out(q / out);
  }
  return 1.0F - smooth01((q - out) / (1.0F - out));
}

auto resolve_humanoid_ready_stance(const HumanoidReadyStanceInputs& inputs) noexcept
    -> HumanoidReadyStanceSample {
  float const t = inputs.phase - std::floor(inputs.phase);
  float const breathe = std::sin(k_two_pi * t);
  float const breathe_late = std::sin(k_two_pi * t + 0.9F);
  float const sway = std::sin(k_two_pi * t * 2.0F + 0.4F);

  HumanoidReadyStanceSample sample{};
  sample.crouch = 0.050F + 0.012F * breathe;
  sample.torso_forward = 0.040F + 0.008F * breathe_late;
  sample.torso_side = 0.010F * sway;
  sample.sway_x = 0.014F * std::sin(k_two_pi * t + 2.1F);
  switch (inputs.weapon) {
  case HumanoidReadyWeapon::SwordAndShield:
    sample.weapon_phase = 0.030F + 0.022F * (0.5F + 0.5F * breathe_late);
    sample.shield_raise = 0.55F + 0.08F * breathe;
    break;
  case HumanoidReadyWeapon::Spear:
    sample.weapon_phase = 0.020F + 0.025F * (0.5F + 0.5F * breathe_late);
    break;
  case HumanoidReadyWeapon::None:
    sample.weapon_phase = 0.0F;
    break;
  }
  return sample;
}

auto resolve_humanoid_reaction_pose(const HumanoidReactionPoseInputs& inputs) noexcept
    -> HumanoidReactionPoseSample {
  float const q = clamp01(inputs.phase);
  bool const shield = inputs.weapon == HumanoidReadyWeapon::SwordAndShield;
  bool const spear = inputs.weapon == HumanoidReadyWeapon::Spear;

  HumanoidReactionPoseSample sample{};
  sample.weapon_phase = shield ? 0.035F : (spear ? 0.025F : 0.0F);
  switch (inputs.kind) {
  case HumanoidReactionKind::Flinch: {
    float const env = reaction_out_and_back(q, 0.30F);
    sample.envelope = env;
    sample.flinch = 1.0F * env;
    sample.crouch = 0.05F * env;
    sample.torso_forward = -0.045F * env;
    sample.head_back = 0.03F * env;
    sample.hand_r_delta = scaled({0.04F, -0.06F, -0.10F}, env);
    sample.hand_l_delta = scaled({0.02F, 0.04F, -0.05F}, env);
    break;
  }
  case HumanoidReactionKind::Block: {
    float const env = reaction_out_and_back(q, 0.22F);
    sample.envelope = env;
    sample.crouch = 0.07F * env;
    sample.torso_forward = 0.045F * env;
    sample.torso_side = -0.03F * env;
    if (shield) {
      sample.shield_raise = env;
      sample.hand_r_delta = scaled({0.05F, 0.03F, -0.12F}, env);
    } else if (spear) {
      sample.hand_l_delta = scaled({0.00F, 0.16F, 0.06F}, env);
      sample.hand_r_delta = scaled({0.00F, 0.12F, 0.02F}, env);
    } else {
      sample.hand_l_delta = scaled({0.05F, 0.18F, 0.12F}, env);
      sample.hand_r_delta = scaled({-0.05F, 0.16F, 0.10F}, env);
    }
    break;
  }
  case HumanoidReactionKind::Evade: {
    float const env = reaction_out_and_back(q, 0.36F);
    sample.envelope = env;
    sample.crouch = 0.40F * env;
    sample.torso_forward = 0.34F * env;
    sample.head_back = 0.02F * env;
    sample.hand_l_delta = scaled({0.00F, -0.16F, 0.20F}, env);
    sample.hand_r_delta = scaled({0.04F, -0.20F, 0.18F}, env);
    break;
  }
  case HumanoidReactionKind::Stagger: {
    float const env = reaction_out_and_back(q, 0.42F);
    float const overshoot =
        std::sin(std::numbers::pi_v<float> * segment(q, 0.62F, 1.0F));
    sample.envelope = env;
    sample.flinch = 1.0F * env;
    sample.crouch = 0.16F * env + 0.03F * overshoot;
    sample.torso_forward = -0.07F * env + 0.02F * overshoot;
    sample.torso_side = 0.05F * env;
    sample.head_back = 0.06F * env;
    sample.hand_r_delta = scaled({0.18F, 0.12F, -0.12F}, env);
    sample.hand_l_delta = scaled({-0.10F, -0.06F, -0.10F}, env);
    break;
  }
  }
  return sample;
}

} // namespace Animation
