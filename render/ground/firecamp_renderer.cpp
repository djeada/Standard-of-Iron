#include "firecamp_renderer.h"
#include "../scene_renderer.h"
#include "decoration_gpu.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "ground_utils.h"
#include "map/terrain.h"
#include "scatter_runtime.h"
#include "spawn_validator.h"
#include <QDebug>
#include <QVector2D>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <qglobal.h>
#include <qvectornd.h>
#include <vector>

namespace {

using std::uint32_t;
using namespace Render::Ground;

inline auto smooth_value_noise(float x, float z, uint32_t seed) -> float {
  int const ix = static_cast<int>(std::floor(x));
  int const iz = static_cast<int>(std::floor(z));
  float fx = x - static_cast<float>(ix);
  float fz = z - static_cast<float>(iz);

  fx = fx * fx * (3.0F - 2.0F * fx);
  fz = fz * fz * (3.0F - 2.0F * fz);

  uint32_t s00 = hash_coords(ix, iz, seed);
  uint32_t s10 = hash_coords(ix + 1, iz, seed);
  uint32_t s01 = hash_coords(ix, iz + 1, seed);
  uint32_t s11 = hash_coords(ix + 1, iz + 1, seed);

  float const v00 = rand_01(s00);
  float const v10 = rand_01(s10);
  float const v01 = rand_01(s01);
  float const v11 = rand_01(s11);

  float const v0 = v00 * (1.0F - fx) + v10 * fx;
  float const v1 = v01 * (1.0F - fx) + v11 * fx;
  return v0 * (1.0F - fz) + v1 * fz;
}

auto rotate_xz(const QVector3D &v, float yaw) -> QVector3D {
  float const c = std::cos(yaw);
  float const s = std::sin(yaw);
  return QVector3D(v.x() * c - v.z() * s, v.y(), v.x() * s + v.z() * c);
}

void draw_storytelling_objects(Render::GL::Renderer &renderer,
                               const QVector3D &camp_pos, float base_radius,
                               float intensity, uint32_t seed_state) {
  constexpr float k_layout_radius_scale = 1.35F;
  constexpr float k_story_base_scale = 0.85F;
  constexpr float k_story_intensity_factor = 0.15F;
  constexpr float k_story_min_scale = 0.8F;
  constexpr float k_story_max_scale = 1.2F;

  uint32_t state = seed_state;
  float const layout_radius =
      std::max(2.0F, base_radius * k_layout_radius_scale);
  float const story_scale =
      std::clamp(k_story_base_scale + intensity * k_story_intensity_factor,
                 k_story_min_scale, k_story_max_scale) *
      base_radius;

  // Position storytelling props on a ring around the fire camp center.
  auto place_around = [&](float angle, float distance_scale) -> QVector3D {
    QVector3D const dir(std::cos(angle), 0.0F, std::sin(angle));
    return camp_pos + dir * (layout_radius * distance_scale);
  };
  auto random_angle = [&]() -> float {
    return rand_01(state) * MathConstants::k_two_pi;
  };

  // Roman tent
  {
    float const angle = random_angle();
    QVector3D const center = place_around(angle, 1.25F);
    QVector3D const axis = rotate_xz(QVector3D(1.0F, 0.0F, 0.0F), angle);
    QVector3D const side = QVector3D(-axis.z(), 0.0F, axis.x());
    float const half_len = story_scale * 0.70F;
    float const half_width = story_scale * 0.45F;
    float const peak_h = story_scale * 0.45F;
    QVector3D const left = center - side * half_width;
    QVector3D const right = center + side * half_width;
    QVector3D const front_left = left - axis * half_len;
    QVector3D const front_right = right - axis * half_len;
    QVector3D const back_left = left + axis * half_len;
    QVector3D const back_right = right + axis * half_len;
    QVector3D const front_peak =
        center - axis * half_len + QVector3D(0, peak_h, 0);
    QVector3D const back_peak =
        center + axis * half_len + QVector3D(0, peak_h, 0);
    QVector3D const tent_color(0.74F, 0.68F, 0.57F);
    renderer.cylinder(front_left, front_peak, 0.04F * story_scale, tent_color);
    renderer.cylinder(front_right, front_peak, 0.04F * story_scale, tent_color);
    renderer.cylinder(back_left, back_peak, 0.04F * story_scale, tent_color);
    renderer.cylinder(back_right, back_peak, 0.04F * story_scale, tent_color);
    renderer.cylinder(front_peak, back_peak, 0.03F * story_scale, tent_color);
  }

  // Supply cart
  {
    float const angle = random_angle();
    QVector3D const center = place_around(angle, 1.55F);
    QVector3D const forward = rotate_xz(QVector3D(1.0F, 0.0F, 0.0F), angle);
    QVector3D const right = QVector3D(-forward.z(), 0.0F, forward.x());
    float const cart_half_len = story_scale * 0.50F;
    float const cart_half_width = story_scale * 0.30F;
    float const bed_y = story_scale * 0.18F;
    QVector3D const wood_color(0.40F, 0.26F, 0.13F);
    QVector3D const wheel_color(0.30F, 0.20F, 0.10F);
    QVector3D const rear =
        center - forward * cart_half_len + QVector3D(0, bed_y, 0);
    QVector3D const front =
        center + forward * cart_half_len + QVector3D(0, bed_y, 0);
    renderer.cylinder(rear - right * cart_half_width,
                      rear + right * cart_half_width, 0.04F * story_scale,
                      wood_color);
    renderer.cylinder(front - right * cart_half_width,
                      front + right * cart_half_width, 0.04F * story_scale,
                      wood_color);
    renderer.cylinder(rear - right * cart_half_width,
                      front - right * cart_half_width, 0.03F * story_scale,
                      wood_color);
    renderer.cylinder(rear + right * cart_half_width,
                      front + right * cart_half_width, 0.03F * story_scale,
                      wood_color);
    QVector3D const axle = center + QVector3D(0, story_scale * 0.10F, 0);
    renderer.cylinder(axle - right * (cart_half_width * 1.2F),
                      axle + right * (cart_half_width * 1.2F),
                      0.025F * story_scale, wood_color);
    renderer.cylinder(axle - right * (cart_half_width * 1.2F) -
                          QVector3D(0, story_scale * 0.20F, 0),
                      axle - right * (cart_half_width * 1.2F) +
                          QVector3D(0, story_scale * 0.20F, 0),
                      0.07F * story_scale, wheel_color);
    renderer.cylinder(axle + right * (cart_half_width * 1.2F) -
                          QVector3D(0, story_scale * 0.20F, 0),
                      axle + right * (cart_half_width * 1.2F) +
                          QVector3D(0, story_scale * 0.20F, 0),
                      0.07F * story_scale, wheel_color);
  }

  // Weapon rack
  {
    float const angle = random_angle();
    QVector3D const center = place_around(angle, 1.35F);
    QVector3D const side = rotate_xz(QVector3D(1.0F, 0.0F, 0.0F), angle);
    QVector3D const up(0.0F, 1.0F, 0.0F);
    QVector3D const rack_color(0.35F, 0.22F, 0.12F);
    QVector3D const spear_color(0.55F, 0.56F, 0.58F);
    float const post_h = story_scale * 0.65F;
    QVector3D const left = center - side * (story_scale * 0.35F);
    QVector3D const right = center + side * (story_scale * 0.35F);
    renderer.cylinder(left, left + up * post_h, 0.035F * story_scale,
                      rack_color);
    renderer.cylinder(right, right + up * post_h, 0.035F * story_scale,
                      rack_color);
    renderer.cylinder(left + up * (post_h * 0.7F), right + up * (post_h * 0.7F),
                      0.03F * story_scale, rack_color);
    QVector3D const lean_dir =
        rotate_xz(QVector3D(0.15F, 1.0F, 0.0F), angle).normalized();
    renderer.cylinder(center - side * (story_scale * 0.2F),
                      center - side * (story_scale * 0.2F) +
                          lean_dir * (story_scale * 0.95F),
                      0.018F * story_scale, spear_color);
    renderer.cylinder(center + side * (story_scale * 0.05F),
                      center + side * (story_scale * 0.05F) +
                          lean_dir * (story_scale * 0.9F),
                      0.018F * story_scale, spear_color);
    renderer.cylinder(center + side * (story_scale * 0.3F),
                      center + side * (story_scale * 0.3F) +
                          lean_dir * (story_scale * 0.92F),
                      0.018F * story_scale, spear_color);
  }

  // Ruins
  {
    float const angle = random_angle();
    QVector3D const center = place_around(angle, 1.75F);
    QVector3D const right = rotate_xz(QVector3D(1.0F, 0.0F, 0.0F), angle);
    QVector3D const ruin_color(0.55F, 0.52F, 0.46F);
    float const col_h_a = story_scale * 0.50F;
    float const col_h_b = story_scale * 0.36F;
    float const col_h_c = story_scale * 0.62F;
    renderer.cylinder(center - right * (story_scale * 0.25F),
                      center - right * (story_scale * 0.25F) +
                          QVector3D(0, col_h_a, 0),
                      0.07F * story_scale, ruin_color);
    renderer.cylinder(center + right * (story_scale * 0.25F),
                      center + right * (story_scale * 0.25F) +
                          QVector3D(0, col_h_b, 0),
                      0.07F * story_scale, ruin_color);
    renderer.cylinder(center + QVector3D(0, 0, story_scale * 0.2F),
                      center + QVector3D(0, col_h_c, story_scale * 0.2F),
                      0.07F * story_scale, ruin_color);
    renderer.cylinder(
        center - right * (story_scale * 0.3F) + QVector3D(0, col_h_a * 0.9F, 0),
        center + right * (story_scale * 0.3F) + QVector3D(0, col_h_b * 0.9F, 0),
        0.03F * story_scale, ruin_color);
  }

  // Dead tree
  {
    float const angle = random_angle();
    QVector3D const base = place_around(angle, 1.9F);
    QVector3D const trunk_top = base + QVector3D(0, story_scale * 0.95F, 0);
    QVector3D const branch_a =
        trunk_top +
        rotate_xz(QVector3D(0.45F, 0.35F, 0.0F), angle) * story_scale;
    QVector3D const branch_b =
        trunk_top +
        rotate_xz(QVector3D(-0.35F, 0.28F, 0.3F), angle) * story_scale;
    QVector3D const bark_color(0.22F, 0.14F, 0.08F);
    renderer.cylinder(base, trunk_top, 0.06F * story_scale, bark_color);
    renderer.cylinder(trunk_top, branch_a, 0.035F * story_scale, bark_color);
    renderer.cylinder(trunk_top - QVector3D(0, story_scale * 0.1F, 0), branch_b,
                      0.03F * story_scale, bark_color);
  }
}

} // namespace

