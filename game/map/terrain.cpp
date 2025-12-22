#include "terrain.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <math.h>
#include <numbers>
#include <vector>

namespace {
constexpr float k_deg_to_rad = std::numbers::pi_v<float> / 180.0F;

constexpr int k_hill_ramp_extra_steps = 12;

constexpr float k_hill_ramp_steepness_exponent = 1.25F;

constexpr float k_entry_ramp_width = 3.0F;

constexpr float k_width_falloff_padding = 0.75F;

constexpr float k_entry_bowl_exponent = 2.0F;

constexpr float k_entry_base_width_scale = 1.55F;
constexpr float k_entry_top_width_scale = 0.70F;

constexpr float k_entry_outward_steps_fraction = 0.65F;
constexpr int k_entry_outward_steps_min = 4;
constexpr int k_entry_outward_steps_max = 18;

constexpr float k_entry_mid_dip_strength = 0.40F;

constexpr float k_entry_mid_depth_strength = 0.34F;

constexpr float k_entry_toe_height_fraction = 0.01F;

constexpr float k_walkable_width_threshold = 0.38F;

inline auto hashCoords(int x, int z, std::uint32_t seed) -> std::uint32_t {
  std::uint32_t const ux = static_cast<std::uint32_t>(x) * 73856093U;
  std::uint32_t const uz = static_cast<std::uint32_t>(z) * 19349663U;
  std::uint32_t const s = seed * 83492791U + 0x9e3779b9U;
  return ux ^ uz ^ s;
}

inline auto hashToFloat01(std::uint32_t h) -> float {
  h ^= h >> 17;
  h *= 0xed5ad4bbU;
  h ^= h >> 11;
  h *= 0xac4c1b51U;
  h ^= h >> 15;
  h *= 0x31848babU;
  h ^= h >> 14;
  return (h & 0x00FFFFFFU) / float(0x01000000);
}

inline auto valueNoise2D(float x, float z, std::uint32_t seed) -> float {
  int const ix0 = static_cast<int>(std::floor(x));
  int const iz0 = static_cast<int>(std::floor(z));
  int const ix1 = ix0 + 1;
  int const iz1 = iz0 + 1;

  float const tx = x - static_cast<float>(ix0);
  float const tz = z - static_cast<float>(iz0);

  float const n00 = hashToFloat01(hashCoords(ix0, iz0, seed));
  float const n10 = hashToFloat01(hashCoords(ix1, iz0, seed));
  float const n01 = hashToFloat01(hashCoords(ix0, iz1, seed));
  float const n11 = hashToFloat01(hashCoords(ix1, iz1, seed));

  float const nx0 = n00 * (1.0F - tx) + n10 * tx;
  float const nx1 = n01 * (1.0F - tx) + n11 * tx;
  return nx0 * (1.0F - tz) + nx1 * tz;
}
} // namespace

namespace Game::Map {

TerrainHeightMap::TerrainHeightMap(int width, int height, float tile_size)
    : m_width(width), m_height(height), m_tile_size(tile_size) {
  const int count = width * height;
  m_heights.resize(count, 0.0F);
  m_terrain_types.resize(count, TerrainType::Flat);
  m_hillEntrances.resize(count, false);
  m_hillWalkable.resize(count, false);
}

void TerrainHeightMap::buildFromFeatures(
    const std::vector<TerrainFeature> &features) {

  std::fill(m_heights.begin(), m_heights.end(), 0.0F);
  std::fill(m_terrain_types.begin(), m_terrain_types.end(), TerrainType::Flat);
  std::fill(m_hillEntrances.begin(), m_hillEntrances.end(), false);
  std::fill(m_hillWalkable.begin(), m_hillWalkable.end(), false);

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;

  for (const auto &feature : features) {

    const float grid_center_x =
        (feature.center_x / m_tile_size) + grid_half_width;
    const float grid_center_z =
        (feature.center_z / m_tile_size) + grid_half_height;
    const float grid_radius = std::max(feature.radius / m_tile_size, 1.0F);

    if (feature.type == TerrainType::Mountain) {
      const float major_radius =
          std::max(grid_radius * 1.8F, grid_radius + 3.0F);
      const float minor_radius = std::max(grid_radius * 0.22F, 0.8F);
      const float bound = std::max(major_radius, minor_radius) + 2.0F;
      const int min_x = std::max(0, int(std::floor(grid_center_x - bound)));
      const int max_x =
          std::min(m_width - 1, int(std::ceil(grid_center_x + bound)));
      const int min_z = std::max(0, int(std::floor(grid_center_z - bound)));
      const int max_z =
          std::min(m_height - 1, int(std::ceil(grid_center_z + bound)));

      const float angle_rad = feature.rotationDeg * k_deg_to_rad;
      const float cos_a = std::cos(angle_rad);
      const float sin_a = std::sin(angle_rad);

      for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
          const float local_x = float(x) - grid_center_x;
          const float local_z = float(z) - grid_center_z;

          const float rotated_x = local_x * cos_a + local_z * sin_a;
          const float rotated_z = -local_x * sin_a + local_z * cos_a;

          const float norm = std::sqrt(
              (rotated_x * rotated_x) / (major_radius * major_radius) +
              (rotated_z * rotated_z) / (minor_radius * minor_radius));

          if (norm <= 1.0F) {
            float const blend = std::clamp(1.0F - norm, 0.0F, 1.0F);

            float height = feature.height * std::pow(blend, 3.5F);
            if (blend > 0.92F) {
              height = feature.height;
            }

            if (height > 0.01F) {
              int const idx = indexAt(x, z);
              if (height > m_heights[idx]) {
                m_heights[idx] = height;
                m_terrain_types[idx] = TerrainType::Mountain;
              }
            }
          }
        }
      }
      continue;
    }

