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
                 const std::vector<Game::Map::WorldProp>& scatter_seed_world_props = {},
                 const std::vector<Game::Map::WorldProp>& runtime_world_props = {},
                 bool use_world_props_exclusively = false);

  void refresh_world_props(const std::vector<Game::Map::WorldProp>& runtime_world_props,
                           bool use_world_props_exclusively);

  void set_light_direction(const QVector3D& dir) override;

  void submit(Renderer& renderer, ResourceManager* resources) override;

  [[nodiscard]] auto species() const -> Game::Map::TreeSpecies { return m_species; }

  [[nodiscard]] auto instances_for_test() const -> const std::vector<TreeInstanceGpu>& {
    return m_state.instances;
  }

private:
  void rebuild_instances();
  void append_world_prop_trees();
  void generate_procedural_trees(std::vector<TreeInstanceGpu>& out) const;

  Game::Map::TreeSpecies m_species;
  const Render::Ground::TreeScatterProfile* m_profile;
};

} // namespace Render::GL
