#include "boulder_renderer.h"

#include <QVector3D>
#include <QVector4D>

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "decoration_gpu.h"
#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/scatter_composition.h"
#include "game/map/scatter/spawn_validator.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/scene_renderer.h"
#include "scatter_runtime.h"
#include "stone_ground_fit.h"

namespace {

using std::uint32_t;
using namespace Render::Ground;
constexpr float k_reference_scatter_extent = 220.0F;

} // namespace

namespace Render::GL {

BoulderRenderer::BoulderRenderer() = default;
BoulderRenderer::~BoulderRenderer() = default;

void BoulderRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                                const Game::Map::BiomeSettings& biome_settings,
                                const std::vector<Game::Map::WorldProp>& world_props) {
  configure_height_scatter_common(height_map, biome_settings, world_props);
  m_state.params.light_direction = m_light_direction;
  rebuild_boulder_instances();
}

void BoulderRenderer::refresh_world_props(
    const std::vector<Game::Map::WorldProp>& world_props) {
  m_world_props = world_props;
  rebuild_boulder_instances();
}

void BoulderRenderer::rebuild_boulder_instances() {
  m_state.instances.clear();
  append_world_prop_boulders();
  finish_instance_rebuild();
}

void BoulderRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, StoneBatchParams::default_light_direction());
}

void BoulderRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  submit_filtered_common<false>(
      renderer,
      resources,
      TerrainScatterCmd::Species::Stone,
      [](TerrainScatterCmd& cmd, const StoneBatchParams& params) {
        cmd.stone = params;
      });
}

void BoulderRenderer::append_world_prop_boulders() {
  const auto& terrain_service = world().terrain_or_empty();
  const auto surface_profile = Game::Map::make_surface_profile(m_biome_settings);

  for (const auto& prop : m_world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Boulder) {
      continue;
    }
    const QVector3D resolved = terrain_service.world_prop_world_position(prop);

    uint32_t state = hash_coords(static_cast<int>(prop.x),
                                 static_cast<int>(prop.z),
                                 m_biome_settings.seed ^ 0xD3A4B1C2U);
    QVector3D const color = stone_instance_color(
        surface_profile.rock_low, surface_profile.rock_high, 0.35F, 0.45F, state);

    float const render_scale = prop.scale * Game::Map::world_prop_render_scale(
                                                Game::Map::WorldProp::Type::Boulder);
    float const gx = world_to_grid_coord(resolved.x(), m_width, m_tile_size);
    float const gz = world_to_grid_coord(resolved.z(), m_height, m_tile_size);
    QVector3D const ground_normal =
        sample_ground_normal(m_height_data, m_width, m_height, m_tile_size, gx, gz);

    StoneInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(), resolved.y(), resolved.z(), render_scale);
    inst.color_rot = QVector4D(color.x(), color.y(), color.z(), prop.rotation);
    inst.ground_fit = pack_stone_ground_fit(ground_normal,
                                            rand_01(state),
                                            render_scale,
                                            stone_sink_fraction(ground_normal, state));
    m_state.instances.push_back(inst);
  }
}

} // namespace Render::GL
