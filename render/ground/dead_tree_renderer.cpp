#include "render/ground/dead_tree_renderer.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>

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
#include "render/math/render_constants.h"

namespace Render::GL {

DeadTreeRenderer::DeadTreeRenderer() = default;
DeadTreeRenderer::~DeadTreeRenderer() = default;

void DeadTreeRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                                 const Game::Map::BiomeSettings& biome_settings,
                                 const std::vector<Game::Map::WorldProp>& world_props) {
  configure_height_scatter_common(height_map, biome_settings, world_props);
  m_state.params.light_direction = m_light_direction;
  rebuild_dead_tree_instances();
}

void DeadTreeRenderer::refresh_world_props(
    const std::vector<Game::Map::WorldProp>& world_props) {
  m_world_props = world_props;
  rebuild_dead_tree_instances();
}

void DeadTreeRenderer::rebuild_dead_tree_instances() {
  adopt_generated(Ground::Scatter::generate_dead_trees(
      {{}, nullptr, m_biome_settings, world().terrain_or_empty(), m_world_props}));
}

void DeadTreeRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, PropBatchParams::default_light_direction());
}

void DeadTreeRenderer::submit(FrameSubmitter& renderer,
                              const DrawResources* resources) {
  submit_prop_common(renderer, resources, TerrainScatterCmd::Species::DeadTree);
}

} // namespace Render::GL
