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
  // On a ladder (a run steeper than any stair): climbed hand over hand,
  // facing (face_x, face_z), the way the ladder leans.
  bool steep{false};
  float face_x{0.0F};
  float face_z{1.0F};
};

// A run rises this much per metre walked or more only on a ladder.
inline constexpr float k_steep_rise = 1.3F;

[[nodiscard]] inline auto segment_is_steep(const Engine::Core::WallWalkSegment& s) -> bool {
  float const run = std::hypot(s.bx - s.ax, s.bz - s.az);
  return std::abs(s.by - s.ay) > k_steep_rise * run && std::abs(s.by - s.ay) > 0.2F;
}

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
      if (segment_is_steep(segment)) {
        // Face up the ladder: towards the end that is higher.
        float const sign = segment.by >= segment.ay ? 1.0F : -1.0F;
        float const len = std::hypot(dx, dz);
        best.steep = true;
        if (len > 1.0e-4F) {
          best.face_x = dx / len * sign;
          best.face_z = dz / len * sign;
        } else {
          best.face_x = 0.0F;
          best.face_z = 0.0F;
        }
      }
    }
  }
  return best;
}

} // namespace Game::Systems::WallWalk
