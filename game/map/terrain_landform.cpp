#include "terrain_landform.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <vector>

#include "terrain_noise.h"

namespace {

using Game::Map::fbm_noise;
using Game::Map::value_noise;

auto smoothstep01(float value) -> float {
  value = std::clamp(value, 0.0F, 1.0F);
  return value * value * (3.0F - 2.0F * value);
}

auto signed_fbm(float x, float z, std::uint32_t seed, int octaves = 4) -> float {
  return fbm_noise(x, z, seed, octaves) * 2.0F - 1.0F;
}

auto organic_ellipse(float x,
                     float z,
                     float radius_x,
                     float radius_z,
                     float phase,
                     float roughness,
                     std::uint32_t seed) -> float {
  radius_x = std::max(radius_x, 0.001F);
  radius_z = std::max(radius_z, 0.001F);

  const float warp_scale = 1.0F / std::max(std::min(radius_x, radius_z), 1.0F);
  const float warp_x =
      signed_fbm(
          x * warp_scale * 0.72F, z * warp_scale * 0.72F, seed ^ 0xB71D4E29U, 3) *
      radius_x * roughness * 0.20F;
  const float warp_z = signed_fbm(x * warp_scale * 0.72F + 17.3F,
                                  z * warp_scale * 0.72F - 9.1F,
                                  seed ^ 0x5C93A167U,
                                  3) *
                       radius_z * roughness * 0.20F;

  const float nx = (x + warp_x) / radius_x;
  const float nz = (z + warp_z) / radius_z;
  const float angle = std::atan2(nz, nx);
  const float boundary =
      std::clamp(1.0F + std::sin(angle * 2.0F + phase) * roughness * 0.35F +
                     std::sin(angle * 3.0F - phase * 1.31F) * roughness * 0.28F +
                     std::sin(angle * 5.0F + phase * 0.73F) * roughness * 0.20F +
                     signed_fbm(std::cos(angle) * 1.7F + phase,
                                std::sin(angle) * 1.7F - phase,
                                seed ^ 0x91E10DA5U,
                                3) *
                         roughness * 0.17F,
                 1.0F - roughness,
                 1.0F + roughness);
  return std::hypot(nx, nz) / boundary;
}

auto smooth_union_distance(float a, float b, float blend) -> float {
  const float width = std::max(blend, 0.001F);
  const float h = std::clamp(0.5F + 0.5F * (b - a) / width, 0.0F, 1.0F);
  return b * (1.0F - h) + a * h - width * h * (1.0F - h);
}

} // namespace

