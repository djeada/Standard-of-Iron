#pragma once

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "pine_gpu.h"
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render {
namespace GL {
class Buffer;
class Renderer;

class PineRenderer : public IRenderPass {
public:
  PineRenderer();
  ~PineRenderer();

  void configure(const Game::Map::TerrainHeightMap &heightMap,
                 const Game::Map::BiomeSettings &biomeSettings);

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void clear();

private:
  void generatePineInstances();

  int m_width = 0;
  int m_height = 0;
  float m_tileSize = 1.0f;

  std::vector<float> m_heightData;
  std::vector<Game::Map::TerrainType> m_terrainTypes;
  Game::Map::BiomeSettings m_biomeSettings;
  std::uint32_t m_noiseSeed = 0u;

  std::vector<PineInstanceGpu> m_pineInstances;
  std::unique_ptr<Buffer> m_pineInstanceBuffer;
  std::size_t m_pineInstanceCount = 0;
  PineBatchParams m_pineParams;
  bool m_pineInstancesDirty = false;
};

} // namespace GL
} // namespace Render
