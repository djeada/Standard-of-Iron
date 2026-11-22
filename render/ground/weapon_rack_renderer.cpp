#include "render/ground/weapon_rack_renderer.h"

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

WeaponRackRenderer::WeaponRackRenderer() = default;
WeaponRackRenderer::~WeaponRackRenderer() = default;

void WeaponRackRenderer::configure(
    const Game::Map::TerrainHeightMap& height_map,
    const Game::Map::BiomeSettings& biome_settings,
    const std::vector<Game::Map::WorldProp>& world_props) {
  configure_biome_common(biome_settings);
  m_state.params.light_direction = m_light_direction;
  generate_instances(world_props, height_map);
}

void WeaponRackRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, PropBatchParams::default_light_direction());
}

void WeaponRackRenderer::submit(FrameSubmitter& renderer,
                                const DrawResources* resources) {
  submit_prop_common(renderer, resources, TerrainScatterCmd::Species::WeaponRack);
}

void WeaponRackRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& height_map) {
  adopt_generated(Ground::Scatter::generate_weapon_racks(
      {{}, nullptr, m_biome_settings, world().terrain_or_empty(), world_props}));
}

} // namespace Render::GL
