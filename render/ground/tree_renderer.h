#pragma once

#include <QVector3D>

#include <vector>

#include "game/map/map_definition.h"
#include "game/map/terrain.h"
#include "render/decoration_gpu.h"
#include "scatter_renderer_base.h"

namespace Render::Ground {
struct TreeScatterProfile;
}

namespace Render::GL {
class Renderer;

class TreeRenderer : public ScatterRendererBase<TreeInstanceGpu, FoliageBatchParams> {
public:
  explicit TreeRenderer(Game::Map::TreeSpecies species);
  ~TreeRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings,
                 const std::vector<Game::Map::WorldProp>& world_props = {});

  void refresh_world_props(const std::vector<Game::Map::WorldProp>& world_props);

  void set_light_direction(const QVector3D& dir) override;

  void submit(Renderer& renderer, ResourceManager* resources) override;

  [[nodiscard]] auto fog_culls_instances() const -> bool override { return false; }

  [[nodiscard]] auto species() const -> Game::Map::TreeSpecies { return m_species; }

  [[nodiscard]] auto instances_for_test() const -> const std::vector<TreeInstanceGpu>& {
    return m_state.instances;
  }

private:
  void rebuild_instances();
  void append_world_prop_trees();

  Game::Map::TreeSpecies m_species;
  const Render::Ground::TreeScatterProfile* m_profile;
};

} // namespace Render::GL
