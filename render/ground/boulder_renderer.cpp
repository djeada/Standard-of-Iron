#include "render/ground/boulder_renderer.h"

#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/scatter_composition.h"
#include "game/map/scatter/spawn_validator.h"
#include "map/biome_settings.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/frame_submitter.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"
#include "render/ground/stone_ground_fit.h"

namespace Render::GL {

BoulderRenderer::BoulderRenderer() = default;
BoulderRenderer::~BoulderRenderer() = default;

void BoulderRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                                const Game::Map::BiomeSettings& biome_settings,
                                const std::vector<Game::Map::WorldProp>& world_props) {
  configure_height_scatter_common(height_map, biome_settings, world_props);
  m_state.params.light_direction = m_light_direction;
  rebuild_boulder_instances();
}

void BoulderRenderer::refresh_world_props(
    const std::vector<Game::Map::WorldProp>& world_props) {
  m_world_props = world_props;
  rebuild_boulder_instances();
}

void BoulderRenderer::rebuild_boulder_instances() {
  adopt_generated(Ground::Scatter::generate_boulders({scatter_grid(),
                                                      nullptr,
                                                      m_biome_settings,
                                                      world().terrain_or_empty(),
                                                      m_world_props}));
}

void BoulderRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, StoneBatchParams::default_light_direction());
}

void BoulderRenderer::submit(FrameSubmitter& renderer, const DrawResources* resources) {
  submit_filtered_common<false>(
      renderer,
      resources,
      TerrainScatterCmd::Species::Stone,
      [](TerrainScatterCmd& cmd, const StoneBatchParams& params) {
        cmd.stone = params;
      });
}

} // namespace Render::GL