namespace Render::GL {

FireCampRenderer::FireCampRenderer() = default;
FireCampRenderer::~FireCampRenderer() = default;

void FireCampRenderer::configure(
    const Game::Map::TerrainHeightMap &height_map,
    const Game::Map::BiomeSettings &biome_settings) {
  m_width = height_map.get_width();
  m_height = height_map.get_height();
  m_tile_size = height_map.get_tile_size();
  m_height_data = height_map.get_height_data();
  m_terrain_types = height_map.getTerrainTypes();
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

    draw_storytelling_objects(renderer, camp_pos, base_radius, intensity,
                              state);
  }
}

void FireCampRenderer::clear() {
  m_firecamp_state.reset_instances();
  m_explicit_positions.clear();
  m_explicit_intensities.clear();
  m_explicit_radii.clear();
}

void FireCampRenderer::set_explicit_fire_camps(
    const std::vector<QVector3D> &positions,
    const std::vector<float> &intensities, const std::vector<float> &radii) {
  m_explicit_positions = positions;
  m_explicit_intensities = intensities;
  m_explicit_radii = radii;
  m_firecamp_state.instances_dirty = true;
  if (m_width > 0 && m_height > 0 && !m_height_data.empty()) {
    generate_firecamp_instances();
  }
}

void FireCampRenderer::add_explicit_firecamps(const SpawnValidator &validator) {
  if (m_explicit_positions.empty()) {
    return;
  }

  for (size_t i = 0; i < m_explicit_positions.size(); ++i) {
    const QVector3D &pos = m_explicit_positions[i];

    if (!validator.can_spawn_at_world(pos.x(), pos.z())) {
      continue;
    }

    float intensity = 1.0F;
    if (i < m_explicit_intensities.size()) {
      intensity = m_explicit_intensities[i];
    }

    float radius = 3.0F;
    if (i < m_explicit_radii.size()) {
      radius = m_explicit_radii[i];
    }

    float const phase = static_cast<float>(i) * 1.234567F;

    FireCampInstanceGpu instance;
    instance.pos_intensity = QVector4D(pos.x(), pos.y(), pos.z(), intensity);
    instance.radius_phase = QVector4D(radius, phase, 1.0F, 0.0F);
    m_firecamp_state.instances.push_back(instance);
  }
}

