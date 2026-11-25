#pragma once

#include <QVector3D>

#include <vector>

#include "game/map/biome_settings.h"
#include "game/map/map_definition.h"
#include "game/map/terrain.h"
#include "render/frame/decoration_gpu.h"
#include "render/ground/scatter_renderer_base.h"

namespace Render::GL {
class FrameSubmitter;

class IronOreRenderer : public ScatterRendererBase<PropInstanceGpu, PropBatchParams> {
public:
  IronOreRenderer();
  ~IronOreRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings,
                 const std::vector<Game::Map::WorldProp>& world_props = {});

  [[nodiscard]] auto
  plan_world_props_refresh(std::vector<Game::Map::WorldProp> world_props,
                           std::shared_ptr<const Game::Map::ScatterWorld> world)
      -> Render::Ground::Scatter::ScatterJob;
  void set_light_direction(const QVector3D& dir) override;

  void submit(FrameSubmitter& renderer, const DrawResources* resources) override;

  [[nodiscard]] auto fog_culls_instances() const -> bool override { return false; }

private:
  void generate_instances(const std::vector<Game::Map::WorldProp>& world_props,
                          const Game::Map::TerrainHeightMap& height_map);
};

} // namespace Render::GL
