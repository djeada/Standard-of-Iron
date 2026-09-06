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

inline constexpr float k_file_recovery_clearance = 0.30F;

inline constexpr float k_transit_separation_scale = 0.72F;

inline constexpr float k_cover_priority_scale = 0.6F;

[[nodiscard]] inline auto compact_spacing(float body_radius,
                                          float authored_spacing) noexcept -> float {
  return std::max({k_minimum_soldier_separation,
                   body_radius * 2.0F * k_body_spacing_scale,
                   authored_spacing * k_formation_spacing_scale});
}

[[nodiscard]] inline auto required_half_width(std::uint32_t files,
                                              float edge_margin,
                                              float file_spacing) noexcept -> float {
  float const gaps = files > 1U ? static_cast<float>(files - 1U) : 0.0F;
  return edge_margin + (0.5F * gaps * std::max(0.0F, file_spacing));
}

[[nodiscard]] inline auto
files_that_fit(float available_half_width,
               float edge_margin,
               float file_spacing,
               std::uint32_t ceiling) noexcept -> std::uint32_t {
  if (ceiling <= 1U || file_spacing <= 0.0F) {
    return std::max<std::uint32_t>(1U, ceiling);
  }
  float const usable = available_half_width - edge_margin;
  if (usable <= 0.0F) {
    return 1U;
  }
  auto const gaps = static_cast<std::uint32_t>(
      std::floor(((2.0F * usable) / file_spacing) + 1.0e-4F));
  return std::clamp(gaps + 1U, 1U, ceiling);
}

} // namespace Game::Formation::TraversalPolicy
