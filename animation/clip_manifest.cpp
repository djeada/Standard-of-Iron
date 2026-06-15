#include "clip_manifest.h"

#include <algorithm>
#include <cmath>

#include "selection_manifest.h"

namespace Animation {

namespace {

[[nodiscard]] constexpr auto attack_sword_markers(bool ready) noexcept -> ClipMarkers {
  return {
      .anticipation_start = ready ? 0.12F : 0.10F,
      .weapon_release = ready ? 0.46F : 0.42F,
      .contact = ready ? 0.58F : 0.54F,
      .recover_unlocked = ready ? 0.76F : 0.72F,
      .exit_safe = 0.92F,
  };
}

[[nodiscard]] constexpr auto attack_spear_markers() noexcept -> ClipMarkers {
  return {
      .anticipation_start = 0.12F,
      .weapon_release = 0.34F,
      .contact = 0.48F,
      .recover_unlocked = 0.72F,
      .exit_safe = 0.90F,
  };
}

[[nodiscard]] constexpr auto riding_sword_markers() noexcept -> ClipMarkers {
  return {
      .anticipation_start = 0.10F,
      .weapon_release = 0.50F,
      .contact = 0.58F,
      .recover_unlocked = 0.76F,
      .exit_safe = 0.92F,
  };
}

[[nodiscard]] constexpr auto bow_markers() noexcept -> ClipMarkers {
  return {
      .anticipation_start = 0.20F,
      .weapon_release = 0.54F,
      .contact = 0.54F,
      .recover_unlocked = 0.64F,
      .exit_safe = 0.84F,
  };
}

[[nodiscard]] constexpr auto hold_markers() noexcept -> ClipMarkers {
  return {
      .anticipation_start = 0.0F,
      .exit_safe = 0.98F,
  };
}

[[nodiscard]] constexpr auto locomotion_markers() noexcept -> ClipMarkers {
  return {
      .exit_safe = 0.98F,
  };
}

[[nodiscard]] constexpr auto
is_sword_ready(HumanoidClipProfile profile) noexcept -> bool {
  return profile == HumanoidClipProfile::SwordReady ||
         profile == HumanoidClipProfile::Skeleton;
}

} // namespace

auto humanoid_attack_clip(AttackClipFamily family,
                          bool mounted,
                          std::uint8_t variant) noexcept -> std::uint16_t {
  switch (family) {
  case AttackClipFamily::Sword:
    if (mounted) {
      return k_humanoid_riding_sword_strike_clip;
    }
    return static_cast<std::uint16_t>(
        k_humanoid_attack_sword_a_clip +
        std::min<std::uint8_t>(variant, k_humanoid_attack_sword_variant_count - 1U));
  case AttackClipFamily::Spear:
    return static_cast<std::uint16_t>(
        k_humanoid_attack_spear_a_clip +
        std::min<std::uint8_t>(variant, k_humanoid_attack_spear_variant_count - 1U));
  case AttackClipFamily::Bow:
    return mounted ? k_humanoid_riding_bow_shot_clip : k_humanoid_attack_bow_clip;
  }
  return k_humanoid_hold_clip;
}

auto humanoid_ambient_idle_clip_variant(HumanoidAmbientIdle idle) noexcept
    -> std::uint8_t {
  switch (idle) {
  case HumanoidAmbientIdle::SitDown:
    return 1U;
  case HumanoidAmbientIdle::Jump:
    return 2U;
  case HumanoidAmbientIdle::RaiseWeapon:
    return 3U;
  case HumanoidAmbientIdle::ShiftWeight:
    return 4U;
  case HumanoidAmbientIdle::PlantFlag:
    return 5U;
  case HumanoidAmbientIdle::None:
  case HumanoidAmbientIdle::ShuffleFeet:
  case HumanoidAmbientIdle::TapFoot:
  case HumanoidAmbientIdle::StepInPlace:
  case HumanoidAmbientIdle::BendKnee:
    break;
  }
  return 0U;
}

auto humanoid_idle_variant_clip_name(std::uint8_t clip_variant) noexcept
    -> std::string_view {
  switch (clip_variant) {
  case 1U:
    return "idle_squat";
  case 2U:
    return "idle_jump";
  case 3U:
    return "idle_weapon";
  case 4U:
    return "idle_weave";
  default:
    return "idle";
  }
}

auto humanoid_construction_role_for_variant_index(std::uint8_t variant_index) noexcept
    -> HumanoidConstructionRole {
  switch (variant_index) {
  case 0U:
    return HumanoidConstructionRole::Hammer;
  case 1U:
    return HumanoidConstructionRole::Saw;
  case 2U:
    return HumanoidConstructionRole::Chisel;
  case 3U:
    return HumanoidConstructionRole::KneelingChisel;
  default:
    return HumanoidConstructionRole::Hammer;
  }
}

auto resolve_humanoid_construction_role(
    const HumanoidConstructionRoleInputs& inputs) noexcept -> HumanoidConstructionRole {
  if (inputs.force_single_soldier || !inputs.variant_table_can_select_roles ||
      inputs.variant_stride == 0U || !inputs.variant_is_seed_based) {
    return HumanoidConstructionRole::Hammer;
  }

  return humanoid_construction_role_for_variant_index(
      seeded_visual_variant_index(inputs.seed, inputs.variant_stride));
}

auto requested_humanoid_clip_variant(const HumanoidClipVariantInputs& inputs) noexcept
    -> std::uint8_t {
  if (inputs.state == StateId::Die || inputs.state == StateId::Dead) {
    return inputs.death_variant;
  }

  if (inputs.is_constructing && inputs.state == StateId::AttackSword) {
    switch (inputs.construction_role) {
    case HumanoidConstructionRole::Hammer:
      return 0U;
    case HumanoidConstructionRole::Saw:
      return 1U;
    case HumanoidConstructionRole::Chisel:
    case HumanoidConstructionRole::KneelingChisel:
      return 2U;
    case HumanoidConstructionRole::None:
      break;
    }

    auto const bucket =
        static_cast<std::uint32_t>(std::floor(inputs.construction_jitter_seed * 64.0F));
    return static_cast<std::uint8_t>(bucket % 3U);
  }

  switch (inputs.state) {
  case StateId::AttackSword:
  case StateId::AttackSpear:
  case StateId::AttackBow:
  case StateId::Cast:
    return inputs.attack_variant;
  case StateId::Idle:
    if (inputs.ambient_idle != HumanoidAmbientIdle::None) {
      return humanoid_ambient_idle_clip_variant(inputs.ambient_idle);
    }
    break;
  case StateId::Walk:
  case StateId::Run:
  case StateId::Hold:
  case StateId::AttackMelee:
  case StateId::AttackRanged:
  case StateId::Die:
  case StateId::Dead:
  case StateId::RidingIdle:
  case StateId::RidingCharge:
  case StateId::RidingReining:
  case StateId::RidingBowShot:
  case StateId::Count:
    break;
  }
  return 0U;
}

auto resolve_humanoid_clip_variant(const HumanoidClipVariantInputs& inputs) noexcept
    -> std::uint8_t {
  if (inputs.available_variant_count <= 1U) {
    return 0U;
  }
  return std::min<std::uint8_t>(requested_humanoid_clip_variant(inputs),
                                inputs.available_variant_count - 1U);
}

auto authored_humanoid_clip_markers(
    std::uint16_t clip_id, HumanoidClipProfile profile) noexcept -> ClipMarkers {
  switch (clip_id) {
  case k_humanoid_attack_sword_a_clip:
  case k_humanoid_attack_sword_b_clip:
  case k_humanoid_attack_sword_c_clip:
    return attack_sword_markers(is_sword_ready(profile));
  case k_humanoid_attack_spear_a_clip:
  case k_humanoid_attack_spear_b_clip:
  case k_humanoid_attack_spear_c_clip:
    return attack_spear_markers();
  case k_humanoid_riding_sword_strike_clip:
    return riding_sword_markers();
  case k_humanoid_attack_bow_clip:
  case k_humanoid_riding_bow_shot_clip:
    return bow_markers();
  case k_humanoid_hold_clip:
  case k_humanoid_hold_bow_clip:
    return hold_markers();
  case k_humanoid_idle_clip:
  case k_humanoid_idle_squat_clip:
  case k_humanoid_idle_jump_clip:
  case k_humanoid_idle_weapon_clip:
  case k_humanoid_idle_weave_clip:
  case k_humanoid_walk_clip:
  case k_humanoid_run_clip:
  case k_humanoid_riding_idle_clip:
  case k_humanoid_riding_charge_clip:
    return locomotion_markers();
  default:
    return {};
  }
}

auto authored_generic_clip_markers(std::string_view clip_name) noexcept -> ClipMarkers {
  if (clip_name == "hold" || clip_name == "hold_bow") {
    return hold_markers();
  }
  if (clip_name == "walk" || clip_name == "run" || clip_name == "idle" ||
      clip_name == "riding_idle" || clip_name == "riding_charge") {
    return locomotion_markers();
  }
  return {};
}

} // namespace Animation
