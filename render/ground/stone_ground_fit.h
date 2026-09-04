#pragma once

#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "game/map/scatter/ground_utils.h"

namespace Render::Ground {

inline auto sample_grid_height_bilinear(const std::vector<float>& heights,
                                        int width,
                                        int height,
                                        float gx,
                                        float gz) -> float {
  if (width < 2 || height < 2 ||
      heights.size() <
          static_cast<std::size_t>(width) * static_cast<std::size_t>(height)) {
    return 0.0F;
  }
  float const cx = std::clamp(gx, 0.0F, static_cast<float>(width - 1));
  float const cz = std::clamp(gz, 0.0F, static_cast<float>(height - 1));
  int const x0 = static_cast<int>(std::floor(cx));
  int const z0 = static_cast<int>(std::floor(cz));
  int const x1 = std::min(x0 + 1, width - 1);
  int const z1 = std::min(z0 + 1, height - 1);
  float const fx = cx - static_cast<float>(x0);
  float const fz = cz - static_cast<float>(z0);
  auto at = [&](int x, int z) {
    return heights[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
                   static_cast<std::size_t>(x)];
  };
  float const h0 = at(x0, z0) * (1.0F - fx) + at(x1, z0) * fx;
  float const h1 = at(x0, z1) * (1.0F - fx) + at(x1, z1) * fx;
  return h0 * (1.0F - fz) + h1 * fz;
}

inline auto sample_ground_normal(const std::vector<float>& heights,
                                 int width,
                                 int height,
                                 float tile_size,
                                 float gx,
                                 float gz) -> QVector3D {
  constexpr float k_half_step = 0.5F;
  constexpr float k_min_tile = 1.0e-4F;
  float const ts = std::max(tile_size, k_min_tile);
  float const left =
      sample_grid_height_bilinear(heights, width, height, gx - k_half_step, gz);
  float const right =
      sample_grid_height_bilinear(heights, width, height, gx + k_half_step, gz);
  float const back =
      sample_grid_height_bilinear(heights, width, height, gx, gz - k_half_step);
  float const front =
      sample_grid_height_bilinear(heights, width, height, gx, gz + k_half_step);
  QVector3D normal(-(right - left) / ts, 1.0F, -(front - back) / ts);
  if (normal.lengthSquared() < k_min_tile) {
    return {0.0F, 1.0F, 0.0F};
  }
  return normal.normalized();
}

inline auto
world_to_grid_coord(float world_coord, int grid_size, float tile_size) -> float {
  constexpr float k_min_tile = 1.0e-4F;
  return world_coord / std::max(tile_size, k_min_tile) +
         (static_cast<float>(grid_size) * 0.5F - 0.5F);
}

inline auto pack_stone_ground_fit(const QVector3D& ground_normal,
                                  float shape_seed,
                                  float render_scale,
                                  float sink_fraction) -> QVector4D {
  constexpr float k_max_tilt_component = 0.60F;
  QVector3D normal = ground_normal;
  if (normal.y() <= 0.0F || normal.lengthSquared() < 1.0e-6F) {
    normal = QVector3D(0.0F, 1.0F, 0.0F);
  }
  normal.normalize();
  float const nx = std::clamp(normal.x(), -k_max_tilt_component, k_max_tilt_component);
  float const nz = std::clamp(normal.z(), -k_max_tilt_component, k_max_tilt_component);
  float const seed = std::clamp(shape_seed, 0.0F, 0.999F);
  float const sink = std::max(0.0F, sink_fraction) * std::max(render_scale, 0.0F);
  return {nx, seed, nz, sink};
}

inline auto stone_sink_fraction(const QVector3D& ground_normal,
                                uint32_t& state) -> float {
  constexpr float k_base_sink_min = 0.07F;
  constexpr float k_base_sink_max = 0.20F;
  constexpr float k_slope_sink = 0.30F;
  constexpr float k_max_sink = 0.34F;
  float const slope = 1.0F - std::clamp(ground_normal.y(), 0.0F, 1.0F);
  float const sink =
      remap(rand_01(state), k_base_sink_min, k_base_sink_max) + slope * k_slope_sink;
  return std::min(sink, k_max_sink);
}

inline auto stone_instance_color(const QVector3D& rock_low,
                                 const QVector3D& rock_high,
                                 float dryness,
                                 float rockiness,
                                 uint32_t& state) -> QVector3D {
  constexpr float k_iron_stain_chance = 0.22F;
  constexpr float k_lichen_grey_chance = 0.14F;
  float const color_var = rand_01(state);
  QVector3D color = rock_low * (1.0F - color_var) + rock_high * color_var;

  QVector3D const earth_tint(0.36F, 0.32F, 0.27F);
  float const earth_mix =
      remap(rand_01(state), 0.08F + dryness * 0.08F, 0.28F + rockiness * 0.10F);
  color = color * (1.0F - earth_mix) + earth_tint * earth_mix;

  float const cast_roll = rand_01(state);
  if (cast_roll < k_iron_stain_chance) {
    QVector3D const iron_stain(0.47F, 0.36F, 0.26F);
    float const stain_mix = remap(rand_01(state), 0.22F, 0.42F);
    color = color * (1.0F - stain_mix) + iron_stain * stain_mix;
  } else if (cast_roll < k_iron_stain_chance + k_lichen_grey_chance) {
    QVector3D const lichen_grey(0.50F, 0.52F, 0.47F);
    float const grey_mix = remap(rand_01(state), 0.20F, 0.36F);
    color = color * (1.0F - grey_mix) + lichen_grey * grey_mix;
  } else {
    (void)rand_01(state);
  }

  color *= 0.82F + rockiness * 0.08F;
  return color;
}

} // namespace Render::Ground
