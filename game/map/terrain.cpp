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

      std::vector<int> plateau_cells;
      plateau_cells.reserve(int(M_PI * plateau_width * plateau_depth));

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
            plateau_cells.push_back(idx);
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
        }
      }

      for (int const idx : plateau_cells) {
        m_hillWalkable[idx] = true;
      }

      for (const auto &entrance : feature.entrances) {
        int const ex = int(std::round(entrance.x()));
        int const ez = int(std::round(entrance.z()));
        if (!inBounds(ex, ez)) {
          continue;
        }

        const int entrance_idx = indexAt(ex, ez);
        m_hillEntrances[entrance_idx] = true;
        m_hillWalkable[entrance_idx] = true;

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

        for (int step = 0; step < steps; ++step) {
          int const ix = int(std::round(cur_x));
          int const iz = int(std::round(cur_z));
          if (!inBounds(ix, iz)) {
            break;
          }

          const int idx = indexAt(ix, iz);

          const float cell_dx = float(ix) - grid_center_x;
          const float cell_dz = float(iz) - grid_center_z;
          const float cell_rot_x = cell_dx * cos_a + cell_dz * sin_a;
          const float cell_rot_z = -cell_dx * sin_a + cell_dz * cos_a;
          const float cell_norm_dist = std::sqrt(
              (cell_rot_x * cell_rot_x) / (slope_width * slope_width) +
              (cell_rot_z * cell_rot_z) / (slope_depth * slope_depth));

          if (cell_norm_dist > 1.1F) {
            break;
          }

          m_hillWalkable[idx] = true;
          if (m_terrain_types[idx] != TerrainType::Mountain) {
            m_terrain_types[idx] = TerrainType::Hill;
          }

          if (m_heights[idx] < feature.height * 0.25F) {
            float const t = std::clamp(cell_norm_dist, 0.0F, 1.0F);
            float const ramp_height = feature.height * (1.0F - t * 0.85F);
            m_heights[idx] = std::max(m_heights[idx], ramp_height);
          }

          for (int oz = -1; oz <= 1; ++oz) {
            for (int ox = -1; ox <= 1; ++ox) {
              if (ox == 0 && oz == 0) {
                continue;
              }
              int const nx = ix + ox;
              int const nz = iz + oz;
              if (!inBounds(nx, nz)) {
                continue;
              }

              const float n_dx = float(nx) - grid_center_x;
              const float n_dz = float(nz) - grid_center_z;
              const float n_rot_x = n_dx * cos_a + n_dz * sin_a;
              const float n_rot_z = -n_dx * sin_a + n_dz * cos_a;
              const float neighbor_norm_dist =
                  std::sqrt((n_rot_x * n_rot_x) / (slope_width * slope_width) +
                            (n_rot_z * n_rot_z) / (slope_depth * slope_depth));

              if (neighbor_norm_dist <= 1.05F) {
                int const n_idx = indexAt(nx, nz);
                if (m_terrain_types[n_idx] != TerrainType::Mountain) {
                  m_hillWalkable[n_idx] = true;
                  if (m_terrain_types[n_idx] == TerrainType::Flat) {
                    m_terrain_types[n_idx] = TerrainType::Hill;
                  }
                  if (m_heights[n_idx] < m_heights[idx] * 0.8F) {
                    m_heights[n_idx] =
                        std::max(m_heights[n_idx], m_heights[idx] * 0.7F);
                  }
                }
              }
            }
          }

          const float plateau_norm_dist = std::sqrt(
              (cell_rot_x * cell_rot_x) / (plateau_width * plateau_width) +
              (cell_rot_z * cell_rot_z) / (plateau_depth * plateau_depth));
          if (plateau_norm_dist <= 1.05F) {
            break;
          }

          cur_x += dir_x;
          cur_z += dir_z;
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

  return h0 * (1.0F - tz) + h1 * tz;
}

auto TerrainHeightMap::getHeightAtGrid(int grid_x, int grid_z) const -> float {
  if (!inBounds(grid_x, grid_z)) {
    return 0.0F;
  }
  return m_heights[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::isWalkable(int grid_x, int grid_z) const -> bool {
  if (!inBounds(grid_x, grid_z)) {
    return false;
  }

  TerrainType const type = m_terrain_types[indexAt(grid_x, grid_z)];

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

  const float amplitude = std::max(0.0F, settings.heightNoiseAmplitude);
  if (amplitude <= 0.0001F) {
    return;
  }

  const float frequency = std::max(0.0001F, settings.heightNoiseFrequency);
  const float half_width = m_width * 0.5F - 0.5F;
  const float half_height = m_height * 0.5F - 0.5F;

  for (int z = 0; z < m_height; ++z) {
    for (int x = 0; x < m_width; ++x) {
      int const idx = indexAt(x, z);
      TerrainType const type = m_terrain_types[idx];
      if (type == TerrainType::Mountain) {
        continue;
      }

      float const world_x = (static_cast<float>(x) - half_width) * m_tile_size;
      float const world_z = (static_cast<float>(z) - half_height) * m_tile_size;
      float const sample_x = world_x * frequency;
      float const sample_z = world_z * frequency;

      float const base_noise = valueNoise2D(sample_x, sample_z, settings.seed);
      float const detail_noise = valueNoise2D(sample_x * 2.0F, sample_z * 2.0F,
                                              settings.seed ^ 0xA21C9E37U);

      float const blended = 0.65F * base_noise + 0.35F * detail_noise;
      float perturb = (blended - 0.5F) * 2.0F * amplitude;

      if (type == TerrainType::Hill) {
        perturb *= 0.6F;
      }

      m_heights[idx] = std::max(0.0F, m_heights[idx] + perturb);
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
  m_bridges = bridges;

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;

  for (const auto &bridge : bridges) {
    QVector3D dir = bridge.end - bridge.start;
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
      QVector3D const center_pos = bridge.start + dir * (length * t);

      float const arch_curve = 4.0F * t * (1.0F - t);
      float const arch_height = bridge.height * arch_curve * 0.8F;
      float const deck_height =
          bridge.start.y() + bridge.height + arch_height * 0.5F;

      float const grid_center_x =
          (center_pos.x() / m_tile_size) + grid_half_width;
      float const grid_center_z =
          (center_pos.z() / m_tile_size) + grid_half_height;

      float const half_width = bridge.width * 0.5F / m_tile_size;

      int const min_x =
          std::max(0, static_cast<int>(std::floor(grid_center_x - half_width)));
      int const max_x = std::min(
          m_width - 1, static_cast<int>(std::ceil(grid_center_x + half_width)));
      int const min_z =
          std::max(0, static_cast<int>(std::floor(grid_center_z - half_width)));
      int const max_z =
          std::min(m_height - 1,
                   static_cast<int>(std::ceil(grid_center_z + half_width)));

      for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
          float const dx = static_cast<float>(x) - grid_center_x;
          float const dz = static_cast<float>(z) - grid_center_z;

          float const dist_along_perp =
              std::abs(dx * perpendicular.x() + dz * perpendicular.z());

          if (dist_along_perp <= half_width) {
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
}

} // namespace Game::Map
