#include "statue_renderer.h"

#include <QVector4D>

#include "../scene_renderer.h"
#include "decoration_gpu.h"
#include "game/map/scatter/ground_utils.h"
#include "gl/render_constants.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "scatter_runtime.h"
#include "scatter_submission.h"

namespace {

using namespace Render::Ground;

}

namespace Render::GL {

StatueRenderer::StatueRenderer() = default;
StatueRenderer::~StatueRenderer() = default;

void StatueRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                               const Game::Map::BiomeSettings& biome_settings,
                               const std::vector<Game::Map::WorldProp>& world_props) {
  m_biome_settings = biome_settings;
  m_state.reset_instances();
  m_state.params.light_direction = m_light_direction;
  generate_instances(world_props, height_map);
}

void StatueRenderer::set_light_direction(const QVector3D& dir) {
  m_light_direction =
      dir.isNull() ? StatueBatchParams::default_light_direction() : dir.normalized();
  m_state.params.light_direction = m_light_direction;
}

void StatueRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  Q_UNUSED(resources);

  const auto visible_count = Scatter::sync_filtered_state(
      m_state,
      [](const StatueInstanceGpu& inst) -> const QVector4D& { return inst.pos_scale; },
      renderer.static_world_visibility_filter_enabled()
          ? renderer.submission_visibility().snapshot()
          : nullptr,
      Scatter::ScatterMemoryMode::Remembered);
  if (visible_count == 0) {
    return;
  }

  m_state.params.time = renderer.get_animation_time();

  TerrainScatterCmd cmd;
  cmd.visibility = renderer.visibility_mask();
  cmd.species = TerrainScatterCmd::Species::Statue;
  cmd.statue = m_state.params;
  Scatter::submit_visible_chunks(renderer, m_state, cmd);
}

void StatueRenderer::clear() {
  m_state.reset_instances();
}

void StatueRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& height_map) {

  auto& terrain_service = Game::Map::TerrainService::instance();
  const float tile_size = height_map.get_tile_size();
  const int width = height_map.get_width();
  const int map_height = height_map.get_height();
  const float half_w = static_cast<float>(width) * 0.5F;
  const float half_h = static_cast<float>(map_height) * 0.5F;

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Statue) {
      continue;
    }
    const float wx = (prop.x - half_w) * tile_size;
    const float wz = (prop.z - half_h) * tile_size;
    const QVector3D resolved =
        terrain_service.resolve_surface_world_position(wx, wz, 0.0F, 0.0F);

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 m_biome_settings.seed ^ 0x1D7B4E63U);

    QVector3D const marble(1.34F, 1.30F, 1.21F);
    QVector3D const weathered_bronze(0.82F, 0.98F, 0.84F);
    float const bronze_mix = remap(rand_01(state), 0.0F, 0.26F);
    QVector3D color = marble * (1.0F - bronze_mix) + weathered_bronze * bronze_mix;
    color *= remap(rand_01(state), 0.94F, 1.06F);

    StatueInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(),
                               resolved.y(),
                               resolved.z(),
                               prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::Statue));
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    m_state.instances.push_back(inst);
  }

  m_state.instance_count = m_state.instances.size();
  m_state.instances_dirty = m_state.instance_count > 0;
}

} // namespace Render::GL
