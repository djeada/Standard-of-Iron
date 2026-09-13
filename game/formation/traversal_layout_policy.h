#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Game::Formation::TraversalPolicy {

inline constexpr float k_minimum_soldier_separation = 0.55F;
inline constexpr float k_body_spacing_scale = 1.08F;
inline constexpr float k_formation_spacing_scale = 0.82F;

inline constexpr float k_enter_clearance = 0.10F;

inline constexpr float k_exit_clearance = 0.60F;

inline constexpr float k_transit_separation_scale = 0.72F;

inline constexpr float k_cover_priority_scale = 0.6F;

[[nodiscard]] inline auto compact_spacing(float body_radius,
                                          float authored_spacing) noexcept -> float {
  return std::max({k_minimum_soldier_separation,
                   body_radius * 2.0F * k_body_spacing_scale,
                   authored_spacing * k_formation_spacing_scale});
}

} // namespace Game::Formation::TraversalPolicy
