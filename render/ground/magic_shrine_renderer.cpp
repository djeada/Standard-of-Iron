#include "render/ground/magic_shrine_renderer.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>

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

MagicShrineRenderer::MagicShrineRenderer() = default;
MagicShrineRenderer::~MagicShrineRenderer() = default;

void MagicShrineRenderer::configure(
    const Game::Map::TerrainHeightMap& height_map,
    const Game::Map::BiomeSettings& biome_settings,
    const std::vector<Game::Map::WorldProp>& world_props) {
  configure_biome_common(biome_settings);

  m_state.track_visible_instances = true;
  m_state.params.light_direction = m_light_direction;
  generate_instances(world_props, height_map);
}

void MagicShrineRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, PropBatchParams::default_light_direction());
}

void MagicShrineRenderer::submit(FrameSubmitter& renderer,
                                 const DrawResources* resources) {
  if (submit_prop_common(
          renderer, resources, TerrainScatterCmd::Species::MagicShrine) == 0) {
    return;
  }

  for (const auto& inst : m_state.visible_instances) {
    const QVector3D shrine_pos = inst.pos_scale.toVector3D();
    const float scale = std::max(inst.pos_scale.w(), 0.1F);
    const float time = m_state.params.time;
    const float phase = inst.color_rot.w();
    const float pulse = 0.86F + (0.08F * std::sin((time * 7.3F) + phase)) +
                        (0.06F * std::sin((time * 3.1F) + (phase * 2.0F)));
    Render::LocalLight votive;
    votive.position = shrine_pos + QVector3D(0.0F, scale * 1.0F, 0.0F);
    votive.color = QVector3D(1.00F, 0.36F, 0.12F);
    votive.radius = std::clamp(scale * 3.8F, 4.0F, 12.0F);
    votive.intensity = 0.95F * pulse;
    renderer.local_light(votive);
  }
}

void MagicShrineRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& height_map) {
  adopt_generated(Ground::Scatter::generate_magic_shrines(
      {{}, nullptr, m_biome_settings, world().terrain_or_empty(), world_props}));
}

auto MagicShrineRenderer::plan_world_props_refresh(
    std::vector<Game::Map::WorldProp> world_props,
    std::shared_ptr<const Game::Map::ScatterWorld> world)
    -> Ground::Scatter::ScatterJob {
  return [this,
          biome = m_biome_settings,
          world_props = std::move(world_props),
          world = std::move(world)]() mutable -> Ground::Scatter::ScatterAdoption {
    auto instances = Ground::Scatter::generate_magic_shrines(
        {{}, nullptr, biome, world->terrain(), world_props, world.get()});
    return [this, instances = std::move(instances)]() mutable {
      adopt_generated(std::move(instances));
    };
  };
}

} // namespace Render::GL
