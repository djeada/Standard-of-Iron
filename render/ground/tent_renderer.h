#pragma once

#include <QVector3D>

#include <cstdint>
#include <vector>

#include "../../game/map/map_definition.h"
#include "../../game/map/terrain.h"
#include "../decoration_gpu.h"
#include "../i_render_pass.h"
#include "scatter_renderer_state.h"

namespace Render::GL {
class Renderer;

class TentRenderer : public IRenderPass {
public:
  TentRenderer();
  ~TentRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings,
                 const std::vector<Game::Map::WorldProp>& world_props = {});
  void set_light_direction(const QVector3D& dir);

  void submit(Renderer& renderer, ResourceManager* resources) override;

  void clear();

  [[nodiscard]] bool is_gpu_ready() const { return m_state.is_gpu_ready(); }
  [[nodiscard]] auto instance_count() const -> std::size_t {
    return m_state.instances.size();
  }
  [[nodiscard]] auto last_sync_stats() const -> Render::Ground::Scatter::SyncStats {
    return m_state.last_sync_stats;
  }

private:
  void generate_instances(const std::vector<Game::Map::WorldProp>& world_props,
                          const Game::Map::TerrainHeightMap& height_map);

  Game::Map::BiomeSettings m_biome_settings;
  QVector3D m_light_direction = TentBatchParams::default_light_direction();

  Render::Ground::Scatter::FilteredRendererState<TentInstanceGpu, TentBatchParams>
      m_state;
};

} // namespace Render::GL
