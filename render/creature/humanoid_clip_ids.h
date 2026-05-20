#pragma once

#include <cstdint>

namespace Render::Creature {

inline constexpr std::uint16_t k_humanoid_idle_clip = 0U;
inline constexpr std::uint16_t k_humanoid_idle_squat_clip = 1U;
inline constexpr std::uint16_t k_humanoid_idle_jump_clip = 2U;
inline constexpr std::uint16_t k_humanoid_idle_weapon_clip = 3U;
inline constexpr std::uint16_t k_humanoid_idle_weave_clip = 4U;

inline constexpr std::uint16_t k_humanoid_walk_clip = 5U;
inline constexpr std::uint16_t k_humanoid_run_clip = 6U;
inline constexpr std::uint16_t k_humanoid_hold_clip = 7U;
inline constexpr std::uint16_t k_humanoid_hold_bow_clip = 8U;
inline constexpr std::uint16_t k_humanoid_attack_sword_a_clip = 9U;
inline constexpr std::uint16_t k_humanoid_attack_sword_b_clip = 10U;
inline constexpr std::uint16_t k_humanoid_attack_sword_c_clip = 11U;
inline constexpr std::uint16_t k_humanoid_attack_spear_a_clip = 12U;
inline constexpr std::uint16_t k_humanoid_attack_spear_b_clip = 13U;
inline constexpr std::uint16_t k_humanoid_attack_spear_c_clip = 14U;
inline constexpr std::uint16_t k_humanoid_attack_bow_clip = 15U;
inline constexpr std::uint16_t k_humanoid_riding_idle_clip = 16U;
inline constexpr std::uint16_t k_humanoid_riding_charge_clip = 17U;
inline constexpr std::uint16_t k_humanoid_riding_reining_clip = 18U;
inline constexpr std::uint16_t k_humanoid_riding_bow_shot_clip = 19U;
inline constexpr std::uint16_t k_humanoid_riding_sword_strike_clip = 20U;
inline constexpr std::uint16_t k_humanoid_die_infantry_clip = 21U;
inline constexpr std::uint16_t k_humanoid_dead_infantry_clip = 22U;
inline constexpr std::uint16_t k_humanoid_die_mounted_clip = 23U;
inline constexpr std::uint16_t k_humanoid_dead_mounted_clip = 24U;

} // namespace Render::Creature