    if (feature.type == TerrainType::Hill) {
      const float grid_width = std::max(feature.width / m_tile_size, 1.0F);
      const float grid_depth = std::max(feature.depth / m_tile_size, 1.0F);

      const float plateau_width = std::max(1.5F, grid_width * 0.45F);
      const float plateau_depth = std::max(1.5F, grid_depth * 0.45F);
      const float slope_width = std::max(plateau_width + 1.5F, grid_width);
      const float slope_depth = std::max(plateau_depth + 1.5F, grid_depth);

      const float max_extent = std::max(slope_width, slope_depth);
      const int min_x =
          std::max(0, int(std::floor(grid_center_x - max_extent - 1.0F)));
      const int max_x = std::min(
          m_width - 1, int(std::ceil(grid_center_x + max_extent + 1.0F)));
      const int min_z =
          std::max(0, int(std::floor(grid_center_z - max_extent - 1.0F)));
      const int max_z = std::min(
          m_height - 1, int(std::ceil(grid_center_z + max_extent + 1.0F)));

      const int map_cell_count = m_width * m_height;
      std::vector<std::uint8_t> walkable_mask(map_cell_count, 0);
      std::vector<std::uint8_t> entrance_line_mask(map_cell_count, 0);
      std::vector<int> entrance_indices;

      const float angle_rad = feature.rotationDeg * k_deg_to_rad;
      const float cos_a = std::cos(angle_rad);
      const float sin_a = std::sin(angle_rad);

      for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
          const float dx = float(x) - grid_center_x;
          const float dz = float(z) - grid_center_z;

          const float rotated_x = dx * cos_a + dz * sin_a;
          const float rotated_z = -dx * sin_a + dz * cos_a;

          const float norm_plateau_dist = std::sqrt(
              (rotated_x * rotated_x) / (plateau_width * plateau_width) +
              (rotated_z * rotated_z) / (plateau_depth * plateau_depth));
          const float norm_slope_dist =
              std::sqrt((rotated_x * rotated_x) / (slope_width * slope_width) +
                        (rotated_z * rotated_z) / (slope_depth * slope_depth));

          if (norm_slope_dist > 1.0F) {
            continue;
          }

          const int idx = indexAt(x, z);

          float height = 0.0F;
          if (norm_plateau_dist <= 1.0F) {
            height = feature.height;
          } else {
            float const t = std::clamp((norm_slope_dist - norm_plateau_dist) /
                                           (1.0F - norm_plateau_dist),
                                       0.0F, 1.0F);
            float const smooth =
                0.5F * (1.0F + std::cos(t * std::numbers::pi_v<float>));
            height = feature.height * smooth;
          }

          if (height > m_heights[idx]) {
            m_heights[idx] = height;
            m_terrain_types[idx] = TerrainType::Hill;
          }
          if (norm_plateau_dist <= 1.0F &&
              m_terrain_types[idx] == TerrainType::Hill) {
            walkable_mask[idx] = 1;
          }
        }
      }

      for (const auto &entrance : feature.entrances) {
        const float entrance_gx =
            (entrance.x() / m_tile_size) + grid_half_width;
        const float entrance_gz =
            (entrance.z() / m_tile_size) + grid_half_height;
        int const ex = int(std::round(entrance_gx));
        int const ez = int(std::round(entrance_gz));
        if (!inBounds(ex, ez)) {
          continue;
        }

        const int entrance_idx = indexAt(ex, ez);
        m_hillEntrances[entrance_idx] = true;
        entrance_indices.push_back(entrance_idx);
        if (m_terrain_types[entrance_idx] != TerrainType::Mountain) {
          if (m_terrain_types[entrance_idx] == TerrainType::Flat) {
            m_terrain_types[entrance_idx] = TerrainType::Hill;
          }
          walkable_mask[entrance_idx] = 1;
          entrance_line_mask[entrance_idx] = 1;
          m_hillWalkable[entrance_idx] = true;
          m_heights[entrance_idx] = std::max(m_heights[entrance_idx], 0.0F);
        }

        float dir_x = grid_center_x - float(ex);
        float dir_z = grid_center_z - float(ez);
        float const length = std::sqrt(dir_x * dir_x + dir_z * dir_z);
        if (length < 0.001F) {
          continue;
        }

        dir_x /= length;
        dir_z /= length;

        auto cur_x = float(ex);
        auto cur_z = float(ez);
        const int steps = int(length) + 3;
        auto smoothstep = [](float t) {
          t = std::clamp(t, 0.0F, 1.0F);
          return t * t * (3.0F - 2.0F * t);
        };
        auto smootherstep = [](float t) {
          t = std::clamp(t, 0.0F, 1.0F);
          return t * t * t * (t * (t * 6.0F - 15.0F) + 10.0F);
        };
        int plateau_steps = steps;
        {
          auto test_x = cur_x;
          auto test_z = cur_z;
          for (int step = 0; step < steps; ++step) {
            int const ix = int(std::round(test_x));
            int const iz = int(std::round(test_z));
            if (!inBounds(ix, iz)) {
              break;
            }
            const float cell_dx = float(ix) - grid_center_x;
            const float cell_dz = float(iz) - grid_center_z;
            const float cell_rot_x = cell_dx * cos_a + cell_dz * sin_a;
            const float cell_rot_z = -cell_dx * sin_a + cell_dz * cos_a;
            const float plateau_norm_dist = std::sqrt(
                (cell_rot_x * cell_rot_x) / (plateau_width * plateau_width) +
                (cell_rot_z * cell_rot_z) / (plateau_depth * plateau_depth));
            if (plateau_norm_dist <= 1.0F) {
              plateau_steps = std::max(1, step);
              break;
            }
            test_x += dir_x;
            test_z += dir_z;
          }
        }

        const int ramp_steps =
            std::min(steps, plateau_steps + k_hill_ramp_extra_steps);

        int const outward_steps = std::clamp(
            int(std::round(float(ramp_steps) * k_entry_outward_steps_fraction)),
            k_entry_outward_steps_min, k_entry_outward_steps_max);
        int const total_ramp_steps = outward_steps + ramp_steps;

        float const hill_min_extent = std::min(plateau_width, plateau_depth);
        float const entry_width = std::max(
            1.5F, std::min(k_entry_ramp_width, hill_min_extent * 0.35F));

        float const perp_x = -dir_z;
        float const perp_z = dir_x;

        cur_x = float(ex) - dir_x * float(outward_steps);
        cur_z = float(ez) - dir_z * float(outward_steps);

        for (int ramp_step = 0; ramp_step < total_ramp_steps; ++ramp_step) {
          bool const is_outward = ramp_step < outward_steps;
          int const center_ix = int(std::round(cur_x));
          int const center_iz = int(std::round(cur_z));
          if (!inBounds(center_ix, center_iz)) {
            break;
          }

          const float cell_dx = float(center_ix) - grid_center_x;
          const float cell_dz = float(center_iz) - grid_center_z;
          const float cell_rot_x = cell_dx * cos_a + cell_dz * sin_a;
          const float cell_rot_z = -cell_dx * sin_a + cell_dz * cos_a;
          const float cell_norm_dist = std::sqrt(
              (cell_rot_x * cell_rot_x) / (slope_width * slope_width) +
              (cell_rot_z * cell_rot_z) / (slope_depth * slope_depth));
          const float plateau_norm_dist = std::sqrt(
              (cell_rot_x * cell_rot_x) / (plateau_width * plateau_width) +
              (cell_rot_z * cell_rot_z) / (plateau_depth * plateau_depth));

          if (!is_outward && cell_norm_dist > 1.1F) {
            cur_x += dir_x;
            cur_z += dir_z;
            continue;
          }

          float const ramp_progress =
              (total_ramp_steps > 1)
                  ? (float(ramp_step) / float(total_ramp_steps - 1))
                  : 1.0F;

          float const s = smootherstep(ramp_progress);
          float const mid = 4.0F * ramp_progress * (1.0F - ramp_progress);

          float const height_base = std::pow(s, k_hill_ramp_steepness_exponent);
          float const height_frac =
              std::clamp(height_base * (1.0F - k_entry_mid_dip_strength * mid),
                         0.0F, 1.0F);

          float const toe_frac =
              k_entry_toe_height_fraction * (1.0F - s) * (1.0F - s);
          float center_ramp_height =
              feature.height * std::max(height_frac, toe_frac);
          center_ramp_height *=
              std::clamp(1.0F - k_entry_mid_depth_strength * mid, 0.0F, 1.0F);

          float const width_scale = (1.0F - s) * k_entry_base_width_scale +
                                    s * k_entry_top_width_scale;
          float tapered_width = std::max(1.0F, entry_width * width_scale);

          if (is_outward && outward_steps > 0) {
            float const outward_t =
                float(ramp_step) / float(std::max(1, outward_steps));
            float const outward_width_mul =
                0.55F + 0.45F * std::clamp(outward_t, 0.0F, 1.0F);
            tapered_width = std::max(1.0F, tapered_width * outward_width_mul);
          }

          int const width_radius = int(std::ceil(tapered_width));
          for (int w = -width_radius; w <= width_radius; ++w) {
            float const offset_x = cur_x + perp_x * float(w);
            float const offset_z = cur_z + perp_z * float(w);
            int const ix = int(std::round(offset_x));
            int const iz = int(std::round(offset_z));

            if (!inBounds(ix, iz)) {
              continue;
            }

            float const edge_t = std::clamp(
                std::abs(float(w)) / (tapered_width + k_width_falloff_padding),
                0.0F, 1.0F);

            int const ramp_idx = indexAt(ix, iz);
            if (m_terrain_types[ramp_idx] != TerrainType::Mountain) {
              float const width_factor = 1.0F - edge_t;
              if (!is_outward) {

                if (m_terrain_types[ramp_idx] == TerrainType::Flat) {
                  m_terrain_types[ramp_idx] = TerrainType::Hill;
                }
                if (width_factor > k_walkable_width_threshold) {
                  walkable_mask[ramp_idx] = 1;
                  entrance_line_mask[ramp_idx] = 1;
                  m_hillEntrances[ramp_idx] = true;
                }
              }

              float const existing_height = m_heights[ramp_idx];

              float const bowl = std::pow(edge_t, k_entry_bowl_exponent);
              float const target_height =
                  (1.0F - bowl) * center_ramp_height + bowl * existing_height;

              float const along =
                  smootherstep(std::clamp((s - 0.20F) / 0.80F, 0.0F, 1.0F));
              float const carved = std::min(existing_height, target_height);
              float const joined = std::max(existing_height, target_height);
              m_heights[ramp_idx] = (1.0F - along) * carved + along * joined;
            }
          }

          cur_x += dir_x;
          cur_z += dir_z;
        }
      }

      for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
          int const idx = indexAt(x, z);
          if (m_terrain_types[idx] != TerrainType::Hill) {
            continue;
          }

          if (entrance_line_mask[idx] == 1) {
            continue;
          }

          const float dx = float(x) - grid_center_x;
          const float dz = float(z) - grid_center_z;
          const float rotated_x = dx * cos_a + dz * sin_a;
          const float rotated_z = -dx * sin_a + dz * cos_a;
          const float norm_plateau_dist = std::sqrt(
              (rotated_x * rotated_x) / (plateau_width * plateau_width) +
              (rotated_z * rotated_z) / (plateau_depth * plateau_depth));

          if (norm_plateau_dist > 1.0F) {
            walkable_mask[idx] = 0;
          }

          if (norm_plateau_dist > 0.85F) {
            bool adjacent_to_non_hill = false;
            constexpr int k_dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1},  {0, -1},
                                          {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
            for (const auto &dir : k_dirs) {
              int const nx = x + dir[0];
              int const nz = z + dir[1];
              if (!inBounds(nx, nz)) {
                adjacent_to_non_hill = true;
                break;
              }
              int const n_idx = indexAt(nx, nz);
              if (m_terrain_types[n_idx] != TerrainType::Hill) {
                adjacent_to_non_hill = true;
                break;
              }
            }
            if (adjacent_to_non_hill) {
              walkable_mask[idx] = 0;
            }
          }
        }
      }

      if (!entrance_line_mask.empty()) {
        for (int z = min_z; z <= max_z; ++z) {
          for (int x = min_x; x <= max_x; ++x) {
            int const idx = indexAt(x, z);
            if (walkable_mask[idx] == 0 || entrance_line_mask[idx] == 1 ||
                m_terrain_types[idx] != TerrainType::Hill) {
              continue;
            }

            bool on_edge = false;
            constexpr int k_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto &dir : k_dirs) {
              int const nx = x + dir[0];
              int const nz = z + dir[1];
              if (!inBounds(nx, nz)) {
                on_edge = true;
                break;
              }
              int const n_idx = indexAt(nx, nz);
              if (m_terrain_types[n_idx] != TerrainType::Hill) {
                on_edge = true;
                break;
              }
            }
            if (on_edge) {
              walkable_mask[idx] = 0;
            }
          }
        }
      }

      if (!entrance_indices.empty()) {
        std::vector<std::uint8_t> visited(map_cell_count, 0);
        std::vector<int> queue;
        queue.reserve(entrance_indices.size());

        for (int const entrance_idx : entrance_indices) {
          if (visited[entrance_idx] || walkable_mask[entrance_idx] == 0) {
            continue;
          }
          visited[entrance_idx] = 1;
          m_hillWalkable[entrance_idx] = true;
          queue.push_back(entrance_idx);

          while (!queue.empty()) {
            int const idx = queue.back();
            queue.pop_back();

            int const cx = idx % m_width;
            int const cz = idx / m_width;

            constexpr int k_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto &dir : k_dirs) {
              int const nx = cx + dir[0];
              int const nz = cz + dir[1];
              if (!inBounds(nx, nz)) {
                continue;
              }
              int const n_idx = indexAt(nx, nz);
              if (visited[n_idx] || walkable_mask[n_idx] == 0) {
                continue;
              }

              visited[n_idx] = 1;
              m_hillWalkable[n_idx] = true;
              queue.push_back(n_idx);
            }
          }
        }
      }

      continue;
    }

    const float flat_radius = grid_radius;
    const int min_x = std::max(0, int(std::floor(grid_center_x - flat_radius)));
    const int max_x =
        std::min(m_width - 1, int(std::ceil(grid_center_x + flat_radius)));
    const int min_z = std::max(0, int(std::floor(grid_center_z - flat_radius)));
    const int max_z =
        std::min(m_height - 1, int(std::ceil(grid_center_z + flat_radius)));

    for (int z = min_z; z <= max_z; ++z) {
      for (int x = min_x; x <= max_x; ++x) {
        const float dx = float(x) - grid_center_x;
        const float dz = float(z) - grid_center_z;
        const float dist = std::sqrt(dx * dx + dz * dz);
        if (dist > flat_radius) {
          continue;
        }

        float const t = dist / std::max(flat_radius, 0.0001F);
        float const height = feature.height * (1.0F - t);
        if (height <= 0.0F) {
          continue;
        }

        int const idx = indexAt(x, z);
        if (height > m_heights[idx]) {
          m_heights[idx] = height;
          m_terrain_types[idx] = TerrainType::Flat;
        }
      }
    }
  }
}

