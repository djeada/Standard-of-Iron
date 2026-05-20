#pragma once

#include <QVector3D>

#include <cstdint>
#include <memory>
#include <vector>

#include "../../game/map/map_definition.h"
#include "../../game/map/terrain.h"
#include "../decoration_gpu.h"
#include "../i_render_pass.h"
#include "scatter_renderer_state.h"

namespace Render::GL {
class Buffer;
class Renderer;

class OliveRenderer : public IRenderPass {
public:
  OliveRenderer();
  ~OliveRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings,
                 const std::vector<Game::Map::WorldProp>& scatter_seed_world_props = {},
                 const std::vector<Game::Map::WorldProp>& runtime_world_props = {},
                 bool use_world_props_exclusively = false);

  void set_light_direction(const QVector3D& dir);

  void submit(Renderer& renderer, ResourceManager* resources) override;

  void clear();

  [[nodiscard]] bool is_gpu_ready() const { return m_olive_state.is_gpu_ready(); }

  [[nodiscard]] auto instance_count() const -> std::size_t {
    return m_olive_state.instances.size();
  }
  [[nodiscard]] auto last_sync_stats() const -> Render::Ground::Scatter::SyncStats {
    return m_olive_state.last_sync_stats;
  }
  [[nodiscard]] auto
  instances_for_test() const -> const std::vector<OliveInstanceGpu>& {
    return m_olive_state.instances;
  }

private:
  void generate_olive_instances();

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;

  std::vector<float> m_height_data;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  std::vector<Game::Map::WorldProp> m_scatter_seed_world_props;
  std::vector<Game::Map::WorldProp> m_runtime_world_props;
  bool m_use_world_props_exclusively = false;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noise_seed = 0U;
  QVector3D m_light_direction{0.35F, 0.8F, 0.45F};

  Render::Ground::Scatter::FilteredRendererState<OliveInstanceGpu, OliveBatchParams>
      m_olive_state;
};

} // namespace Render::GL
