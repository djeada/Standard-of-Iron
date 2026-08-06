#pragma once

#include <QVector3D>

#include <cstdint>
#include <vector>

#include "../../game/map/map_definition.h"
#include "../../game/map/terrain.h"
#include "../decoration_gpu.h"
#include "scatter_renderer_base.h"

namespace Render::GL {
class Renderer;

class DeadTreeRenderer : public ScatterRendererBase<PropInstanceGpu, PropBatchParams> {
public:
  DeadTreeRenderer();
  ~DeadTreeRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings,
                 const std::vector<Game::Map::WorldProp>& scatter_seed_world_props = {},
                 const std::vector<Game::Map::WorldProp>& runtime_world_props = {});

  void
  refresh_world_props(const std::vector<Game::Map::WorldProp>& runtime_world_props);

  void set_light_direction(const QVector3D& dir) override;

  void submit(Renderer& renderer, ResourceManager* resources) override;

private:
  void rebuild_dead_tree_instances();
  void append_world_prop_dead_trees();
  void generate_procedural_dead_trees(std::vector<PropInstanceGpu>& out) const;
};

} // namespace Render::GL