auto TerrainHeightMap::getHeightAt(float world_x,
                                   float world_z) const -> float {

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;

  float const gx = world_x / m_tile_size + grid_half_width;
  float const gz = world_z / m_tile_size + grid_half_height;

  int const x0 = int(std::floor(gx));
  int const z0 = int(std::floor(gz));
  int const x1 = x0 + 1;
  int const z1 = z0 + 1;

  if (!inBounds(x0, z0)) {
    return 0.0F;
  }

  float const tx = gx - x0;
  float const tz = gz - z0;

  float const h00 = inBounds(x0, z0) ? m_heights[indexAt(x0, z0)] : 0.0F;
  float const h10 = inBounds(x1, z0) ? m_heights[indexAt(x1, z0)] : 0.0F;
  float const h01 = inBounds(x0, z1) ? m_heights[indexAt(x0, z1)] : 0.0F;
  float const h11 = inBounds(x1, z1) ? m_heights[indexAt(x1, z1)] : 0.0F;

  float const h0 = h00 * (1.0F - tx) + h10 * tx;
  float const h1 = h01 * (1.0F - tx) + h11 * tx;

  float base_height = h0 * (1.0F - tz) + h1 * tz;

  if (isOnBridge(world_x, world_z)) {
    auto bridge_height_opt = getBridgeDeckHeight(world_x, world_z);
    if (bridge_height_opt.has_value()) {
      return bridge_height_opt.value();
    }
  }

  return base_height;
}

