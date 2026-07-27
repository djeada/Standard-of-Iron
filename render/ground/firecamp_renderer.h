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

class FireCampRenderer : public IRenderPass {
public:
  FireCampRenderer();
  ~FireCampRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings,
                 const std::vector<Game::Map::WorldProp>& world_props = {});

  void submit(Renderer& renderer, ResourceManager* resources) override;

  void clear();

  [[nodiscard]] bool is_gpu_ready() const { return m_firecamp_state.is_gpu_ready(); }

  [[nodiscard]] auto instance_count() const -> std::size_t {
    return m_firecamp_state.instances.size();
  }
  [[nodiscard]] auto last_sync_stats() const -> Render::Ground::Scatter::SyncStats {
    return m_firecamp_state.last_sync_stats;
  }

private:
  // A camp's logs, stone ring and coals are fully determined by its seed and
  // never move, so the geometry is built once here instead of being re-derived
  // from hashes and trig on every frame.  Only the ember glow animates, and it
  // is a single lerp per cylinder against `ember_weight`.
  struct DecorCylinder {
    QVector3D start;
    QVector3D end;
    QVector3D base_color;
    float radius = 0.0F;
    float ember_weight = 0.0F;
  };

  struct CampDecor {
    float phase = 0.0F;
    std::vector<DecorCylinder> cylinders;
  };

  void generate_firecamp_instances();
  void build_camp_decor(const QVector3D& camp_pos,
                        float base_radius,
                        float phase,
                        CampDecor& decor) const;

  std::vector<CampDecor> m_camp_decor;

  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;

  std::vector<float> m_height_data;
  std::vector<Game::Map::TerrainType> m_terrain_types;
  Game::Map::BiomeSettings m_biome_settings;
  std::uint32_t m_noise_seed = 0U;
  std::vector<Game::Map::WorldProp> m_world_props;

  Render::Ground::Scatter::FilteredRendererState<FireCampInstanceGpu,
                                                 FireCampBatchParams>
      m_firecamp_state;
};

} // namespace Render::GL
