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

class BoulderRenderer : public ScatterRendererBase<StoneInstanceGpu, StoneBatchParams> {
public:
  BoulderRenderer();
  ~BoulderRenderer() override;

  void configure(const Game::Map::TerrainHeightMap& height_map,
                 const Game::Map::BiomeSettings& biome_settings,
                 const std::vector<Game::Map::WorldProp>& world_props = {});

  void refresh_world_props(const std::vector<Game::Map::WorldProp>& world_props);

  void set_light_direction(const QVector3D& dir) override;

  void submit(FrameSubmitter& renderer, const DrawResources* resources) override;

  [[nodiscard]] auto fog_culls_instances() const -> bool override { return false; }

private:
  void rebuild_boulder_instances();
};

} // namespace Render::GL
