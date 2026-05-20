#include "iron_ore_renderer.h"

#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "../scene_renderer.h"
#include "decoration_gpu.h"
#include "ground_utils.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "scatter_composition.h"
#include "scatter_runtime.h"
#include "spawn_validator.h"

namespace {

using std::uint32_t;
using namespace Render::Ground;

} // namespace

namespace Render::GL {

IronOreRenderer::IronOreRenderer() = default;
IronOreRenderer::~IronOreRenderer() = default;

void IronOreRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                                const Game::Map::BiomeSettings& biome_settings,
                                const std::vector<Game::Map::WorldProp>& world_props) {
  m_biome_settings = biome_settings;
  m_state.reset_instances();
  m_state.params.light_direction = m_light_direction;
  generate_instances(world_props, height_map);
}

void IronOreRenderer::set_light_direction(const QVector3D& dir) {
  m_light_direction =
      dir.isNull() ? IronOreBatchParams::default_light_direction() : dir.normalized();
  m_state.params.light_direction = m_light_direction;
}

void IronOreRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  Q_UNUSED(resources);

  const auto visible_count = Scatter::sync_filtered_state(
      m_state, [](const IronOreInstanceGpu& inst) -> const QVector4D& {
        return inst.pos_scale;
      });
  if (visible_count == 0 || !m_state.instance_buffer) {
    return;
  }

  TerrainScatterCmd cmd;
  cmd.species = TerrainScatterCmd::Species::IronOre;
  cmd.instance_buffer = m_state.instance_buffer.get();
  cmd.instance_count = visible_count;
  cmd.iron_ore = m_state.params;
  renderer.terrain_scatter(cmd);
}

void IronOreRenderer::clear() {
  m_state.reset_instances();
}

void IronOreRenderer::generate_instances(
    const std::vector<Game::Map::WorldProp>& world_props,
    const Game::Map::TerrainHeightMap& /*height_map*/) {

  auto& terrain_service = Game::Map::TerrainService::instance();

  const auto surface_profile = Game::Map::make_surface_profile(m_biome_settings);

  for (const auto& prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::IronOre) {
      continue;
    }
    const QVector3D resolved = terrain_service.world_prop_world_position(prop);

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 m_biome_settings.seed ^ 0xB7C3D1A4U);

    // Dark rock base with slight iron-brown tint
    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D color = surface_profile.rock_low * (1.0F - color_var) +
                      surface_profile.rock_high * color_var;
    // Darken and shift toward iron-brown
    color = color * 0.76F;
    QVector3D const iron_tint(0.38F, 0.20F, 0.14F);
    float const iron_mix = remap(rand_01(state), 0.14F, 0.34F);
    color = color * (1.0F - iron_mix) + iron_tint * iron_mix;

    IronOreInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(),
                               resolved.y(),
                               resolved.z(),
                               prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::IronOre));
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    m_state.instances.push_back(inst);
  }

  m_state.instance_count = m_state.instances.size();
  m_state.instances_dirty = m_state.instance_count > 0;
}

} // namespace Render::GL
