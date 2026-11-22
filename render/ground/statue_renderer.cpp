#include "render/ground/statue_renderer.h"

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

namespace {

using namespace Render::Ground;

constexpr float k_votive_height = 0.22F;
constexpr float k_votive_radius_scale = 2.4F;
constexpr float k_votive_intensity = 0.42F;

} // namespace

namespace Render::GL {

StatueRenderer::StatueRenderer() = default;
StatueRenderer::~StatueRenderer() = default;

void StatueRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                               const Game::Map::BiomeSettings& biome_settings,
                               const std::vector<Game::Map::WorldProp>& world_props) {
  configure_biome_common(biome_settings);
  m_state.track_visible_instances = true;
  m_state.params.light_direction = m_light_direction;
  generate_instances(world_props, height_map);
}

void StatueRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, PropBatchParams::default_light_direction());
}

void StatueRenderer::submit(FrameSubmitter& renderer, const DrawResources* resources) {
  if (submit_prop_common(renderer, resources, TerrainScatterCmd::Species::Statue) ==
      0) {
    return;
  }
  const float night = environment_night_amount(renderer.environment_lighting());
  if (night <= 0.01F) {
    return;
  }
  const float time = m_state.params.time;
  for (const auto& inst : m_state.visible_instances) {
    const QVector3D statue_pos = inst.pos_scale.toVector3D();
    const float scale = std::max(inst.pos_scale.w(), 0.1F);
    if (!renderer.submission_visibility().accepts_sphere(
            statue_pos, scale, SubmissionFogMode::Revealed)) {
      continue;
    }
    const float phase = inst.color_rot.w() * 2.3F + statue_pos.x() * 0.11F;
    const float flicker = 0.88F + 0.12F * std::sin(time * 6.1F + phase) *
                                      std::sin(time * 2.3F + phase * 0.7F);
    Render::LocalLight votive;
    votive.position = statue_pos + QVector3D(0.0F, scale * k_votive_height, 0.0F);
    votive.color = QVector3D(1.0F, 0.66F, 0.30F);
    votive.radius = std::clamp(scale * k_votive_radius_scale, 1.8F, 4.5F);
    votive.intensity = k_votive_intensity * night * flicker;
    renderer.local_light(votive);
  }
}

void StatueRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& height_map) {
  adopt_generated(Ground::Scatter::generate_statues(
      {{}, nullptr, m_biome_settings, world().terrain_or_empty(), world_props}));
}

auto StatueRenderer::plan_world_props_refresh(
    std::vector<Game::Map::WorldProp> world_props,
    std::shared_ptr<const Game::Map::ScatterWorld> world)
    -> Ground::Scatter::ScatterJob {
  return [this,
          biome = m_biome_settings,
          world_props = std::move(world_props),
          world = std::move(world)]() mutable -> Ground::Scatter::ScatterAdoption {
    auto instances = Ground::Scatter::generate_statues(
        {{}, nullptr, biome, world->terrain(), world_props, world.get()});
    return [this, instances = std::move(instances)]() mutable {
      adopt_generated(std::move(instances));
    };
  };
}

} // namespace Render::GL
