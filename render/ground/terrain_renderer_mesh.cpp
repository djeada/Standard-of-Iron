#include <QDebug>
#include <QQuaternion>
#include <QVector2D>
#include <QtGlobal>
#include <qglobal.h>
#include <qmatrix4x4.h>
#include <qvectornd.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

#include "game/map/render_visibility_rules.h"
#include "game/map/scatter/ground_utils.h"
#include "game/map/terrain_service.h"
#include "game/map/terrain_surface.h"
#include "game/map/visibility_service.h"
#include "map/biome_settings.h"
#include "map/ground_type.h"
#include "render/frame/world_chunk.h"
#include "render/gl/mesh.h"
#include "render/gl/render_constants.h"
#include "render/gl/resources.h"
#include "render/ground/terrain_mesh_builder.h"
#include "render/ground/terrain_renderer.h"
#include "render/scene/scene_renderer.h"

namespace {

constexpr float k_chunk_tint = 1.0F;

using std::uint32_t;
using namespace Render::GL::Geometry;
using namespace Render::Ground;

inline auto apply_tint(const QVector3D& color, float tint) -> QVector3D {
  QVector3D const c = color * tint;
  return {std::clamp(c.x(), 0.0F, 1.0F),
          std::clamp(c.y(), 0.0F, 1.0F),
          std::clamp(c.z(), 0.0F, 1.0F)};
}

inline auto clamp01(const QVector3D& c) -> QVector3D {
  return {std::clamp(c.x(), 0.0F, 1.0F),
          std::clamp(c.y(), 0.0F, 1.0F),
          std::clamp(c.z(), 0.0F, 1.0F)};
}

} // namespace

namespace Render::GL {

auto TerrainRenderer::make_chunk_params(
    const Game::Map::TerrainSurfaceProfile& surface_profile,
    const Game::Map::ClimateProfile& climate_profile,
    Game::Map::TerrainType chunk_type,
    float tint) const -> TerrainChunkParams {
  return make_terrain_chunk_params(m_biome_settings,
                                   surface_profile,
                                   climate_profile,
                                   m_tile_size,
                                   m_noise_seed,
                                   chunk_type,
                                   tint);
}

auto make_terrain_chunk_params(const Game::Map::BiomeSettings& biome_settings,
                               const Game::Map::TerrainSurfaceProfile& surface_profile,
                               const Game::Map::ClimateProfile& climate_profile,
                               float tile_size,
                               std::uint32_t noise_seed,
                               Game::Map::TerrainType chunk_type,
                               float tint) -> TerrainChunkParams {
  TerrainChunkParams params;
  params.ground_type = static_cast<int>(biome_settings.ground_type);
  params.terrain_type = static_cast<int>(chunk_type);
  auto tint_color = [&](const QVector3D& base) {
    return clamp01(apply_tint(base, tint));
  };
  params.grass_primary = tint_color(surface_profile.grass_primary);
  params.grass_secondary = tint_color(surface_profile.grass_secondary);
  params.grass_dry = tint_color(surface_profile.grass_dry);
  params.soil_color = tint_color(surface_profile.soil_color);
  params.rock_low = tint_color(surface_profile.rock_low);
  params.rock_high = tint_color(surface_profile.rock_high);

  params.tile_size = std::max(0.001F, tile_size);

  params.macro_noise_scale = surface_profile.terrain_macro_noise_scale;
  params.detail_noise_scale = surface_profile.terrain_detail_noise_scale;
  params.slope_rock_threshold =
      std::clamp(surface_profile.terrain_rock_threshold, 0.018F, 0.9F);
  params.slope_rock_sharpness = std::max(1.0F, surface_profile.terrain_rock_sharpness);
  params.soil_blend_height = surface_profile.terrain_soil_height;
  params.soil_blend_sharpness = std::max(0.75F, surface_profile.terrain_soil_sharpness);

  constexpr float k_noise_offset_scale = 256.0F;
  const uint32_t noise_key_a = hash_coords(0, 0, noise_seed ^ 0xB5297A4DU);
  const uint32_t noise_key_b = hash_coords(0, 0, noise_seed ^ 0x68E31DA4U);
  params.noise_offset = QVector2D(hash_to_01(noise_key_a) * k_noise_offset_scale,
                                  hash_to_01(noise_key_b) * k_noise_offset_scale);

  params.height_noise_strength = 0.0F;
  params.height_noise_frequency = surface_profile.height_noise_frequency;

  params.ambient_boost = surface_profile.terrain_ambient_boost;
  params.rock_detail_strength = surface_profile.terrain_rock_detail_strength;

  params.tint = clamp01(QVector3D(tint, tint, tint));
  params.light_direction = QVector3D(0.65F, 0.50F, 0.40F);
  params.curvature_response = 0.52F;
  params.ridge_response = 0.46F;
  params.gully_response = 0.44F;
  params.snow_coverage = std::clamp(climate_profile.snow_coverage, 0.0F, 1.0F);
  params.moisture_level = std::clamp(climate_profile.moisture_level, 0.0F, 1.0F);
  params.crack_intensity = std::clamp(climate_profile.crack_intensity, 0.0F, 1.0F);
  params.rock_exposure = std::clamp(climate_profile.rock_exposure, 0.0F, 1.0F);
  params.grass_saturation = std::clamp(climate_profile.grass_saturation, 0.0F, 1.5F);
  params.soil_roughness = std::clamp(climate_profile.soil_roughness, 0.0F, 1.0F);
  params.snow_color = clamp01(climate_profile.snow_color);
  params.soil_foot_height = 0.18F;
  params.screen_toe_mul = 0.82F;
  params.screen_toe_clamp = 0.20F;

  return params;
}

void TerrainRenderer::build_meshes() {
  m_chunks.clear();
  m_chunk_visibility_cache.clear();

  const auto surface_profile = Game::Map::make_surface_profile(m_biome_settings);
  const auto climate_profile = Game::Map::make_climate_profile(m_biome_settings);
  const Render::Ground::TerrainMeshBuilder builder(
      {.width = m_width,
       .height = m_height,
       .tile_size = m_tile_size,
       .heights = m_height_data,
       .terrain_types = m_terrain_types,
       .hill_entrances = m_hill_entrances});
  auto geometry = builder.build();
  m_chunks.reserve(geometry.size());
  for (auto& part : geometry) {
    ChunkMesh chunk;
    chunk.mesh = std::make_unique<Mesh>(part.vertices, part.indices);
    chunk.type = part.type;
    chunk.cull_center = (part.bounds_min + part.bounds_max) * 0.5F;
    chunk.cull_radius =
        (part.bounds_max - chunk.cull_center).length() + m_tile_size * 0.5F;
    chunk.params =
        make_chunk_params(surface_profile, climate_profile, chunk.type, k_chunk_tint);
    m_chunks.push_back(std::move(chunk));
  }
  m_chunk_visibility_cache.assign(m_chunks.size(), {});
}

} // namespace Render::GL
