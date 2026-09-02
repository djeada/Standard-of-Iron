#pragma once

#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

namespace Game::Map {

inline constexpr int k_ring_river_default_segments = 48;
inline constexpr int k_ring_river_min_segments = 12;
inline constexpr int k_ring_river_max_segments = 256;

struct RingRiver {
  float center_x = 0.0F;
  float center_z = 0.0F;
  float radius_x = 0.0F;
  float radius_z = 0.0F;
  int segments = k_ring_river_default_segments;
};

[[nodiscard]] inline auto
ring_river_points(const RingRiver& ring) -> std::vector<std::pair<float, float>> {
  const int segments =
      ring.segments < k_ring_river_min_segments
          ? k_ring_river_min_segments
          : (ring.segments > k_ring_river_max_segments ? k_ring_river_max_segments
                                                       : ring.segments);
  std::vector<std::pair<float, float>> points;
  points.reserve(static_cast<std::size_t>(segments) + 1U);
  for (int index = 0; index <= segments; ++index) {
    const float angle = 2.0F * std::numbers::pi_v<float> *
                        static_cast<float>(index % segments) /
                        static_cast<float>(segments);
    points.emplace_back(ring.center_x + std::cos(angle) * ring.radius_x,
                        ring.center_z + std::sin(angle) * ring.radius_z);
  }
  return points;
}

} // namespace Game::Map
