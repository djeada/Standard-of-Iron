#pragma once

namespace Game::Systems::Combat {

namespace Constants {
inline constexpr float k_engagement_cooldown = 0.5F;
inline constexpr float k_ideal_melee_distance = 0.6F;
inline constexpr float k_max_melee_separation = 0.9F;
inline constexpr float k_melee_pull_factor = 0.3F;
inline constexpr float k_melee_pull_speed = 5.0F;
inline constexpr float k_move_amount_factor = 0.5F;
inline constexpr float k_min_distance = 0.001F;
inline constexpr float k_max_displacement_per_frame = 0.02F;
inline constexpr float k_range_multiplier_hold = 1.5F;
inline constexpr float k_range_multiplier_spearman_hold = 2.0F;
inline constexpr float k_damage_multiplier_archer_hold = 1.5F;
inline constexpr float k_damage_multiplier_spearman_hold = 1.5F;
inline constexpr float k_damage_multiplier_default_hold = 1.75F;
inline constexpr float k_health_multiplier_hold = 1.2F;
inline constexpr float k_spearman_vs_cavalry_multiplier = 2.5F;
inline constexpr float k_archer_vs_elephant_multiplier = 2.0F;
inline constexpr float k_archer_high_ground_multiplier = 1.8F;
inline constexpr float k_spearman_high_ground_multiplier = 1.8F;
inline constexpr float k_high_ground_armor_multiplier = 0.85F;
inline constexpr float k_high_ground_health_multiplier = 1.15F;
inline constexpr float k_high_ground_height_threshold = 0.5F;
inline constexpr float k_optimal_range_factor = 0.85F;
inline constexpr float k_optimal_range_buffer = 0.5F;
inline constexpr float k_new_command_threshold = 0.25F;
inline constexpr float k_arrow_spread_min = -0.8F;
inline constexpr float k_arrow_spread_max = 0.8F;
inline constexpr float k_arrow_vertical_spread_factor = 3.5F;
inline constexpr float k_arrow_depth_spread_factor = 3.0F;
inline constexpr float k_arrow_start_height = 0.6F;
inline constexpr float k_arrow_start_offset = 0.35F;
inline constexpr float k_arrow_target_offset = 0.5F;
inline constexpr float k_arrow_speed = 14.0F;
} // namespace Constants

} // namespace Game::Systems::Combat
