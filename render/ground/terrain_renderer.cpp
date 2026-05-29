#include "terrain_renderer.h"

#include "../gl/mesh.h"

namespace Render::GL {

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
  build_meshes();
}

void TerrainRenderer::set_light_direction(const QVector3D& dir) {
  m_light_direction = dir.isNull() ? QVector3D(0.65F, 0.50F, 0.40F) : dir.normalized();
}

} // namespace Render::GL
