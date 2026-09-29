#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "forest_outline.h"
#include "terrain.h"
#include "terrain_build_internal.h"
#include "terrain_footprint.h"
#include "terrain_landform.h"
#include "terrain_surface.h"
#include "terrain_value_noise.h"

namespace Game::Map {

void TerrainHeightMap::build_from_features(
    const std::vector<TerrainFeature>& features) {
  terrain_build::GridLayers layers{m_width,
                                   m_height,
                                   m_tile_size,
                                   m_heights,
                                   m_terrain_types,
                                   m_hill_entrances,
                                   m_hill_walkable,
                                   m_fields,
                                   m_hill_entrance_centerlines};
  terrain_build::BuildContext ctx = terrain_build::make_build_context(layers, features);

  for (const auto& feature : features) {
    const terrain_build::FeatureFrame frame =
        terrain_build::frame_feature(layers, feature);
    switch (feature.type) {
    case TerrainType::Mountain:
      terrain_build::stamp_mountain(ctx, feature, frame);
      break;
    case TerrainType::Hill:
      terrain_build::stamp_hill(ctx, feature, frame);
      break;
    case TerrainType::Forest:
    case TerrainType::River:
      terrain_build::stamp_forest_or_river(ctx, feature, frame);
      break;
    default:
      terrain_build::stamp_flat_plateau(ctx, feature, frame);
      break;
    }
  }

  terrain_build::settle_erosion(ctx);
}

namespace terrain_build {

using terrain_value_noise::hash_coords;
using terrain_value_noise::hash_to_float01;

auto make_build_context(GridLayers layers,
                        const std::vector<TerrainFeature>& features) -> BuildContext {
  const int map_cell_count = layers.cell_count();
  BuildContext ctx{
      layers,
      features,
      layers.heights,
      std::vector<float>(static_cast<std::size_t>(map_cell_count), 0.0F),
      std::vector<std::uint8_t>(static_cast<std::size_t>(map_cell_count), 0)};
  std::fill(layers.types.begin(), layers.types.end(), TerrainType::Flat);
  std::fill(layers.hill_entrances.begin(), layers.hill_entrances.end(), false);
  std::fill(layers.hill_walkable.begin(), layers.hill_walkable.end(), false);
  layers.fields.assign(static_cast<std::size_t>(map_cell_count), 0);
  layers.entrance_centerlines.clear();
  return ctx;
}

auto frame_feature(const GridLayers& layers,
                   const TerrainFeature& feature) -> FeatureFrame {
  return {(feature.center_x / layers.tile_size) + layers.half_width(),
          (feature.center_z / layers.tile_size) + layers.half_height(),
          std::max(feature.radius / layers.tile_size, 1.0F)};
}

void stamp_mountain(BuildContext& ctx,
                    const TerrainFeature& feature,
                    const FeatureFrame& frame) {
  GridLayers& L = ctx.layers;
  const auto& features = ctx.features;
  const auto& base_heights = ctx.base_heights;
  auto& erosion_strength = ctx.erosion_strength;
  const float grid_center_x = frame.grid_center_x;
  const float grid_center_z = frame.grid_center_z;

  const bool has_authored_extents = feature.width > 0.0F && feature.depth > 0.0F;
  const bool campaign_landform_scale =
      Game::Map::is_campaign_landform_scale(L.width, L.height);

  const float mountain_height =
      feature.height * (campaign_landform_scale ? 1.90F : 1.0F);
  const auto footprint =
      Game::Map::mountain_footprint_cells({.width = feature.width,
                                           .depth = feature.depth,
                                           .radius = feature.radius,
                                           .rotation_deg = feature.rotation_deg,
                                           .tile_size = L.tile_size});
  const float major_radius = footprint.half_width;
  const float minor_radius = footprint.half_depth;
  const float bound = std::max(major_radius, minor_radius) + 2.0F;
  const int min_x = std::max(0, int(std::floor(grid_center_x - bound)));
  const int max_x = std::min(L.width - 1, int(std::ceil(grid_center_x + bound)));
  const int min_z = std::max(0, int(std::floor(grid_center_z - bound)));
  const int max_z = std::min(L.height - 1, int(std::ceil(grid_center_z + bound)));

  float organic_rotation = 0.0F;
  if (!has_authored_extents) {

    float nearest_distance_sq = std::numeric_limits<float>::max();
    float nearest_angle = 0.0F;
    for (const auto& candidate : features) {
      if (&candidate == &feature || candidate.type != TerrainType::Mountain) {
        continue;
      }
      const float dx = candidate.center_x - feature.center_x;
      const float dz = candidate.center_z - feature.center_z;
      const float distance_sq = dx * dx + dz * dz;
      if (distance_sq < nearest_distance_sq) {
        nearest_distance_sq = distance_sq;
        nearest_angle = std::atan2(dz, dx) / k_deg_to_rad;
      }
    }
    const float direction_jitter =
        (hash_to_float01(hash_coords(
             int(std::round(grid_center_x)), int(std::round(grid_center_z)), 0x5A17U)) -
         0.5F) *
        16.0F;
    organic_rotation = std::isfinite(nearest_distance_sq)
                           ? nearest_angle + direction_jitter
                           : direction_jitter;
  }
  const float angle_rad = (feature.rotation_deg + organic_rotation) * k_deg_to_rad;
  const float cos_a = std::cos(angle_rad);
  const float sin_a = std::sin(angle_rad);
  const float feature_phase =
      grid_center_x * 0.071F + grid_center_z * 0.113F + mountain_height * 0.19F;
  const auto mountain_seed =
      0xA17E35D9U ^ static_cast<std::uint32_t>(
                        std::abs(grid_center_x * 31.0F + grid_center_z * 17.0F));
  const Landform::MountainConfig mountain_config{
      .ridge_radius = major_radius,
      .slope_radius = minor_radius,
      .phase = feature_phase,
      .seed = mountain_seed,
  };

  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      const float local_x = float(x) - grid_center_x;
      const float local_z = float(z) - grid_center_z;

      const float rotated_x = local_x * cos_a + local_z * sin_a;
      const float rotated_z = -local_x * sin_a + local_z * cos_a;
      const auto landform =
          Landform::sample_mountain(rotated_x, rotated_z, mountain_config);
      const float feature_height = mountain_height * landform.elevation_fraction;

      if (feature_height > 0.01F) {
        int const idx = L.index_at(x, z);
        float const height = base_heights[idx] + feature_height;
        if (height > L.heights[idx]) {
          L.heights[idx] = height;
          L.types[idx] = TerrainType::Mountain;
          erosion_strength[idx] = std::max(erosion_strength[idx], 1.0F);
        }
      }
    }
  }
}

