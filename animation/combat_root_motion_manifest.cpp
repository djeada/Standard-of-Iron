#include "combat_root_motion_manifest.h"

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

[[nodiscard]] auto ease_in(float t) noexcept -> float {
  t = clamp01(t);
  return t * t;
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

[[nodiscard]] auto out_and_back(float progress, float out_fraction) noexcept -> float {
  float const q = clamp01(progress);
  float const out = std::clamp(out_fraction, 0.05F, 0.95F);
  if (q < out) {
    return ease_out(q / out);
  }
  return 1.0F - smooth01((q - out) / (1.0F - out));
}

[[nodiscard]] auto lunge_amplitude(CombatAttackFamily family) noexcept -> float {
  switch (family) {
  case CombatAttackFamily::Sword:
    return 0.24F;
  case CombatAttackFamily::Spear:
    return 0.30F;
  case CombatAttackFamily::Bow:
    return 0.16F;
  case CombatAttackFamily::None:
    return 0.18F;
  }
  return 0.18F;
}

[[nodiscard]] auto seed_sign(std::uint32_t seed) noexcept -> float {
  std::uint32_t v = seed ^ 0x51ED2705U;
  v ^= v >> 16U;
  v *= 0x7feb352dU;
  v ^= v >> 15U;
  return ((v >> 7U) & 1U) == 0U ? 1.0F : -1.0F;
}

[[nodiscard]] auto settle(float q, float peak) noexcept -> float {
  if (q <= peak) {
    return smooth01(q / std::max(peak, 1.0e-4F));
  }

  float const t = std::clamp((q - peak) / std::max(1.0F - peak, 1.0e-4F), 0.0F, 1.0F);
  return 1.0F - smooth01(std::pow(t, 0.55F));
}

} // namespace

auto hit_reaction_form_from_kind(std::uint8_t kind) noexcept -> HitReactionForm {
  switch (kind) {
  case 1U:
    return HitReactionForm::Block;
  case 2U:
    return HitReactionForm::Evade;
  case 3U:
    return HitReactionForm::Stagger;
  case 4U:
    return HitReactionForm::Recoil;
  default:
    return HitReactionForm::Flinch;
  }
}

auto melee_lunge_offset(CombatAttackFamily family,
                        float attack_phase,
                        MeleeSwingOutcome outcome,
                        bool formation_member) noexcept -> float {
  float const p = clamp01(attack_phase);
  float amplitude = lunge_amplitude(family);
  if (formation_member) {
    amplitude *= 0.55F;
  }
  float return_end = 0.95F;
  switch (outcome) {
  case MeleeSwingOutcome::Evaded:
    amplitude *= 1.40F;
    return_end = 1.0F;
    break;
  case MeleeSwingOutcome::Heavy:
    amplitude *= 1.15F;
    break;
  case MeleeSwingOutcome::Blocked:
  case MeleeSwingOutcome::Clean:
  case MeleeSwingOutcome::Plain:
    break;
  }

  constexpr float k_load_back = -0.035F;
  if (p < 0.14F) {
    return k_load_back * smooth01(p / 0.14F);
  }
  if (p < 0.30F) {
    return k_load_back - 0.01F * smooth01((p - 0.14F) / 0.16F);
  }
  if (p < 0.55F) {
    float const t = ease_in(segment(p, 0.30F, 0.55F));
    return (k_load_back - 0.01F) + (amplitude - (k_load_back - 0.01F)) * t;
  }
  if (p < 0.70F) {
    return amplitude * (1.0F - 0.08F * segment(p, 0.55F, 0.70F));
  }
  return amplitude * 0.92F * (1.0F - smooth01(segment(p, 0.70F, return_end)));
}

