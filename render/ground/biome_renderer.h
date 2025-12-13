#pragma once

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "grass_gpu.h"
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
                 const Game::Map::BiomeSettings &biomeSettings);

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void refreshGrass();

  void clear();

private:
  void generate_grass_instances();

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;

  std::vector<float> m_heightData;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  Game::Map::BiomeSettings m_biomeSettings;
  std::uint32_t m_noiseSeed = 0U;

  std::vector<GrassInstanceGpu> m_grassInstances;
  std::unique_ptr<Buffer> m_grassInstanceBuffer;
  std::size_t m_grassInstanceCount = 0;
  GrassBatchParams m_grassParams;
  bool m_grassInstancesDirty = false;
};

} // namespace Render::GL