namespace Game::Map::Landform {

auto sample_hill(float local_x, float local_z, const HillConfig& config) -> HillSample {
  const float min_radius =
      std::max(std::min(config.outer_radius_x, config.outer_radius_z), 1.0F);
  const float scale = 1.0F / min_radius;
  const float domain_x = local_x * scale;
  const float domain_z = local_z * scale;
  const float warp_strength = config.rounded_crown ? 0.085F : 0.055F;
  const float warp_x =
      signed_fbm(domain_x * 0.75F, domain_z * 0.75F, config.seed ^ 0x243F6A88U, 4) *
      config.outer_radius_x * warp_strength;
  const float warp_z = signed_fbm(domain_x * 0.75F + 11.0F,
                                  domain_z * 0.75F - 7.0F,
                                  config.seed ^ 0x85A308D3U,
                                  4) *
                       config.outer_radius_z * warp_strength;
  const float x = local_x + warp_x;
  const float z = local_z + warp_z;

  float outer_distance = organic_ellipse(x,
                                         z,
                                         config.outer_radius_x,
                                         config.outer_radius_z,
                                         config.phase,
                                         config.rounded_crown ? 0.34F : 0.16F,
                                         config.seed);
  const float crown_shift_x =
      signed_fbm(config.phase, 0.37F, config.seed ^ 0x27D4EB2FU, 3) *
      std::max(config.outer_radius_x - config.crown_radius_x, 0.0F) * 0.22F;
  const float crown_shift_z =
      signed_fbm(-0.61F, config.phase, config.seed ^ 0x165667B1U, 3) *
      std::max(config.outer_radius_z - config.crown_radius_z, 0.0F) * 0.22F;
  float crown_distance = organic_ellipse(x - crown_shift_x,
                                         z - crown_shift_z,
                                         config.crown_radius_x,
                                         config.crown_radius_z,
                                         config.phase + 0.10F,
                                         config.rounded_crown ? 0.21F : 0.075F,
                                         config.seed ^ 0x13198A2EU);
  if (!config.rounded_crown) {
    // Small tactical/test hills historically guarantee the complete authored
    // ellipse as usable crown. Organic edge noise may extend that crown, but
    // must not bite into it and turn a broad plateau into a narrow summit.
    const float authored_crown_distance =
        std::hypot(local_x / std::max(config.crown_radius_x, 0.001F),
                   local_z / std::max(config.crown_radius_z, 0.001F));
    crown_distance = std::min(crown_distance, authored_crown_distance);
  }
  if (config.rounded_crown) {
    // A low, overlapping uplift lobe makes campaign hills into irregular
    // ridges and mesas. Using a smooth distance union keeps one continuous
    // slope and summit for pathing, unlike adding a second visual-only mound.
    const float side = signed_fbm(config.phase,
                                  -0.83F,
                                  config.seed ^ 0x9E3779B9U,
                                  3) >= 0.0F
                           ? 1.0F
                           : -1.0F;
    const float lobe_x = side * config.outer_radius_x * 0.20F;
    const float lobe_z = signed_fbm(-1.17F,
                                    config.phase,
                                    config.seed ^ 0xBB67AE85U,
                                    3) *
                         config.outer_radius_z * 0.12F;
    const float outer_lobe = organic_ellipse(x - lobe_x,
                                             z - lobe_z,
                                             config.outer_radius_x * 0.79F,
                                             config.outer_radius_z * 0.82F,
                                             config.phase - 0.37F,
                                             0.30F,
                                             config.seed ^ 0x3C6EF372U);
    const float crown_lobe = organic_ellipse(x - lobe_x * 0.72F - crown_shift_x,
                                             z - lobe_z * 0.72F - crown_shift_z,
                                             config.crown_radius_x * 0.82F,
                                             config.crown_radius_z * 0.84F,
                                             config.phase - 0.23F,
                                             0.18F,
                                             config.seed ^ 0xA54FF53AU);
    outer_distance = smooth_union_distance(outer_distance, outer_lobe, 0.11F);
    crown_distance = smooth_union_distance(crown_distance, crown_lobe, 0.09F);
  }
  if (outer_distance > 1.0F) {
    return {.outer_distance = outer_distance, .crown_distance = crown_distance};
  }

  // Parameterise the slope between the independently organic outer and crown
  // contours. This gives each lobe its own real slope run instead of wrapping
  // a circular height profile around a non-circular silhouette.
  const float slope_band = std::max(crown_distance - outer_distance, 0.08F);
  const float slope_position =
      std::clamp((1.0F - outer_distance) / slope_band, 0.0F, 1.0F);
  const float lower_slope = std::pow(slope_position, 1.28F);
  const float upper_slope = 1.0F - std::pow(1.0F - slope_position, 1.38F);
  const float shoulder_blend = smoothstep01((slope_position - 0.42F) / 0.36F);
  float elevation =
      (lower_slope * (1.0F - shoulder_blend) + upper_slope * shoulder_blend) * 0.94F;

  const float angle = std::atan2(z / std::max(config.outer_radius_z, 0.001F),
                                 x / std::max(config.outer_radius_x, 0.001F));
  const float mid_slope = 4.0F * elevation * (1.0F - elevation);
  const float angular_warp =
      signed_fbm(domain_x * 1.4F, domain_z * 1.4F, config.seed ^ 0x03707344U, 3) *
      1.25F;
  const float drainage_a = std::pow(
      0.5F + 0.5F * std::cos(angle * 7.0F + angular_warp + config.phase), 14.0F);
  const float drainage_b = std::pow(
      0.5F + 0.5F * std::cos(angle * 11.0F - angular_warp * 0.7F - config.phase),
      20.0F);
  const float drainage = std::max(drainage_a, drainage_b * 0.72F);
  elevation *= 1.0F - drainage * mid_slope * 0.11F;

  // Small, slope-following erosion breaks up the profile without roughening
  // the tactical summit or turning the silhouette into high-frequency noise.
  const float weathering = signed_fbm(domain_x * 3.1F + config.phase,
                                      domain_z * 3.1F - config.phase,
                                      config.seed ^ 0xC2B2AE35U,
                                      4);
  elevation += weathering * mid_slope * 0.022F;

  const float toe = smoothstep01((elevation - 0.04F) / 0.18F) *
                    (1.0F - smoothstep01((elevation - 0.24F) / 0.18F));
  const float sediment = (0.55F + 0.45F * value_noise(domain_x * 4.0F,
                                                      domain_z * 4.0F,
                                                      config.seed ^ 0xA4093822U)) *
                         toe * 0.055F;
  elevation = std::clamp(elevation + sediment, 0.0F, 1.0F);
  if (crown_distance <= 1.0F) {
    // Preserve a useful flat summit, but round its outer shoulder into the
    // natural slope. The previous full-radius flat crown was the visible disc.
    const float summit_core = smoothstep01((1.0F - crown_distance) / 0.22F);
    elevation = config.rounded_crown ? 0.94F + 0.06F * summit_core : 1.0F;
  }
  return {.outer_distance = outer_distance,
          .crown_distance = crown_distance,
          .elevation_fraction = elevation};
}

auto sample_mountain(float local_x,
                     float local_z,
                     const MountainConfig& config) -> MountainSample {
  const float rx = std::max(config.ridge_radius, 1.0F);
  const float rz = std::max(config.slope_radius, 1.0F);
  const float nx = local_x / rx;
  const float nz = local_z / rz;
  const float outer_distance = organic_ellipse(
      local_x, local_z, rx, rz, config.phase, 0.31F, config.seed ^ 0x4F1BBCDCU);
  if (outer_distance >= 1.0F) {
    return {};
  }

  // One continuous uplift field. Longitudinal taper and a wandering crest
  // create a range rather than a radial cone, while the low foothill envelope
  // keeps overlapping authored features seamless.
  const float envelope_position = std::max(1.0F - outer_distance, 0.0F);
  const float edge_fade = smoothstep01(envelope_position / 0.16F);
  const float centerline =
      std::sin(nx * 3.1F + config.phase) * 0.10F +
      signed_fbm(nx * 1.7F + config.phase,
                 config.phase * 0.31F,
                 config.seed ^ 0xD1B54A35U,
                 3) *
          0.075F;
  const float cross_distance = std::abs(nz - centerline);
  const float longitudinal_taper =
      std::pow(std::max(1.0F - std::abs(nx), 0.0F), 0.30F);
  const float main_fold = std::pow(
      std::max(1.0F - cross_distance / 0.82F, 0.0F), 1.18F);
  const float branch_centerline =
      -0.34F * nx + std::sin(nx * 4.6F - config.phase) * 0.10F;
  const float branch_window = smoothstep01((0.72F - std::abs(nx + 0.10F)) / 0.38F);
  const float branch_fold =
      std::pow(std::max(1.0F - std::abs(nz - branch_centerline) / 0.48F, 0.0F), 1.15F) *
      branch_window;
  const float broad_noise = signed_fbm(
      nx * 2.6F + config.phase, nz * 2.6F - config.phase, config.seed ^ 0xB7E15162U, 5);
  const float fine_noise = signed_fbm(
      nx * 7.4F - config.phase, nz * 7.4F + config.phase, config.seed ^ 0x94D049BBU, 4);
  const float drainage_ridge =
      1.0F - std::abs(signed_fbm(nx * 5.2F + broad_noise * 0.55F,
                                 nz * 5.2F - broad_noise * 0.55F,
                                 config.seed ^ 0x369DEA0FU,
                                 4));
  const float gully = std::pow(std::clamp(drainage_ridge, 0.0F, 1.0F), 8.0F) *
                      main_fold * edge_fade;
  const float peak_chain = std::clamp(0.74F + signed_fbm(nx * 3.0F + config.phase,
                                                         config.phase * 0.47F,
                                                         config.seed ^ 0xA4093822U,
                                                         4) *
                                                  0.28F,
                                      0.46F,
                                      1.0F);
  const float foothill = std::pow(envelope_position, 0.72F) * 0.19F;
  const float ridge = main_fold * longitudinal_taper * peak_chain * 0.78F;
  const float folded_relief = branch_fold * longitudinal_taper * 0.18F;
  const float weathering = broad_noise * 0.075F + fine_noise * 0.026F - gully * 0.075F;
  const float height = edge_fade *
                       std::clamp(foothill + ridge + folded_relief + weathering,
                                  0.0F,
                                  1.08F);
  return {.footprint = smoothstep01(envelope_position / 0.12F),
          .elevation_fraction = std::clamp(height, 0.0F, 1.06F)};
}

void apply_constrained_erosion(std::vector<float>& heights,
                               const std::vector<float>& strength,
                               const std::vector<std::uint8_t>& protected_cells,
                               int width,
                               int height,
                               float tile_size,
                               const ErosionConfig& config) {
  const auto cell_count =
      static_cast<std::size_t>(std::max(width, 0) * std::max(height, 0));
  if (width < 3 || height < 3 || heights.size() != cell_count ||
      strength.size() != cell_count || protected_cells.size() != cell_count) {
    return;
  }

  std::vector<int> downstream(cell_count, -1);
  std::vector<float> accumulation(cell_count, 0.0F);
  std::vector<int> order(cell_count);
  std::iota(order.begin(), order.end(), 0);
  constexpr std::array<std::array<int, 2>, 8> k_dirs = {{{{1, 0}},
                                                         {{-1, 0}},
                                                         {{0, 1}},
                                                         {{0, -1}},
                                                         {{1, 1}},
                                                         {{1, -1}},
                                                         {{-1, 1}},
                                                         {{-1, -1}}}};

  for (int z = 1; z < height - 1; ++z) {
    for (int x = 1; x < width - 1; ++x) {
      const int idx = z * width + x;
      if (strength[idx] <= 0.0F || protected_cells[idx] != 0) {
        continue;
      }
      accumulation[idx] = 1.0F;
      float lowest = heights[idx];
      for (const auto& dir : k_dirs) {
        const int neighbor = (z + dir[1]) * width + x + dir[0];
        if (heights[neighbor] < lowest) {
          lowest = heights[neighbor];
          downstream[idx] = neighbor;
        }
      }
    }
  }

  std::sort(order.begin(), order.end(), [&](int lhs, int rhs) {
    return heights[lhs] > heights[rhs];
  });
  for (const int idx : order) {
    const int next = downstream[idx];
    if (next >= 0) {
      accumulation[next] += accumulation[idx];
    }
  }
  for (std::size_t idx = 0; idx < cell_count; ++idx) {
    if (strength[idx] <= 0.0F || protected_cells[idx] != 0 ||
        accumulation[idx] <= 1.5F) {
      continue;
    }
    const float carve =
        std::min(config.channel_strength * strength[idx] * std::log2(accumulation[idx]),
                 0.22F * strength[idx]);
    heights[idx] = std::max(0.0F, heights[idx] - carve);
  }

  std::vector<float> delta(cell_count, 0.0F);
  const float talus = std::max(tile_size, 0.001F) * config.talus_height_per_cell;
  for (int iteration = 0; iteration < std::max(config.iterations, 0); ++iteration) {
    std::fill(delta.begin(), delta.end(), 0.0F);
    for (int z = 1; z < height - 1; ++z) {
      for (int x = 1; x < width - 1; ++x) {
        const int idx = z * width + x;
        if (strength[idx] <= 0.0F || protected_cells[idx] != 0) {
          continue;
        }
        int lowest_idx = -1;
        float largest_drop = 0.0F;
        for (const auto& dir : k_dirs) {
          const int neighbor = (z + dir[1]) * width + x + dir[0];
          if (protected_cells[neighbor] != 0 || strength[neighbor] <= 0.0F) {
            continue;
          }
          const float diagonal = (dir[0] != 0 && dir[1] != 0) ? 1.41421356F : 1.0F;
          const float drop = heights[idx] - heights[neighbor] - talus * diagonal;
          if (drop > largest_drop) {
            largest_drop = drop;
            lowest_idx = neighbor;
          }
        }
        if (lowest_idx < 0) {
          continue;
        }
        const float amount = largest_drop * config.thermal_rate * strength[idx];
        delta[idx] -= amount;
        delta[lowest_idx] += amount;
      }
    }
    for (std::size_t idx = 0; idx < cell_count; ++idx) {
      heights[idx] = std::max(0.0F, heights[idx] + delta[idx]);
    }
  }
}

} // namespace Game::Map::Landform
