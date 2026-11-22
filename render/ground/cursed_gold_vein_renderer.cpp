#include "render/ground/cursed_gold_vein_renderer.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>

#include "game/map/scatter/ground_utils.h"
#include "map/biome_settings.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/frame/frame_submitter.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"

namespace Render::GL {

CursedGoldVeinRenderer::CursedGoldVeinRenderer() = default;
CursedGoldVeinRenderer::~CursedGoldVeinRenderer() = default;

void CursedGoldVeinRenderer::configure(
    const Game::Map::TerrainHeightMap& height_map,
    const Game::Map::BiomeSettings& biome_settings,
    const std::vector<Game::Map::WorldProp>& world_props) {
  configure_biome_common(biome_settings);

  m_state.track_visible_instances = true;
  m_state.params.light_direction = m_light_direction;
  generate_instances(world_props, height_map);
}

void CursedGoldVeinRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, PropBatchParams::default_light_direction());
}

void CursedGoldVeinRenderer::submit(FrameSubmitter& renderer,
                                    const DrawResources* resources) {
  if (submit_prop_common(
          renderer, resources, TerrainScatterCmd::Species::CursedGoldVein) == 0) {
    return;
  }

  for (const auto& inst : m_state.visible_instances) {
    const QVector3D vein_pos = inst.pos_scale.toVector3D();
    const float scale = std::max(inst.pos_scale.w(), 0.1F);
    const float phase = inst.color_rot.w();
    const float t = m_state.params.time;
    const float flicker = 0.82F + 0.12F * std::sin(t * 2.3F + phase) +
                          0.06F * std::sin(t * 5.1F + phase * 1.7F);
    Render::LocalLight ore_light;
    ore_light.position = vein_pos + QVector3D(0.0F, scale * 0.55F, 0.0F);
    ore_light.color = QVector3D(1.0F, 0.72F, 0.26F);
    ore_light.radius = std::clamp(scale * 3.0F, 3.0F, 9.5F);
    ore_light.intensity = 0.62F * flicker;
    renderer.local_light(ore_light);
  }
}

void CursedGoldVeinRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& height_map) {
  adopt_generated(Ground::Scatter::generate_cursed_gold_veins(
      {{}, nullptr, m_biome_settings, world().terrain_or_empty(), world_props}));
}

} // namespace Render::GL