auto TerrainHeightMap::getHeightAtGrid(int grid_x, int grid_z) const -> float {
  if (!inBounds(grid_x, grid_z)) {
    return 0.0F;
  }
  return m_heights[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::is_walkable(int grid_x, int grid_z) const -> bool {
  if (!inBounds(grid_x, grid_z)) {
    return false;
  }

  int const idx = indexAt(grid_x, grid_z);
  if (!m_onBridge.empty() && m_onBridge[idx]) {
    return true;
  }

  TerrainType const type = m_terrain_types[idx];

  if (type == TerrainType::Mountain) {
    return false;
  }

  if (type == TerrainType::River) {
    return false;
  }

  if (type == TerrainType::Hill) {
    return m_hillWalkable[indexAt(grid_x, grid_z)];
  }

  return true;
}

auto TerrainHeightMap::isHillEntrance(int grid_x, int grid_z) const -> bool {
  if (!inBounds(grid_x, grid_z)) {
    return false;
  }
  return m_hillEntrances[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::getTerrainType(int grid_x,
                                      int grid_z) const -> TerrainType {
  if (!inBounds(grid_x, grid_z)) {
    return TerrainType::Flat;
  }
  return m_terrain_types[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::isRiverOrNearby(int grid_x, int grid_z,
                                       int margin) const -> bool {
  if (!inBounds(grid_x, grid_z)) {
    return false;
  }

  if (m_terrain_types[indexAt(grid_x, grid_z)] == TerrainType::River) {
    return true;
  }

  for (int dz = -margin; dz <= margin; ++dz) {
    for (int dx = -margin; dx <= margin; ++dx) {
      if (dx == 0 && dz == 0) {
        continue;
      }
      int const nx = grid_x + dx;
      int const nz = grid_z + dz;
      if (inBounds(nx, nz) &&
          m_terrain_types[indexAt(nx, nz)] == TerrainType::River) {
        return true;
      }
    }
  }

  return false;
}

auto TerrainHeightMap::indexAt(int x, int z) const -> int {
  return z * m_width + x;
}

auto TerrainHeightMap::inBounds(int x, int z) const -> bool {
  return x >= 0 && x < m_width && z >= 0 && z < m_height;
}

auto TerrainHeightMap::calculateFeatureHeight(const TerrainFeature &feature,
                                              float world_x,
                                              float world_z) -> float {
  float const dx = world_x - feature.center_x;
  float const dz = world_z - feature.center_z;
  float const dist = std::sqrt(dx * dx + dz * dz);

  if (dist > feature.radius) {
    return 0.0F;
  }

  float const t = dist / feature.radius;
  float const height_factor = (std::cos(t * M_PI) + 1.0F) * 0.5F;

  return feature.height * height_factor;
}

void TerrainHeightMap::applyBiomeVariation(const BiomeSettings &settings) {
  if (m_heights.empty()) {
    return;
  }

  if (settings.ground_irregularity_enabled) {
    const float amplitude = std::max(0.0F, settings.irregularity_amplitude);
    if (amplitude > 0.0001F) {
      const float frequency = std::max(0.0001F, settings.irregularity_scale);
      const float half_width = m_width * 0.5F - 0.5F;
      const float half_height = m_height * 0.5F - 0.5F;

      for (int z = 0; z < m_height; ++z) {
        for (int x = 0; x < m_width; ++x) {
          int const idx = indexAt(x, z);
          TerrainType const type = m_terrain_types[idx];

          if (type != TerrainType::Flat) {
            continue;
          }

          if (isRiverOrNearby(x, z, 2)) {
            continue;
          }

          float const world_x =
              (static_cast<float>(x) - half_width) * m_tile_size;
          float const world_z =
              (static_cast<float>(z) - half_height) * m_tile_size;
          float const sample_x = world_x * frequency;
          float const sample_z = world_z * frequency;

          float const base_noise =
              valueNoise2D(sample_x, sample_z, settings.seed);
          float const detail_noise = valueNoise2D(
              sample_x * 2.5F, sample_z * 2.5F, settings.seed ^ 0xA21C9E37U);
          float const fine_noise = valueNoise2D(
              sample_x * 5.0F, sample_z * 5.0F, settings.seed ^ 0x7E4B92F1U);

          float const blended =
              0.5F * base_noise + 0.35F * detail_noise + 0.15F * fine_noise;
          float const perturb = (blended - 0.5F) * 2.0F * amplitude;

          m_heights[idx] = std::max(0.0F, m_heights[idx] + perturb);
        }
      }
    }
  }

  const float legacy_amplitude =
      std::max(0.0F, settings.height_noise_amplitude);
  if (legacy_amplitude > 0.0001F) {
    const float frequency = std::max(0.0001F, settings.height_noise_frequency);
    const float half_width = m_width * 0.5F - 0.5F;
    const float half_height = m_height * 0.5F - 0.5F;

    for (int z = 0; z < m_height; ++z) {
      for (int x = 0; x < m_width; ++x) {
        int const idx = indexAt(x, z);
        TerrainType const type = m_terrain_types[idx];
        if (type == TerrainType::Mountain) {
          continue;
        }

        float const world_x =
            (static_cast<float>(x) - half_width) * m_tile_size;
        float const world_z =
            (static_cast<float>(z) - half_height) * m_tile_size;
        float const sample_x = world_x * frequency;
        float const sample_z = world_z * frequency;

        float const base_noise =
            valueNoise2D(sample_x, sample_z, settings.seed);
        float const detail_noise = valueNoise2D(
            sample_x * 2.0F, sample_z * 2.0F, settings.seed ^ 0xA21C9E37U);

        float const blended = 0.65F * base_noise + 0.35F * detail_noise;
        float perturb = (blended - 0.5F) * 2.0F * legacy_amplitude;

        if (type == TerrainType::Hill) {
          perturb *= 0.6F;
        }

        m_heights[idx] = std::max(0.0F, m_heights[idx] + perturb);
      }
    }
  }
}

void TerrainHeightMap::addRiverSegments(
    const std::vector<RiverSegment> &riverSegments) {
  m_riverSegments = riverSegments;

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;

  for (const auto &river : riverSegments) {
    QVector3D dir = river.end - river.start;
    float const length = dir.length();
    if (length < 0.01F) {
      continue;
    }

    dir.normalize();
    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());

    int const steps = static_cast<int>(std::ceil(length / m_tile_size)) + 1;

    for (int i = 0; i < steps; ++i) {
      float const t =
          static_cast<float>(i) / std::max(1.0F, static_cast<float>(steps - 1));
      QVector3D const center_pos = river.start + dir * (length * t);

      float const grid_center_x =
          (center_pos.x() / m_tile_size) + grid_half_width;
      float const grid_center_z =
          (center_pos.z() / m_tile_size) + grid_half_height;

      float const half_width = river.width * 0.5F / m_tile_size;

      int const min_x = std::max(
          0, static_cast<int>(std::floor(grid_center_x - half_width - 1.0F)));
      int const max_x = std::min(
          m_width - 1,
          static_cast<int>(std::ceil(grid_center_x + half_width + 1.0F)));
      int const min_z = std::max(
          0, static_cast<int>(std::floor(grid_center_z - half_width - 1.0F)));
      int const max_z = std::min(
          m_height - 1,
          static_cast<int>(std::ceil(grid_center_z + half_width + 1.0F)));

      for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
          float const dx = static_cast<float>(x) - grid_center_x;
          float const dz = static_cast<float>(z) - grid_center_z;

          float const dist_along_perp =
              std::abs(dx * perpendicular.x() + dz * perpendicular.z());

          if (dist_along_perp <= half_width) {
            int const idx = indexAt(x, z);
            if (m_terrain_types[idx] != TerrainType::Mountain) {
              m_terrain_types[idx] = TerrainType::River;
              m_heights[idx] = 0.0F;
            }
          }
        }
      }
    }
  }
}

void TerrainHeightMap::addBridges(const std::vector<Bridge> &bridges) {
  constexpr float kBridgeSinkMin = 0.25F;
  constexpr float kBridgeSinkMax = 0.65F;
  constexpr float kBridgeWalkableHalfWidth = 0.45F;
  constexpr float kBridgeEntryMarginTiles = 1.0F;

  m_bridges.clear();
  m_bridges.reserve(bridges.size());

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;

  for (const auto &bridge : bridges) {

    const float sink_amount =
        std::clamp(bridge.width * 0.25F, kBridgeSinkMin, kBridgeSinkMax);

    Bridge adjusted = bridge;
    float const start_ground = getHeightAt(bridge.start.x(), bridge.start.z());
    float const end_ground = getHeightAt(bridge.end.x(), bridge.end.z());
    adjusted.start.setY(std::max(bridge.start.y(), start_ground - sink_amount));
    adjusted.end.setY(std::max(bridge.end.y(), end_ground - sink_amount));

    QVector3D dir = adjusted.end - adjusted.start;
    float const length = dir.length();
    if (length < 0.01F) {
      continue;
    }

    m_bridges.push_back(adjusted);
    const Bridge &stored_bridge = m_bridges.back();

    dir.normalize();
    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());

    float const entry_margin = m_tile_size * kBridgeEntryMarginTiles;
    float const extended_length = length + (entry_margin * 2.0F);
    int const steps =
        static_cast<int>(std::ceil(extended_length / m_tile_size)) + 1;

    for (int i = 0; i < steps; ++i) {
      float const t =
          static_cast<float>(i) / std::max(1.0F, static_cast<float>(steps - 1));
      float const along = -entry_margin + extended_length * t;
      float const t_curve =
          std::clamp(along / std::max(length, 0.01F), 0.0F, 1.0F);
      QVector3D const center_pos = stored_bridge.start + dir * along;

      float const arch_curve = 4.0F * t_curve * (1.0F - t_curve);
      float const arch_height = stored_bridge.height * arch_curve * 0.8F;
      float const base_deck_height =
          stored_bridge.start.y() + stored_bridge.height + arch_height * 0.5F;
      float const terrain_height = getHeightAt(center_pos.x(), center_pos.z());
      float const deck_height = std::max(base_deck_height - sink_amount,
                                         terrain_height - sink_amount);

      float const grid_center_x =
          (center_pos.x() / m_tile_size) + grid_half_width;
      float const grid_center_z =
          (center_pos.z() / m_tile_size) + grid_half_height;

      int const min_x =
          std::max(0, static_cast<int>(std::floor(grid_center_x -
                                                  kBridgeWalkableHalfWidth)));
      int const max_x =
          std::min(m_width - 1, static_cast<int>(std::ceil(
                                    grid_center_x + kBridgeWalkableHalfWidth)));
      int const min_z =
          std::max(0, static_cast<int>(std::floor(grid_center_z -
                                                  kBridgeWalkableHalfWidth)));
      int const max_z = std::min(
          m_height - 1, static_cast<int>(std::ceil(grid_center_z +
                                                   kBridgeWalkableHalfWidth)));

      for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
          float const dx = static_cast<float>(x) - grid_center_x;
          float const dz = static_cast<float>(z) - grid_center_z;

          float const dist_along_perp =
              std::abs(dx * perpendicular.x() + dz * perpendicular.z());

          if (dist_along_perp <= kBridgeWalkableHalfWidth) {
            int const idx = indexAt(x, z);

            if (m_terrain_types[idx] == TerrainType::River) {
              m_terrain_types[idx] = TerrainType::Flat;

              m_heights[idx] = deck_height;
            }
          }
        }
      }
    }
  }

  precomputeBridgeData();
}

