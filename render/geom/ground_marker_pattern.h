#pragma once

#include <array>
#include <cstddef>

#include "../../game/accessibility/team_identity.h"

namespace Render::Geom {

struct GroundMarkerPatternSpec {
  float dash_count{1.0F};
  float dash_duty{1.0F};
  float second_ring_start{0.0F};
  float second_ring_end{0.0F};
  float tick_count{0.0F};
  float tick_length{0.0F};
};

inline constexpr float k_marker_band_inner = 0.0F;
inline constexpr float k_marker_band_outer = 1.0F;

inline constexpr float k_marker_geometry_inner = -4.6F;
inline constexpr float k_marker_geometry_outer = 3.8F;

inline constexpr std::array<GroundMarkerPatternSpec,
                            Game::Accessibility::k_team_pattern_count>
    k_ground_marker_patterns{{
        {.dash_count = 1.0F, .dash_duty = 1.0F},
        {.dash_count = 8.0F, .dash_duty = 0.62F},
        {.dash_count = 1.0F,
         .dash_duty = 1.0F,
         .second_ring_start = -4.0F,
         .second_ring_end = -3.0F},
        {.dash_count = 4.0F, .dash_duty = 0.88F},
        {.dash_count = 16.0F, .dash_duty = 0.38F},
        {.dash_count = 1.0F,
         .dash_duty = 1.0F,
         .tick_count = 4.0F,
         .tick_length = 2.5F},
    }};

[[nodiscard]] inline auto ground_marker_pattern(
    Game::Accessibility::TeamPattern pattern) -> const GroundMarkerPatternSpec& {
  const auto index = static_cast<std::size_t>(pattern);
  return index < k_ground_marker_patterns.size() ? k_ground_marker_patterns[index]
                                                 : k_ground_marker_patterns[0];
}

[[nodiscard]] inline auto
ground_marker_angular_coverage(const GroundMarkerPatternSpec& spec) -> float {
  float coverage = spec.dash_duty;
  if (spec.second_ring_end > spec.second_ring_start) {
    coverage += 1.0F;
  }
  if (spec.tick_count > 0.0F) {
    coverage += spec.tick_count * 0.05F;
  }
  return coverage;
}

} // namespace Render::Geom
