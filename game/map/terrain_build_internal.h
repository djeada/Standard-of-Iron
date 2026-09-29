#pragma once

#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

#include "terrain.h"

namespace Game::Map::terrain_build {

inline constexpr float k_deg_to_rad = std::numbers::pi_v<float> / 180.0F;

struct GridLayers {
  int width;
  int height;
  float tile_size;
  std::vector<float>& heights;
  std::vector<TerrainType>& types;
  std::vector<bool>& hill_entrances;
  std::vector<bool>& hill_walkable;
  std::vector<std::uint8_t>& fields;
  std::vector<HillEntranceCenterline>& entrance_centerlines;

  [[nodiscard]] auto index_at(int x, int z) const -> int { return z * width + x; }
  [[nodiscard]] auto in_bounds(int x, int z) const -> bool {
    return x >= 0 && x < width && z >= 0 && z < height;
  }
  [[nodiscard]] auto cell_count() const -> int { return width * height; }
  [[nodiscard]] auto half_width() const -> float { return width * 0.5F - 0.5F; }
  [[nodiscard]] auto half_height() const -> float { return height * 0.5F - 0.5F; }
};

struct BuildContext {
  GridLayers layers;
  const std::vector<TerrainFeature>& features;
  std::vector<float> base_heights;
  std::vector<float> erosion_strength;
  std::vector<std::uint8_t> erosion_protected;
};

struct FeatureFrame {
  float grid_center_x;
  float grid_center_z;
  float grid_radius;
};

[[nodiscard]] auto
make_build_context(GridLayers layers,
                   const std::vector<TerrainFeature>& features) -> BuildContext;

[[nodiscard]] auto frame_feature(const GridLayers& layers,
                                 const TerrainFeature& feature) -> FeatureFrame;

void stamp_mountain(BuildContext& ctx,
                    const TerrainFeature& feature,
                    const FeatureFrame& frame);

void stamp_hill(BuildContext& ctx,
                const TerrainFeature& feature,
                const FeatureFrame& frame);

void stamp_forest_or_river(BuildContext& ctx,
                           const TerrainFeature& feature,
                           const FeatureFrame& frame);

void stamp_flat_plateau(BuildContext& ctx,
                        const TerrainFeature& feature,
                        const FeatureFrame& frame);

void settle_erosion(BuildContext& ctx);

} // namespace Game::Map::terrain_build
