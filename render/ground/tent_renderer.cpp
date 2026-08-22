#include "tent_renderer.h"

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

constexpr float k_lantern_height = 0.55F;
constexpr float k_lantern_radius_scale = 3.4F;
constexpr float k_lantern_intensity = 0.62F;

} // namespace

namespace Render::GL {

TentRenderer::TentRenderer() = default;
TentRenderer::~TentRenderer() = default;

void TentRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                             const Game::Map::BiomeSettings& biome_settings,
                             const std::vector<Game::Map::WorldProp>& world_props) {
  configure_biome_common(biome_settings);
  m_state.track_visible_instances = true;
  m_state.params.light_direction = m_light_direction;
  generate_instances(world_props, height_map);
}

void TentRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, PropBatchParams::default_light_direction());
}

void TentRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  if (submit_prop_common(renderer, resources, TerrainScatterCmd::Species::Tent) == 0) {
    return;
  }
  const float night = environment_night_amount(renderer.environment_lighting());
  if (night <= 0.01F) {
    return;
  }
  const float time = m_state.params.time;
  for (const auto& inst : m_state.visible_instances) {
    const QVector3D tent_pos = inst.pos_scale.toVector3D();
    const float scale = std::max(inst.pos_scale.w(), 0.1F);
    if (!renderer.submission_visibility().accepts_sphere(
            tent_pos, scale, SubmissionFogMode::Revealed)) {
      continue;
    }
    const float phase = inst.color_rot.w() * 3.1F;
    const float flicker = 0.90F + 0.10F * std::sin(time * 3.1F + phase) *
                                      std::sin(time * 1.7F + phase * 0.5F);
    Render::LocalLight lantern;
    lantern.position = tent_pos + QVector3D(0.0F, scale * k_lantern_height, 0.0F);
    lantern.color = QVector3D(1.0F, 0.64F, 0.32F);
    lantern.radius = std::clamp(scale * k_lantern_radius_scale, 2.5F, 7.0F);
    lantern.intensity = k_lantern_intensity * night * flicker;
    renderer.local_light(lantern);
  }
}

namespace {
constexpr float k_tent_footprint_radius = 1.15F;
}

void TentRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& height_map) {

  const auto& terrain_service = world().terrain_or_empty();
  const float tile_size = height_map.get_tile_size();
  const int width = height_map.get_width();
  const int map_height = height_map.get_height();
  const float half_w = static_cast<float>(width) * 0.5F;
  const float half_h = static_cast<float>(map_height) * 0.5F;

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Tent) {
      continue;
    }
    const float wx = (prop.x - half_w) * tile_size;
    const float wz = (prop.z - half_h) * tile_size;
    const float tent_scale = prop.scale * Game::Map::world_prop_render_scale(
                                              Game::Map::WorldProp::Type::Tent);
    const QVector3D resolved = terrain_service.resolve_footprint_world_position(
        wx, wz, tent_scale * k_tent_footprint_radius, 0.0F, 0.0F);

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 m_biome_settings.seed ^ 0xA7124D93U);
    float const dye = rand_01(state);
    QVector3D canvas_color;
    if (dye < 0.34F) {
      canvas_color = QVector3D(0.48F, 0.18F, 0.13F);
    } else if (dye < 0.68F) {
      canvas_color = QVector3D(0.58F, 0.43F, 0.23F);
    } else {
      canvas_color = QVector3D(0.17F, 0.32F, 0.34F);
    }
    canvas_color *= remap(rand_01(state), 0.88F, 1.06F);

    PropInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(), resolved.y(), resolved.z(), tent_scale);
    inst.color_rot =
        QVector4D(canvas_color.x(), canvas_color.y(), canvas_color.z(), prop.rotation);
    m_state.instances.push_back(inst);
  }

  m_state.instance_count = m_state.instances.size();
  m_state.instances_dirty = m_state.instance_count > 0;
}

} // namespace Render::GL
