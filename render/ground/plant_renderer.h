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

class PlantRenderer : public IRenderPass {
public:
  PlantRenderer();
  ~PlantRenderer() override;

  void configure(const Game::Map::TerrainHeightMap &height_map,
                 const Game::Map::BiomeSettings &biome_settings);

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void clear();

  [[nodiscard]] bool is_gpu_ready() const {
    if (m_plantInstances.empty()) {
      return true;
    }
    if (!m_visibility_dirty && m_visible_instances.empty()) {
      return true;
    }
    return (m_visibleInstanceBuffer != nullptr) && !m_visibility_dirty;
  }

  [[nodiscard]] auto instance_count() const -> std::size_t {
    return m_plantInstances.size();
  }

private:
  void generate_plant_instances();

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;

  std::vector<float> m_height_data;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noiseSeed = 0U;

  std::vector<PlantInstanceGpu> m_plantInstances;
  std::unique_ptr<Buffer> m_visibleInstanceBuffer;
  std::size_t m_plantInstanceCount = 0;
  PlantBatchParams m_plantParams;
  bool m_plantInstancesDirty = false;

  std::vector<PlantInstanceGpu> m_visible_instances;
  std::uint64_t m_cachedVisibilityVersion = 0;
  bool m_visibility_dirty = true;
};

} // namespace Render::GL
