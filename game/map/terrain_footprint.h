#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace Game::Map {

inline constexpr int k_campaign_landform_grid_threshold = 128;
inline constexpr float k_campaign_hill_width_scale = 1.18F;
inline constexpr float k_hill_min_slope_cells = 2.0F;
inline constexpr float k_hill_campaign_rotation_span_deg = 180.0F;
inline constexpr std::uint32_t k_hill_rotation_seed = 0x81E7U;

inline constexpr float k_mountain_major_scale = 1.38F;
inline constexpr float k_mountain_major_bonus_cells = 6.0F;
inline constexpr float k_mountain_minor_scale = 0.55F;
inline constexpr float k_mountain_min_minor_cells = 5.0F;
inline constexpr float k_mountain_min_authored_cells = 2.0F;

inline constexpr float k_hill_organic_spread = 0.49F;
inline constexpr float k_hill_compact_organic_spread = 0.25F;
inline constexpr float k_mountain_organic_spread = 0.37F;

inline constexpr float k_campaign_hill_height_scale = 2.80F;
inline constexpr float k_hill_footprint_height_ratio = 0.18F;
inline constexpr float k_hill_min_elevation_cells = 0.25F;
inline constexpr float k_campaign_slope_run_min_cells = 7.0F;
inline constexpr float k_campaign_slope_run_scale = 4.2F;
inline constexpr float k_slope_run_min_cells = 3.25F;
inline constexpr float k_slope_run_scale = 3.15F;
inline constexpr float k_campaign_min_crown_fraction = 0.42F;
inline constexpr float k_min_crown_fraction = 0.68F;
inline constexpr float k_campaign_max_slope_fraction = 0.62F;
inline constexpr float k_max_slope_fraction = 0.46F;
inline constexpr float k_hill_min_crown_cells = 1.5F;

inline constexpr float k_hill_entry_ramp_cells = 3.0F;
inline constexpr float k_campaign_hill_entry_ramp_cells = 7.25F;
inline constexpr float k_hill_entry_min_half_width_cells = 1.5F;
inline constexpr float k_hill_entry_crown_fraction = 0.62F;
inline constexpr float k_hill_entry_base_width_scale = 1.72F;
inline constexpr float k_hill_entry_mouth_flare_strength = 0.38F;

struct FootprintCells {
  float half_width = 1.0F;
  float half_depth = 1.0F;
  float width_cells = 2.0F;
  float depth_cells = 2.0F;
  float rotation_deg = 0.0F;
  float organic_spread = 0.0F;
};

struct HillFootprintInput {
  float width = 0.0F;
  float depth = 0.0F;
  float radius = 0.0F;
  float rotation_deg = 0.0F;
  float tile_size = 1.0F;
  float grid_center_x = 0.0F;
  float grid_center_z = 0.0F;
  bool campaign_scale = false;
};

struct MountainFootprintInput {
  float width = 0.0F;
  float depth = 0.0F;
  float radius = 0.0F;
  float rotation_deg = 0.0F;
  float tile_size = 1.0F;
};

inline auto footprint_hash_coords(int x, int z, std::uint32_t seed) -> std::uint32_t {
  std::uint32_t const ux = static_cast<std::uint32_t>(x) * 73856093U;
  std::uint32_t const uz = static_cast<std::uint32_t>(z) * 19349663U;
  std::uint32_t const s = seed * 83492791U + 0x9e3779b9U;
  return ux ^ uz ^ s;
}

inline auto footprint_hash_to_float01(std::uint32_t hash) -> float {
  hash ^= hash >> 17;
  hash *= 0xed5ad4bbU;
  hash ^= hash >> 11;
  hash *= 0xac4c1b51U;
  hash ^= hash >> 15;
  hash *= 0x31848babU;
  hash ^= hash >> 14;
  return static_cast<float>(hash & 0x00FFFFFFU) / static_cast<float>(0x01000000);
}

[[nodiscard]] inline auto is_campaign_landform_scale(int grid_width,
                                                     int grid_height) -> bool {
  return std::max(grid_width, grid_height) >= k_campaign_landform_grid_threshold;
}

[[nodiscard]] inline auto authored_hill_width(float width, float radius) -> float {
  return width > 0.0F ? width : radius * 2.0F;
}

[[nodiscard]] inline auto authored_hill_depth(float depth, float radius) -> float {
  return depth > 0.0F ? depth : radius * 2.0F;
}

[[nodiscard]] inline auto
hill_radius_is_authoritative(float width, float depth, float radius) -> bool {
  return radius > 0.0F && std::abs(width - radius * 2.0F) < 0.01F &&
         std::abs(depth - radius * 2.0F) < 0.01F;
}

