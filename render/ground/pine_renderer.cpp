#include "pine_renderer.h"
#include "../scene_renderer.h"
#include "decoration_gpu.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "ground_utils.h"
#include "map/terrain.h"
#include "scatter_runtime.h"
#include "spawn_validator.h"
#include <QVector2D>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace {

using std::uint32_t;
using namespace Render::Ground;

constexpr int k_tree_cell_span = 4;
constexpr float k_tree_density_area_scale = 16.0F / 36.0F;
constexpr float k_tree_edge_padding_scale = 0.35F;

} // namespace

namespace Render::GL {

PineRenderer::PineRenderer() = default;
PineRenderer::~PineRenderer() = default;

void PineRenderer::configure(const Game::Map::TerrainHeightMap &height_map,
                             const Game::Map::BiomeSettings &biome_settings) {
  m_width = height_map.get_width();
  m_height = height_map.get_height();
  m_tile_size = height_map.get_tile_size();
  m_height_data = height_map.get_height_data();
  m_terrain_types = height_map.getTerrainTypes();
  m_biome_settings = biome_settings;
  m_noise_seed = biome_settings.seed;

  m_pine_state.reset_instances();

  const auto wind_profile = Game::Map::make_wind_profile(m_biome_settings);
  auto &pine_params = m_pine_state.params;
  pine_params.light_direction = QVector3D(0.35F, 0.8F, 0.45F);
  pine_params.time = 0.0F;
  pine_params.wind_strength = wind_profile.sway_strength;
  pine_params.wind_speed = wind_profile.sway_speed;

  generate_pine_instances();
}

void PineRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  (void)resources;

  const auto visible_count = Scatter::sync_filtered_state(
      m_pine_state, [](const PineInstanceGpu &instance) -> const QVector4D & {
        return instance.pos_scale;
      });
  if (visible_count == 0 || !m_pine_state.instance_buffer) {
    return;
  }

  PineBatchParams params = m_pine_state.params;
  params.time = renderer.get_animation_time();
  TerrainScatterCmd cmd;
  cmd.species = TerrainScatterCmd::Species::Pine;
  cmd.instance_buffer = m_pine_state.instance_buffer.get();
  cmd.instance_count = visible_count;
  cmd.pine = params;
  renderer.terrain_scatter(cmd);
}

void PineRenderer::clear() { m_pine_state.reset_instances(); }

void PineRenderer::generate_pine_instances() {
  auto &pine_instances = m_pine_state.instances;
  auto &pine_instance_count = m_pine_state.instance_count;
  auto &pine_instances_dirty = m_pine_state.instances_dirty;

  pine_instances.clear();

  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    return;
  }

  const auto scatter_profile =
      Game::Map::make_scatter_profile(m_biome_settings);
  const auto scatter_rules =
      Game::Map::make_scatter_rules(scatter_profile.ground_type);
  if (!scatter_rules.allow_pines) {
    pine_instances_dirty = false;
    return;
  }

  const float tile_safe = std::max(0.1F, m_tile_size);

  float pine_density = scatter_rules.pine_base_density;
  if (scatter_profile.plant_density > 0.0F) {
    pine_density =
        scatter_profile.plant_density * scatter_rules.pine_density_scale;
  }

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(m_height_data, m_terrain_types, m_width,
                                      m_height, m_tile_size);

  SpawnValidationConfig config = make_tree_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding =
      scatter_profile.spawn_edge_padding * k_tree_edge_padding_scale;

  SpawnValidator validator(terrain_cache, config);

  auto add_pine = [&](float gx, float gz, uint32_t &state) -> bool {
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

    float const scale = remap(rand_01(state), 3.0F, 6.0F) * tile_safe;

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_color(0.15F, 0.35F, 0.20F);
    QVector3D const var_color(0.20F, 0.40F, 0.25F);
    QVector3D tint_color =
        base_color * (1.0F - color_var) + var_color * color_var;

    float const brown_mix = remap(rand_01(state), 0.10F, 0.25F);
    QVector3D const brown_tint(0.35F, 0.30F, 0.20F);
    tint_color = tint_color * (1.0F - brown_mix) + brown_tint * brown_mix;

    float const sway_phase = rand_01(state) * MathConstants::k_two_pi;

    float const rotation = rand_01(state) * MathConstants::k_two_pi;

    float const silhouette_seed = rand_01(state);
    float const needle_seed = rand_01(state);
    float const bark_seed = rand_01(state);

    PineInstanceGpu instance;

    instance.pos_scale = QVector4D(world_x, world_y, world_z, scale);
    instance.color_sway =
        QVector4D(tint_color.x(), tint_color.y(), tint_color.z(), sway_phase);
    instance.rotation =
        QVector4D(rotation, silhouette_seed, needle_seed, bark_seed);
    pine_instances.push_back(instance);
    return true;
  };

  for (int z = 0; z < m_height; z += k_tree_cell_span) {
    for (int x = 0; x < m_width; x += k_tree_cell_span) {
      int const sample_x = std::min(x + k_tree_cell_span / 2, m_width - 1);
      int const sample_z = std::min(z + k_tree_cell_span / 2, m_height - 1);
      int const idx = sample_z * m_width + sample_x;

      float const slope = terrain_cache.get_slope_at(sample_x, sample_z);
      if (slope > 0.75F) {
        continue;
      }

      uint32_t state = hash_coords(
          x, z, m_noise_seed ^ 0xAB12CD34U ^ static_cast<uint32_t>(idx));

      Game::Map::TerrainType const terrain_type =
          terrain_cache.get_terrain_type_at(sample_x, sample_z);
      float density_mult = 1.0F;
      if (terrain_type == Game::Map::TerrainType::Hill) {
        density_mult = 1.2F;
      } else if (terrain_type == Game::Map::TerrainType::Mountain) {
        density_mult = 0.4F;
      }

      uint32_t cls_state =
          hash_coords(x / 8, z / 8, m_noise_seed ^ 0x4F2E9A7BU);
      float const macro_noise = rand_01(cls_state);
      uint32_t mid_state =
          hash_coords(x / 4, z / 4, m_noise_seed ^ 0xB3C71E4DU);
      float const mid_noise = rand_01(mid_state);
      float const cluster_noise = macro_noise * 0.65F + mid_noise * 0.35F;
      float const cluster_mult = cluster_noise * cluster_noise * 2.2F;

      float const effective_density = pine_density * density_mult * 0.8F *
                                      k_tree_density_area_scale * cluster_mult;
      int pine_count = static_cast<int>(std::floor(effective_density));
      float const frac = effective_density - float(pine_count);
      if (rand_01(state) < frac) {
        pine_count += 1;
      }

      for (int i = 0; i < pine_count; ++i) {
        float const gx = float(x) + rand_01(state) * float(k_tree_cell_span);
        float const gz = float(z) + rand_01(state) * float(k_tree_cell_span);
        add_pine(gx, gz, state);
      }
    }
  }

  pine_instance_count = pine_instances.size();
  pine_instances_dirty = pine_instance_count > 0;
}

} // namespace Render::GL
