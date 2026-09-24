#pragma once

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstddef>

namespace Game::Map {

// The rendered terrain is a triangle mesh over the height grid: every quad
// (x, z)..(x + 1, z + 1) is split along the (x + 1, z)-(x, z + 1) diagonal.
// Subdivided quads place their extra vertices on those same triangles, so this
// is the exact height of the drawn surface. Every consumer that plants
// something on the ground samples through here, so nothing floats or sinks by
// the difference between a bilinear patch and the triangles on screen.
[[nodiscard]] inline auto sample_triangulated_height(
    const float* heights, int width, int height, float gx, float gz) -> float {
  if (heights == nullptr || width <= 0 || height <= 0) {
    return 0.0F;
  }
  if (width == 1 || height == 1) {
    return heights[0];
  }
  float const cx = std::clamp(gx, 0.0F, static_cast<float>(width - 1));
  float const cz = std::clamp(gz, 0.0F, static_cast<float>(height - 1));
  int const x0 = std::min(static_cast<int>(std::floor(cx)), width - 2);
  int const z0 = std::min(static_cast<int>(std::floor(cz)), height - 2);
  float const tx = cx - static_cast<float>(x0);
  float const tz = cz - static_cast<float>(z0);
  auto at = [&](int x, int z) {
    return heights[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
                   static_cast<std::size_t>(x)];
  };
  float const h10 = at(x0 + 1, z0);
  float const h01 = at(x0, z0 + 1);
  if (tx + tz <= 1.0F) {
    float const h00 = at(x0, z0);
    return h00 + (h10 - h00) * tx + (h01 - h00) * tz;
  }
  float const h11 = at(x0 + 1, z0 + 1);
  return h11 + (h01 - h11) * (1.0F - tx) + (h10 - h11) * (1.0F - tz);
}

// Smoothed ground normal from central differences one tile apart, the same
// stencil the terrain mesh uses for its shading normals. Per-triangle normals
// would flip between neighbouring triangles, so anything tilted to the ground
// uses this to stay steady as it moves.
[[nodiscard]] inline auto
sample_smoothed_ground_normal(const float* heights,
                              int width,
                              int height,
                              float tile_size,
                              float gx,
                              float gz,
                              float half_step = 1.0F) -> QVector3D {
  float const ts = std::max(tile_size, 1.0e-4F);
  float const step = std::max(half_step, 1.0e-3F);
  float const left = sample_triangulated_height(heights, width, height, gx - step, gz);
  float const right = sample_triangulated_height(heights, width, height, gx + step, gz);
  float const back = sample_triangulated_height(heights, width, height, gx, gz - step);
  float const front = sample_triangulated_height(heights, width, height, gx, gz + step);
  float const run = 2.0F * step * ts;
  QVector3D normal(-(right - left) / run, 1.0F, -(front - back) / run);
  return normal.normalized();
}

// How far an upright object resting on a disc of `contact_radius` must sink so
// its downhill edge meets the ground: radius * tan(slope). Capped so a cliff
// sample cannot bury a prop whole.
[[nodiscard]] inline auto slope_bed_depth(const QVector3D& ground_normal,
                                          float contact_radius,
                                          float max_depth) -> float {
  float const ny = std::clamp(ground_normal.y(), 0.05F, 1.0F);
  float const tangent = std::sqrt(std::max(0.0F, 1.0F - ny * ny)) / ny;
  return std::clamp(std::max(contact_radius, 0.0F) * tangent, 0.0F, max_depth);
}

} // namespace Game::Map
