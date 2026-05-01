#pragma once

#include "../../game/map/terrain.h"
#include "../decoration_gpu.h"
#include "../i_render_pass.h"
#include "scatter_renderer_state.h"
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render::GL {
class Buffer;
class Renderer;

class BiomeRenderer : public IRenderPass {
public:
  BiomeRenderer();
  ~BiomeRenderer() override;

  void configure(const Game::Map::TerrainHeightMap &height_map,
                 const Game::Map::BiomeSettings &biome_settings);

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void refresh_grass();

  void clear();

  [[nodiscard]] bool is_gpu_ready() const {
    return m_grass_state.is_gpu_ready();
  }

  [[nodiscard]] auto instance_count() const -> std::size_t {
    return m_grass_state.instances.size();
  }

private:
  void generate_grass_instances();

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;

  std::vector<float> m_height_data;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noiseSeed = 0U;

  Render::Ground::Scatter::DirectRendererState<GrassInstanceGpu,
                                               GrassBatchParams>
      m_grass_state;
};

} // namespace Render::GL
