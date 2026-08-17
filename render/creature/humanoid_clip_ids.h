#pragma once

#include <cstdint>

#include "animation/clip_manifest.h"

namespace Render::Creature {

inline constexpr std::uint16_t k_humanoid_idle_clip = Animation::k_humanoid_idle_clip;
inline constexpr std::uint16_t k_humanoid_idle_squat_clip =
    Animation::k_humanoid_idle_squat_clip;
inline constexpr std::uint16_t k_humanoid_idle_jump_clip =
    Animation::k_humanoid_idle_jump_clip;
inline constexpr std::uint16_t k_humanoid_idle_weapon_clip =
    Animation::k_humanoid_idle_weapon_clip;
inline constexpr std::uint16_t k_humanoid_idle_weave_clip =
    Animation::k_humanoid_idle_weave_clip;
inline constexpr std::uint16_t k_humanoid_idle_plant_flag_clip =
    Animation::k_humanoid_idle_plant_flag_clip;

inline constexpr std::uint16_t k_humanoid_walk_clip = Animation::k_humanoid_walk_clip;
inline constexpr std::uint16_t k_humanoid_run_clip = Animation::k_humanoid_run_clip;
inline constexpr std::uint16_t k_humanoid_hold_clip = Animation::k_humanoid_hold_clip;
inline constexpr std::uint16_t k_humanoid_hold_bow_clip =
    Animation::k_humanoid_hold_bow_clip;
inline constexpr std::uint16_t k_humanoid_attack_sword_a_clip =
    Animation::k_humanoid_attack_sword_a_clip;
inline constexpr std::uint16_t k_humanoid_attack_sword_b_clip =
    Animation::k_humanoid_attack_sword_b_clip;
inline constexpr std::uint16_t k_humanoid_attack_sword_c_clip =
    Animation::k_humanoid_attack_sword_c_clip;
inline constexpr std::uint16_t k_humanoid_attack_spear_a_clip =
    Animation::k_humanoid_attack_spear_a_clip;
inline constexpr std::uint16_t k_humanoid_attack_spear_b_clip =
    Animation::k_humanoid_attack_spear_b_clip;
inline constexpr std::uint16_t k_humanoid_attack_spear_c_clip =
    Animation::k_humanoid_attack_spear_c_clip;
inline constexpr std::uint16_t k_humanoid_attack_bow_clip =
    Animation::k_humanoid_attack_bow_clip;
inline constexpr std::uint16_t k_humanoid_riding_idle_clip =
    Animation::k_humanoid_riding_idle_clip;
inline constexpr std::uint16_t k_humanoid_riding_charge_clip =
    Animation::k_humanoid_riding_charge_clip;
inline constexpr std::uint16_t k_humanoid_riding_reining_clip =
    Animation::k_humanoid_riding_reining_clip;
inline constexpr std::uint16_t k_humanoid_riding_bow_shot_clip =
    Animation::k_humanoid_riding_bow_shot_clip;
inline constexpr std::uint16_t k_humanoid_riding_sword_strike_clip =
    Animation::k_humanoid_riding_sword_strike_clip;
inline constexpr std::uint16_t k_humanoid_riding_spear_thrust_clip =
    Animation::k_humanoid_riding_spear_thrust_clip;
inline constexpr std::uint16_t k_humanoid_die_infantry_clip =
    Animation::k_humanoid_die_infantry_clip;
inline constexpr std::uint16_t k_humanoid_die_infantry_face_clip =
    Animation::k_humanoid_die_infantry_face_clip;
inline constexpr std::uint16_t k_humanoid_die_infantry_side_clip =
    Animation::k_humanoid_die_infantry_side_clip;
inline constexpr std::uint16_t k_humanoid_dead_infantry_clip =
    Animation::k_humanoid_dead_infantry_clip;
inline constexpr std::uint16_t k_humanoid_dead_infantry_face_clip =
    Animation::k_humanoid_dead_infantry_face_clip;
inline constexpr std::uint16_t k_humanoid_dead_infantry_side_clip =
    Animation::k_humanoid_dead_infantry_side_clip;
inline constexpr std::uint8_t k_humanoid_infantry_death_variant_count =
    Animation::k_humanoid_infantry_death_variant_count;
inline constexpr std::uint16_t k_humanoid_die_mounted_clip =
    Animation::k_humanoid_die_mounted_clip;
inline constexpr std::uint16_t k_humanoid_dead_mounted_clip =
    Animation::k_humanoid_dead_mounted_clip;
inline constexpr std::uint16_t k_humanoid_rpg_sword_slash_left_clip =
    Animation::k_humanoid_rpg_sword_slash_left_clip;
inline constexpr std::uint16_t k_humanoid_rpg_sword_slash_right_clip =
    Animation::k_humanoid_rpg_sword_slash_right_clip;
inline constexpr std::uint16_t k_humanoid_rpg_sword_overhead_clip =
    Animation::k_humanoid_rpg_sword_overhead_clip;
inline constexpr std::uint16_t k_humanoid_rpg_sword_thrust_clip =
    Animation::k_humanoid_rpg_sword_thrust_clip;
inline constexpr std::uint16_t k_humanoid_rpg_sword_finisher_clip =
    Animation::k_humanoid_rpg_sword_finisher_clip;
inline constexpr std::uint16_t k_humanoid_testudo_first_clip =
    Animation::k_humanoid_testudo_first_clip;
inline constexpr std::uint16_t k_humanoid_testudo_rear_clip =
    Animation::k_humanoid_testudo_rear_clip;
inline constexpr std::uint8_t k_humanoid_testudo_clip_count =
    Animation::k_humanoid_testudo_clip_count;
inline constexpr std::uint16_t k_humanoid_carthage_shield_wall_first_clip =
    Animation::k_humanoid_carthage_shield_wall_first_clip;
inline constexpr std::uint16_t k_humanoid_carthage_shield_wall_right_clip =
    Animation::k_humanoid_carthage_shield_wall_right_clip;
inline constexpr std::uint8_t k_humanoid_carthage_shield_wall_clip_count =
    Animation::k_humanoid_carthage_shield_wall_clip_count;
inline constexpr std::uint16_t k_humanoid_archer_melee_clip =
    Animation::k_humanoid_archer_melee_clip;
inline constexpr std::uint16_t k_humanoid_hold_spear_attack_clip =
    Animation::k_humanoid_hold_spear_attack_clip;
inline constexpr std::uint16_t k_humanoid_hold_bow_attack_clip =
    Animation::k_humanoid_hold_bow_attack_clip;
inline constexpr std::uint16_t k_humanoid_unarmed_jab_clip =
    Animation::k_humanoid_unarmed_jab_clip;
inline constexpr std::uint16_t k_humanoid_unarmed_cross_clip =
    Animation::k_humanoid_unarmed_cross_clip;
inline constexpr std::uint16_t k_humanoid_unarmed_hook_clip =
    Animation::k_humanoid_unarmed_hook_clip;
inline constexpr std::uint8_t k_humanoid_unarmed_variant_count =
    Animation::k_humanoid_unarmed_variant_count;
inline constexpr std::uint16_t k_humanoid_showcase_rest_sit_clip =
    Animation::k_humanoid_showcase_rest_sit_clip;
inline constexpr std::uint16_t k_humanoid_showcase_rest_sit_knees_clip =
    Animation::k_humanoid_showcase_rest_sit_knees_clip;
inline constexpr std::uint16_t k_humanoid_showcase_rest_kneel_clip =
    Animation::k_humanoid_showcase_rest_kneel_clip;
inline constexpr std::uint16_t k_humanoid_showcase_rest_sit_down_clip =
    Animation::k_humanoid_showcase_rest_sit_down_clip;
inline constexpr std::uint16_t k_humanoid_showcase_rest_sit_knees_down_clip =
    Animation::k_humanoid_showcase_rest_sit_knees_down_clip;

} // namespace Render::Creature
