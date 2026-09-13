#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace Render::Geom {

inline constexpr std::array<int, 5> k_marker_segment_lods{24, 48, 96, 192, 384};

inline constexpr float k_marker_samples_per_cell = 3.0F;
inline constexpr float k_marker_min_arc_world = 0.12F;
inline constexpr float k_marker_max_arc_world = 1.0F;

[[nodiscard]] inline auto marker_segment_count(std::size_t lod) -> int {
  return k_marker_segment_lods[std::min(lod, k_marker_segment_lods.size() - 1U)];
}

[[nodiscard]] inline auto marker_angular_step(std::size_t lod) -> float {
  return 2.0F * std::numbers::pi_v<float> /
         static_cast<float>(marker_segment_count(lod));
}

[[nodiscard]] inline auto marker_target_arc_world(float world_per_cell) -> float {
  const float cell = world_per_cell > 0.0F ? world_per_cell : 1.0F;
  return std::clamp(
      cell / k_marker_samples_per_cell, k_marker_min_arc_world, k_marker_max_arc_world);
}

[[nodiscard]] inline auto marker_segment_lod(float outer_radius,
                                             float world_per_cell) -> std::size_t {
  const float radius = std::max(outer_radius, 0.0F);
  const float wanted = 2.0F * std::numbers::pi_v<float> * radius /
                       marker_target_arc_world(world_per_cell);
  for (std::size_t lod = 0; lod + 1U < k_marker_segment_lods.size(); ++lod) {
    if (static_cast<float>(k_marker_segment_lods[lod]) >= wanted) {
      return lod;
    }
  }
  return k_marker_segment_lods.size() - 1U;
}

[[nodiscard]] inline auto marker_chord_world(float outer_radius,
                                             std::size_t lod) -> float {
  return 2.0F * std::max(outer_radius, 0.0F) *
         std::sin(marker_angular_step(lod) * 0.5F);
}

} // namespace Render::Geom
