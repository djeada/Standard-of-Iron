#include "tent_renderer.h"
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

// Ring distance and type salt – unique per prop type so props spread out.
constexpr float k_ring_distance_scale = 1.35F;
constexpr uint32_t k_type_salt = 0xA1B2C3D4U;
constexpr float k_base_scale = 0.55F;
constexpr float k_base_color_r = 0.74F;
constexpr float k_base_color_g = 0.68F;
constexpr float k_base_color_b = 0.57F;

} // namespace

namespace Render::GL {

TentRenderer::TentRenderer() = default;
TentRenderer::~TentRenderer() = default;

void TentRenderer::configure(
    const Game::Map::TerrainHeightMap &height_map,
    const Game::Map::BiomeSettings &biome_settings,
    const std::vector<Game::Map::FireCamp> &fire_camps,
    const std::vector<Game::Map::WorldProp> &world_props) {
  m_biome_settings = biome_settings;
  m_state.reset_instances();
  m_state.params.light_direction = TentBatchParams::default_light_direction();
  generate_instances(fire_camps, world_props, height_map);
}

void TentRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  Q_UNUSED(resources);

  const auto visible_count = Scatter::sync_filtered_state(
      m_state,
      [](const TentInstanceGpu &inst) -> const QVector4D & {
        return inst.pos_scale;
      });
  if (visible_count == 0 || !m_state.instance_buffer) {
    return;
  }

  m_state.params.time = renderer.get_animation_time();

  TerrainScatterCmd cmd;
  cmd.species = TerrainScatterCmd::Species::Tent;
  cmd.instance_buffer = m_state.instance_buffer.get();
  cmd.instance_count = visible_count;
  cmd.tent = m_state.params;
  renderer.terrain_scatter(cmd);
}

void TentRenderer::clear() { m_state.reset_instances(); }

void TentRenderer::generate_instances(
    const std::vector<Game::Map::FireCamp> &fire_camps,
    const std::vector<Game::Map::WorldProp> &world_props,
    const Game::Map::TerrainHeightMap &height_map) {

  auto &terrain_service = Game::Map::TerrainService::instance();
  const float tile_size = height_map.get_tile_size();
  const int width = height_map.get_width();
  const int map_height = height_map.get_height();
  const float half_w = static_cast<float>(width) * 0.5F;
  const float half_h = static_cast<float>(map_height) * 0.5F;

  // Auto-place one tent per firecamp on a deterministic ring.
  for (const auto &camp : fire_camps) {
    const float world_cx = (camp.x - half_w) * tile_size;
    const float world_cz = (camp.z - half_h) * tile_size;

    uint32_t state = hash_coords(
        static_cast<int>(std::floor(world_cx)),
        static_cast<int>(std::floor(world_cz)),
        k_type_salt);

    const float angle = rand_01(state) * MathConstants::k_two_pi;
    const float dist = std::max(camp.radius, 1.5F) * k_ring_distance_scale;

    const float wx = world_cx + std::cos(angle) * dist;
    const float wz = world_cz + std::sin(angle) * dist;
    const QVector3D resolved =
        terrain_service.resolve_surface_world_position(wx, wz, 0.0F, 0.0F);

    const float scale = k_base_scale * std::clamp(camp.radius * 0.4F, 0.6F, 1.4F);
    const float rotation = rand_01(state) * MathConstants::k_two_pi;

    TentInstanceGpu inst;
    inst.pos_scale = QVector4D(resolved.x(), resolved.y(), resolved.z(), scale);
    inst.color_rot = QVector4D(k_base_color_r, k_base_color_g, k_base_color_b, rotation);
    m_state.instances.push_back(inst);
  }

  // Explicit world props from the map definition.
  for (const auto &prop : world_props) {
    if (prop.type != Game::Map::WorldProp::Type::Tent) {
      continue;
    }
    const float wx = (prop.x - half_w) * tile_size;
    const float wz = (prop.z - half_h) * tile_size;
    const QVector3D resolved =
        terrain_service.resolve_surface_world_position(wx, wz, 0.0F, 0.0F);

    TentInstanceGpu inst;
    inst.pos_scale =
        QVector4D(resolved.x(), resolved.y(), resolved.z(), prop.scale);
    inst.color_rot =
        QVector4D(k_base_color_r, k_base_color_g, k_base_color_b, prop.rotation);
    m_state.instances.push_back(inst);
  }

  m_state.instance_count = m_state.instances.size();
  m_state.instances_dirty = m_state.instance_count > 0;
}

} // namespace Render::GL
