#include "magic_shrine_renderer.h"

#include <QVector4D>

#include <algorithm>
#include <cmath>

#include "decoration_gpu.h"
#include "game/map/scatter/ground_utils.h"
#include "map/biome_settings.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/scene_renderer.h"
#include "scatter_runtime.h"

namespace {

using namespace Render::Ground;

constexpr float k_base_color_r = 0.128F;
constexpr float k_base_color_g = 0.118F;
constexpr float k_base_color_b = 0.124F;

} // namespace

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

void MagicShrineRenderer::submit(Renderer& renderer, ResourceManager* resources) {
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

  const auto& terrain_service = world().terrain_or_empty();

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::MagicShrine) {
      continue;
    }
    const QVector3D resolved = terrain_service.world_prop_footprint_world_position(
        prop, Game::Map::world_prop_ground_bounding_radius(prop.type, prop.scale));

    PropInstanceGpu inst;
    inst.pos_scale =
        QVector4D(resolved.x(),
                  resolved.y(),
                  resolved.z(),
                  prop.scale * Game::Map::world_prop_render_scale(
                                   Game::Map::WorldProp::Type::MagicShrine));
    inst.color_rot =
        QVector4D(k_base_color_r, k_base_color_g, k_base_color_b, prop.rotation);
    m_state.instances.push_back(inst);
  }

  m_state.instance_count = m_state.instances.size();
  m_state.instances_dirty = m_state.instance_count > 0;
}

} // namespace Render::GL