void stamp_forest_or_river(BuildContext& ctx,
                           const TerrainFeature& feature,
                           const FeatureFrame& frame) {
  GridLayers& L = ctx.layers;
  const float grid_center_x = frame.grid_center_x;
  const float grid_center_z = frame.grid_center_z;
  const float grid_radius = frame.grid_radius;
  const bool lobed =
      feature.type == TerrainType::Forest && feature.outline_seed >= 0.0F;
  const float reach = lobed ? k_forest_outline_reach : 1.0F;
  const float half_width =
      (feature.width > 0.0F ? feature.width * 0.5F / L.tile_size : grid_radius) * reach;
  const float half_depth =
      (feature.depth > 0.0F ? feature.depth * 0.5F / L.tile_size : grid_radius) * reach;
  const int min_x = std::max(0, int(std::floor(grid_center_x - half_width - 1.0F)));
  const int max_x =
      std::min(L.width - 1, int(std::ceil(grid_center_x + half_width + 1.0F)));
  const int min_z = std::max(0, int(std::floor(grid_center_z - half_depth - 1.0F)));
  const int max_z =
      std::min(L.height - 1, int(std::ceil(grid_center_z + half_depth + 1.0F)));

  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      const float normalized_x =
          (float(x) - grid_center_x) / std::max(half_width, 0.0001F);
      const float normalized_z =
          (float(z) - grid_center_z) / std::max(half_depth, 0.0001F);
      const float edge = lobed ? forest_outline_scale(feature.outline_seed,
                                                      float(x) - grid_center_x,
                                                      float(z) - grid_center_z) /
                                     reach
                               : 1.0F;
      if (normalized_x * normalized_x + normalized_z * normalized_z > edge * edge) {
        continue;
      }

      const int idx = L.index_at(x, z);
      if (feature.type == TerrainType::River) {
        L.heights[idx] = 0.0F;
        L.types[idx] = TerrainType::River;
      } else if (L.types[idx] == TerrainType::Flat && L.fields[idx] == 0) {
        L.types[idx] = TerrainType::Forest;
      }
    }
  }
}

