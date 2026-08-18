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

[[nodiscard]] constexpr auto
rpg_sword_markers(SwordAttackAnimation animation) noexcept -> ClipMarkers {
  switch (animation) {
  case SwordAttackAnimation::RpgThrust:
    return {
        .anticipation_start = 0.08F,
        .weapon_release = 0.34F,
        .contact = 0.46F,
        .recover_unlocked = 0.70F,
        .exit_safe = 0.90F,
    };
  case SwordAttackAnimation::RpgOverhead:
    return {
        .anticipation_start = 0.10F,
        .weapon_release = 0.44F,
        .contact = 0.58F,
        .recover_unlocked = 0.80F,
        .exit_safe = 0.94F,
    };
  case SwordAttackAnimation::RpgFinisher:
    return {
        .anticipation_start = 0.06F,
        .weapon_release = 0.50F,
        .contact = 0.66F,
        .recover_unlocked = 0.84F,
        .exit_safe = 0.96F,
    };
  case SwordAttackAnimation::RpgSlashLeft:
  case SwordAttackAnimation::RpgSlashRight:
  case SwordAttackAnimation::InfantrySlashA:
  case SwordAttackAnimation::InfantrySlashB:
  case SwordAttackAnimation::InfantrySlashC:
  case SwordAttackAnimation::MountedSlash:
    break;
  }
  return {
      .anticipation_start = 0.09F,
      .weapon_release = 0.38F,
      .contact = 0.52F,
      .recover_unlocked = 0.74F,
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

[[nodiscard]] constexpr auto unarmed_markers() noexcept -> ClipMarkers {
  return {
      .anticipation_start = 0.10F,
      .weapon_release = 0.30F,
      .contact = 0.50F,
      .recover_unlocked = 0.70F,
      .exit_safe = 0.94F,
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
  case AttackClipFamily::Unarmed:
    return static_cast<std::uint16_t>(k_humanoid_unarmed_jab_clip +
                                      (variant % k_humanoid_unarmed_variant_count));
  case AttackClipFamily::Sword:
    if (mounted) {
      return k_humanoid_riding_sword_strike_clip;
    }
    return static_cast<std::uint16_t>(
        k_humanoid_attack_sword_a_clip +
        std::min<std::uint8_t>(variant, k_humanoid_attack_sword_variant_count - 1U));
  case AttackClipFamily::Spear:
    if (mounted) {
      return k_humanoid_riding_spear_thrust_clip;
    }
    return static_cast<std::uint16_t>(
        k_humanoid_attack_spear_a_clip +
        std::min<std::uint8_t>(variant, k_humanoid_attack_spear_variant_count - 1U));
  case AttackClipFamily::Bow:
    return mounted ? k_humanoid_riding_bow_shot_clip : k_humanoid_attack_bow_clip;
  }
  return k_humanoid_hold_clip;
}

auto humanoid_sword_attack_clip(SwordAttackAnimation animation) noexcept
    -> std::uint16_t {
  switch (animation) {
  case SwordAttackAnimation::InfantrySlashA:
    return k_humanoid_attack_sword_a_clip;
  case SwordAttackAnimation::InfantrySlashB:
    return k_humanoid_attack_sword_b_clip;
  case SwordAttackAnimation::InfantrySlashC:
    return k_humanoid_attack_sword_c_clip;
  case SwordAttackAnimation::RpgSlashLeft:
    return k_humanoid_rpg_sword_slash_left_clip;
  case SwordAttackAnimation::RpgSlashRight:
    return k_humanoid_rpg_sword_slash_right_clip;
  case SwordAttackAnimation::RpgOverhead:
    return k_humanoid_rpg_sword_overhead_clip;
  case SwordAttackAnimation::RpgThrust:
    return k_humanoid_rpg_sword_thrust_clip;
  case SwordAttackAnimation::RpgFinisher:
    return k_humanoid_rpg_sword_finisher_clip;
  case SwordAttackAnimation::MountedSlash:
    return k_humanoid_riding_sword_strike_clip;
  }
  return k_humanoid_attack_sword_a_clip;
}

auto humanoid_sword_attack_name(SwordAttackAnimation animation) noexcept
    -> std::string_view {
  switch (animation) {
  case SwordAttackAnimation::InfantrySlashA:
    return "attack_sword_a";
  case SwordAttackAnimation::InfantrySlashB:
    return "attack_sword_b";
  case SwordAttackAnimation::InfantrySlashC:
    return "attack_sword_c";
  case SwordAttackAnimation::RpgSlashLeft:
    return "rpg_sword_slash_left";
  case SwordAttackAnimation::RpgSlashRight:
    return "rpg_sword_slash_right";
  case SwordAttackAnimation::RpgOverhead:
    return "rpg_sword_overhead";
  case SwordAttackAnimation::RpgThrust:
    return "rpg_sword_thrust";
  case SwordAttackAnimation::RpgFinisher:
    return "rpg_sword_finisher";
  case SwordAttackAnimation::MountedSlash:
    return "riding_sword_strike";
  }
  return "attack_sword_a";
}

auto humanoid_idle_breath_offset(std::uint32_t inst_seed) noexcept -> float {
  std::uint32_t x = inst_seed ^ 0x2F1B3C7DU;
  x ^= x >> 16U;
  x *= 0x7feb352dU;
  x ^= x >> 15U;
  x *= 0x846ca68bU;
  x ^= x >> 16U;
  return static_cast<float>(x & 0x7FFFFFU) / static_cast<float>(0x7FFFFFU);
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
  case 5U:
    return "idle_plant_flag";
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
  case StateId::AttackMelee:
    return static_cast<std::uint8_t>(inputs.attack_variant %
                                     inputs.available_variant_count);
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
  case StateId::AttackRanged:
  case StateId::Die:
  case StateId::Dead:
  case StateId::RidingIdle:
  case StateId::RidingCharge:
  case StateId::RidingReining:
  case StateId::RidingBowShot:
  case StateId::RpgSwordSlashLeft:
  case StateId::RpgSwordSlashRight:
  case StateId::RpgSwordOverhead:
  case StateId::RpgSwordThrust:
  case StateId::RpgSwordFinisher:
  case StateId::WildlifeTense:
  case StateId::WildlifeStartle:
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
  case k_humanoid_unarmed_jab_clip:
  case k_humanoid_unarmed_cross_clip:
  case k_humanoid_unarmed_hook_clip:
    return unarmed_markers();
  case k_humanoid_attack_sword_a_clip:
  case k_humanoid_attack_sword_b_clip:
  case k_humanoid_attack_sword_c_clip:
    return attack_sword_markers(is_sword_ready(profile));
  case k_humanoid_attack_spear_a_clip:
  case k_humanoid_attack_spear_b_clip:
  case k_humanoid_attack_spear_c_clip:
  case k_humanoid_riding_spear_thrust_clip:
    return attack_spear_markers();
  case k_humanoid_riding_sword_strike_clip:
    return riding_sword_markers();
  case k_humanoid_rpg_sword_slash_left_clip:
    return rpg_sword_markers(SwordAttackAnimation::RpgSlashLeft);
  case k_humanoid_rpg_sword_slash_right_clip:
    return rpg_sword_markers(SwordAttackAnimation::RpgSlashRight);
  case k_humanoid_rpg_sword_overhead_clip:
    return rpg_sword_markers(SwordAttackAnimation::RpgOverhead);
  case k_humanoid_rpg_sword_thrust_clip:
    return rpg_sword_markers(SwordAttackAnimation::RpgThrust);
  case k_humanoid_rpg_sword_finisher_clip:
    return rpg_sword_markers(SwordAttackAnimation::RpgFinisher);
  case k_humanoid_archer_melee_clip:
    return attack_sword_markers(false);
  case k_humanoid_hold_spear_attack_clip:
    return attack_spear_markers();
  case k_humanoid_hold_bow_attack_clip:
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
  case k_humanoid_idle_plant_flag_clip:
  case k_humanoid_walk_clip:
  case k_humanoid_run_clip:
  case k_humanoid_riding_idle_clip:
  case k_humanoid_riding_charge_clip:
    return locomotion_markers();
  case k_humanoid_showcase_spear_throw_clip:
    return ClipMarkers{0.16F, 0.58F, 0.58F, 0.72F, 0.92F};
  case k_humanoid_showcase_sword_flourish_clip:
    return ClipMarkers{0.08F, 0.36F, 0.44F, 0.80F, 0.95F};
  case k_humanoid_showcase_jump_clip:
  case k_humanoid_showcase_front_flip_clip:
  case k_humanoid_showcase_handstand_clip:
  case k_humanoid_showcase_side_aerial_clip:
  case k_humanoid_showcase_rest_sit_clip:
  case k_humanoid_showcase_rest_sit_knees_clip:
  case k_humanoid_showcase_rest_kneel_clip:
  case k_humanoid_showcase_rest_sit_down_clip:
  case k_humanoid_showcase_rest_sit_knees_down_clip:
    return locomotion_markers();
  default:
    return {};
  }
}

auto humanoid_showcase_clip(std::uint8_t showcase_move) noexcept -> std::uint16_t {
  if (showcase_move == 0U) {
    return k_unmapped_clip;
  }
  if (showcase_move <= k_humanoid_showcase_clip_count) {
    return static_cast<std::uint16_t>(k_humanoid_showcase_first_clip + showcase_move -
                                      1U);
  }

  const std::uint8_t taunt_ordinal =
      static_cast<std::uint8_t>(showcase_move - k_humanoid_showcase_clip_count - 1U);
  if (taunt_ordinal < k_humanoid_taunt_clip_count) {
    return static_cast<std::uint16_t>(k_humanoid_taunt_first_clip + taunt_ordinal);
  }
  return k_unmapped_clip;
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
