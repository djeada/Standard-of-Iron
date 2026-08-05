#pragma once

#include <QVector3D>

#include <vector>

#include "../../game/map/map_definition.h"
#include "../../game/map/terrain.h"
#include "../decoration_gpu.h"
#include "scatter_renderer_base.h"

namespace Render::GL {
class Renderer;

class RuinsRenderer : public ScatterRendererBase<PropInstanceGpu, PropBatchParams> {
public:
  RuinsRenderer();
  ~RuinsRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings,
                 const std::vector<Game::Map::WorldProp>& world_props = {});
  void set_light_direction(const QVector3D& dir) override;

  void submit(Renderer& renderer, ResourceManager* resources) override;

private:
  void generate_instances(const std::vector<Game::Map::WorldProp>& world_props,
                          const Game::Map::TerrainHeightMap& height_map);
};

} // namespace Render::GL
