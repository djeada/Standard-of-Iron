#pragma once

#include <QVector3D>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

#include "game/map/scatter/spawn_validator.h"
#include "game/map/terrain.h"
#include "i_scatter_pass.h"
#include "render/decoration_gpu.h"

namespace Render::GL {
class Buffer;
class Renderer;

class BiomeRenderer : public IScatterPass {
public:
  BiomeRenderer();
  ~BiomeRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings);

  void set_light_direction(const QVector3D& dir) override;

  void submit(Renderer& renderer, ResourceManager* resources) override;

  void refresh_grass();

  void clear() override;

  [[nodiscard]] auto is_gpu_ready() const -> bool override {
    return m_grass_state.is_gpu_ready();
  }

  [[nodiscard]] auto instance_count() const -> std::size_t override {
    return m_grass_state.instances.size();
  }
  [[nodiscard]] auto
  last_sync_stats() const -> Render::Ground::Scatter::SyncStats override {
    return m_grass_state.last_sync_stats;
  }

private:
  struct GrassScatterContext {
    const Game::Map::TerrainScatterProfile& scatter_profile;
    const Render::Ground::SpawnTerrainCache& terrain_cache;
    float tile_safe;
    int chunk_size;
    std::size_t cluster_count_per_chunk;
    std::size_t background_blades_per_cell;
    const std::function<bool(float, float, std::uint32_t&)>& add_grass_blade;
    const std::function<int(Game::Map::TerrainType,
                            Game::Map::TerrainType,
                            Game::Map::TerrainType,
                            Game::Map::TerrainType)>& quad_section;
  };

  void generate_grass_instances();

  void scatter_grass_clusters(const GrassScatterContext& ctx);

  void scatter_background_grass(const GrassScatterContext& ctx);

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;

  std::vector<float> m_height_data;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noise_seed = 0U;
  float m_typical_blade_height = 0.0F;
  QVector3D m_light_direction{0.35F, 0.8F, 0.45F};

  Render::Ground::Scatter::FilteredRendererState<GrassInstanceGpu, GrassBatchParams>
      m_grass_state;
};

} // namespace Render::GL
