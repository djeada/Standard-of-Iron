#pragma once

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#include "core/component_gameplay.h"

namespace Game::Systems::WallWalk {

struct PathPoint {
  float x{0.0F};
  float z{0.0F};
  float y{0.0F};
  bool valid{false};
};

// Nearest point on a walker's published path (balcony runs, stairs, a tower
// bridge), with the walking height there.
[[nodiscard]] inline auto
project_onto_path(const std::vector<Engine::Core::WallWalkSegment>& path,
                  float x,
                  float z) -> PathPoint {
  PathPoint best;
  float best_d2 = std::numeric_limits<float>::max();
  for (auto const& segment : path) {
    float const dx = segment.bx - segment.ax;
    float const dz = segment.bz - segment.az;
    float const length2 = dx * dx + dz * dz;
    float t = 0.0F;
    if (length2 > 1.0e-8F) {
      t = std::clamp(
          ((x - segment.ax) * dx + (z - segment.az) * dz) / length2, 0.0F, 1.0F);
    }
    float const px = segment.ax + dx * t;
    float const pz = segment.az + dz * t;
    float const d2 = (px - x) * (px - x) + (pz - z) * (pz - z);
    if (d2 < best_d2) {
      best_d2 = d2;
      best = {px, pz, segment.ay + (segment.by - segment.ay) * t, true};
    }
  }
  return best;
}

} // namespace Game::Systems::WallWalk
