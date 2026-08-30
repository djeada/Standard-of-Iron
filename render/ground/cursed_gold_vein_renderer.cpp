#include "cursed_gold_vein_renderer.h"

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

// Dark, iron-stained rock. The gold and the curse glow come from the shader.
constexpr float k_base_color_r = 0.24F;
constexpr float k_base_color_g = 0.21F;
constexpr float k_base_color_b = 0.18F;

} // namespace

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

void CursedGoldVeinRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  if (submit_prop_common(
          renderer, resources, TerrainScatterCmd::Species::CursedGoldVein) == 0) {
    return;
  }

  // A low, warm ore-light with a slow uneasy flicker: the vein is lit from within,
  // unlike the shrine's cool steady votive glow.
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

  const auto& terrain_service = world().terrain_or_empty();

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::CursedGoldVein) {
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
                                   Game::Map::WorldProp::Type::CursedGoldVein));
    inst.color_rot =
        QVector4D(k_base_color_r, k_base_color_g, k_base_color_b, prop.rotation);
    m_state.instances.push_back(inst);
  }

  m_state.instance_count = m_state.instances.size();
  m_state.instances_dirty = m_state.instance_count > 0;
}

} // namespace Render::GL
