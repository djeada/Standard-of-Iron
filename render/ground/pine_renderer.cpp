#include "pine_renderer.h"

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "../scene_renderer.h"
#include "decoration_gpu.h"
#include "gl/render_constants.h"
#include "gl/resources.h"
#include "ground_utils.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "scatter_composition.h"
#include "scatter_runtime.h"
#include "spawn_validator.h"

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

void PineRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                             const Game::Map::BiomeSettings& biome_settings,
                             const std::vector<Game::Map::WorldProp>& world_props,
                             bool use_world_props_exclusively) {
  m_width = height_map.get_width();
  m_height = height_map.get_height();
  m_tile_size = height_map.get_tile_size();
  m_height_data = height_map.get_height_data();
  m_terrain_types = height_map.getTerrainTypes();
  m_world_props = world_props;
  m_use_world_props_exclusively = use_world_props_exclusively;
  m_biome_settings = biome_settings;
  m_noise_seed = biome_settings.seed;

  m_pine_state.reset_instances();

  const auto wind_profile = Game::Map::make_wind_profile(m_biome_settings);
  auto& pine_params = m_pine_state.params;
  pine_params.light_direction = m_light_direction;
  pine_params.time = 0.0F;
  pine_params.wind_strength = wind_profile.sway_strength;
  pine_params.wind_speed = wind_profile.sway_speed;

  generate_pine_instances();
}

void PineRenderer::set_light_direction(const QVector3D& dir) {
  m_light_direction = dir.isNull() ? QVector3D(0.35F, 0.8F, 0.45F) : dir.normalized();
  m_pine_state.params.light_direction = m_light_direction;
}

void PineRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  (void)resources;

  const auto visible_count = Scatter::sync_filtered_state(
      m_pine_state, [](const PineInstanceGpu& instance) -> const QVector4D& {
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

void PineRenderer::clear() {
  m_pine_state.reset_instances();
}

void PineRenderer::generate_pine_instances() {
  auto& pine_instances = m_pine_state.instances;
  auto& pine_instance_count = m_pine_state.instance_count;
  auto& pine_instances_dirty = m_pine_state.instances_dirty;
  pine_instances.clear();

  {
    const float half_w = static_cast<float>(m_width) * 0.5F - 0.5F;
    const float half_h = static_cast<float>(m_height) * 0.5F - 0.5F;
    for (const auto& prop : m_world_props) {
      if (prop.type != Game::Map::WorldProp::Type::PineTree) {
        continue;
      }
      const float wx = (prop.x - half_w) * m_tile_size;
      const float wz = (prop.z - half_h) * m_tile_size;
      const QVector3D pos =
          Game::Map::TerrainService::instance().resolve_surface_world_position(
              wx, wz, 0.0F, 0.0F);

      uint32_t var_state = hash_coords(static_cast<int>(std::round(prop.x)),
                                       static_cast<int>(std::round(prop.z)),
                                       m_noise_seed ^ 0x4A7F2C9EU);
      const float color_var = rand_01(var_state);
      const QVector3D base_color(0.16F, 0.34F, 0.20F);
      const QVector3D var_color(0.24F, 0.44F, 0.28F);
      const QVector3D tint = base_color * (1.0F - color_var) + var_color * color_var;
      const float sway_phase = rand_01(var_state) * MathConstants::k_two_pi;
      const float silhouette_seed = rand_01(var_state);
      const float needle_seed = rand_01(var_state);
      const float bark_seed = rand_01(var_state);

      PineInstanceGpu inst;
      inst.pos_scale =
          QVector4D(pos.x(),
                    pos.y(),
                    pos.z(),
                    prop.scale * Game::Map::world_prop_render_scale(
                                     Game::Map::WorldProp::Type::PineTree));
      inst.color_sway = QVector4D(tint.x(), tint.y(), tint.z(), sway_phase);
      inst.rotation = QVector4D(prop.rotation, silhouette_seed, needle_seed, bark_seed);
      pine_instances.push_back(inst);
    }
  }

  if (m_use_world_props_exclusively) {
    pine_instance_count = pine_instances.size();
    pine_instances_dirty = pine_instance_count > 0;
    return;
  }

  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    pine_instance_count = pine_instances.size();
    pine_instances_dirty = pine_instance_count > 0;
    return;
  }

  const auto scatter_profile = Game::Map::make_scatter_profile(m_biome_settings);
  const auto scatter_rules = Game::Map::make_scatter_rules(scatter_profile.ground_type);
  if (!scatter_rules.allow_pines) {
    pine_instance_count = pine_instances.size();
    pine_instances_dirty = pine_instance_count > 0;
    return;
  }

  const float tile_safe = std::max(0.1F, m_tile_size);

  float pine_density = scatter_rules.pine_base_density;
  if (scatter_profile.plant_density > 0.0F) {
    pine_density = scatter_profile.plant_density * scatter_rules.pine_density_scale;
  }

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(
      m_height_data, m_terrain_types, m_width, m_height, m_tile_size);

  SpawnValidationConfig config = make_tree_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding = scatter_profile.spawn_edge_padding * k_tree_edge_padding_scale;

  SpawnValidator validator(terrain_cache, config);
  ScatterCompositionContext composition(
      terrain_cache, m_width, m_height, m_tile_size, m_biome_settings, m_world_props);

  auto add_pine = [&](float gx, float gz, uint32_t& state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    auto const scene = composition.sample_grid(gx, gz, state ^ 0x92C3B17FU);
    if (rand_01(state) > scatter_spawn_chance(ScatterRuleSpecies::Pine, scene)) {
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

    float const scale = remap(rand_01(state), 3.0F, 6.0F) * tile_safe *
                        scatter_scale_bias(ScatterRuleSpecies::Pine, scene);

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_color(0.16F + scene.shelter * 0.04F,
                               0.34F + scene.shelter * 0.10F,
                               0.20F + scene.shelter * 0.06F);
    QVector3D const var_color(0.24F + scene.cluster_bias * 0.04F,
                              0.44F + scene.shelter * 0.08F,
                              0.28F + scene.cluster_bias * 0.04F);
    QVector3D tint_color = base_color * (1.0F - color_var) + var_color * color_var;

    float const brown_mix = remap(
        rand_01(state), 0.03F + scene.dryness * 0.05F, 0.10F + scene.rockiness * 0.06F);
    QVector3D const brown_tint(0.32F, 0.29F, 0.19F);
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
    instance.rotation = QVector4D(rotation, silhouette_seed, needle_seed, bark_seed);
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

      uint32_t state =
          hash_coords(x, z, m_noise_seed ^ 0xAB12CD34U ^ static_cast<uint32_t>(idx));
      auto const cell_scene = composition.sample_grid(static_cast<float>(sample_x),
                                                      static_cast<float>(sample_z),
                                                      state ^ 0x5E2C4B81U);

      Game::Map::TerrainType const terrain_type =
          terrain_cache.get_terrain_type_at(sample_x, sample_z);
      float density_mult = 1.0F;
      if (terrain_type == Game::Map::TerrainType::Hill) {
        density_mult = 1.2F;
      } else if (terrain_type == Game::Map::TerrainType::Mountain) {
        density_mult = 0.4F;
      } else if (terrain_type == Game::Map::TerrainType::Forest) {
        density_mult = 2.25F;
      }

      uint32_t cls_state = hash_coords(x / 8, z / 8, m_noise_seed ^ 0x4F2E9A7BU);
      float const macro_noise = rand_01(cls_state);
      uint32_t mid_state = hash_coords(x / 4, z / 4, m_noise_seed ^ 0xB3C71E4DU);
      float const mid_noise = rand_01(mid_state);
      float const cluster_noise = macro_noise * 0.65F + mid_noise * 0.35F;
      float const cluster_mult = 0.45F + cluster_noise * cluster_noise * 1.75F;
      density_mult *= scatter_density_multiplier(ScatterRuleSpecies::Pine, cell_scene);

      float const effective_density = pine_density * density_mult *
                                      (0.70F + cell_scene.cluster_bias * 1.10F) *
                                      k_tree_density_area_scale * cluster_mult;
      if (effective_density < 0.04F) {
        continue;
      }
      if (rand_01(state) > scatter_spawn_chance(ScatterRuleSpecies::Pine, cell_scene)) {
        continue;
      }
      int pine_count = static_cast<int>(std::floor(effective_density));
      float const frac = effective_density - float(pine_count);
      if (rand_01(state) < frac) {
        pine_count += 1;
      }

      for (int i = 0; i < pine_count; ++i) {
        float const gx = float(x) + rand_01(state) * float(k_tree_cell_span);
        float const gz = float(z) + rand_01(state) * float(k_tree_cell_span);
        if (!add_pine(gx, gz, state)) {
          continue;
        }

        auto const leader_scene = composition.sample_grid(gx, gz, state ^ 0x07E84CD3U);
        int const satellite_count = scatter_cluster_satellite_count(
            ScatterRuleSpecies::Pine, leader_scene, state);
        for (int satellite = 0; satellite < satellite_count; ++satellite) {
          float const angle = rand_01(state) * MathConstants::k_two_pi;
          float const radius_tiles = scatter_cluster_radius_tiles(
              ScatterRuleSpecies::Pine, leader_scene, state);
          add_pine(gx + std::cos(angle) * radius_tiles,
                   gz + std::sin(angle) * radius_tiles,
                   state);
        }
      }
    }
  }

  pine_instance_count = pine_instances.size();
  pine_instances_dirty = pine_instance_count > 0;
}

} // namespace Render::GL