void TerrainHeightMap::precomputeBridgeData() {

  const size_t grid_size = static_cast<size_t>(m_width * m_height);
  m_onBridge.clear();
  m_onBridge.resize(grid_size, false);
  m_bridgeCenters.clear();
  m_bridgeCenters.resize(grid_size, QVector3D(0.0F, 0.0F, 0.0F));

  constexpr float kBridgeWalkableHalfWidth = 0.45F;
  constexpr float kBridgeEntryMarginTiles = 1.0F;
  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;

  for (const auto &bridge : m_bridges) {
    QVector3D dir = bridge.end - bridge.start;
    float const length = dir.length();
    if (length < 0.01F) {
      continue;
    }

    dir.normalize();
    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());

    float const entry_margin = m_tile_size * kBridgeEntryMarginTiles;
    float const extended_length = length + (entry_margin * 2.0F);
    int const steps =
        static_cast<int>(std::ceil(extended_length / m_tile_size)) + 1;

    for (int i = 0; i < steps; ++i) {
      float const t =
          static_cast<float>(i) / std::max(1.0F, static_cast<float>(steps - 1));
      float const along = -entry_margin + extended_length * t;
      QVector3D const center_pos = bridge.start + dir * along;

      float const grid_center_x =
          (center_pos.x() / m_tile_size) + grid_half_width;
      float const grid_center_z =
          (center_pos.z() / m_tile_size) + grid_half_height;

      int const min_x =
          std::max(0, static_cast<int>(std::floor(grid_center_x -
                                                  kBridgeWalkableHalfWidth)));
      int const max_x =
          std::min(m_width - 1, static_cast<int>(std::ceil(
                                    grid_center_x + kBridgeWalkableHalfWidth)));
      int const min_z =
          std::max(0, static_cast<int>(std::floor(grid_center_z -
                                                  kBridgeWalkableHalfWidth)));
      int const max_z = std::min(
          m_height - 1, static_cast<int>(std::ceil(grid_center_z +
                                                   kBridgeWalkableHalfWidth)));

      for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
          float const dx = static_cast<float>(x) - grid_center_x;
          float const dz = static_cast<float>(z) - grid_center_z;

          float const dist_along_perp =
              std::abs(dx * perpendicular.x() + dz * perpendicular.z());

          if (dist_along_perp <= kBridgeWalkableHalfWidth) {
            int const idx = indexAt(x, z);

            m_onBridge[idx] = true;

            float const cell_world_x =
                (static_cast<float>(x) - grid_half_width) * m_tile_size;
            float const cell_world_z =
                (static_cast<float>(z) - grid_half_height) * m_tile_size;
            QVector3D const cell_point(cell_world_x, 0.0F, cell_world_z);
            QVector3D const to_cell = cell_point - bridge.start;
            float const center_along = QVector3D::dotProduct(to_cell, dir);
            float const clamped_along = std::clamp(center_along, 0.0F, length);
            m_bridgeCenters[idx] = bridge.start + dir * clamped_along;
          }
        }
      }
    }
  }
}

