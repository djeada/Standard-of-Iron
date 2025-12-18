#pragma once

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "stone_gpu.h"
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render::GL {
class Buffer;
class Renderer;

class StoneRenderer : public IRenderPass {
public:
  StoneRenderer();
  ~StoneRenderer() override;

  void configure(const Game::Map::TerrainHeightMap &height_map,
                 const Game::Map::BiomeSettings &biome_settings);

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void clear();

private:
  void generateStoneInstances();

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;

  std::vector<float> m_heightData;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noiseSeed = 0U;

  std::vector<StoneInstanceGpu> m_stoneInstances;
  std::unique_ptr<Buffer> m_stoneInstanceBuffer;
  std::size_t m_stoneInstanceCount = 0;
  StoneBatchParams m_stoneParams;
  bool m_stoneInstancesDirty = false;
};

} 
