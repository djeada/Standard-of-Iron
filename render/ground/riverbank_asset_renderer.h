#pragma once

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "riverbank_asset_gpu.h"
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

  void configure(const std::vector<Game::Map::RiverSegment> &riverSegments,
                 const Game::Map::TerrainHeightMap &height_map,
                 const Game::Map::BiomeSettings &biome_settings);

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void clear();

private:
  void generate_asset_instances();

  std::vector<Game::Map::RiverSegment> m_riverSegments;
  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;

  std::vector<float> m_heightData;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noiseSeed = 0U;

  std::vector<RiverbankAssetInstanceGpu> m_assetInstances;
  std::unique_ptr<Buffer> m_assetInstanceBuffer;
  std::size_t m_assetInstanceCount = 0;
  RiverbankAssetBatchParams m_assetParams;
  bool m_assetInstancesDirty = false;

  std::vector<RiverbankAssetInstanceGpu> m_visibleInstances;
  std::uint64_t m_cachedVisibilityVersion = 0;
  bool m_visibilityDirty = true;
};

} 