auto resolve_combat_root_motion(const CombatRootMotionInputs& inputs) noexcept
    -> CombatRootMotionSample {
  CombatRootMotionSample sample{};

  if (inputs.attacking && inputs.melee && !inputs.mounted &&
      inputs.phase != CombatTransactionPhase::None &&
      inputs.phase != CombatTransactionPhase::ExitBlend) {
    float const p = clamp01(inputs.attack_phase);

    sample.forward_offset = inputs.simulation_owns_lunge
                                ? 0.0F
                                : melee_lunge_offset(inputs.attack_family,
                                                     p,
                                                     inputs.swing_outcome,
                                                     inputs.formation_member);

    float const windup = smooth01(segment(p, 0.08F, 0.30F)) *
                         (1.0F - smooth01(segment(p, 0.30F, 0.48F)));
    float const drive = smooth01(segment(p, 0.34F, 0.58F)) *
                        (1.0F - smooth01(segment(p, 0.66F, 0.94F)));
    float lean = 8.0F;
    if (inputs.swing_outcome == MeleeSwingOutcome::Heavy) {
      lean = 11.0F;
    } else if (inputs.swing_outcome == MeleeSwingOutcome::Evaded) {
      lean = 12.0F;
    }
    if (inputs.formation_member) {
      lean *= 0.7F;
    }
    sample.pitch_degrees = -3.0F * windup + lean * drive;
    sample.active = true;
  }

  if (inputs.hit_reacting) {
    float const q = clamp01(inputs.reaction_progress);
    float const intensity = std::clamp(inputs.reaction_intensity, 0.5F, 1.5F);
    float const translation_scale =
        intensity * (inputs.body_displaced_by_simulation ? 0.40F : 1.0F);
    float const sign = seed_sign(inputs.seed);

    float back = 0.0F;
    float lateral = 0.0F;
    float pitch = 0.0F;
    float roll = 0.0F;
    float squash = 0.0F;

    float stumble = 0.0F;
    switch (inputs.reaction) {

    case HitReactionForm::Flinch: {
      float const env = out_and_back(q, 0.30F);
      back = 0.30F * env;
      pitch = -8.0F * env;
      squash = 0.04F * env;
      roll = 3.0F * sign * env;
      stumble = 0.06F * settle(q, 0.30F);
      break;
    }
    case HitReactionForm::Block: {
      float const env = out_and_back(q, 0.26F);
      back = 0.17F * env;
      pitch = 5.0F * env;
      roll = -4.0F * env;
      squash = 0.03F * env;
      stumble = 0.05F * settle(q, 0.26F);
      break;
    }
    case HitReactionForm::Evade: {
      float const env = out_and_back(q, 0.36F);
      back = 0.34F * env;
      lateral = 0.30F * sign * env;
      pitch = -6.0F * env;
      roll = -5.0F * sign * env;
      break;
    }
    case HitReactionForm::Stagger: {
      float const env = out_and_back(q, 0.42F);
      float const overshoot =
          std::sin(std::numbers::pi_v<float> * segment(q, 0.62F, 1.0F));
      back = 0.58F * env;
      lateral = 0.16F * sign * env;
      pitch = -12.0F * env + 4.0F * overshoot;
      roll = 7.0F * sign * env;
      squash = 0.06F * env;
      stumble = 0.22F * settle(q, 0.42F);
      break;
    }
    case HitReactionForm::Recoil: {
      float const env = out_and_back(q, 0.30F);
      back = 0.22F * env;
      pitch = -6.0F * env;
      squash = 0.03F * env;
      roll = -2.0F * sign * env;
      break;
    }
    }
    back += stumble;

    float dir_x = inputs.recoil_dir_x;
    float dir_z = inputs.recoil_dir_z;
    float const dir_length = std::hypot(dir_x, dir_z);
    if (dir_length > 1.0e-4F) {
      dir_x /= dir_length;
      dir_z /= dir_length;
    } else {
      dir_x = 0.0F;
      dir_z = 0.0F;
    }
    float const side_x = dir_z;
    float const side_z = -dir_x;
    sample.world_offset_x += (dir_x * back + side_x * lateral) * translation_scale;
    sample.world_offset_z += (dir_z * back + side_z * lateral) * translation_scale;
    float const rotation_scale = std::min(intensity, 1.0F);
    sample.pitch_degrees += pitch * rotation_scale;
    sample.roll_degrees += roll * rotation_scale;
    sample.squash += squash * intensity;
    sample.active = true;
  }

  return sample;
}

} // namespace Animation
