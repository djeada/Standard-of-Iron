#pragma once

#include "../../game/map/terrain.h"
#include "grass_gpu.h"
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render {
namespace GL {
class Buffer;
class Renderer;

class BiomeRenderer {
public:
  BiomeRenderer();
  ~BiomeRenderer();

  void configure(const Game::Map::TerrainHeightMap &heightMap,
                 const Game::Map::BiomeSettings &biomeSettings);

  void submit(Renderer &renderer);

  void refreshGrass();

  void clear();

private:
  void generateGrassInstances();

  int m_width = 0;
  int m_height = 0;
  float m_tileSize = 1.0f;

  std::vector<float> m_heightData;
  std::vector<Game::Map::TerrainType> m_terrainTypes;
  Game::Map::BiomeSettings m_biomeSettings;
  std::uint32_t m_noiseSeed = 0u;

  std::vector<GrassInstanceGpu> m_grassInstances;
  std::unique_ptr<Buffer> m_grassInstanceBuffer;
  std::size_t m_grassInstanceCount = 0;
  GrassBatchParams m_grassParams;
  bool m_grassInstancesDirty = false;
};

} // namespace GL
} // namespace Render
