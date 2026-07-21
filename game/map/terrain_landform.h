#pragma once

#include <cstdint>
#include <vector>

namespace Game::Map::Landform {

struct HillConfig {
  float outer_radius_x = 8.0F;
  float outer_radius_z = 8.0F;
  float crown_radius_x = 5.0F;
  float crown_radius_z = 5.0F;
  float height = 2.0F;
  float phase = 0.0F;
  std::uint32_t seed = 1337U;
  bool rounded_crown = true;
};

struct HillSample {
  float outer_distance = 2.0F;
  float crown_distance = 2.0F;
  float elevation_fraction = 0.0F;
};

[[nodiscard]] auto
sample_hill(float local_x, float local_z, const HillConfig& config) -> HillSample;

struct MountainConfig {
  float ridge_radius = 18.0F;
  float slope_radius = 6.0F;
  float phase = 0.0F;
  std::uint32_t seed = 1337U;
};

struct MountainSample {
  float footprint = 0.0F;
  float elevation_fraction = 0.0F;
};

[[nodiscard]] auto sample_mountain(float local_x,
                                   float local_z,
                                   const MountainConfig& config) -> MountainSample;

struct ErosionConfig {
  int iterations = 3;
  float talus_height_per_cell = 0.62F;
  float thermal_rate = 0.04F;
  float channel_strength = 0.025F;
};

// Applies a deterministic, constrained erosion pass to authored uplift. A
// zero strength cell is outside a landform; protected cells are tactical
// crowns, settlements, and hill entries that erosion must not deform.
void apply_constrained_erosion(std::vector<float>& heights,
                               const std::vector<float>& strength,
                               const std::vector<std::uint8_t>& protected_cells,
                               int width,
                               int height,
                               float tile_size,
                               const ErosionConfig& config = {});

} // namespace Game::Map::Landform
