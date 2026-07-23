#include "terrain.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <math.h>
#include <numbers>
#include <vector>

#include "terrain_landform.h"

namespace {
constexpr float k_deg_to_rad = std::numbers::pi_v<float> / 180.0F;

constexpr int k_hill_ramp_extra_steps = 7;

constexpr float k_hill_ramp_steepness_exponent = 0.92F;

constexpr float k_entry_ramp_width = 3.0F;

constexpr float k_width_falloff_padding = 2.50F;

constexpr float k_entry_bowl_exponent = 1.30F;

constexpr float k_entry_base_width_scale = 1.72F;
constexpr float k_entry_top_width_scale = 1.12F;

constexpr float k_entry_outward_steps_fraction = 1.08F;
constexpr int k_entry_outward_steps_min = 12;
constexpr int k_entry_outward_steps_max = 32;

constexpr float k_entry_mid_dip_strength = 0.08F;

constexpr float k_entry_mid_depth_strength = 0.06F;

constexpr float k_entry_toe_height_fraction = 0.0F;

constexpr float k_walkable_width_threshold = 0.38F;

constexpr float k_entry_lower_ramp_delay = 0.0F;
constexpr float k_entry_mouth_flare_strength = 0.38F;
constexpr float k_entry_mouth_soften_strength = 0.045F;
constexpr float k_entry_floor_flatten_strength = 0.075F;
constexpr float k_entry_shoulder_raise_strength = 0.065F;

inline auto hash_coords(int x, int z, std::uint32_t seed) -> std::uint32_t {
  std::uint32_t const ux = static_cast<std::uint32_t>(x) * 73856093U;
  std::uint32_t const uz = static_cast<std::uint32_t>(z) * 19349663U;
  std::uint32_t const s = seed * 83492791U + 0x9e3779b9U;
  return ux ^ uz ^ s;
}

inline auto hash_to_float01(std::uint32_t h) -> float {
  h ^= h >> 17;
  h *= 0xed5ad4bbU;
  h ^= h >> 11;
  h *= 0xac4c1b51U;
  h ^= h >> 15;
  h *= 0x31848babU;
  h ^= h >> 14;
  return (h & 0x00FFFFFFU) / float(0x01000000);
}

inline auto value_noise_2d(float x, float z, std::uint32_t seed) -> float {
  int const ix0 = static_cast<int>(std::floor(x));
  int const iz0 = static_cast<int>(std::floor(z));
  int const ix1 = ix0 + 1;
  int const iz1 = iz0 + 1;

  float const tx = x - static_cast<float>(ix0);
  float const tz = z - static_cast<float>(iz0);
  const auto fade = [](float value) {
    return value * value * value * (value * (value * 6.0F - 15.0F) + 10.0F);
  };
  const float sx = fade(tx);
  const float sz = fade(tz);

  float const n00 = hash_to_float01(hash_coords(ix0, iz0, seed));
  float const n10 = hash_to_float01(hash_coords(ix1, iz0, seed));
  float const n01 = hash_to_float01(hash_coords(ix0, iz1, seed));
  float const n11 = hash_to_float01(hash_coords(ix1, iz1, seed));

  float const nx0 = n00 * (1.0F - sx) + n10 * sx;
  float const nx1 = n01 * (1.0F - sx) + n11 * sx;
  return nx0 * (1.0F - sz) + nx1 * sz;
}

} // namespace

namespace Game::Map {

void TerrainField::clear() {
  width = 0;
  height = 0;
  tile_size = 1.0F;
  heights.clear();
  slopes.clear();
  curvature.clear();
}

auto TerrainField::empty() const -> bool {
  return width <= 0 || height <= 0 || heights.empty();
}

auto TerrainField::sample_height_at(float gx, float gz) const -> float {
  if (empty()) {
    return 0.0F;
  }

  gx = std::clamp(gx, 0.0F, static_cast<float>(width - 1));
  gz = std::clamp(gz, 0.0F, static_cast<float>(height - 1));

  int const x0 = static_cast<int>(std::floor(gx));
  int const z0 = static_cast<int>(std::floor(gz));
  int const x1 = std::min(x0 + 1, width - 1);
  int const z1 = std::min(z0 + 1, height - 1);

  float const tx = gx - static_cast<float>(x0);
  float const tz = gz - static_cast<float>(z0);

  float const h00 = heights[static_cast<size_t>(z0 * width + x0)];
  float const h10 = heights[static_cast<size_t>(z0 * width + x1)];
  float const h01 = heights[static_cast<size_t>(z1 * width + x0)];
  float const h11 = heights[static_cast<size_t>(z1 * width + x1)];

  float const h0 = h00 * (1.0F - tx) + h10 * tx;
  float const h1 = h01 * (1.0F - tx) + h11 * tx;
  return h0 * (1.0F - tz) + h1 * tz;
}

auto TerrainField::sample_slope_at(int grid_x, int grid_z) const -> float {
  if (slopes.empty() || grid_x < 0 || grid_x >= width || grid_z < 0 ||
      grid_z >= height) {
    return 0.0F;
  }

  return slopes[static_cast<size_t>(grid_z * width + grid_x)];
}

auto TerrainField::sample_curvature_at(int grid_x, int grid_z) const -> float {
  if (curvature.empty() || grid_x < 0 || grid_x >= width || grid_z < 0 ||
      grid_z >= height) {
    return 0.0F;
  }

  return curvature[static_cast<size_t>(grid_z * width + grid_x)];
}

TerrainHeightMap::TerrainHeightMap(int width, int height, float tile_size)
    : m_width(width)
    , m_height(height)
    , m_tile_size(tile_size) {
  const int count = width * height;
  m_heights.resize(count, 0.0F);
  m_terrain_types.resize(count, TerrainType::Flat);
  m_hill_entrances.resize(count, false);
  m_hill_walkable.resize(count, false);
}

void TerrainHeightMap::build_from_features(
    const std::vector<TerrainFeature>& features) {

  const std::vector<float> base_heights = m_heights;
  const int map_cell_count = m_width * m_height;
  std::vector<float> erosion_strength(static_cast<std::size_t>(map_cell_count), 0.0F);
  std::vector<std::uint8_t> erosion_protected(static_cast<std::size_t>(map_cell_count),
                                              0);
  std::fill(m_terrain_types.begin(), m_terrain_types.end(), TerrainType::Flat);
  std::fill(m_hill_entrances.begin(), m_hill_entrances.end(), false);
  std::fill(m_hill_walkable.begin(), m_hill_walkable.end(), false);

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;

  for (const auto& feature : features) {

    const float grid_center_x = (feature.center_x / m_tile_size) + grid_half_width;
    const float grid_center_z = (feature.center_z / m_tile_size) + grid_half_height;
    const float grid_radius = std::max(feature.radius / m_tile_size, 1.0F);

    if (feature.type == TerrainType::Mountain) {

      const bool has_authored_extents = feature.width > 0.0F && feature.depth > 0.0F;
      const bool campaign_landform_scale = std::max(m_width, m_height) >= 128;
      const float mountain_height =
          feature.height * (campaign_landform_scale ? 1.90F : 1.0F);
      const float major_radius =
          has_authored_extents ? std::max(feature.width * 0.5F / m_tile_size, 2.0F)
                               : std::max(grid_radius * 1.38F, grid_radius + 6.0F);
      const float minor_radius =
          has_authored_extents ? std::max(feature.depth * 0.5F / m_tile_size, 2.0F)
                               : std::max(grid_radius * 0.55F, 5.0F);
      const float bound = std::max(major_radius, minor_radius) + 2.0F;
      const int min_x = std::max(0, int(std::floor(grid_center_x - bound)));
      const int max_x = std::min(m_width - 1, int(std::ceil(grid_center_x + bound)));
      const int min_z = std::max(0, int(std::floor(grid_center_z - bound)));
      const int max_z = std::min(m_height - 1, int(std::ceil(grid_center_z + bound)));

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
            (hash_to_float01(hash_coords(int(std::round(grid_center_x)),
                                         int(std::round(grid_center_z)),
                                         0x5A17U)) -
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
            int const idx = indexAt(x, z);
            float const height = base_heights[idx] + feature_height;
            if (height > m_heights[idx]) {
              m_heights[idx] = height;
              m_terrain_types[idx] = TerrainType::Mountain;
              erosion_strength[idx] = std::max(erosion_strength[idx], 1.0F);
            }
          }
        }
      }
      continue;
    }

