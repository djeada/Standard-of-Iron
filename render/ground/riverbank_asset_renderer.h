#pragma once

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "riverbank_asset_gpu.h"
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render {
namespace GL {
class Buffer;
class Renderer;

class RiverbankAssetRenderer : public IRenderPass {
public:
  RiverbankAssetRenderer();
  ~RiverbankAssetRenderer();

  void configure(const std::vector<Game::Map::RiverSegment> &riverSegments,
                 const Game::Map::TerrainHeightMap &heightMap,
                 const Game::Map::BiomeSettings &biomeSettings);

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void clear();

private:
  void generateAssetInstances();

  std::vector<Game::Map::RiverSegment> m_riverSegments;
  int m_width = 0;
  int m_height = 0;
  float m_tileSize = 1.0f;

  std::vector<float> m_heightData;
  std::vector<Game::Map::TerrainType> m_terrainTypes;
  Game::Map::BiomeSettings m_biomeSettings;
  std::uint32_t m_noiseSeed = 0u;

  std::vector<RiverbankAssetInstanceGpu> m_assetInstances;
  std::unique_ptr<Buffer> m_assetInstanceBuffer;
  std::size_t m_assetInstanceCount = 0;
  RiverbankAssetBatchParams m_assetParams;
  bool m_assetInstancesDirty = false;
};

} // namespace GL
} // namespace Render
