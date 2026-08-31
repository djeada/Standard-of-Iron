#pragma once

#include <algorithm>
#include <cmath>

namespace Game::Systems {

struct Point {
  int x = 0;
  int y = 0;

  constexpr Point() = default;
  constexpr Point(int x_, int y_)
      : x(x_)
      , y(y_) {}

  constexpr auto operator==(const Point& other) const -> bool {
    return x == other.x && y == other.y;
  }
};

struct DirtyRegion {
  int min_x;
  int max_x;
  int min_z;
  int max_z;

  DirtyRegion(int x1, int x2, int z1, int z2)
      : min_x(x1)
      , max_x(x2)
      , min_z(z1)
      , max_z(z2) {}
};

struct CellRange {
  int min_x{0};
  int max_x{0};
  int min_z{0};
  int max_z{0};
};

struct WorldRect {
  float min_x{0.0F};
  float max_x{0.0F};
  float min_z{0.0F};
  float max_z{0.0F};
};

[[nodiscard]] inline auto body_cell_range(float center_u,
                                          float center_v,
                                          float radius,
                                          float half_cell) noexcept -> CellRange {
  return {.min_x = static_cast<int>(std::floor(center_u - radius - half_cell)),
          .max_x = static_cast<int>(std::ceil(center_u + radius + half_cell)),
          .min_z = static_cast<int>(std::floor(center_v - radius - half_cell)),
          .max_z = static_cast<int>(std::ceil(center_v + radius + half_cell))};
}

struct CellGap {
  float u{0.0F};
  float v{0.0F};

  [[nodiscard]] auto squared() const noexcept -> float { return (u * u) + (v * v); }

  [[nodiscard]] auto length() const noexcept -> float { return std::hypot(u, v); }
};

[[nodiscard]] inline auto cell_gap(float center_u,
                                   float center_v,
                                   int cell_x,
                                   int cell_z,
                                   float half_cell) noexcept -> CellGap {
  return {
      .u = std::max(0.0F, std::abs(center_u - static_cast<float>(cell_x)) - half_cell),
      .v = std::max(0.0F, std::abs(center_v - static_cast<float>(cell_z)) - half_cell)};
}

template <typename Visit>
void for_each_ring_cell(int ring, Visit&& visit) {
  if (ring <= 0) {
    visit(0, 0);
    return;
  }
  for (int dz = -ring; dz <= ring; ++dz) {
    if (dz == -ring || dz == ring) {
      for (int dx = -ring; dx <= ring; ++dx) {
        visit(dx, dz);
      }
      continue;
    }
    visit(-ring, dz);
    visit(ring, dz);
  }
}

[[nodiscard]] inline auto cell_count(const CellRange& range) noexcept -> long long {
  const long long span_x = range.max_x - range.min_x + 1;
  const long long span_z = range.max_z - range.min_z + 1;
  if (span_x <= 0 || span_z <= 0) {
    return 0;
  }
  return span_x * span_z;
}

} // namespace Game::Systems
