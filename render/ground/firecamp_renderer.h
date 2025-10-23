#pragma once

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "firecamp_gpu.h"
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render {
namespace GL {
class Buffer;
class Renderer;

class FireCampRenderer : public IRenderPass {
public:
  FireCampRenderer();
  ~FireCampRenderer();

  void configure(const Game::Map::TerrainHeightMap &heightMap,
                 const Game::Map::BiomeSettings &biomeSettings);

  void setExplicitFireCamps(const std::vector<QVector3D> &positions,
                           const std::vector<float> &intensities = {},
                           const std::vector<float> &radii = {});

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void clear();

private:
  void generateFireCampInstances();
  void addExplicitFireCamps();

  int m_width = 0;
  int m_height = 0;
  float m_tileSize = 1.0f;

  std::vector<float> m_heightData;
  std::vector<Game::Map::TerrainType> m_terrainTypes;
  Game::Map::BiomeSettings m_biomeSettings;
  std::uint32_t m_noiseSeed = 0u;

  std::vector<FireCampInstanceGpu> m_fireCampInstances;
  std::unique_ptr<Buffer> m_fireCampInstanceBuffer;
  std::size_t m_fireCampInstanceCount = 0;
  FireCampBatchParams m_fireCampParams;
  bool m_fireCampInstancesDirty = false;

  std::vector<QVector3D> m_explicitPositions;
  std::vector<float> m_explicitIntensities;
  std::vector<float> m_explicitRadii;
};

} // namespace GL
} // namespace Render
