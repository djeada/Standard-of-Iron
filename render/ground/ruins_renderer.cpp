#include "ruins_renderer.h"
#include "../scene_renderer.h"
#include "decoration_gpu.h"
#include "gl/render_constants.h"
#include "ground_utils.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "scatter_runtime.h"
#include <QVector4D>
#include <cmath>
#include <cstdint>

namespace {

using std::uint32_t;
using namespace Render::Ground;

constexpr float k_ring_distance_scale = 2.50F;
constexpr uint32_t k_type_salt = 0xD7E8F901U;
constexpr float k_base_scale = 0.90F;
constexpr float k_base_color_r = 0.55F;
constexpr float k_base_color_g = 0.52F;
constexpr float k_base_color_b = 0.46F;

} // namespace

namespace Render::GL {

RuinsRenderer::RuinsRenderer() = default;
RuinsRenderer::~RuinsRenderer() = default;

void RuinsRenderer::configure(
    const Game::Map::TerrainHeightMap &height_map,
    const Game::Map::BiomeSettings &biome_settings,
    const std::vector<Game::Map::FireCamp> &fire_camps,
    const std::vector<Game::Map::WorldProp> &world_props) {
  m_biome_settings = biome_settings;
  m_state.reset_instances();
  m_state.params.light_direction = RuinsBatchParams::default_light_direction();
  generate_instances(fire_camps, world_props, height_map);
}

void RuinsRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  Q_UNUSED(resources);

  const auto visible_count = Scatter::sync_filtered_state(
      m_state, [](const RuinsInstanceGpu &inst) -> const QVector4D & {
        return inst.pos_scale;
      });
  if (visible_count == 0 || !m_state.instance_buffer) {
    return;
  }

  m_state.params.time = renderer.get_animation_time();

  TerrainScatterCmd cmd;
  cmd.species = TerrainScatterCmd::Species::Ruins;
  cmd.instance_buffer = m_state.instance_buffer.get();
  cmd.instance_count = visible_count;
  cmd.ruins = m_state.params;
  renderer.terrain_scatter(cmd);
}

void RuinsRenderer::clear() { m_state.reset_instances(); }

void RuinsRenderer::generate_instances(
    const std::vector<Game::Map::FireCamp> &fire_camps,
    const std::vector<Game::Map::WorldProp> &world_props,
    const Game::Map::TerrainHeightMap &height_map) {

  auto &terrain_service = Game::Map::TerrainService::instance();
  const float tile_size = height_map.get_tile_size();
  const int width = height_map.get_width();
  const int map_height = height_map.get_height();
  const float half_w = static_cast<float>(width) * 0.5F;
  const float half_h = static_cast<float>(map_height) * 0.5F;

  // Ruins are placed only via explicit world_props in the map definition.
  // They do not scatter near fire camps since ancient ruins near military
  // camps would be historically incongruous.
  Q_UNUSED(fire_camps);

  for (const auto &prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Ruins) {
      continue;
    }
    const float wx = (prop.x - half_w) * tile_size;
    const float wz = (prop.z - half_h) * tile_size;
    const QVector3D resolved =
        terrain_service.resolve_surface_world_position(wx, wz, 0.0F, 0.0F);

    RuinsInstanceGpu inst;
    inst.pos_scale =
        QVector4D(resolved.x(), resolved.y(), resolved.z(), prop.scale);
    inst.color_rot = QVector4D(k_base_color_r, k_base_color_g, k_base_color_b,
                               prop.rotation);
    m_state.instances.push_back(inst);
  }

  m_state.instance_count = m_state.instances.size();
  m_state.instances_dirty = m_state.instance_count > 0;
}

} // namespace Render::GL