void TerrainHeightMap::restoreFromData(
    const std::vector<float> &heights,
    const std::vector<TerrainType> &terrain_types,
    const std::vector<RiverSegment> &rivers,
    const std::vector<Bridge> &bridges) {

  const auto expected_size = static_cast<size_t>(m_width * m_height);

  if (heights.size() == expected_size) {
    m_heights = heights;
  }

  if (terrain_types.size() == expected_size) {
    m_terrain_types = terrain_types;
  }

  m_hillEntrances.clear();
  m_hillEntrances.resize(expected_size, false);
  m_hillWalkable.clear();
  m_hillWalkable.resize(expected_size, true);

  for (size_t i = 0; i < m_terrain_types.size(); ++i) {
    if (m_terrain_types[i] == TerrainType::Hill) {
      m_hillWalkable[i] = false;
    }
  }

  m_riverSegments = rivers;
  m_bridges = bridges;

  precomputeBridgeData();
}

auto TerrainHeightMap::getBridgeDeckHeight(float world_x, float world_z) const
    -> std::optional<float> {

  constexpr float kBridgeWalkableHalfWidth = 0.45F;

  for (const auto &bridge : m_bridges) {
    QVector3D dir = bridge.end - bridge.start;
    float const length = dir.length();
    if (length < 0.01F) {
      continue;
    }

    dir.normalize();
    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());

    QVector3D const query_point(world_x, 0.0F, world_z);
    QVector3D const to_query = query_point - bridge.start;

    float const along = QVector3D::dotProduct(to_query, dir);

    if (along < -0.5F || along > length + 0.5F) {
      continue;
    }

    float const perp_dist =
        std::abs(QVector3D::dotProduct(to_query, perpendicular));

    if (perp_dist > kBridgeWalkableHalfWidth) {
      continue;
    }

    float const t = std::clamp(along / length, 0.0F, 1.0F);

    float const arch_curve = 4.0F * t * (1.0F - t);
    float const arch_height = bridge.height * arch_curve * 0.8F;

    float const deck_height =
        bridge.start.y() + bridge.height + arch_height * 0.3F;

    return deck_height;
  }

  return std::nullopt;
}

auto TerrainHeightMap::isOnBridge(float world_x, float world_z) const -> bool {

  if (m_onBridge.empty()) {
    return false;
  }

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;
  const int grid_x =
      static_cast<int>(std::round((world_x / m_tile_size) + grid_half_width));
  const int grid_z =
      static_cast<int>(std::round((world_z / m_tile_size) + grid_half_height));

  if (!inBounds(grid_x, grid_z)) {
    return false;
  }
  return m_onBridge[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::getBridgeCenterPosition(
    float world_x, float world_z) const -> std::optional<QVector3D> {

  if (m_onBridge.empty()) {
    return std::nullopt;
  }

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;
  const int grid_x =
      static_cast<int>(std::round((world_x / m_tile_size) + grid_half_width));
  const int grid_z =
      static_cast<int>(std::round((world_z / m_tile_size) + grid_half_height));

  if (!inBounds(grid_x, grid_z)) {
    return std::nullopt;
  }

  const int idx = indexAt(grid_x, grid_z);
  if (!m_onBridge[idx]) {
    return std::nullopt;
  }

  return m_bridgeCenters[idx];
}

} // namespace Game::Map
