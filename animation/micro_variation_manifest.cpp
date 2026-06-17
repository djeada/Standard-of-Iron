#include "micro_variation_manifest.h"

#include <algorithm>
#include <cmath>

namespace Animation {

auto resolve_humanoid_combat_micro_variation(
    const HumanoidCombatMicroVariationInputs& inputs) noexcept
    -> HumanoidCombatMicroVariation {
  HumanoidCombatMicroVariation variation{};
  if (!inputs.multi_soldier_unit || !inputs.authoritative_combat || inputs.is_dying ||
      inputs.is_dead) {
    return variation;
  }

  variation.active = true;
  float const attack_phase = std::clamp(inputs.attack_phase, 0.0F, 1.0F);
  variation.lane_sign = ((inputs.inst_seed >> 7U) & 1U) == 0U ? -1.0F : 1.0F;
  variation.breath =
      std::sin(inputs.sample_time * 5.3F + static_cast<float>(inputs.inst_seed & 31U)) *
      0.004F;
  float const pressure = inputs.is_attacking ? 1.0F : 0.45F;

  switch (inputs.lane) {
  case SoldierCombatLane::LeadStrike:
    variation.torso_lean = 0.030F * pressure;
    variation.shoulder_delay = 0.018F;
    variation.wrist_angle = 0.014F;
    variation.foot_adjust = 0.022F;
    variation.head_tracking = 0.010F;
    break;
  case SoldierCombatLane::SupportStrike:
    variation.torso_lean = 0.018F * pressure;
    variation.shoulder_delay = -0.010F;
    variation.wrist_angle = 0.010F;
    variation.foot_adjust = 0.014F;
    variation.head_tracking = 0.008F;
    break;
  case SoldierCombatLane::ShieldBrace:
    variation.lateral_tilt = -0.018F * variation.lane_sign;
    variation.shield_reaction = 0.020F;
    variation.foot_adjust = -0.010F;
    variation.head_tracking = -0.006F;
    break;
  case SoldierCombatLane::StepIn:
    variation.torso_lean = 0.016F;
    variation.foot_adjust = 0.030F;
    variation.head_tracking = 0.006F;
    break;
  case SoldierCombatLane::StepOut:
    variation.torso_lean = -0.012F;
    variation.foot_adjust = -0.028F;
    variation.lateral_tilt = 0.010F * variation.lane_sign;
    break;
  case SoldierCombatLane::IdleReady:
    variation.lateral_tilt = 0.008F * variation.lane_sign;
    variation.head_tracking = 0.005F;
    break;
  case SoldierCombatLane::RangedReload:
    variation.torso_lean = -0.014F;
    variation.shoulder_delay = -0.012F;
    variation.wrist_angle = -0.010F;
    variation.foot_adjust = -0.012F;
    break;
  case SoldierCombatLane::None:
    break;
  }

  if (inputs.combat_phase == CombatPhase::Impact ||
      (attack_phase >= 0.50F && attack_phase <= 0.68F)) {
    variation.impact_stagger = 0.016F * pressure;
  }

  return variation;
}

auto resolve_humanoid_hit_reaction_transform(
    const HumanoidHitReactionTransformInputs& inputs) noexcept
    -> HumanoidHitReactionTransformSample {
  HumanoidHitReactionTransformSample sample{};
  if (!inputs.active || inputs.intensity <= 0.01F) {
    return sample;
  }

  float const desync =
      0.7F + 0.6F *
                 static_cast<float>(
                     ((inputs.inst_seed ^ 0x68F2A3B1U) * 2654435761U) >> 24U & 0xFFU) /
                 255.0F;
  constexpr float k_hit_recoil_scale = 1.6F;
  float const intensity = std::min(inputs.intensity, 1.5F);
  sample.active = true;
  sample.recoil_x = inputs.recoil_x * k_hit_recoil_scale * desync;
  sample.recoil_z = inputs.recoil_z * k_hit_recoil_scale * desync;
  sample.squash = intensity * 0.05F * desync;
  sample.tilt_degrees = intensity * 9.0F * desync;
  return sample;
}

} // namespace Animation
