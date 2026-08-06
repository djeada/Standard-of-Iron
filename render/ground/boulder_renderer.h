#pragma once

#include <QVector3D>

#include <vector>

#include "../../game/map/map_definition.h"
#include "../../game/map/terrain.h"
#include "../decoration_gpu.h"
#include "scatter_renderer_base.h"

namespace Render::GL {
class Renderer;

class BoulderRenderer : public ScatterRendererBase<StoneInstanceGpu, StoneBatchParams> {
public:
  BoulderRenderer();
  ~BoulderRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings,
                 const std::vector<Game::Map::WorldProp>& scatter_seed_world_props = {},
                 const std::vector<Game::Map::WorldProp>& runtime_world_props = {},
                 bool use_world_props_exclusively = false);

  void refresh_world_props(const std::vector<Game::Map::WorldProp>& runtime_world_props,
                           bool use_world_props_exclusively);

  void set_light_direction(const QVector3D& dir) override;

  void submit(Renderer& renderer, ResourceManager* resources) override;

private:
  void rebuild_boulder_instances();
  void append_world_prop_boulders();
  void generate_procedural_boulders(std::vector<StoneInstanceGpu>& out) const;
};

} // namespace Render::GL
