#include "render/ground/ruins_renderer.h"

#include <QVector4D>

#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/scatter_world.h"
#include "map/biome_settings.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/frame_submitter.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"

namespace Render::GL {

RuinsRenderer::RuinsRenderer() = default;
RuinsRenderer::~RuinsRenderer() = default;

void RuinsRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                              const Game::Map::BiomeSettings& biome_settings,
                              const std::vector<Game::Map::WorldProp>& world_props) {
  configure_biome_common(biome_settings);
  m_state.params.light_direction = m_light_direction;
  generate_instances(world_props, height_map);
}

void RuinsRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, PropBatchParams::default_light_direction());
}

void RuinsRenderer::submit(FrameSubmitter& renderer, const DrawResources* resources) {
  submit_prop_common(renderer, resources, TerrainScatterCmd::Species::Ruins);
}

void RuinsRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& height_map) {
  adopt_generated(Ground::Scatter::generate_ruins(
      {{}, nullptr, m_biome_settings, world().terrain_or_empty(), world_props}));
}

auto RuinsRenderer::plan_world_props_refresh(
    std::vector<Game::Map::WorldProp> world_props,
    std::shared_ptr<const Game::Map::ScatterWorld> world)
    -> Ground::Scatter::ScatterJob {
  return [this,
          biome = m_biome_settings,
          world_props = std::move(world_props),
          world = std::move(world)]() mutable -> Ground::Scatter::ScatterAdoption {
    auto instances = Ground::Scatter::generate_ruins(
        {{}, nullptr, biome, world->terrain(), world_props, world.get()});
    return [this, instances = std::move(instances)]() mutable {
      adopt_generated(std::move(instances));
    };
  };
}

} // namespace Render::GL
