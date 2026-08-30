#include "statue_renderer.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>

#include "decoration_gpu.h"
#include "game/map/scatter/ground_utils.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/scene_renderer.h"
#include "scatter_runtime.h"

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

void StatueRenderer::submit(Renderer& renderer, ResourceManager* resources) {
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

  const auto& terrain_service = world().terrain_or_empty();

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Statue) {
      continue;
    }
    const QVector3D resolved = terrain_service.world_prop_footprint_world_position(
        prop, Game::Map::world_prop_ground_bounding_radius(prop.type, prop.scale));

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 m_biome_settings.seed ^ 0x1D7B4E63U);

    QVector3D const pentelic_marble(0.955F, 0.940F, 0.900F);
    QVector3D const travertine(0.905F, 0.845F, 0.735F);
    QVector3D const grey_limestone(0.790F, 0.795F, 0.780F);

    float const quarry = rand_01(state);
    QVector3D color =
        quarry < 0.55F
            ? pentelic_marble
            : (quarry < 0.82F ? pentelic_marble * 0.45F + travertine * 0.55F
                              : pentelic_marble * 0.35F + grey_limestone * 0.65F);
    float const weathering = remap(rand_01(state), 0.0F, 0.14F);
    color *= 1.0F - weathering;
    color *= remap(rand_01(state), 0.96F, 1.04F);

    PropInstanceGpu inst;
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
