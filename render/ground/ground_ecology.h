#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <vector>

#include "game/map/ground_type.h"
#include "game/map/scatter/value_noise.h"
#include "game/map/terrain.h"
#include "game/map/terrain_features.h"
#include "game/map/terrain_service.h"

namespace Render::Ground {

struct GroundEcologyField {
  int width = 0;
  int height = 0;
  float tile_size = 1.0F;
  std::uint32_t seed = 0U;
  std::vector<float> forest;
  std::vector<float> margin;
  std::vector<float> verge;
  std::vector<float> talus;
  std::vector<float> coverage;
  std::vector<float> road_proximity;

  [[nodiscard]] auto empty() const noexcept -> bool {
    return width <= 0 || height <= 0;
  }

  [[nodiscard]] auto
  sample(const std::vector<float>& layer, float grid_x, float grid_z) const -> float {
    if (layer.empty() || empty()) {
      return 0.0F;
    }
    float const gx = std::clamp(grid_x, 0.0F, static_cast<float>(width - 1));
    float const gz = std::clamp(grid_z, 0.0F, static_cast<float>(height - 1));
    int const x0 = static_cast<int>(std::floor(gx));
    int const z0 = static_cast<int>(std::floor(gz));
    int const x1 = std::min(x0 + 1, width - 1);
    int const z1 = std::min(z0 + 1, height - 1);
    float const tx = gx - static_cast<float>(x0);
    float const tz = gz - static_cast<float>(z0);
    auto at = [&](int x, int z) {
      return layer[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
                   static_cast<std::size_t>(x)];
    };
    float const a = at(x0, z0) * (1.0F - tx) + at(x1, z0) * tx;
    float const b = at(x0, z1) * (1.0F - tx) + at(x1, z1) * tx;
    return a * (1.0F - tz) + b * tz;
  }
};

namespace ecology_detail {

inline auto smooth01(float t) -> float {
  t = std::clamp(t, 0.0F, 1.0F);
  return t * t * (3.0F - 2.0F * t);
}

inline auto step_between(float edge0, float edge1, float x) -> float {
  return smooth01((x - edge0) / std::max(1e-6F, edge1 - edge0));
}

inline auto smooth_value_noise(float x, float z, std::uint32_t salt) -> float {
  int const x0 = static_cast<int>(std::floor(x));
  int const z0 = static_cast<int>(std::floor(z));
  float const tx = smooth01(x - static_cast<float>(x0));
  float const tz = smooth01(z - static_cast<float>(z0));
  float const n00 = hash_to_01(hash_coords(x0, z0, salt));
  float const n10 = hash_to_01(hash_coords(x0 + 1, z0, salt));
  float const n01 = hash_to_01(hash_coords(x0, z0 + 1, salt));
  float const n11 = hash_to_01(hash_coords(x0 + 1, z0 + 1, salt));
  float const a = n00 + (n10 - n00) * tx;
  float const b = n01 + (n11 - n01) * tx;
  return a + (b - a) * tz;
}

inline auto fbm(float x, float z, std::uint32_t salt) -> float {
  float value = 0.0F;
  float amplitude = 0.5F;
  float norm = 0.0F;
  for (int octave = 0; octave < 3; ++octave) {
    value +=
        smooth_value_noise(x, z, salt + static_cast<std::uint32_t>(octave) * 977U) *
        amplitude;
    norm += amplitude;
    x = x * 2.03F + 17.3F;
    z = z * 2.03F - 9.1F;
    amplitude *= 0.5F;
  }
  return value / norm;
}

inline void box_blur(std::vector<float>& field, int width, int height, int radius) {
  if (radius <= 0 || field.empty()) {
    return;
  }
  std::vector<float> scratch(field.size(), 0.0F);
  for (int z = 0; z < height; ++z) {
    float sum = 0.0F;
    int count = 0;
    std::size_t const row =
        static_cast<std::size_t>(z) * static_cast<std::size_t>(width);
    for (int x = -radius; x <= radius; ++x) {
      if (x >= 0 && x < width) {
        sum += field[row + static_cast<std::size_t>(x)];
        ++count;
      }
    }
    for (int x = 0; x < width; ++x) {
      scratch[row + static_cast<std::size_t>(x)] =
          count > 0 ? sum / static_cast<float>(count) : 0.0F;
      int const leaving = x - radius;
      int const entering = x + radius + 1;
      if (leaving >= 0) {
        sum -= field[row + static_cast<std::size_t>(leaving)];
        --count;
      }
      if (entering < width) {
        sum += field[row + static_cast<std::size_t>(entering)];
        ++count;
      }
    }
  }
  for (int x = 0; x < width; ++x) {
    float sum = 0.0F;
    int count = 0;
    for (int z = -radius; z <= radius; ++z) {
      if (z >= 0 && z < height) {
        sum += scratch[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(x)];
        ++count;
      }
    }
    for (int z = 0; z < height; ++z) {
      field[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
            static_cast<std::size_t>(x)] =
          count > 0 ? sum / static_cast<float>(count) : 0.0F;
      int const leaving = z - radius;
      int const entering = z + radius + 1;
      if (leaving >= 0) {
        sum -= scratch[static_cast<std::size_t>(leaving) *
                           static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(x)];
        --count;
      }
      if (entering < height) {
        sum += scratch[static_cast<std::size_t>(entering) *
                           static_cast<std::size_t>(width) +
                       static_cast<std::size_t>(x)];
        ++count;
      }
    }
  }
}

inline auto sample_bilinear(const std::vector<float>& field,
                            int width,
                            int height,
                            float gx,
                            float gz) -> float {
  gx = std::clamp(gx, 0.0F, static_cast<float>(width - 1));
  gz = std::clamp(gz, 0.0F, static_cast<float>(height - 1));
  int const x0 = static_cast<int>(gx);
  int const z0 = static_cast<int>(gz);
  int const x1 = std::min(x0 + 1, width - 1);
  int const z1 = std::min(z0 + 1, height - 1);
  float const tx = gx - static_cast<float>(x0);
  float const tz = gz - static_cast<float>(z0);
  auto at = [&](int x, int z) {
    return field[static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
                 static_cast<std::size_t>(x)];
  };
  float const a = at(x0, z0) + (at(x1, z0) - at(x0, z0)) * tx;
  float const b = at(x0, z1) + (at(x1, z1) - at(x0, z1)) * tx;
  return a + (b - a) * tz;
}

} // namespace ecology_detail

[[nodiscard]] inline auto build_ground_ecology(
    const std::vector<float>& heights,
    const std::vector<Game::Map::TerrainType>& types,
    int width,
    int height,
    float tile_size,
    std::uint32_t seed,
    const std::vector<Game::Map::RoadSegment>* roads) -> GroundEcologyField {
  using namespace ecology_detail;
  GroundEcologyField field;
  if (width < 2 || height < 2) {
    return field;
  }
  auto const count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
  if (types.size() < count || heights.size() < count) {
    return field;
  }
  field.width = width;
  field.height = height;
  field.tile_size = std::max(tile_size, 1e-4F);
  field.seed = seed;
  float const tile = field.tile_size;
  auto index = [width](int x, int z) {
    return static_cast<std::size_t>(z) * static_cast<std::size_t>(width) +
           static_cast<std::size_t>(x);
  };

  {
    std::vector<float> mask(count, 0.0F);
    bool any_forest = false;
    for (std::size_t i = 0; i < count; ++i) {
      if (types[i] == Game::Map::TerrainType::Forest) {
        mask[i] = 1.0F;
        any_forest = true;
      }
    }
    field.forest.assign(count, 0.0F);
    field.margin.assign(count, 0.0F);
    if (any_forest) {
      int const near_radius = std::max(1, static_cast<int>(std::lround(2.5F / tile)));
      int const wide_radius = std::max(2, static_cast<int>(std::lround(7.0F / tile)));
      std::vector<float> coverage = mask;
      box_blur(coverage, width, height, near_radius);
      box_blur(coverage, width, height, near_radius);
      std::vector<float> halo = mask;
      box_blur(halo, width, height, wide_radius);
      box_blur(halo, width, height, wide_radius);

      float const warp_cells = 12.0F / tile;
      for (int z = 0; z < height; ++z) {
        for (int x = 0; x < width; ++x) {
          std::size_t const i = index(x, z);
          if (halo[i] <= 1e-4F) {
            continue;
          }
          float const wx = static_cast<float>(x) * tile;
          float const wz = static_cast<float>(z) * tile;
          float const warp_x =
              (fbm(wx * 0.022F, wz * 0.022F, seed ^ 0x6D2B79F5U) - 0.5F) * 2.0F;
          float const warp_z =
              (fbm(wx * 0.022F, wz * 0.022F, seed ^ 0x1B873593U) - 0.5F) * 2.0F;
          float const gx = static_cast<float>(x) + warp_x * warp_cells;
          float const gz = static_cast<float>(z) + warp_z * warp_cells;
          float const warped = sample_bilinear(coverage, width, height, gx, gz);
          float const warped_halo = sample_bilinear(halo, width, height, gx, gz);
          float const edge_noise =
              fbm(wx * 0.06F, wz * 0.06F, seed ^ 0x85EBCA6BU) - 0.5F;
          float forest = step_between(0.30F, 0.66F, warped + edge_noise * 0.55F);

          float const clearing_noise =
              fbm(wx * 0.055F, wz * 0.055F, seed ^ 0xC2B2AE35U);
          float const deep = step_between(0.80F, 0.97F, warped);
          float const clearing = step_between(0.58F, 0.68F, clearing_noise) * deep;
          forest *= 1.0F - 0.80F * clearing;

          float const spill_noise = fbm(wx * 0.07F, wz * 0.07F, seed ^ 0x27D4EB2FU);
          float const spill =
              step_between(0.04F, 0.34F, warped_halo) *
              step_between(0.34F, 0.66F, spill_noise + warped_halo * 0.35F);
          field.forest[i] = forest;
          field.margin[i] = std::clamp(
              std::max(spill * (1.0F - forest * 0.55F), clearing * deep * 0.65F),
              0.0F,
              1.0F);
        }
      }
      field.coverage = std::move(coverage);
    }
  }

  if (roads != nullptr && !roads->empty()) {
    field.verge.assign(count, 0.0F);
    field.road_proximity.assign(count, 0.0F);
    constexpr float k_verge_reach = 8.0F;
    float const half_extent_x = static_cast<float>(width) * 0.5F - 0.5F;
    float const half_extent_z = static_cast<float>(height) * 0.5F - 0.5F;
    for (const auto& road : *roads) {
      float const ax = road.start.x();
      float const az = road.start.z();
      float const bx = road.end.x();
      float const bz = road.end.z();
      float const dx = bx - ax;
      float const dz = bz - az;
      float const length_sq = dx * dx + dz * dz;
      float const half_width = std::max(road.width * 0.5F, 0.25F);
      float const reach = half_width + k_verge_reach;
      int const min_x =
          std::max(0,
                   static_cast<int>(
                       std::floor((std::min(ax, bx) - reach) / tile + half_extent_x)));
      int const max_x =
          std::min(width - 1,
                   static_cast<int>(
                       std::ceil((std::max(ax, bx) + reach) / tile + half_extent_x)));
      int const min_z =
          std::max(0,
                   static_cast<int>(
                       std::floor((std::min(az, bz) - reach) / tile + half_extent_z)));
      int const max_z =
          std::min(height - 1,
                   static_cast<int>(
                       std::ceil((std::max(az, bz) + reach) / tile + half_extent_z)));
      for (int z = min_z; z <= max_z; ++z) {
        float const wz = (static_cast<float>(z) - half_extent_z) * tile;
        for (int x = min_x; x <= max_x; ++x) {
          float const wx = (static_cast<float>(x) - half_extent_x) * tile;
          float t = 0.0F;
          if (length_sq > 1e-6F) {
            t = std::clamp(((wx - ax) * dx + (wz - az) * dz) / length_sq, 0.0F, 1.0F);
          }
          float const px = ax + dx * t - wx;
          float const pz = az + dz * t - wz;
          float const distance = std::sqrt(px * px + pz * pz);
          float const beyond = distance - half_width;
          if (beyond > k_verge_reach) {
            continue;
          }
          std::size_t const i = index(x, z);
          field.road_proximity[i] =
              std::max(field.road_proximity[i],
                       1.0F - std::clamp(beyond / k_verge_reach, 0.0F, 1.0F));
          float const verge_width =
              2.0F + 3.0F * fbm(wx * 0.11F, wz * 0.11F, seed ^ 0x9E3779B9U);
          float const value = 1.0F - step_between(-0.4F, verge_width, beyond);
          field.verge[i] = std::max(field.verge[i], value);
        }
      }
    }
  }

  {
    field.talus.assign(count, 0.0F);
    constexpr int k_directions = 8;
    constexpr float k_dir_x[k_directions] = {
        1.0F, 0.7071F, 0.0F, -0.7071F, -1.0F, -0.7071F, 0.0F, 0.7071F};
    constexpr float k_dir_z[k_directions] = {
        0.0F, 0.7071F, 1.0F, 0.7071F, 0.0F, -0.7071F, -1.0F, -0.7071F};
    constexpr float k_radii_m[3] = {3.0F, 6.5F, 11.0F};
    constexpr float k_radius_weight[3] = {1.0F, 0.85F, 0.65F};
    constexpr int k_taps = 3 * k_directions;
    std::array<int, k_taps> tap_dx{};
    std::array<int, k_taps> tap_dz{};
    std::array<std::ptrdiff_t, k_taps> tap_offset{};
    std::array<float, k_taps> tap_radius{};
    std::array<float, k_taps> tap_weight{};
    int reach_cells = 1;
    for (int ring = 0; ring < 3; ++ring) {
      float const cells = k_radii_m[ring] / tile;
      for (int d = 0; d < k_directions; ++d) {
        int const tap = ring * k_directions + d;
        tap_dx[tap] = static_cast<int>(std::lround(k_dir_x[d] * cells));
        tap_dz[tap] = static_cast<int>(std::lround(k_dir_z[d] * cells));
        tap_offset[tap] =
            static_cast<std::ptrdiff_t>(tap_dz[tap]) * width + tap_dx[tap];
        tap_radius[tap] = k_radii_m[ring];
        tap_weight[tap] = k_radius_weight[ring];
        reach_cells =
            std::max({reach_cells, std::abs(tap_dx[tap]), std::abs(tap_dz[tap])});
      }
    }
    auto height_at = [&](int x, int z) {
      x = std::clamp(x, 0, width - 1);
      z = std::clamp(z, 0, height - 1);
      return heights[index(x, z)];
    };
    for (int z = 0; z < height; ++z) {
      bool const interior_row = z >= reach_cells && z < height - reach_cells;
      for (int x = 0; x < width; ++x) {
        std::size_t const i = index(x, z);
        if (Game::Map::is_water_terrain(types[i])) {
          continue;
        }
        float const h = heights[i];
        float const gx = (height_at(x + 1, z) - height_at(x - 1, z)) / (2.0F * tile);
        float const gz = (height_at(x, z + 1) - height_at(x, z - 1)) / (2.0F * tile);
        float const own_grade = std::sqrt(gx * gx + gz * gz);
        if (own_grade > 0.75F) {
          continue;
        }
        float rise = 0.0F;
        if (interior_row && x >= reach_cells && x < width - reach_cells) {
          const float* centre = heights.data() + i;
          for (int tap = 0; tap < k_taps; ++tap) {
            rise = std::max(rise,
                            (centre[tap_offset[tap]] - h) / tap_radius[tap] *
                                tap_weight[tap]);
          }
        } else {
          for (int tap = 0; tap < k_taps; ++tap) {
            rise = std::max(rise,
                            (height_at(x + tap_dx[tap], z + tap_dz[tap]) - h) /
                                tap_radius[tap] * tap_weight[tap]);
          }
        }
        if (rise <= 0.10F) {
          continue;
        }
        float const wx = static_cast<float>(x) * tile;
        float const wz = static_cast<float>(z) * tile;
        float const breakup = fbm(wx * 0.09F, wz * 0.09F, seed ^ 0x5851F42DU);
        float const apron = step_between(0.14F, 0.50F, rise) *
                            (1.0F - step_between(0.22F, 0.60F, own_grade));
        field.talus[i] = std::clamp(apron * (0.35F + 1.05F * breakup), 0.0F, 1.0F);
      }
    }
  }

  return field;
}

[[nodiscard]] inline auto ecology_seed(std::uint32_t biome_seed) -> std::uint32_t {
  return biome_seed ^ 0x3C6EF372U;
}

[[nodiscard]] inline auto build_ground_ecology(
    const Game::Map::TerrainHeightMap& height_map,
    std::uint32_t biome_seed,
    const std::vector<Game::Map::RoadSegment>* roads) -> GroundEcologyField {
  return build_ground_ecology(height_map.get_height_data(),
                              height_map.getTerrainTypes(),
                              height_map.get_width(),
                              height_map.get_height(),
                              height_map.get_tile_size(),
                              ecology_seed(biome_seed),
                              roads);
}

[[nodiscard]] inline auto shared_ground_ecology(
    const Game::Map::TerrainService* terrain,
    const Game::Map::TerrainHeightMap& height_map,
    std::uint32_t biome_seed) -> std::shared_ptr<const GroundEcologyField> {
  if (terrain == nullptr || terrain->get_height_map() != &height_map) {
    return std::make_shared<const GroundEcologyField>(
        build_ground_ecology(height_map, biome_seed, nullptr));
  }
  struct Slot {
    std::mutex mutex;
    const Game::Map::TerrainService* terrain = nullptr;
    std::uint64_t topology_revision = 0U;
    std::uint32_t biome_seed = 0U;
    std::weak_ptr<const GroundEcologyField> field;
  };
  static Slot slot;
  std::lock_guard<std::mutex> const lock(slot.mutex);
  std::uint64_t const revision = terrain->navigation_topology_revision();
  if (slot.terrain == terrain && slot.topology_revision == revision &&
      slot.biome_seed == biome_seed) {
    if (auto cached = slot.field.lock()) {
      return cached;
    }
  }
  auto field = std::make_shared<const GroundEcologyField>(
      build_ground_ecology(height_map, biome_seed, &terrain->road_segments()));
  slot.terrain = terrain;
  slot.topology_revision = revision;
  slot.biome_seed = biome_seed;
  slot.field = field;
  return field;
}

[[nodiscard]] inline auto
ecology_grid_from_world(float world, int cells, float tile) -> float {
  return world / std::max(tile, 1e-4F) + static_cast<float>(cells) * 0.5F - 0.5F;
}

[[nodiscard]] inline auto ecology_clump(const GroundEcologyField& ecology,
                                        int grid_x,
                                        int grid_z,
                                        std::uint32_t salt) -> float {
  using namespace ecology_detail;
  float const wx = static_cast<float>(grid_x) * ecology.tile_size;
  float const wz = static_cast<float>(grid_z) * ecology.tile_size;
  return 0.30F + 1.40F * step_between(0.38F,
                                      0.70F,
                                      fbm(wx * 0.03F, wz * 0.03F, ecology.seed ^ salt));
}

template <typename TerrainCache>
[[nodiscard]] auto plant_ecology_density(const GroundEcologyField& ecology,
                                         const TerrainCache& cache,
                                         int grid_x,
                                         int grid_z) -> float {
  using namespace ecology_detail;
  if (ecology.empty()) {
    return 1.0F;
  }
  auto const gx = static_cast<float>(grid_x);
  auto const gz = static_cast<float>(grid_z);
  float const slope_band =
      step_between(0.05F, 0.24F, cache.get_slope_at(grid_x, grid_z));
  float const presence = 0.30F + 1.90F * ecology.sample(ecology.margin, gx, gz) +
                         1.30F * ecology.sample(ecology.talus, gx, gz) +
                         0.90F * slope_band +
                         0.35F * ecology.sample(ecology.forest, gx, gz);
  float const verge = ecology.sample(ecology.verge, gx, gz);
  return ecology_clump(ecology, grid_x, grid_z, 0x2545F491U) * presence *
         (1.0F - 0.85F * verge);
}

template <typename TerrainCache>
[[nodiscard]] auto stone_ecology_density(const GroundEcologyField& ecology,
                                         const TerrainCache& cache,
                                         int grid_x,
                                         int grid_z) -> float {
  using namespace ecology_detail;
  if (ecology.empty()) {
    return 1.0F;
  }
  auto const gx = static_cast<float>(grid_x);
  auto const gz = static_cast<float>(grid_z);
  float const slope_band =
      step_between(0.04F, 0.20F, cache.get_slope_at(grid_x, grid_z));
  float const presence = 0.25F + 2.20F * ecology.sample(ecology.talus, gx, gz) +
                         1.00F * slope_band +
                         0.50F * ecology.sample(ecology.margin, gx, gz);
  float const verge = ecology.sample(ecology.verge, gx, gz);
  return ecology_clump(ecology, grid_x, grid_z, 0x9E3779B1U) * presence *
         (1.0F - 0.80F * verge);
}

template <typename TerrainCache, typename DensityFn>
[[nodiscard]] auto ecology_density_normaliser(const GroundEcologyField& ecology,
                                              const TerrainCache& cache,
                                              int width,
                                              int height,
                                              int cell_span,
                                              DensityFn&& density) -> float {
  if (ecology.empty() || cell_span <= 0) {
    return 1.0F;
  }
  double sum = 0.0;
  int samples = 0;
  for (int z = 0; z < height; z += cell_span) {
    for (int x = 0; x < width; x += cell_span) {
      int const sample_x = std::min(x + cell_span / 2, width - 1);
      int const sample_z = std::min(z + cell_span / 2, height - 1);
      auto const type = cache.get_terrain_type_at(sample_x, sample_z);
      if (type == Game::Map::TerrainType::Mountain ||
          Game::Map::is_water_terrain(type)) {
        continue;
      }
      sum += static_cast<double>(density(ecology, cache, sample_x, sample_z));
      ++samples;
    }
  }
  if (samples == 0 || sum <= 1e-6) {
    return 1.0F;
  }
  float const mean = static_cast<float>(sum / static_cast<double>(samples));
  return std::clamp(1.0F / mean, 0.25F, 4.0F);
}

} // namespace Render::Ground