[[nodiscard]] inline auto
hill_footprint_cells(const HillFootprintInput& input) -> FootprintCells {
  const float tile = std::max(input.tile_size, 0.0001F);
  const float full_width = authored_hill_width(input.width, input.radius);
  const float full_depth = authored_hill_depth(input.depth, input.radius);

  float width_cells = std::max(full_width / tile, 1.0F);
  const float depth_cells = std::max(full_depth / tile, 1.0F);

  FootprintCells footprint;
  footprint.rotation_deg = input.rotation_deg;
  footprint.organic_spread =
      input.campaign_scale ? k_hill_organic_spread : k_hill_compact_organic_spread;

  if (input.campaign_scale &&
      hill_radius_is_authoritative(full_width, full_depth, input.radius)) {
    width_cells *= k_campaign_hill_width_scale;
    footprint.rotation_deg += footprint_hash_to_float01(footprint_hash_coords(
                                  static_cast<int>(std::lround(input.grid_center_x)),
                                  static_cast<int>(std::lround(input.grid_center_z)),
                                  k_hill_rotation_seed)) *
                              k_hill_campaign_rotation_span_deg;
  }

  footprint.width_cells = width_cells;
  footprint.depth_cells = depth_cells;
  footprint.half_width = std::max(k_hill_min_slope_cells, width_cells * 0.5F);
  footprint.half_depth = std::max(k_hill_min_slope_cells, depth_cells * 0.5F);
  return footprint;
}

[[nodiscard]] inline auto mountain_major_radius_cells(float radius_cells) -> float {
  return std::max(radius_cells * k_mountain_major_scale,
                  radius_cells + k_mountain_major_bonus_cells);
}

[[nodiscard]] inline auto mountain_minor_radius_cells(float radius_cells) -> float {
  return std::max(radius_cells * k_mountain_minor_scale, k_mountain_min_minor_cells);
}

[[nodiscard]] inline auto
mountain_footprint_cells(const MountainFootprintInput& input) -> FootprintCells {
  const float tile = std::max(input.tile_size, 0.0001F);
  const float radius_cells = std::max(input.radius / tile, 1.0F);
  const bool has_authored_extents = input.width > 0.0F && input.depth > 0.0F;

  FootprintCells footprint;
  footprint.rotation_deg = input.rotation_deg;
  footprint.organic_spread = k_mountain_organic_spread;
  footprint.half_width =
      has_authored_extents
          ? std::max(input.width * 0.5F / tile, k_mountain_min_authored_cells)
          : mountain_major_radius_cells(radius_cells);
  footprint.half_depth =
      has_authored_extents
          ? std::max(input.depth * 0.5F / tile, k_mountain_min_authored_cells)
          : mountain_minor_radius_cells(radius_cells);
  footprint.width_cells = footprint.half_width * 2.0F;
  footprint.depth_cells = footprint.half_depth * 2.0F;
  return footprint;
}

struct HillCrownCells {
  float half_width = 1.5F;
  float half_depth = 1.5F;
  float height = 0.0F;
};

[[nodiscard]] inline auto hill_crown_cells(const FootprintCells& footprint,
                                           float authored_height,
                                           float tile_size,
                                           bool campaign_scale) -> HillCrownCells {
  const float tile = std::max(tile_size, 0.0001F);
  const float scaled_height =
      authored_height * (campaign_scale ? k_campaign_hill_height_scale : 1.0F);
  const float footprint_height =
      std::min(footprint.width_cells, footprint.depth_cells) * tile *
      k_hill_footprint_height_ratio;

  HillCrownCells crown;
  crown.height =
      campaign_scale ? std::max(scaled_height, footprint_height) : scaled_height;

  const float elevation_cells =
      std::max(crown.height / tile, k_hill_min_elevation_cells);
  const float slope_run =
      campaign_scale
          ? std::max(k_campaign_slope_run_min_cells,
                     elevation_cells * k_campaign_slope_run_scale)
          : std::max(k_slope_run_min_cells, elevation_cells * k_slope_run_scale);
  const float minimum_crown_fraction =
      campaign_scale ? k_campaign_min_crown_fraction : k_min_crown_fraction;
  const float maximum_slope_fraction =
      campaign_scale ? k_campaign_max_slope_fraction : k_max_slope_fraction;

  const auto crown_extent = [&](float slope_extent) {
    return std::max(
        {k_hill_min_crown_cells,
         slope_extent * minimum_crown_fraction,
         slope_extent - std::min(slope_extent * maximum_slope_fraction, slope_run)});
  };
  crown.half_width = crown_extent(footprint.half_width);
  crown.half_depth = crown_extent(footprint.half_depth);
  return crown;
}

[[nodiscard]] inline auto hill_entry_half_width_cells(const HillCrownCells& crown,
                                                      float entrance_radius_cells,
                                                      bool campaign_scale) -> float {
  const float authored =
      (campaign_scale ? k_campaign_hill_entry_ramp_cells : k_hill_entry_ramp_cells) +
      std::max(entrance_radius_cells, 0.0F);
  const float crown_limit =
      std::min(crown.half_width, crown.half_depth) * k_hill_entry_crown_fraction;
  return std::max(k_hill_entry_min_half_width_cells, std::min(authored, crown_limit));
}

[[nodiscard]] inline auto
hill_entry_mouth_half_width_cells(float entry_half_width_cells) -> float {
  return entry_half_width_cells * k_hill_entry_base_width_scale *
         (1.0F + k_hill_entry_mouth_flare_strength);
}

} // namespace Game::Map
