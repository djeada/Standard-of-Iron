#pragma once

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "olive_gpu.h"
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render::GL {
class Buffer;
class Renderer;

class OliveRenderer : public IRenderPass {
public:
  OliveRenderer();
  ~OliveRenderer() override;

  void configure(const Game::Map::TerrainHeightMap &height_map,
                 const Game::Map::BiomeSettings &biome_settings);

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void clear();

  [[nodiscard]] bool is_gpu_ready() const {
    return (m_oliveInstanceBuffer != nullptr || m_oliveInstanceCount == 0) &&
           !m_visibilityDirty;
  }

private:
  void generate_olive_instances();

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;

  std::vector<float> m_heightData;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noiseSeed = 0U;

  std::vector<OliveInstanceGpu> m_oliveInstances;
  std::unique_ptr<Buffer> m_oliveInstanceBuffer;
  std::size_t m_oliveInstanceCount = 0;
  OliveBatchParams m_oliveParams;
  bool m_oliveInstancesDirty = false;

  std::vector<OliveInstanceGpu> m_visibleInstances;
  std::uint64_t m_cachedVisibilityVersion = 0;
  bool m_visibilityDirty = true;
};

} // namespace Render::GL
