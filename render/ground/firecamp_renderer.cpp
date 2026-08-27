#include "firecamp_renderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "decoration_gpu.h"
#include "game/map/scatter/ground_utils.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/scene_renderer.h"
#include "scatter_runtime.h"
#include "scatter_submission.h"

namespace {

using std::uint32_t;
using namespace Render::Ground;

} // namespace

namespace Render::GL {

FireCampRenderer::FireCampRenderer() = default;
FireCampRenderer::~FireCampRenderer() = default;

void FireCampRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                                 const Game::Map::BiomeSettings& biome_settings,
                                 const std::vector<Game::Map::WorldProp>& world_props) {
  configure_height_scatter_common(height_map, biome_settings, {}, world_props, false);

  m_state.track_visible_instances = true;

  auto& firecamp_params = m_state.params;
  firecamp_params.time = 0.0F;
  firecamp_params.flicker_speed = 4.4F;
  firecamp_params.flicker_amount = 0.028F;
  firecamp_params.glow_strength = 1.16F;

  generate_firecamp_instances();
}

void FireCampRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  (void)resources;

  const auto visible_count = Scatter::sync_filtered_state(
      m_state,
      [](const FireCampInstanceGpu& instance) -> const QVector4D& {
        return instance.pos_intensity;
      },
      renderer.static_world_visibility_filter_enabled()
          ? renderer.submission_visibility().snapshot()
          : nullptr);
  if (visible_count == 0) {
    return;
  }

  FireCampBatchParams params = m_state.params;
  params.time = renderer.get_animation_time();
  params.flicker_amount =
      m_state.params.flicker_amount * (0.9F + 0.25F * std::sin(params.time * 1.3F));
  params.glow_strength = m_state.params.glow_strength *
                         (0.85F + 0.2F * std::sin(params.time * 1.7F + 1.2F));
  TerrainScatterCmd cmd;
  cmd.visibility = renderer.visibility_mask();
  cmd.species = TerrainScatterCmd::Species::FireCamp;
  cmd.firecamp = params;
  Scatter::submit_visible_chunks(renderer, m_state, cmd);

  for (const auto& instance : m_state.visible_instances) {
    const QVector4D pos_intensity = instance.pos_intensity;
    const QVector4D radius_phase = instance.radius_phase;

    const QVector3D camp_pos = pos_intensity.toVector3D();

    if (!renderer.submission_visibility().accepts_sphere(
            camp_pos, 2.0F, SubmissionFogMode::Revealed)) {
      continue;
    }
    const float intensity = std::clamp(pos_intensity.w(), 0.6F, 1.6F);
    const float base_radius = std::max(radius_phase.x(), 1.0F);

    {
      const float flicker =
          0.92F + 0.08F * std::sin((params.time * 2.3F) + (radius_phase.y() * 6.28F));
      const FireLightShape shape = fire_light_shape(base_radius);
      Render::LocalLight firelight;
      firelight.position = camp_pos + QVector3D(0.0F, shape.height_above_ground, 0.0F);
      firelight.color = QVector3D(1.0F, 0.52F, 0.19F);
      firelight.radius = shape.reach;
      firelight.intensity = intensity * flicker;
      renderer.local_light(firelight);
    }

    const auto decor_index = static_cast<std::size_t>(radius_phase.w());
    if (decor_index >= m_camp_decor.size()) {
      continue;
    }
    const CampDecor& decor = m_camp_decor[decor_index];

    const QVector3D ember_color(0.58F, 0.105F, 0.025F);
    const float ember_pulse =
        0.5F + 0.5F * std::sin(params.time * 3.8F + decor.phase * 2.1F);

    for (const auto& piece : decor.cylinders) {
      const float weight = piece.ember_weight * ember_pulse;
      const QVector3D color = piece.base_color * (1.0F - weight) + ember_color * weight;
      renderer.cylinder(piece.start, piece.end, piece.radius, color, 1.0F);
    }
  }
}

auto FireCampRenderer::fire_light_shape(float camp_radius) -> FireLightShape {
  // The flame sits just above the logs, not at the height of the camp's own
  // radius - a light lifted nearly two metres spreads its pool out over the
  // ground instead of gathering it where the fire is.
  constexpr float k_flame_height = 0.55F;
  constexpr float k_reach_per_camp_radius = 1.8F;
  constexpr float k_min_reach = 3.0F;
  constexpr float k_max_reach = 6.0F;

  const float radius = std::max(camp_radius, 1.0F);
  return {std::min(radius * 0.55F, k_flame_height),
          std::clamp(radius * k_reach_per_camp_radius, k_min_reach, k_max_reach)};
}

