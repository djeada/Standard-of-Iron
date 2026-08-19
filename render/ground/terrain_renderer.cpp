#include "terrain_renderer.h"

#include <algorithm>
#include <cstddef>

#include "render/gl/mesh.h"
#include "render/gl/shader.h"

namespace Render::GL {

namespace {

constexpr float k_ground_fog_low_percentile = 0.08F;
constexpr float k_ground_fog_high_percentile = 0.92F;
constexpr float k_ground_fog_min_relief = 0.9F;
constexpr float k_ground_fog_full_relief = 3.5F;
constexpr float k_ground_fog_ceiling_fraction = 0.40F;
constexpr float k_ground_fog_min_thickness = 0.7F;
constexpr float k_ground_fog_max_thickness = 2.6F;

} // namespace

TerrainRenderer::TerrainRenderer() = default;
TerrainRenderer::~TerrainRenderer() = default;

void TerrainRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                                const Game::Map::BiomeSettings& biome_settings) {
  m_width = height_map.get_width();
  m_height = height_map.get_height();
  m_tile_size = height_map.get_tile_size();

  m_height_data = height_map.get_height_data();
  m_terrain_types = height_map.getTerrainTypes();
  m_hill_entrances = height_map.getHillEntrances();
  m_biome_settings = biome_settings;
  m_noise_seed = biome_settings.seed;
  m_height_texture_dirty = true;
  m_terrain_fields_dirty = true;
  m_noise_atlas_dirty = true;
  m_ground_fog = compute_ground_fog(m_height_data);
  build_meshes();
}

auto TerrainRenderer::compute_ground_fog(const std::vector<float>& heights)
    -> Render::GroundFogSettings {
  Render::GroundFogSettings fog;
  if (heights.size() < 4) {
    return fog;
  }
  std::vector<float> sorted(heights);
  std::sort(sorted.begin(), sorted.end());
  const auto percentile = [&sorted](float fraction) {
    const auto index = static_cast<std::size_t>(std::clamp(fraction, 0.0F, 1.0F) *
                                                static_cast<float>(sorted.size() - 1));
    return sorted[index];
  };
  const float low = percentile(k_ground_fog_low_percentile);
  const float high = percentile(k_ground_fog_high_percentile);
  const float relief = high - low;
  if (relief < k_ground_fog_min_relief) {
    return fog;
  }
  fog.floor_y = low;
  fog.ceiling_y = low + std::clamp(relief * k_ground_fog_ceiling_fraction,
                                   k_ground_fog_min_thickness,
                                   k_ground_fog_max_thickness);
  fog.strength = std::clamp(
      (relief - k_ground_fog_min_relief) / k_ground_fog_full_relief, 0.0F, 1.0F);
  return fog;
}

void TerrainRenderer::set_light_direction(const QVector3D& dir) {
  m_light_direction = dir.isNull() ? QVector3D(0.65F, 0.50F, 0.40F) : dir.normalized();
}

} // namespace Render::GL
