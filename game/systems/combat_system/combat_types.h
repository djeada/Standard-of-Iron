#pragma once

namespace Game::Systems::Combat {

namespace Constants {
inline constexpr float k_engagement_cooldown = 0.5F;
inline constexpr float k_chase_near_range_buffer = 0.25F;
inline constexpr float k_ideal_melee_distance = 0.6F;
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
inline constexpr float k_arrow_spread_min = -1.2F;
inline constexpr float k_arrow_spread_max = 1.2F;
inline constexpr float k_arrow_vertical_spread_factor = 3.5F;
inline constexpr float k_arrow_depth_spread_factor = 3.0F;
inline constexpr float k_arrow_start_height = 0.6F;
inline constexpr float k_arrow_start_offset = 0.35F;
inline constexpr float k_arrow_target_offset = 0.5F;
inline constexpr int k_arrow_volley_wave_count = 3;
inline constexpr float k_arrow_volley_rank_spacing = 0.42F;
inline constexpr float k_arrow_volley_wave_height = 0.28F;
inline constexpr float k_arrow_volley_wave_depth = 0.40F;
inline constexpr float k_arrow_speed = 14.0F;
inline constexpr int k_max_visual_arrows_per_volley = 20;
} // namespace Constants

} // namespace Game::Systems::Combat