void FireCampRenderer::build_camp_decor(const QVector3D& camp_pos,
                                        float base_radius,
                                        float phase,
                                        CampDecor& decor) const {
  const QVector3D log_color(0.31F, 0.17F, 0.075F);
  const QVector3D char_color(0.055F, 0.034F, 0.022F);

  decor.phase = phase;
  decor.cylinders.clear();

  uint32_t state = hash_coords(
      static_cast<int>(std::floor(camp_pos.x())),
      static_cast<int>(std::floor(camp_pos.z())),
      static_cast<uint32_t>(phase * HashConstants::k_temporal_variation_frequency));

  const float char_amount = remap(rand_01(state), 0.58F, 0.84F);
  const QVector3D blended_log_color =
      log_color * (1.0F - char_amount) + char_color * char_amount;

  const float log_length = std::clamp(base_radius * 0.85F, 0.45F, 1.1F);
  const float log_radius = std::clamp(base_radius * 0.08F, 0.03F, 0.08F);

  const float base_yaw = (rand_01(state) - 0.5F) * 0.35F;
  const QVector3D axis_a(std::cos(base_yaw), 0.0F, std::sin(base_yaw));
  const QVector3D axis_b(-axis_a.z(), 0.0F, axis_a.x());

  const QVector3D base_center = camp_pos + QVector3D(0.0F, -0.02F, 0.0F);
  const QVector3D base_half_a = axis_a * (log_length * 0.5F);
  const QVector3D base_half_b = axis_b * (log_length * 0.45F);

  constexpr float k_log_ember_weight = 0.08F;
  decor.cylinders.push_back({.start = base_center - base_half_a,
                             .end = base_center + base_half_a,
                             .base_color = blended_log_color,
                             .radius = log_radius,
                             .ember_weight = k_log_ember_weight});
  decor.cylinders.push_back({.start = base_center - base_half_b,
                             .end = base_center + base_half_b,
                             .base_color = blended_log_color,
                             .radius = log_radius,
                             .ember_weight = k_log_ember_weight});

  if (rand_01(state) > 0.25F) {
    float const top_yaw = base_yaw + 0.6F + (rand_01(state) - 0.5F) * 0.35F;
    QVector3D const top_axis(std::cos(top_yaw), 0.0F, std::sin(top_yaw));
    QVector3D const top_half = top_axis * (log_length * 0.35F);
    QVector3D const top_center = camp_pos + QVector3D(0.0F, log_radius * 1.6F, 0.0F);
    decor.cylinders.push_back({.start = top_center - top_half,
                               .end = top_center + top_half,
                               .base_color = blended_log_color,
                               .radius = log_radius * 0.85F,
                               .ember_weight = k_log_ember_weight});
  }

  const float ring_radius = std::clamp(base_radius * 0.23F, 0.52F, 0.82F);
  const int stone_count = 9;
  for (int stone = 0; stone < stone_count; ++stone) {
    float const angle = MathConstants::k_two_pi * static_cast<float>(stone) /
                            static_cast<float>(stone_count) +
                        base_yaw * 0.35F;
    float const jitter = remap(rand_01(state), -0.035F, 0.035F);
    QVector3D const radial(std::cos(angle), 0.0F, std::sin(angle));
    QVector3D const tangent(-radial.z(), 0.0F, radial.x());
    QVector3D const stone_center =
        camp_pos + radial * (ring_radius + jitter) + QVector3D(0.0F, 0.015F, 0.0F);
    float const stone_half_length = remap(rand_01(state), 0.065F, 0.105F);
    float const stone_radius = remap(rand_01(state), 0.075F, 0.115F);
    float const stone_tone = remap(rand_01(state), 0.76F, 1.05F);

    decor.cylinders.push_back(
        {.start = stone_center - tangent * stone_half_length,
         .end = stone_center + tangent * stone_half_length,
         .base_color = QVector3D(0.25F, 0.235F, 0.21F) * stone_tone,
         .radius = stone_radius,
         .ember_weight = 0.0F});
  }

  for (int coal = 0; coal < 5; ++coal) {
    float const angle = rand_01(state) * MathConstants::k_two_pi;
    float const distance = remap(rand_01(state), 0.08F, ring_radius * 0.48F);
    QVector3D const coal_center =
        camp_pos + QVector3D(std::cos(angle), 0.0F, std::sin(angle)) * distance +
        QVector3D(0.0F, 0.025F, 0.0F);
    QVector3D const coal_axis(-std::sin(angle), 0.0F, std::cos(angle));

    float const heat_factor = remap(rand_01(state), 0.22F, 0.68F);
    decor.cylinders.push_back({.start = coal_center - coal_axis * 0.035F,
                               .end = coal_center + coal_axis * 0.035F,
                               .base_color = char_color,
                               .radius = 0.032F,
                               .ember_weight = heat_factor});
  }
}

void FireCampRenderer::clear() {
  m_state.reset_instances();
  m_camp_decor.clear();
}

void FireCampRenderer::generate_firecamp_instances() {
  auto& firecamp_instances = m_state.instances;
  auto& firecamp_instance_count = m_state.instance_count;
  auto& firecamp_instances_dirty = m_state.instances_dirty;

  firecamp_instances.clear();
  m_camp_decor.clear();

  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    return;
  }
  const auto& terrain_service = world().terrain_or_empty();
  const float half_w = static_cast<float>(m_width) * 0.5F;
  const float half_h = static_cast<float>(m_height) * 0.5F;

  for (size_t i = 0; i < m_world_props.size(); ++i) {
    const auto& prop = m_world_props[i];
    if (prop.type != Game::Map::WorldProp::Type::FireCamp) {
      continue;
    }

    const float world_x = (prop.x - half_w) * m_tile_size;
    const float world_z = (prop.z - half_h) * m_tile_size;
    const QVector3D resolved =
        terrain_service.resolve_surface_world_position(world_x, world_z, 0.0F, 0.0F);

    const float base_radius = std::max(prop.radius, 1.0F);
    const float phase = static_cast<float>(i) * 1.234567F;

    CampDecor decor;
    build_camp_decor(resolved, base_radius, phase, decor);
    const auto decor_index = static_cast<float>(m_camp_decor.size());
    m_camp_decor.push_back(std::move(decor));

    FireCampInstanceGpu instance;
    instance.pos_intensity =
        QVector4D(resolved.x(), resolved.y(), resolved.z(), prop.intensity);

    instance.radius_phase = QVector4D(base_radius, phase, 1.0F, decor_index);
    firecamp_instances.push_back(instance);
  }

  firecamp_instance_count = firecamp_instances.size();
  firecamp_instances_dirty = firecamp_instance_count > 0;
}

} // namespace Render::GL
