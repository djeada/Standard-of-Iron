#pragma once

#include "../../game/map/terrain.h"
#include "../i_render_pass.h"
#include "firecamp_gpu.h"
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <vector>

namespace Render::Ground {
class SpawnValidator;
}

namespace Render::GL {
class Buffer;
class Renderer;

class FireCampRenderer : public IRenderPass {
public:
  FireCampRenderer();
  ~FireCampRenderer() override;

  void configure(const Game::Map::TerrainHeightMap &height_map,
                 const Game::Map::BiomeSettings &biome_settings);

  void setExplicitFireCamps(const std::vector<QVector3D> &positions,
                            const std::vector<float> &intensities = {},
                            const std::vector<float> &radii = {});

  void submit(Renderer &renderer, ResourceManager *resources) override;

  void clear();

  [[nodiscard]] bool is_gpu_ready() const {
    return m_fireCampInstanceBuffer != nullptr || m_fireCampInstanceCount == 0;
  }

private:
  void generate_firecamp_instances();
  void add_explicit_firecamps(const Render::Ground::SpawnValidator &validator);

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;

  std::vector<float> m_heightData;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noiseSeed = 0U;

  std::vector<FireCampInstanceGpu> m_fireCampInstances;
  std::unique_ptr<Buffer> m_fireCampInstanceBuffer;
  std::size_t m_fireCampInstanceCount = 0;
  FireCampBatchParams m_fireCampParams;
  bool m_fireCampInstancesDirty = false;

  std::vector<FireCampInstanceGpu> m_visibleInstances;
  std::uint64_t m_cachedVisibilityVersion = 0;
  bool m_visibilityDirty = true;

  std::vector<QVector3D> m_explicitPositions;
  std::vector<float> m_explicitIntensities;
  std::vector<float> m_explicitRadii;
};

} // namespace Render::GL
