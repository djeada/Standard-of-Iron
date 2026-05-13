#include "firecamp_renderer.h"
#include "../scene_renderer.h"
#include "decoration_gpu.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "ground_utils.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "scatter_runtime.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using std::uint32_t;
using namespace Render::Ground;

} // namespace

namespace Render::GL {

FireCampRenderer::FireCampRenderer() = default;
FireCampRenderer::~FireCampRenderer() = default;

void FireCampRenderer::configure(
    const Game::Map::TerrainHeightMap &height_map,
    const Game::Map::BiomeSettings &biome_settings,
    const std::vector<Game::Map::WorldProp> &world_props) {
  m_width = height_map.get_width();
  m_height = height_map.get_height();
  m_tile_size = height_map.get_tile_size();
  m_height_data = height_map.get_height_data();
  m_terrain_types = height_map.getTerrainTypes();
  m_world_props = world_props;
  m_biome_settings = biome_settings;
  m_noise_seed = biome_settings.seed;

  m_firecamp_state.reset_instances();
  auto &firecamp_params = m_firecamp_state.params;

  firecamp_params.time = 0.0F;
  firecamp_params.flicker_speed = 5.0F;
  firecamp_params.flicker_amount = 0.02F;
  firecamp_params.glow_strength = 1.1F;

  generate_firecamp_instances();
}

void FireCampRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  (void)resources;

  const auto visible_count = Scatter::sync_filtered_state(
      m_firecamp_state,
      [](const FireCampInstanceGpu &instance) -> const QVector4D & {
        return instance.pos_intensity;
      });
  if (visible_count == 0 || !m_firecamp_state.instance_buffer) {
    return;
  }

  FireCampBatchParams params = m_firecamp_state.params;
  params.time = renderer.get_animation_time();
  params.flicker_amount = m_firecamp_state.params.flicker_amount *
                          (0.9F + 0.25F * std::sin(params.time * 1.3F));
  params.glow_strength = m_firecamp_state.params.glow_strength *
                         (0.85F + 0.2F * std::sin(params.time * 1.7F + 1.2F));
  TerrainScatterCmd cmd;
  cmd.species = TerrainScatterCmd::Species::FireCamp;
  cmd.instance_buffer = m_firecamp_state.instance_buffer.get();
  cmd.instance_count = visible_count;
  cmd.firecamp = params;
  renderer.terrain_scatter(cmd);

  const QVector3D log_color(0.26F, 0.15F, 0.08F);
  const QVector3D char_color(0.08F, 0.05F, 0.03F);

  for (const auto &instance : m_firecamp_state.visible_instances) {
    const QVector4D pos_intensity = instance.pos_intensity;
    const QVector4D radius_phase = instance.radius_phase;

    const QVector3D camp_pos = pos_intensity.toVector3D();
    const float intensity = std::clamp(pos_intensity.w(), 0.6F, 1.6F);
    const float base_radius = std::max(radius_phase.x(), 1.0F);

    uint32_t state = hash_coords(
        static_cast<int>(std::floor(camp_pos.x())),
        static_cast<int>(std::floor(camp_pos.z())),
        static_cast<uint32_t>(radius_phase.y() *
                              HashConstants::k_temporal_variation_frequency));

    const float time = params.time;
    const float char_amount =
        std::clamp(time * 0.015F + rand_01(state) * 0.05F, 0.0F, 1.0F);

    const QVector3D blended_log_color =
        log_color * (1.0F - char_amount) + char_color * (char_amount + 0.15F);

    const float log_length = std::clamp(base_radius * 0.85F, 0.45F, 1.1F);
    const float log_radius = std::clamp(base_radius * 0.08F, 0.03F, 0.08F);

    const float base_yaw = (rand_01(state) - 0.5F) * 0.35F;
    const float cos_base = std::cos(base_yaw);
    const float sin_base = std::sin(base_yaw);
    const QVector3D axis_a(cos_base, 0.0F, sin_base);
    const QVector3D axis_b(-axis_a.z(), 0.0F, axis_a.x());

    const QVector3D base_center = camp_pos + QVector3D(0.0F, -0.02F, 0.0F);
    const QVector3D base_half_a = axis_a * (log_length * 0.5F);
    const QVector3D base_half_b = axis_b * (log_length * 0.45F);

    renderer.cylinder(base_center - base_half_a, base_center + base_half_a,
                      log_radius, blended_log_color, 1.0F);
    renderer.cylinder(base_center - base_half_b, base_center + base_half_b,
                      log_radius, blended_log_color, 1.0F);

    if (rand_01(state) > 0.25F) {
      float const top_yaw = base_yaw + 0.6F + (rand_01(state) - 0.5F) * 0.35F;
      QVector3D const top_axis(std::cos(top_yaw), 0.0F, std::sin(top_yaw));
      QVector3D const top_half = top_axis * (log_length * 0.35F);
      QVector3D const top_center =
          camp_pos + QVector3D(0.0F, log_radius * 1.6F, 0.0F);
      float const top_radius = log_radius * 0.85F;
      renderer.cylinder(top_center - top_half, top_center + top_half,
                        top_radius, blended_log_color, 1.0F);
    }
  }
}

void FireCampRenderer::clear() { m_firecamp_state.reset_instances(); }

void FireCampRenderer::generate_firecamp_instances() {
  auto &firecamp_instances = m_firecamp_state.instances;
  auto &firecamp_instance_count = m_firecamp_state.instance_count;
  auto &firecamp_instances_dirty = m_firecamp_state.instances_dirty;

  firecamp_instances.clear();

  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    return;
  }
  auto &terrain_service = Game::Map::TerrainService::instance();
  const float half_w = static_cast<float>(m_width) * 0.5F;
  const float half_h = static_cast<float>(m_height) * 0.5F;

  for (size_t i = 0; i < m_world_props.size(); ++i) {
    const auto &prop = m_world_props[i];
    if (prop.type != Game::Map::WorldProp::Type::FireCamp) {
      continue;
    }

    const float world_x = (prop.x - half_w) * m_tile_size;
    const float world_z = (prop.z - half_h) * m_tile_size;
    const QVector3D resolved = terrain_service.resolve_surface_world_position(
        world_x, world_z, 0.0F, 0.0F);

    FireCampInstanceGpu instance;
    instance.pos_intensity =
        QVector4D(resolved.x(), resolved.y(), resolved.z(), prop.intensity);
    instance.radius_phase =
        QVector4D(std::max(prop.radius, 1.0F),
                  static_cast<float>(i) * 1.234567F, 1.0F, 0.0F);
    firecamp_instances.push_back(instance);
  }

  firecamp_instance_count = firecamp_instances.size();
  firecamp_instances_dirty = firecamp_instance_count > 0;
}

} // namespace Render::GL
