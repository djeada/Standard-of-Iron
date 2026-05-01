#pragma once

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "riverbank_asset_gpu.h"
#include "scatter_renderer_state.h"
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render::GL {
class Buffer;
class Renderer;

class RiverbankAssetRenderer : public IRenderPass {
public:
  RiverbankAssetRenderer();
  ~RiverbankAssetRenderer() override;

  void configure(const std::vector<Game::Map::RiverSegment> &river_segments,
                 const Game::Map::TerrainHeightMap &height_map,
                 const Game::Map::BiomeSettings &biome_settings);

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void clear();

private:
  void generate_asset_instances();

  std::vector<Game::Map::RiverSegment> m_river_segments;
  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;

  std::vector<float> m_height_data;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noiseSeed = 0U;

  Render::Ground::Scatter::FilteredRendererState<RiverbankAssetInstanceGpu,
                                                 RiverbankAssetBatchParams>
      m_asset_state;
};

} // namespace Render::GL
