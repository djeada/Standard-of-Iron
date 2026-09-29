#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

#include "biome_settings.h"
#include "terrain.h"
#include "terrain_value_noise.h"

namespace Game::Map {

using terrain_value_noise::value_noise_2d;

void TerrainHeightMap::apply_biome_variation(const BiomeSettings& settings) {
  if (m_heights.empty()) {
    return;
  }

  const auto surface_profile = make_surface_profile(settings);

  if (surface_profile.ground_irregularity_enabled) {
    apply_ground_irregularity(surface_profile);
  }
  apply_legacy_height_noise(surface_profile);
}

void TerrainHeightMap::apply_ground_irregularity(
    const TerrainSurfaceProfile& surface_profile) {
  const float amplitude =
      std::clamp(std::max(0.0F, surface_profile.irregularity_amplitude), 0.0F, 1.50F);
  if (amplitude > 0.0001F) {
    const float authored_frequency =
        std::clamp(surface_profile.irregularity_scale * 0.28F, 0.022F, 0.070F);
    const float world_extent = std::max(
        static_cast<float>(std::max(m_width, m_height) - 1) * m_tile_size, m_tile_size);

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

        const float base_clearance = 0.12F + amplitude;
        const float perturb = base_clearance + amplitude * relief;

        m_heights[idx] += perturb;
      }
    }
  }
}

void TerrainHeightMap::apply_legacy_height_noise(
    const TerrainSurfaceProfile& surface_profile) {
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

} // namespace Game::Map
