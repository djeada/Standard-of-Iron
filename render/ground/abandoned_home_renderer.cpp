#include "render/ground/abandoned_home_renderer.h"

#include <QVector4D>

#include "game/map/scatter/ground_utils.h"
#include "map/biome_settings.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/frame_submitter.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"

namespace Render::GL {

AbandonedHomeRenderer::AbandonedHomeRenderer() = default;
AbandonedHomeRenderer::~AbandonedHomeRenderer() = default;

void AbandonedHomeRenderer::configure(
    const Game::Map::TerrainHeightMap& height_map,
    const Game::Map::BiomeSettings& biome_settings,
    const std::vector<Game::Map::WorldProp>& world_props) {
  configure_biome_common(biome_settings);
  m_state.params.light_direction = m_light_direction;
  generate_instances(world_props, height_map);
}

void AbandonedHomeRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, PropBatchParams::default_light_direction());
}

void AbandonedHomeRenderer::submit(FrameSubmitter& renderer,
                                   const DrawResources* resources) {
  submit_prop_common(renderer, resources, TerrainScatterCmd::Species::AbandonedHome);
}

void AbandonedHomeRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& height_map) {
  adopt_generated(Ground::Scatter::generate_abandoned_homes(
      {{}, nullptr, m_biome_settings, world().terrain_or_empty(), world_props}));
}

} // namespace Render::GL
