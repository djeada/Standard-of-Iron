#include "render/ground/stone_renderer.h"

#include <QDebug>
#include <QVector2D>
#include <qglobal.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/scatter_composition.h"
#include "game/map/scatter/spawn_validator.h"
#include "map/biome_settings.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/draw_resources.h"
#include "render/frame/frame_submitter.h"
#include "render/ground/ground_ecology.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"
#include "render/ground/stone_ground_fit.h"
#include "render/math/render_constants.h"

namespace Render::GL {

StoneRenderer::StoneRenderer() = default;
StoneRenderer::~StoneRenderer() = default;

void StoneRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                              const Game::Map::BiomeSettings& biome_settings,
                              const std::vector<Game::Map::WorldProp>& world_props) {
  configure_height_scatter_common(height_map, biome_settings, world_props);
  auto& stone_params = m_state.params;

  stone_params.light_direction = m_light_direction;
  stone_params.time = 0.0F;

  m_ecology = Render::Ground::shared_ground_ecology(
      world().terrain(), height_map, m_noise_seed);
  generate_stone_instances();
}

void StoneRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, QVector3D(0.35F, 0.8F, 0.45F));
}

void StoneRenderer::submit(FrameSubmitter& renderer, const DrawResources* resources) {
  submit_filtered_common<false>(
      renderer,
      resources,
      TerrainScatterCmd::Species::Stone,
      [](TerrainScatterCmd& cmd, const StoneBatchParams& params) {
        cmd.stone = params;
      });
}

void StoneRenderer::generate_stone_instances() {
  adopt_generated(Ground::Scatter::generate_stones({scatter_grid(),
                                                    m_ecology.get(),
                                                    m_biome_settings,
                                                    world().terrain_or_empty(),
                                                    m_world_props}));
}

} // namespace Render::GL
