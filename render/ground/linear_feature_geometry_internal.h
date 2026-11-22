#pragma once

// Internal to the linear feature geometry: the height sampling and blending
// helpers that the ribbon, bridge and riverbank builders share.

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>
#include <utility>
#include <vector>

#include "game/map/bridge_geometry.h"
#include "game/map/river_ribbon.h"
#include "game/map/scatter/ground_utils.h"
#include "game/map/terrain_surface.h"
#include "render/ground/linear_feature_geometry.h"
#include "render/math/render_constants.h"

namespace Render::Ground::linear_feature_detail {

inline auto sample_height_clamped(const Game::Map::TerrainHeightMap& height_map,
                                  float world_x,
                                  float world_z) -> float {
  const auto& heights = height_map.get_height_data();
  const int grid_width = height_map.get_width();
  const int grid_height = height_map.get_height();
  const float tile_size = height_map.get_tile_size();

  if (heights.empty() || grid_width <= 0 || grid_height <= 0) {
    return 0.0F;
  }

  float const half_width = static_cast<float>(grid_width) * 0.5F - 0.5F;
  float const half_height = static_cast<float>(grid_height) * 0.5F - 0.5F;
  float gx = (world_x / tile_size) + half_width;
  float gz = (world_z / tile_size) + half_height;

  return Game::Map::sample_triangulated_height(
      heights.data(), grid_width, grid_height, gx, gz);
}

inline auto
sample_water_surface_height_clamped(const Game::Map::TerrainHeightMap& height_map,
                                    float world_x,
                                    float world_z) -> float {

  const float shoreline_blend = height_map.get_tile_size() * 1.25F;
  for (const auto& lake : height_map.get_lakes()) {
    if (Game::Map::point_in_lake(lake, world_x, world_z, shoreline_blend)) {
      return lake.center.y();
    }
  }
  return sample_height_clamped(height_map, world_x, world_z);
}

inline auto sample_height_envelope(const Game::Map::TerrainHeightMap& height_map,
                                   const QVector3D& center,
                                   const QVector3D& dir,
                                   const QVector3D& perpendicular,
                                   float longitudinal_radius,
                                   float lateral_radius,
                                   float tile_size) -> float {
  float const clamped_longitudinal = std::max(longitudinal_radius, 0.0F);
  float const clamped_lateral = std::max(lateral_radius, 0.0F);

  float const sample_spacing = std::max(tile_size * 0.25F, 0.1F);
  int const longitudinal_samples =
      clamped_longitudinal > 1e-4F
          ? std::max(2,
                     static_cast<int>(
                         std::ceil((clamped_longitudinal * 2.0F) / sample_spacing)) +
                         1)
          : 1;
  int const lateral_samples =
      clamped_lateral > 1e-4F
          ? std::max(
                2,
                static_cast<int>(std::ceil((clamped_lateral * 2.0F) / sample_spacing)) +
                    1)
          : 1;

  float max_height = sample_height_clamped(height_map, center.x(), center.z());
  for (int longitudinal_index = 0; longitudinal_index < longitudinal_samples;
       ++longitudinal_index) {
    float const longitudinal_t = longitudinal_samples > 1
                                     ? static_cast<float>(longitudinal_index) /
                                           static_cast<float>(longitudinal_samples - 1)
                                     : 0.5F;
    float const longitudinal_offset =
        -clamped_longitudinal + longitudinal_t * (clamped_longitudinal * 2.0F);

    for (int lateral_index = 0; lateral_index < lateral_samples; ++lateral_index) {
      float const lateral_t = lateral_samples > 1
                                  ? static_cast<float>(lateral_index) /
                                        static_cast<float>(lateral_samples - 1)
                                  : 0.5F;
      float const lateral_offset =
          -clamped_lateral + lateral_t * (clamped_lateral * 2.0F);

      QVector3D const sample_pos =
          center + dir * longitudinal_offset + perpendicular * lateral_offset;
      max_height =
          std::max(max_height,
                   sample_height_clamped(height_map, sample_pos.x(), sample_pos.z()));
    }
  }
  return max_height;
}

inline auto smoothstep01(float value) -> float {
  float const t = std::clamp(value, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

inline auto mixf(float a, float b, float t) -> float {
  return a + (b - a) * t;
}

} // namespace Render::Ground::linear_feature_detail