void FireCampRenderer::generate_firecamp_instances() {
  auto &firecamp_instances = m_firecamp_state.instances;
  auto &firecamp_instance_count = m_firecamp_state.instance_count;
  auto &firecamp_instances_dirty = m_firecamp_state.instances_dirty;

  firecamp_instances.clear();

  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    return;
  }

  const auto scatter_profile =
      Game::Map::make_scatter_profile(m_biome_settings);
  const float tile_safe = std::max(0.1F, m_tile_size);

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(m_height_data, m_terrain_types, m_width,
                                      m_height, m_tile_size);

  SpawnValidationConfig config = make_firecamp_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding = scatter_profile.spawn_edge_padding;

  SpawnValidator validator(terrain_cache, config);

  float const fire_camp_density = 0.02F;

  auto add_fire_camp = [&](float gx, float gz, uint32_t &state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    float const sgx = std::clamp(gx, 0.0F, float(m_width - 1));
    float const sgz = std::clamp(gz, 0.0F, float(m_height - 1));

    int const ix = std::clamp(int(std::floor(sgx + 0.5F)), 0, m_width - 1);
    int const iz = std::clamp(int(std::floor(sgz + 0.5F)), 0, m_height - 1);
    int const normal_idx = iz * m_width + ix;

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    float const world_y = m_height_data[static_cast<size_t>(normal_idx)];

    float const intensity = remap(rand_01(state), 0.8F, 1.2F);
    float const radius = remap(rand_01(state), 2.0F, 4.0F) * tile_safe;

    float const phase = rand_01(state) * MathConstants::k_two_pi;

    float const duration = 1.0F;

    FireCampInstanceGpu instance;
    instance.pos_intensity = QVector4D(world_x, world_y, world_z, intensity);
    instance.radius_phase = QVector4D(radius, phase, duration, 0.0F);
    firecamp_instances.push_back(instance);
    return true;
  };

  constexpr int k_grid_spacing = 20;

  for (int z = 0; z < m_height; z += k_grid_spacing) {
    for (int x = 0; x < m_width; x += k_grid_spacing) {
      int const idx = z * m_width + x;

      float const slope = terrain_cache.get_slope_at(x, z);
      if (slope > 0.3F) {
        continue;
      }

      uint32_t state = hash_coords(
          x, z, m_noise_seed ^ 0xF12ECA3FU ^ static_cast<uint32_t>(idx));

      float world_x = 0.0F;
      float world_z = 0.0F;
      validator.grid_to_world(static_cast<float>(x), static_cast<float>(z),
                              world_x, world_z);

      float const cluster_noise = smooth_value_noise(
          world_x * 0.02F, world_z * 0.02F, m_noise_seed ^ 0xCA3F12E0U);

      if (cluster_noise < 0.4F) {
        continue;
      }

      Game::Map::TerrainType const terrain_type =
          terrain_cache.get_terrain_type_at(x, z);
      float density_mult = 1.0F;
      if (terrain_type == Game::Map::TerrainType::Hill) {
        density_mult = 0.5F;
      } else if (terrain_type == Game::Map::TerrainType::Mountain) {
        density_mult = 0.0F;
      }

      float const effective_density = fire_camp_density * density_mult;
      if (rand_01(state) < effective_density) {
        float const gx = float(x) + rand_01(state) * float(k_grid_spacing);
        float const gz = float(z) + rand_01(state) * float(k_grid_spacing);
        add_fire_camp(gx, gz, state);
      }
    }
  }

  add_explicit_firecamps(validator);

  firecamp_instance_count = firecamp_instances.size();
  firecamp_instances_dirty = firecamp_instance_count > 0;

  qDebug() << "FireCampRenderer: Generated" << firecamp_instance_count
           << "total instances";
}

} // namespace Render::GL
