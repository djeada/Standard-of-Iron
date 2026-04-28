#pragma once

#include "../../game/map/terrain.h"
#include "../decoration_gpu.h"
#include "../i_render_pass.h"
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render::GL {
class Buffer;
class Renderer;

class PineRenderer : public IRenderPass {
public:
  PineRenderer();
  ~PineRenderer() override;

  void configure(const Game::Map::TerrainHeightMap &height_map,
                 const Game::Map::BiomeSettings &biome_settings);

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void clear();

  [[nodiscard]] bool is_gpu_ready() const {
    if (m_pineInstances.empty()) {
      return true;
    }
    if (!m_visibility_dirty && m_visible_instances.empty()) {
      return true;
    }
    return (m_pineInstanceBuffer != nullptr) && !m_visibility_dirty;
  }

  [[nodiscard]] auto instance_count() const -> std::size_t {
    return m_pineInstances.size();
  }

private:
  void generate_pine_instances();

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;

  std::vector<float> m_height_data;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noiseSeed = 0U;

  std::vector<PineInstanceGpu> m_pineInstances;
  std::unique_ptr<Buffer> m_pineInstanceBuffer;
  std::size_t m_pineInstanceCount = 0;
  PineBatchParams m_pineParams;
  bool m_pineInstancesDirty = false;

  std::vector<PineInstanceGpu> m_visible_instances;
  std::uint64_t m_cachedVisibilityVersion = 0;
  bool m_visibility_dirty = true;
};

} // namespace Render::GL
