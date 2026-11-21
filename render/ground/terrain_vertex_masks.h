#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "game/map/terrain.h"

namespace Render::Ground {

inline constexpr float k_landform_levels = 16.0F;
inline constexpr float k_landform_mountain_stride = 32.0F;
inline constexpr float k_entry_mask_pack_scale = 0.5F;

struct LandformWeights {
  std::vector<float> hill;
  std::vector<float> mountain;
};

[[nodiscard]] inline auto
compute_landform_weights(const std::vector<Game::Map::TerrainType>& types,
                         int width,
                         int height) -> LandformWeights {
  LandformWeights weights;
  const auto count = static_cast<std::size_t>(std::max(width, 0)) *
                     static_cast<std::size_t>(std::max(height, 0));
  weights.hill.assign(count, 0.0F);
  weights.mountain.assign(count, 0.0F);
  if (count == 0 || types.size() < count) {
    return weights;
  }

  constexpr int k_kernel[3] = {1, 2, 1};
  for (int z = 0; z < height; ++z) {
    for (int x = 0; x < width; ++x) {
      float hill = 0.0F;
      float mountain = 0.0F;
      for (int dz = -1; dz <= 1; ++dz) {
        const int sz = std::clamp(z + dz, 0, height - 1);
        for (int dx = -1; dx <= 1; ++dx) {
          const int sx = std::clamp(x + dx, 0, width - 1);
          const auto weight = static_cast<float>(k_kernel[dx + 1] * k_kernel[dz + 1]);
          const auto type = types[static_cast<std::size_t>(sz * width + sx)];
          if (type == Game::Map::TerrainType::Hill) {
            hill += weight;
          } else if (type == Game::Map::TerrainType::Mountain) {
            mountain += weight;
          }
        }
      }
      const auto index = static_cast<std::size_t>(z * width + x);
      weights.hill[index] = hill / 16.0F;
      weights.mountain[index] = mountain / 16.0F;
    }
  }
  return weights;
}

[[nodiscard]] inline auto sample_grid_bilinear(const std::vector<float>& field,
                                               int width,
                                               int height,
                                               float gx,
                                               float gz) -> float {
  if (field.empty() || width <= 0 || height <= 0) {
    return 0.0F;
  }
  gx = std::clamp(gx, 0.0F, float(width - 1));
  gz = std::clamp(gz, 0.0F, float(height - 1));
  const int x0 = int(std::floor(gx));
  const int z0 = int(std::floor(gz));
  const int x1 = std::min(x0 + 1, width - 1);
  const int z1 = std::min(z0 + 1, height - 1);
  const float tx = gx - float(x0);
  const float tz = gz - float(z0);
  const auto at = [&](int x, int z) {
    return field[static_cast<std::size_t>(z * width + x)];
  };
  const float near_row = at(x0, z0) * (1.0F - tx) + at(x1, z0) * tx;
  const float far_row = at(x0, z1) * (1.0F - tx) + at(x1, z1) * tx;
  return near_row * (1.0F - tz) + far_row * tz;
}

[[nodiscard]] inline auto
pack_entry_and_landform(float entry_mask, float hill, float mountain) -> float {
  const float entry_part =
      std::clamp(entry_mask, 0.0F, 0.999F) * k_entry_mask_pack_scale;
  const float hill_level = std::round(std::clamp(hill, 0.0F, 1.0F) * k_landform_levels);
  const float mountain_level =
      std::round(std::clamp(mountain, 0.0F, 1.0F) * k_landform_levels);
  return entry_part + hill_level + mountain_level * k_landform_mountain_stride;
}

struct UnpackedVertexMask {
  float entry_mask;
  float hill;
  float mountain;
};

[[nodiscard]] inline auto
unpack_entry_and_landform(float packed) -> UnpackedVertexMask {
  const float mountain_level = std::floor(packed / k_landform_mountain_stride + 1e-4F);
  packed -= mountain_level * k_landform_mountain_stride;
  const float hill_level = std::floor(packed + 1e-4F);
  return {.entry_mask =
              std::clamp((packed - hill_level) / k_entry_mask_pack_scale, 0.0F, 1.0F),
          .hill = hill_level / k_landform_levels,
          .mountain = mountain_level / k_landform_levels};
}

} // namespace Render::Ground