void stamp_flat_plateau(BuildContext& ctx,
                        const TerrainFeature& feature,
                        const FeatureFrame& frame) {
  GridLayers& L = ctx.layers;
  auto& erosion_strength = ctx.erosion_strength;
  auto& erosion_protected = ctx.erosion_protected;
  const float grid_center_x = frame.grid_center_x;
  const float grid_center_z = frame.grid_center_z;
  const float grid_radius = frame.grid_radius;
  const float half_width =
      feature.width > 0.0F ? feature.width * 0.5F / L.tile_size : grid_radius;
  const float half_depth =
      feature.depth > 0.0F ? feature.depth * 0.5F / L.tile_size : grid_radius;
  const float bound = std::max(half_width, half_depth) + 1.0F;
  const int min_x = std::max(0, int(std::floor(grid_center_x - bound)));
  const int max_x = std::min(L.width - 1, int(std::ceil(grid_center_x + bound)));
  const int min_z = std::max(0, int(std::floor(grid_center_z - bound)));
  const int max_z = std::min(L.height - 1, int(std::ceil(grid_center_z + bound)));
  const float angle_rad = feature.rotation_deg * k_deg_to_rad;
  const float cos_a = std::cos(angle_rad);
  const float sin_a = std::sin(angle_rad);

  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      const float local_x = float(x) - grid_center_x;
      const float local_z = float(z) - grid_center_z;
      const float rotated_x = local_x * cos_a + local_z * sin_a;
      const float rotated_z = -local_x * sin_a + local_z * cos_a;
      const float normalized_x = rotated_x / std::max(half_width, 0.0001F);
      const float normalized_z = rotated_z / std::max(half_depth, 0.0001F);
      const float normalized_distance =
          std::sqrt(normalized_x * normalized_x + normalized_z * normalized_z);
      if (normalized_distance > 1.0F) {
        continue;
      }

      int const idx = L.index_at(x, z);

      constexpr float k_default_taper = 0.20F;
      const float taper = feature.taper > 0.0F ? std::clamp(feature.taper, 0.01F, 1.0F)
                                               : k_default_taper;
      const float feather =
          std::clamp((1.0F - normalized_distance) / taper, 0.0F, 1.0F);
      const float blend = feather * feather * (3.0F - 2.0F * feather);
      L.heights[idx] = feature.raise_only
                           ? std::max(L.heights[idx], feature.height * blend)
                           : L.heights[idx] * (1.0F - blend) + feature.height * blend;
      if (blend >= 0.5F) {
        L.types[idx] = TerrainType::Flat;
        L.hill_entrances[idx] = false;
        L.hill_walkable[idx] = false;
        erosion_strength[idx] = 0.0F;
        erosion_protected[idx] = 1;
      }
      if (feature.fields && feather >= 1.0F) {
        L.fields[idx] = 1;
      }
    }
  }
}

void settle_erosion(BuildContext& ctx) {
  GridLayers& L = ctx.layers;
  const int map_cell_count = L.cell_count();
  for (int idx = 0; idx < map_cell_count; ++idx) {
    if (L.hill_walkable[idx]) {
      ctx.erosion_protected[idx] = 1;
    }
    if (L.types[idx] != TerrainType::Flat) {
      L.fields[idx] = 0;
    }
  }
  if (std::max(L.width, L.height) >= 128) {
    Landform::apply_constrained_erosion(L.heights,
                                        ctx.erosion_strength,
                                        ctx.erosion_protected,
                                        L.width,
                                        L.height,
                                        L.tile_size);
  }
}

} // namespace terrain_build

} // namespace Game::Map
