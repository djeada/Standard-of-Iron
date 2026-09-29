#pragma once

namespace Game::Formation::Tuning {

inline constexpr float k_maintain_speed_multiplier = 0.55F;
inline constexpr float k_in_slot_radius_scale = 1.35F;

inline constexpr float k_formed_cohesion = 0.8F;
inline constexpr float k_disrupted_cohesion = 0.45F;
inline constexpr float k_opening_progress_spacing_scale = 1.5F;

inline constexpr float k_formed_damage_floor = 0.88F;
inline constexpr float k_disrupted_damage_penalty = 1.08F;

inline constexpr float k_facing_aligned_degrees = 4.0F;

inline constexpr float k_straggler_idle_seconds = 2.0F;
inline constexpr int k_straggler_max_attempts = 2;
inline constexpr float k_straggler_retry_seconds = 3.0F;

inline constexpr float k_replan_interval_seconds = 0.5F;
inline constexpr float k_advance_interval_seconds = 0.25F;
inline constexpr float k_cohesion_interval_seconds = 0.35F;

} // namespace Game::Formation::Tuning
