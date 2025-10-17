#pragma once

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "plant_gpu.h"
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render {
namespace GL {
class Buffer;
class Renderer;

class PlantRenderer : public IRenderPass {
public:
  PlantRenderer();
  ~PlantRenderer();

  void configure(const Game::Map::TerrainHeightMap &heightMap,
                 const Game::Map::BiomeSettings &biomeSettings);

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void clear();

private:
  void generatePlantInstances();

  int m_width = 0;
  int m_height = 0;
  float m_tileSize = 1.0f;

  std::vector<float> m_heightData;
  std::vector<Game::Map::TerrainType> m_terrainTypes;
  Game::Map::BiomeSettings m_biomeSettings;
  std::uint32_t m_noiseSeed = 0u;

  std::vector<PlantInstanceGpu> m_plantInstances;
  std::unique_ptr<Buffer> m_plantInstanceBuffer;
  std::size_t m_plantInstanceCount = 0;
  PlantBatchParams m_plantParams;
  bool m_plantInstancesDirty = false;
};

} // namespace GL
} // namespace Render