    if (feature.type == TerrainType::Hill) {
      float grid_width = std::max(feature.width / m_tile_size, 1.0F);
      float grid_depth = std::max(feature.depth / m_tile_size, 1.0F);

      const bool campaign_landform_scale = std::max(m_width, m_height) >= 128;
      const bool radius_authored =
          feature.radius > 0.0F &&
          std::abs(feature.width - feature.radius * 2.0F) < 0.01F &&
          std::abs(feature.depth - feature.radius * 2.0F) < 0.01F;
      float hill_rotation_deg = feature.rotation_deg;
      if (campaign_landform_scale && radius_authored) {

        grid_width *= 1.18F;
        hill_rotation_deg += hash_to_float01(hash_coords(int(std::round(grid_center_x)),
                                                         int(std::round(grid_center_z)),
                                                         0x81E7U)) *
                             180.0F;
      }
      const float authored_hill_height =
          feature.height * (campaign_landform_scale ? 2.80F : 1.0F);

      const float footprint_height =
          std::min(grid_width, grid_depth) * m_tile_size * 0.18F;
      const float hill_height = campaign_landform_scale
                                    ? std::max(authored_hill_height, footprint_height)
                                    : authored_hill_height;

      const float slope_width = std::max(2.0F, grid_width * 0.50F);
      const float slope_depth = std::max(2.0F, grid_depth * 0.50F);
      const float elevation_cells = std::max(hill_height / m_tile_size, 0.25F);
      const float slope_run = campaign_landform_scale
                                  ? std::max(7.0F, elevation_cells * 4.2F)
                                  : std::max(3.25F, elevation_cells * 3.15F);
      const float minimum_crown_fraction = campaign_landform_scale ? 0.42F : 0.68F;
      const float maximum_slope_fraction = campaign_landform_scale ? 0.62F : 0.46F;
      const float plateau_width = std::max(
          {1.5F,
           slope_width * minimum_crown_fraction,
           slope_width - std::min(slope_width * maximum_slope_fraction, slope_run)});
      const float plateau_depth = std::max(
          {1.5F,
           slope_depth * minimum_crown_fraction,
           slope_depth - std::min(slope_depth * maximum_slope_fraction, slope_run)});

      const float max_extent = std::max(slope_width, slope_depth) * 1.18F;
      const int min_x = std::max(0, int(std::floor(grid_center_x - max_extent - 1.0F)));
      const int max_x =
          std::min(m_width - 1, int(std::ceil(grid_center_x + max_extent + 1.0F)));
      const int min_z = std::max(0, int(std::floor(grid_center_z - max_extent - 1.0F)));
      const int max_z =
          std::min(m_height - 1, int(std::ceil(grid_center_z + max_extent + 1.0F)));

      const int map_cell_count = m_width * m_height;
      std::vector<std::uint8_t> walkable_mask(map_cell_count, 0);
      std::vector<std::uint8_t> entrance_line_mask(map_cell_count, 0);
      std::vector<int> entrance_indices;

      const float angle_rad = hill_rotation_deg * k_deg_to_rad;
      const float cos_a = std::cos(angle_rad);
      const float sin_a = std::sin(angle_rad);
      const float feature_phase =
          grid_center_x * 0.083F + grid_center_z * 0.127F + hill_height * 0.31F;
      const auto hill_seed =
          0x6A09E667U ^ static_cast<std::uint32_t>(
                            std::abs(grid_center_x * 29.0F + grid_center_z * 43.0F));
      const Landform::HillConfig hill_config{
          .outer_radius_x = slope_width,
          .outer_radius_z = slope_depth,
          .crown_radius_x = plateau_width,
          .crown_radius_z = plateau_depth,
          .height = hill_height,
          .phase = feature_phase,
          .seed = hill_seed,
          .rounded_crown = campaign_landform_scale,
      };

      std::vector<QVector3D> hill_entrances = feature.entrances;
      if (hill_entrances.empty()) {

        const float fallback_distance = slope_width * m_tile_size * 0.98F;
        hill_entrances.emplace_back(feature.center_x - cos_a * fallback_distance,
                                    0.0F,
                                    feature.center_z - sin_a * fallback_distance);
      }

      auto slope_distance = [&](float local_x, float local_z) {
        return Landform::sample_hill(local_x, local_z, hill_config).outer_distance;
      };
      auto crown_distance = [&](float local_x, float local_z) {
        return Landform::sample_hill(local_x, local_z, hill_config).crown_distance;
      };

      for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
          const float dx = float(x) - grid_center_x;
          const float dz = float(z) - grid_center_z;

          const float rotated_x = dx * cos_a + dz * sin_a;
          const float rotated_z = -dx * sin_a + dz * cos_a;

          const auto landform =
              Landform::sample_hill(rotated_x, rotated_z, hill_config);
          const float norm_plateau_dist = landform.crown_distance;
          const float norm_slope_dist = landform.outer_distance;

          if (norm_slope_dist > 1.0F) {
            continue;
          }

          const int idx = indexAt(x, z);

          const float height = hill_height * landform.elevation_fraction;

          float const surface_height = base_heights[idx] + height;
          if (surface_height > m_heights[idx]) {
            m_heights[idx] = surface_height;
            m_terrain_types[idx] = TerrainType::Hill;
            erosion_strength[idx] = std::max(
                erosion_strength[idx],
                0.50F + 0.18F * (1.0F - height / std::max(hill_height, 0.001F)));
          }
          if (norm_plateau_dist <= 1.0F && m_terrain_types[idx] == TerrainType::Hill) {
            walkable_mask[idx] = 1;
            erosion_protected[idx] = 1;
          }
        }
      }

      struct EntranceCluster {
        float grid_x{};
        float grid_z{};
        float radius{};
        int samples{};
      };
      std::vector<EntranceCluster> entrance_clusters;
      entrance_clusters.reserve(hill_entrances.size());
      constexpr float k_entrance_cluster_distance = 5.0F;
      for (const auto& entrance : hill_entrances) {
        const float entrance_gx = (entrance.x() / m_tile_size) + grid_half_width;
        const float entrance_gz = (entrance.z() / m_tile_size) + grid_half_height;
        auto cluster_it =
            std::find_if(entrance_clusters.begin(),
                         entrance_clusters.end(),
                         [&](const EntranceCluster& cluster) {
                           return std::hypot(entrance_gx - cluster.grid_x,
                                             entrance_gz - cluster.grid_z) <=
                                  k_entrance_cluster_distance;
                         });
        if (cluster_it == entrance_clusters.end()) {
          entrance_clusters.push_back({.grid_x = entrance_gx,
                                       .grid_z = entrance_gz,
                                       .radius = 0.0F,
                                       .samples = 1});
          continue;
        }

        const float previous_x = cluster_it->grid_x;
        const float previous_z = cluster_it->grid_z;
        cluster_it->samples += 1;
        const float weight = 1.0F / float(cluster_it->samples);
        cluster_it->grid_x += (entrance_gx - cluster_it->grid_x) * weight;
        cluster_it->grid_z += (entrance_gz - cluster_it->grid_z) * weight;
        const float center_shift = std::hypot(cluster_it->grid_x - previous_x,
                                              cluster_it->grid_z - previous_z);
        cluster_it->radius = std::max(cluster_it->radius + center_shift,
                                      std::hypot(entrance_gx - cluster_it->grid_x,
                                                 entrance_gz - cluster_it->grid_z));
      }

      for (const auto& entrance : entrance_clusters) {
        const float entrance_gx = entrance.grid_x;
        const float entrance_gz = entrance.grid_z;
        int const ex = int(std::round(entrance_gx));
        int const ez = int(std::round(entrance_gz));
        if (!in_bounds(ex, ez)) {
          continue;
        }

        const int entrance_idx = indexAt(ex, ez);
        m_hill_entrances[entrance_idx] = true;
        entrance_indices.push_back(entrance_idx);
        if (m_terrain_types[entrance_idx] != TerrainType::Mountain) {
          if (m_terrain_types[entrance_idx] == TerrainType::Flat) {
            m_terrain_types[entrance_idx] = TerrainType::Hill;
          }
          walkable_mask[entrance_idx] = 1;
          entrance_line_mask[entrance_idx] = 1;
          erosion_protected[entrance_idx] = 1;
          m_hill_walkable[entrance_idx] = true;
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
        auto smooth_range = [&](float a, float b, float x) {
          return smoothstep((x - a) / std::max(1e-4F, b - a));
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
            if (!in_bounds(ix, iz)) {
              break;
            }
            const float cell_dx = float(ix) - grid_center_x;
            const float cell_dz = float(iz) - grid_center_z;
            const float cell_rot_x = cell_dx * cos_a + cell_dz * sin_a;
            const float cell_rot_z = -cell_dx * sin_a + cell_dz * cos_a;
            const float plateau_norm_dist = crown_distance(cell_rot_x, cell_rot_z);
            if (plateau_norm_dist <= 1.0F) {
              plateau_steps = std::max(1, step);
              break;
            }
            test_x += dir_x;
            test_z += dir_z;
          }
        }

        const int ramp_steps = std::min(steps, plateau_steps + k_hill_ramp_extra_steps);

        int const outward_steps = std::clamp(
            int(std::round(float(ramp_steps) * k_entry_outward_steps_fraction)),
            k_entry_outward_steps_min,
            k_entry_outward_steps_max);
        int const total_ramp_steps = outward_steps + ramp_steps;

        float const hill_min_extent = std::min(plateau_width, plateau_depth);
        const float authored_entry_width =
            (campaign_landform_scale ? 7.25F : k_entry_ramp_width) + entrance.radius;
        float const entry_width =
            std::max(1.5F, std::min(authored_entry_width, hill_min_extent * 0.62F));

        float const perp_x = -dir_z;
        float const perp_z = dir_x;

        cur_x = float(ex) - dir_x * float(outward_steps);
        cur_z = float(ez) - dir_z * float(outward_steps);

        for (int ramp_step = 0; ramp_step < total_ramp_steps; ++ramp_step) {
          bool const is_outward = ramp_step < outward_steps;
          int const center_ix = int(std::round(cur_x));
          int const center_iz = int(std::round(cur_z));
          if (!in_bounds(center_ix, center_iz)) {

            cur_x += dir_x;
            cur_z += dir_z;
            continue;
          }

          const float cell_dx = float(center_ix) - grid_center_x;
          const float cell_dz = float(center_iz) - grid_center_z;
          const float cell_rot_x = cell_dx * cos_a + cell_dz * sin_a;
          const float cell_rot_z = -cell_dx * sin_a + cell_dz * cos_a;
          const float cell_norm_dist = slope_distance(cell_rot_x, cell_rot_z);

          bool const builds_apron = is_outward || cell_norm_dist > 1.0F;

          float const ramp_progress =
              (total_ramp_steps > 1)
                  ? std::clamp(
                        float(ramp_step) / float(total_ramp_steps - 1), 0.0F, 1.0F)
                  : 1.0F;

          float const delayed_progress =
              std::clamp((ramp_progress - k_entry_lower_ramp_delay) /
                             std::max(1e-4F, 1.0F - k_entry_lower_ramp_delay),
                         0.0F,
                         1.0F);
          float const s = smootherstep(delayed_progress);
          float const mid = 4.0F * ramp_progress * (1.0F - ramp_progress);
          float const lower_ramp = 1.0F - smooth_range(0.58F, 0.95F, s);
          float const mouth = 1.0F - smooth_range(0.18F, 0.58F, s);

          float const height_base = std::pow(s, k_hill_ramp_steepness_exponent);
          float const height_frac = std::clamp(
              height_base * (1.0F - k_entry_mid_dip_strength * mid), 0.0F, 1.0F);

          float const toe_frac = k_entry_toe_height_fraction * (1.0F - s) * (1.0F - s);
          float center_ramp_height = hill_height * std::max(height_frac, toe_frac);
          center_ramp_height *=
              std::clamp(1.0F - k_entry_mid_depth_strength * mid, 0.0F, 1.0F);

          float const width_scale =
              (1.0F - s) * k_entry_base_width_scale + s * k_entry_top_width_scale;
          float tapered_width =
              std::max(1.0F,
                       entry_width * width_scale *
                           (1.0F + k_entry_mouth_flare_strength * mouth));

          if (is_outward && outward_steps > 0) {
            float const outward_t =
                float(ramp_step) / float(std::max(1, outward_steps));

            float const outward_width_mul = 0.82F + 0.18F * smootherstep(outward_t);
            tapered_width = std::max(1.0F, tapered_width * outward_width_mul);
          }

          int const width_radius =
              int(std::ceil(tapered_width + k_width_falloff_padding));
          for (int w = -width_radius; w <= width_radius; ++w) {
            float const offset_x = cur_x + perp_x * float(w);
            float const offset_z = cur_z + perp_z * float(w);
            int const ix = int(std::round(offset_x));
            int const iz = int(std::round(offset_z));

            if (!in_bounds(ix, iz)) {
              continue;
            }

            float const edge_t = smooth_range(tapered_width * 0.16F,
                                              tapered_width + k_width_falloff_padding,
                                              std::abs(float(w)));

            int const ramp_idx = indexAt(ix, iz);
            if (m_terrain_types[ramp_idx] != TerrainType::Mountain) {
              float const width_factor = 1.0F - edge_t;

              constexpr bool raised_apron = true;
              if (m_terrain_types[ramp_idx] == TerrainType::Flat && raised_apron &&
                  width_factor > 0.04F) {
                m_terrain_types[ramp_idx] = TerrainType::Hill;
              }

              if ((builds_apron || width_factor > k_walkable_width_threshold) &&
                  raised_apron) {
                walkable_mask[ramp_idx] = 1;
                entrance_line_mask[ramp_idx] = 1;
                erosion_protected[ramp_idx] = 1;
                m_hill_entrances[ramp_idx] = true;
              }

              float const existing_height = m_heights[ramp_idx];

              float const bowl = std::pow(edge_t, k_entry_bowl_exponent);
              float target_height =
                  (1.0F - bowl) * (base_heights[ramp_idx] + center_ramp_height) +
                  bowl * existing_height;
              float const floor_core = smooth_range(0.22F, 0.82F, width_factor);
              float const shoulder_band =
                  smooth_range(0.16F, 0.46F, width_factor) *
                  (1.0F - smooth_range(0.60F, 0.90F, width_factor));
              float const mouth_soften = hill_height * k_entry_mouth_soften_strength *
                                         floor_core * mouth * (1.0F - s);

              float const center_channel =
                  std::pow(std::clamp(width_factor, 0.0F, 1.0F), 4.0F);
              float const floor_flatten = hill_height * k_entry_floor_flatten_strength *
                                          center_channel * (0.35F + 0.65F * lower_ramp);
              float const apron_shoulder_blend =
                  builds_apron ? smooth_range(0.48F, 0.88F, ramp_progress) : 1.0F;
              float const shoulder_raise =
                  hill_height * k_entry_shoulder_raise_strength * shoulder_band *
                  (0.25F + 0.75F * lower_ramp) * apron_shoulder_blend;
              target_height = std::max(
                  0.0F, target_height - mouth_soften - floor_flatten + shoulder_raise);

              m_heights[ramp_idx] = target_height;
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
          const float norm_plateau_dist = crown_distance(rotated_x, rotated_z);

          if (norm_plateau_dist > 1.0F) {
            walkable_mask[idx] = 0;
          }

          if (norm_plateau_dist > 0.85F) {
            bool adjacent_to_non_hill = false;
            constexpr int k_dirs[8][2] = {
                {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
            for (const auto& dir : k_dirs) {
              int const nx = x + dir[0];
              int const nz = z + dir[1];
              if (!in_bounds(nx, nz)) {
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
            for (const auto& dir : k_dirs) {
              int const nx = x + dir[0];
              int const nz = z + dir[1];
              if (!in_bounds(nx, nz)) {
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
          m_hill_walkable[entrance_idx] = true;
          queue.push_back(entrance_idx);

          while (!queue.empty()) {
            int const idx = queue.back();
            queue.pop_back();

            int const cx = idx % m_width;
            int const cz = idx / m_width;

            constexpr int k_dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
            for (const auto& dir : k_dirs) {
              int const nx = cx + dir[0];
              int const nz = cz + dir[1];
              if (!in_bounds(nx, nz)) {
                continue;
              }
              int const n_idx = indexAt(nx, nz);
              if (visited[n_idx] || walkable_mask[n_idx] == 0) {
                continue;
              }

              visited[n_idx] = 1;
              m_hill_walkable[n_idx] = true;
              queue.push_back(n_idx);
            }
          }
        }
      }

      continue;
    }

    if (feature.type == TerrainType::Forest || feature.type == TerrainType::River) {
      const float half_width =
          feature.width > 0.0F ? feature.width * 0.5F / m_tile_size : grid_radius;
      const float half_depth =
          feature.depth > 0.0F ? feature.depth * 0.5F / m_tile_size : grid_radius;
      const int min_x = std::max(0, int(std::floor(grid_center_x - half_width - 1.0F)));
      const int max_x =
          std::min(m_width - 1, int(std::ceil(grid_center_x + half_width + 1.0F)));
      const int min_z = std::max(0, int(std::floor(grid_center_z - half_depth - 1.0F)));
      const int max_z =
          std::min(m_height - 1, int(std::ceil(grid_center_z + half_depth + 1.0F)));

      for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
          const float normalized_x =
              (float(x) - grid_center_x) / std::max(half_width, 0.0001F);
          const float normalized_z =
              (float(z) - grid_center_z) / std::max(half_depth, 0.0001F);
          if (normalized_x * normalized_x + normalized_z * normalized_z > 1.0F) {
            continue;
          }

          const int idx = indexAt(x, z);
          if (feature.type == TerrainType::River) {
            m_heights[idx] = 0.0F;
            m_terrain_types[idx] = TerrainType::River;
          } else if (m_terrain_types[idx] == TerrainType::Flat) {
            m_terrain_types[idx] = TerrainType::Forest;
          }
        }
      }
      continue;
    }

    const float half_width =
        feature.width > 0.0F ? feature.width * 0.5F / m_tile_size : grid_radius;
    const float half_depth =
        feature.depth > 0.0F ? feature.depth * 0.5F / m_tile_size : grid_radius;
    const float bound = std::max(half_width, half_depth) + 1.0F;
    const int min_x = std::max(0, int(std::floor(grid_center_x - bound)));
    const int max_x = std::min(m_width - 1, int(std::ceil(grid_center_x + bound)));
    const int min_z = std::max(0, int(std::floor(grid_center_z - bound)));
    const int max_z = std::min(m_height - 1, int(std::ceil(grid_center_z + bound)));
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

        int const idx = indexAt(x, z);

        const float feather =
            std::clamp((1.0F - normalized_distance) / 0.20F, 0.0F, 1.0F);
        const float blend = feather * feather * (3.0F - 2.0F * feather);
        m_heights[idx] = m_heights[idx] * (1.0F - blend) + feature.height * blend;
        if (blend >= 0.5F) {
          m_terrain_types[idx] = TerrainType::Flat;
          m_hill_entrances[idx] = false;
          m_hill_walkable[idx] = false;
          erosion_strength[idx] = 0.0F;
          erosion_protected[idx] = 1;
        }
      }
    }
  }

  for (int idx = 0; idx < map_cell_count; ++idx) {
    if (m_hill_walkable[idx]) {
      erosion_protected[idx] = 1;
    }
  }
  if (std::max(m_width, m_height) >= 128) {
    Landform::apply_constrained_erosion(
        m_heights, erosion_strength, erosion_protected, m_width, m_height, m_tile_size);
  }
}

auto TerrainHeightMap::get_height_at(float world_x, float world_z) const -> float {
  const float base_height = get_base_height_at(world_x, world_z);
  if (isOnBridge(world_x, world_z)) {
    if (auto const bridge_height = getBridgeDeckHeight(world_x, world_z);
        bridge_height.has_value()) {
      return *bridge_height;
    }
  }
  return base_height;
}

auto TerrainHeightMap::get_base_height_at(float world_x, float world_z) const -> float {

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;

  float const gx = world_x / m_tile_size + grid_half_width;
  float const gz = world_z / m_tile_size + grid_half_height;

  int const x0 = int(std::floor(gx));
  int const z0 = int(std::floor(gz));
  int const x1 = x0 + 1;
  int const z1 = z0 + 1;

  if (!in_bounds(x0, z0)) {
    return 0.0F;
  }

  float const tx = gx - x0;
  float const tz = gz - z0;

  float const h00 = in_bounds(x0, z0) ? m_heights[indexAt(x0, z0)] : 0.0F;
  float const h10 = in_bounds(x1, z0) ? m_heights[indexAt(x1, z0)] : 0.0F;
  float const h01 = in_bounds(x0, z1) ? m_heights[indexAt(x0, z1)] : 0.0F;
  float const h11 = in_bounds(x1, z1) ? m_heights[indexAt(x1, z1)] : 0.0F;

  float const h0 = h00 * (1.0F - tx) + h10 * tx;
  float const h1 = h01 * (1.0F - tx) + h11 * tx;

  return h0 * (1.0F - tz) + h1 * tz;
}

auto TerrainHeightMap::get_height_at_grid(int grid_x, int grid_z) const -> float {
  if (!in_bounds(grid_x, grid_z)) {
    return 0.0F;
  }
  return m_heights[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::is_walkable(int grid_x, int grid_z) const -> bool {
  if (!in_bounds(grid_x, grid_z)) {
    return false;
  }

  int const idx = indexAt(grid_x, grid_z);
  if (!m_on_bridge.empty() && m_on_bridge[idx]) {
    return true;
  }

  TerrainType const type = m_terrain_types[idx];

  if (type == TerrainType::Mountain) {
    return false;
  }

  if (is_water_terrain(type)) {
    return false;
  }

  if (type == TerrainType::Hill) {
    return m_hill_walkable[indexAt(grid_x, grid_z)];
  }

  return true;
}

auto TerrainHeightMap::isHillEntrance(int grid_x, int grid_z) const -> bool {
  if (!in_bounds(grid_x, grid_z)) {
    return false;
  }
  return m_hill_entrances[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::getTerrainType(int grid_x, int grid_z) const -> TerrainType {
  if (!in_bounds(grid_x, grid_z)) {
    return TerrainType::Flat;
  }
  return m_terrain_types[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::isRiverOrNearby(int grid_x,
                                       int grid_z,
                                       int margin) const -> bool {
  if (!in_bounds(grid_x, grid_z)) {
    return false;
  }

  if (is_water_terrain(m_terrain_types[indexAt(grid_x, grid_z)])) {
    return true;
  }

  for (int dz = -margin; dz <= margin; ++dz) {
    for (int dx = -margin; dx <= margin; ++dx) {
      if (dx == 0 && dz == 0) {
        continue;
      }
      int const nx = grid_x + dx;
      int const nz = grid_z + dz;
      if (in_bounds(nx, nz) && is_water_terrain(m_terrain_types[indexAt(nx, nz)])) {
        return true;
      }
    }
  }

  return false;
}

auto TerrainHeightMap::indexAt(int x, int z) const -> int {
  return z * m_width + x;
}

auto TerrainHeightMap::in_bounds(int x, int z) const -> bool {
  return x >= 0 && x < m_width && z >= 0 && z < m_height;
}

auto TerrainHeightMap::calculateFeatureHeight(const TerrainFeature& feature,
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

void TerrainHeightMap::apply_biome_variation(const BiomeSettings& settings) {
  if (m_heights.empty()) {
    return;
  }

  const auto surface_profile = make_surface_profile(settings);

  if (surface_profile.ground_irregularity_enabled) {
    const float amplitude = std::clamp(
        std::max(0.0F, surface_profile.irregularity_amplitude) * 18.0F, 0.75F, 2.20F);
    if (amplitude > 0.0001F) {
      const float authored_frequency =
          std::clamp(surface_profile.irregularity_scale * 0.28F, 0.022F, 0.070F);
      const float world_extent =
          std::max(static_cast<float>(std::max(m_width, m_height) - 1) * m_tile_size,
                   m_tile_size);

      const float frequency = std::max(authored_frequency, 2.5F / world_extent);
      const float half_width = m_width * 0.5F - 0.5F;
      const float half_height = m_height * 0.5F - 0.5F;

      for (int z = 0; z < m_height; ++z) {
        for (int x = 0; x < m_width; ++x) {
          int const idx = indexAt(x, z);
          TerrainType const type = m_terrain_types[idx];

          if (type != TerrainType::Flat) {
            continue;
          }

          float const world_x = (static_cast<float>(x) - half_width) * m_tile_size;
          float const world_z = (static_cast<float>(z) - half_height) * m_tile_size;
          float const sample_x = world_x * frequency;
          float const sample_z = world_z * frequency;

          const float warp_x = (value_noise_2d(sample_x * 0.43F,
                                               sample_z * 0.43F,
                                               surface_profile.seed ^ 0x19B4C7A1U) -
                                0.5F) *
                               1.65F;
          const float warp_z = (value_noise_2d(sample_x * 0.43F + 23.7F,
                                               sample_z * 0.43F - 11.3F,
                                               surface_profile.seed ^ 0x63D2E95BU) -
                                0.5F) *
                               1.65F;
          const float warped_x = sample_x + warp_x;
          const float warped_z = sample_z + warp_z;

          const float regional_noise = value_noise_2d(
              warped_x * 0.52F, warped_z * 0.52F, surface_profile.seed ^ 0xC36E71D9U);
          const float base_noise =
              value_noise_2d(warped_x, warped_z, surface_profile.seed);
          const float detail_noise = value_noise_2d(
              warped_x * 2.25F, warped_z * 2.25F, surface_profile.seed ^ 0xA21C9E37U);
          const float fine_noise = value_noise_2d(
              warped_x * 4.8F, warped_z * 4.8F, surface_profile.seed ^ 0x7E4B92F1U);

          const float regional_signed = regional_noise * 2.0F - 1.0F;
          const float rolling_signed = base_noise * 2.0F - 1.0F;
          const float detail_signed = detail_noise * 2.0F - 1.0F;
          const float fine_signed = fine_noise * 2.0F - 1.0F;
          const float drainage =
              std::pow(std::clamp(1.0F - std::abs(detail_signed), 0.0F, 1.0F), 6.0F);

          const float relief = regional_signed * 0.46F + rolling_signed * 0.29F +
                               detail_signed * 0.13F + fine_signed * 0.035F -
                               drainage * 0.055F;
          const float perturb = std::max(amplitude * (0.78F + relief), 0.10F);

          m_heights[idx] += perturb;
        }
      }
    }
  }

  const float legacy_amplitude = std::max(0.0F, surface_profile.height_noise_amplitude);
  if (legacy_amplitude > 0.0001F) {
    const float frequency = std::max(0.0001F, surface_profile.height_noise_frequency);
    const float half_width = m_width * 0.5F - 0.5F;
    const float half_height = m_height * 0.5F - 0.5F;

    for (int z = 0; z < m_height; ++z) {
      for (int x = 0; x < m_width; ++x) {
        int const idx = indexAt(x, z);
        TerrainType const type = m_terrain_types[idx];
        if (type == TerrainType::Mountain || is_water_terrain(type)) {
          continue;
        }

        float const world_x = (static_cast<float>(x) - half_width) * m_tile_size;
        float const world_z = (static_cast<float>(z) - half_height) * m_tile_size;
        float const sample_x = world_x * frequency;
        float const sample_z = world_z * frequency;

        float const base_noise =
            value_noise_2d(sample_x, sample_z, surface_profile.seed);
        float const detail_noise = value_noise_2d(
            sample_x * 2.0F, sample_z * 2.0F, surface_profile.seed ^ 0xA21C9E37U);

        float const blended = 0.65F * base_noise + 0.35F * detail_noise;
        float perturb = (blended - 0.5F) * 2.0F * legacy_amplitude;

        if (type == TerrainType::Hill) {
          perturb *= 0.6F;
        } else if (type == TerrainType::Flat) {
          perturb = perturb * 0.85F + legacy_amplitude * 0.28F;
        }

        m_heights[idx] = std::max(0.0F, m_heights[idx] + perturb);
      }
    }
  }
}

void TerrainHeightMap::add_river_segments(
    const std::vector<RiverSegment>& river_segments) {
  m_river_segments.clear();
  m_river_segments.reserve(river_segments.size());

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;

  auto water_surface_height = [this](const QVector3D& point) {
    const float shoreline_blend = m_tile_size * 1.25F;
    for (const auto& lake : m_lakes) {
      if (point_in_lake(lake, point.x(), point.z(), shoreline_blend)) {
        return lake.center.y();
      }
    }
    return get_height_at(point.x(), point.z());
  };

  for (const auto& authored_river : river_segments) {
    RiverSegment river = authored_river;
    if (river.elevation_mode == WaterElevationMode::Terrain) {
      river.start.setY(water_surface_height(river.start));
      river.end.setY(water_surface_height(river.end));
    }
    m_river_segments.push_back(river);
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

      float const grid_center_x = (center_pos.x() / m_tile_size) + grid_half_width;
      float const grid_center_z = (center_pos.z() / m_tile_size) + grid_half_height;

      float const half_width = river.width * 0.5F / m_tile_size;

      int const min_x =
          std::max(0, static_cast<int>(std::floor(grid_center_x - half_width - 1.0F)));
      int const max_x = std::min(
          m_width - 1, static_cast<int>(std::ceil(grid_center_x + half_width + 1.0F)));
      int const min_z =
          std::max(0, static_cast<int>(std::floor(grid_center_z - half_width - 1.0F)));
      int const max_z = std::min(
          m_height - 1, static_cast<int>(std::ceil(grid_center_z + half_width + 1.0F)));

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
              m_heights[idx] = center_pos.y() - 0.10F;
            }
          }
        }
      }
    }
  }
}

void TerrainHeightMap::add_lakes(const std::vector<Lake>& lakes) {
  m_lakes.clear();
  m_lakes.reserve(lakes.size());
  const float half_grid_width = m_width * 0.5F - 0.5F;
  const float half_grid_height = m_height * 0.5F - 0.5F;

  for (const auto& authored_lake : lakes) {
    Lake lake = authored_lake;
    if (lake.elevation_mode == WaterElevationMode::Terrain) {
      lake.center.setY(get_height_at(lake.center.x(), lake.center.z()));
    }
    m_lakes.push_back(lake);
    const float extent = std::max(lake.width, lake.depth) * 0.55F;
    const float grid_center_x = lake.center.x() / m_tile_size + half_grid_width;
    const float grid_center_z = lake.center.z() / m_tile_size + half_grid_height;
    const float grid_extent = extent / m_tile_size + 1.0F;
    const int min_x =
        std::max(0, static_cast<int>(std::floor(grid_center_x - grid_extent)));
    const int max_x =
        std::min(m_width - 1, static_cast<int>(std::ceil(grid_center_x + grid_extent)));
    const int min_z =
        std::max(0, static_cast<int>(std::floor(grid_center_z - grid_extent)));
    const int max_z = std::min(
        m_height - 1, static_cast<int>(std::ceil(grid_center_z + grid_extent)));

    for (int z = min_z; z <= max_z; ++z) {
      for (int x = min_x; x <= max_x; ++x) {
        const float world_x = (static_cast<float>(x) - half_grid_width) * m_tile_size;
        const float world_z = (static_cast<float>(z) - half_grid_height) * m_tile_size;
        const bool gameplay_water = point_in_lake(lake, world_x, world_z);

        const bool submerged_margin =
            point_in_lake(lake, world_x, world_z, m_tile_size * 0.85F);
        if (!submerged_margin) {
          continue;
        }
        const int index = indexAt(x, z);
        if (m_terrain_types[index] == TerrainType::Mountain) {
          continue;
        }
        const float bed_height = lake.center.y() - 0.10F;
        if (gameplay_water) {
          m_terrain_types[index] = TerrainType::Lake;
          m_heights[index] = bed_height;
        } else {
          m_heights[index] = std::min(m_heights[index], bed_height);
        }
      }
    }
  }
}

void TerrainHeightMap::add_bridges(const std::vector<Bridge>& bridges) {
  constexpr float k_bridge_sink_min = 0.25F;
  constexpr float k_bridge_sink_max = 0.65F;
  constexpr float k_bridge_entry_margin_tiles = 1.0F;

  m_bridges.clear();
  m_bridges.reserve(bridges.size());

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;

  for (const auto& bridge : bridges) {
    Bridge adjusted = bridge;
    adjusted.width = std::max(adjusted.width, k_min_bridge_width);
    const float sink_amount =
        std::clamp(adjusted.width * 0.25F, k_bridge_sink_min, k_bridge_sink_max);
    adjusted.height = bridge_effective_height(adjusted);
    float const start_ground = get_height_at(bridge.start.x(), bridge.start.z());
    float const end_ground = get_height_at(bridge.end.x(), bridge.end.z());
    adjusted.start.setY(std::max(bridge.start.y(), start_ground - sink_amount));
    adjusted.end.setY(std::max(bridge.end.y(), end_ground - sink_amount));

    QVector3D dir = adjusted.end - adjusted.start;
    float const length = dir.length();
    if (length < 0.01F) {
      continue;
    }

    m_bridges.push_back(adjusted);
    const Bridge& stored_bridge = m_bridges.back();

    dir.normalize();
    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
    float const bridge_half_width =
        stored_bridge.width * 0.5F / std::max(m_tile_size, 0.0001F);

    float const entry_margin = m_tile_size * k_bridge_entry_margin_tiles;
    float const extended_length = length + (entry_margin * 2.0F);
    int const steps = static_cast<int>(std::ceil(extended_length / m_tile_size)) + 1;

    for (int i = 0; i < steps; ++i) {
      float const t =
          static_cast<float>(i) / std::max(1.0F, static_cast<float>(steps - 1));
      float const along = -entry_margin + extended_length * t;
      float const t_curve = std::clamp(along / std::max(length, 0.01F), 0.0F, 1.0F);
      QVector3D const center_pos = stored_bridge.start + dir * along;

      float const visual_deck_height = bridge_deck_world_y(stored_bridge, t_curve);

      float const relief_deck_height = visual_deck_height - k_bridge_deck_visual_lift;
      float const terrain_height = get_height_at(center_pos.x(), center_pos.z());
      float const deck_height =
          std::max(relief_deck_height - sink_amount, terrain_height - sink_amount);

      float const grid_center_x = (center_pos.x() / m_tile_size) + grid_half_width;
      float const grid_center_z = (center_pos.z() / m_tile_size) + grid_half_height;

      int const min_x =
          std::max(0, static_cast<int>(std::floor(grid_center_x - bridge_half_width)));
      int const max_x = std::min(
          m_width - 1, static_cast<int>(std::ceil(grid_center_x + bridge_half_width)));
      int const min_z =
          std::max(0, static_cast<int>(std::floor(grid_center_z - bridge_half_width)));
      int const max_z = std::min(
          m_height - 1, static_cast<int>(std::ceil(grid_center_z + bridge_half_width)));

      for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
          float const dx = static_cast<float>(x) - grid_center_x;
          float const dz = static_cast<float>(z) - grid_center_z;

          float const dist_along_perp =
              std::abs(dx * perpendicular.x() + dz * perpendicular.z());

          if (dist_along_perp <= bridge_half_width) {
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

  precompute_bridge_data();
}

void TerrainHeightMap::precompute_bridge_data() {

  const size_t grid_size = static_cast<size_t>(m_width * m_height);
  m_on_bridge.clear();
  m_on_bridge.resize(grid_size, false);
  m_bridge_centerline.clear();
  m_bridge_centerline.resize(grid_size, false);
  m_bridge_centers.clear();
  m_bridge_centers.resize(grid_size, QVector3D(0.0F, 0.0F, 0.0F));

  constexpr float k_bridge_entry_margin_tiles = 1.0F;
  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;

  for (const auto& bridge : m_bridges) {
    QVector3D dir = bridge.end - bridge.start;
    float const length = dir.length();
    if (length < 0.01F) {
      continue;
    }

    dir.normalize();
    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
    float const bridge_half_width =
        bridge.width * 0.5F / std::max(m_tile_size, 0.0001F);

    float const entry_margin = m_tile_size * k_bridge_entry_margin_tiles;
    float const extended_length = length + (entry_margin * 2.0F);
    int const steps = static_cast<int>(std::ceil(extended_length / m_tile_size)) + 1;

    for (int i = 0; i < steps; ++i) {
      float const t =
          static_cast<float>(i) / std::max(1.0F, static_cast<float>(steps - 1));
      float const along = -entry_margin + extended_length * t;
      QVector3D const center_pos = bridge.start + dir * along;

      int const center_x = static_cast<int>(
          std::round((center_pos.x() / m_tile_size) + grid_half_width));
      int const center_z = static_cast<int>(
          std::round((center_pos.z() / m_tile_size) + grid_half_height));
      if (in_bounds(center_x, center_z)) {
        m_bridge_centerline[indexAt(center_x, center_z)] = true;
      }

      float const grid_center_x = (center_pos.x() / m_tile_size) + grid_half_width;
      float const grid_center_z = (center_pos.z() / m_tile_size) + grid_half_height;

      int const min_x =
          std::max(0, static_cast<int>(std::floor(grid_center_x - bridge_half_width)));
      int const max_x = std::min(
          m_width - 1, static_cast<int>(std::ceil(grid_center_x + bridge_half_width)));
      int const min_z =
          std::max(0, static_cast<int>(std::floor(grid_center_z - bridge_half_width)));
      int const max_z = std::min(
          m_height - 1, static_cast<int>(std::ceil(grid_center_z + bridge_half_width)));

      for (int z = min_z; z <= max_z; ++z) {
        for (int x = min_x; x <= max_x; ++x) {
          float const dx = static_cast<float>(x) - grid_center_x;
          float const dz = static_cast<float>(z) - grid_center_z;

          float const dist_along_perp =
              std::abs(dx * perpendicular.x() + dz * perpendicular.z());

          if (dist_along_perp <= bridge_half_width) {
            int const idx = indexAt(x, z);

            m_on_bridge[idx] = true;

            float const cell_world_x =
                (static_cast<float>(x) - grid_half_width) * m_tile_size;
            float const cell_world_z =
                (static_cast<float>(z) - grid_half_height) * m_tile_size;
            QVector3D const cell_point(cell_world_x, 0.0F, cell_world_z);
            QVector3D const to_cell = cell_point - bridge.start;
            float const center_along = QVector3D::dotProduct(to_cell, dir);
            float const clamped_along = std::clamp(center_along, 0.0F, length);
            m_bridge_centers[idx] = bridge.start + dir * clamped_along;
          }
        }
      }
    }
  }
}

void TerrainHeightMap::restore_from_data(const std::vector<float>& heights,
                                         const std::vector<TerrainType>& terrain_types,
                                         const std::vector<RiverSegment>& rivers,
                                         const std::vector<Bridge>& bridges,
                                         const std::vector<Lake>& lakes) {

  const auto expected_size = static_cast<size_t>(m_width * m_height);

  if (heights.size() == expected_size) {
    m_heights = heights;
  }

  if (terrain_types.size() == expected_size) {
    m_terrain_types = terrain_types;
  }

  m_hill_entrances.clear();
  m_hill_entrances.resize(expected_size, false);
  m_hill_walkable.clear();
  m_hill_walkable.resize(expected_size, true);

  for (size_t i = 0; i < m_terrain_types.size(); ++i) {
    if (m_terrain_types[i] == TerrainType::Hill) {
      m_hill_walkable[i] = false;
    }
  }

  m_river_segments = rivers;
  m_lakes = lakes;
  m_bridges = bridges;
  for (Bridge& bridge : m_bridges) {
    bridge.width = std::max(bridge.width, k_min_bridge_width);
  }

  precompute_bridge_data();
}

auto TerrainHeightMap::getBridgeDeckHeight(float world_x, float world_z) const
    -> std::optional<float> {

  for (const auto& bridge : m_bridges) {
    QVector3D dir = bridge.end - bridge.start;
    float const length = dir.length();
    if (length < 0.01F) {
      continue;
    }

    dir.normalize();
    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
    float const bridge_half_width = bridge.width * 0.5F;

    QVector3D const query_point(world_x, 0.0F, world_z);
    QVector3D const to_query = query_point - bridge.start;

    float const along = QVector3D::dotProduct(to_query, dir);

    if (along < -0.5F || along > length + 0.5F) {
      continue;
    }

    float const perp_dist = std::abs(QVector3D::dotProduct(to_query, perpendicular));

    if (perp_dist > bridge_half_width) {
      continue;
    }

    float const t = std::clamp(along / length, 0.0F, 1.0F);

    return bridge_deck_world_y(bridge, t);
  }

  return std::nullopt;
}

auto TerrainHeightMap::isOnBridge(float world_x, float world_z) const -> bool {

  if (m_on_bridge.empty()) {
    return false;
  }

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;
  const int grid_x =
      static_cast<int>(std::round((world_x / m_tile_size) + grid_half_width));
  const int grid_z =
      static_cast<int>(std::round((world_z / m_tile_size) + grid_half_height));

  if (!in_bounds(grid_x, grid_z)) {
    return false;
  }
  return m_on_bridge[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::isBridgeCell(int grid_x, int grid_z) const -> bool {
  if (!in_bounds(grid_x, grid_z) || m_on_bridge.empty()) {
    return false;
  }
  return m_on_bridge[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::isBridgeCenterline(int grid_x, int grid_z) const -> bool {
  if (!in_bounds(grid_x, grid_z) || m_bridge_centerline.empty()) {
    return false;
  }
  return m_bridge_centerline[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::getBridgeCenterPosition(float world_x, float world_z) const
    -> std::optional<QVector3D> {

  if (m_on_bridge.empty()) {
    return std::nullopt;
  }

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;
  const int grid_x =
      static_cast<int>(std::round((world_x / m_tile_size) + grid_half_width));
  const int grid_z =
      static_cast<int>(std::round((world_z / m_tile_size) + grid_half_height));

  if (!in_bounds(grid_x, grid_z)) {
    return std::nullopt;
  }

  const int idx = indexAt(grid_x, grid_z);
  if (!m_on_bridge[idx]) {
    return std::nullopt;
  }

  return m_bridge_centers[idx];
}

auto TerrainHeightMap::getBridgeTraversalPosition(float world_x, float world_z) const
    -> std::optional<QVector3D> {

  for (const auto& bridge : m_bridges) {
    QVector3D dir = bridge.end - bridge.start;
    float const length = dir.length();
    if (length < 0.01F) {
      continue;
    }

    dir.normalize();
    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
    float const entry_margin = bridge_crossing_entry_margin(bridge.width, m_tile_size);
    float const alignment_half_width =
        bridge_crossing_alignment_half_width(bridge.width, m_tile_size);

    QVector3D const query_point(world_x, 0.0F, world_z);
    QVector3D const to_query = query_point - bridge.start;
    float const along = QVector3D::dotProduct(to_query, dir);
    if (along < -entry_margin || along > length + entry_margin) {
      continue;
    }

    float const perp_dist = std::abs(QVector3D::dotProduct(to_query, perpendicular));
    if (perp_dist > alignment_half_width) {
      continue;
    }

    return bridge.start + dir * along;
  }

  return std::nullopt;
}

} // namespace Game::Map
