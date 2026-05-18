#include "plant_renderer.h"

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

constexpr int k_plant_cell_span = 2;
constexpr float k_plant_density_area_scale = 4.0F / 9.0F;
constexpr float k_plant_edge_padding_scale = 0.35F;

} // namespace

namespace Render::GL {

PlantRenderer::PlantRenderer() = default;
PlantRenderer::~PlantRenderer() = default;

void PlantRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                              const Game::Map::BiomeSettings& biome_settings,
                              const std::vector<Game::Map::WorldProp>& world_props) {
  m_width = height_map.get_width();
  m_height = height_map.get_height();
  m_tile_size = height_map.get_tile_size();
  m_height_data = height_map.get_height_data();
  m_terrain_types = height_map.getTerrainTypes();
  m_world_props = world_props;
  m_biome_settings = biome_settings;
  m_noise_seed = biome_settings.seed;

  m_plant_state.reset_instances();

  const auto profiles = Game::Map::make_biome_profiles(m_biome_settings);
  const auto& wind_profile = profiles.wind;
  auto& plant_params = m_plant_state.params;
  plant_params.light_direction = m_light_direction;
  plant_params.time = 0.0F;
  plant_params.wind_strength = wind_profile.sway_strength;
  plant_params.wind_speed = wind_profile.sway_speed;

  generate_plant_instances();
}

void PlantRenderer::set_light_direction(const QVector3D& dir) {
  m_light_direction = dir.isNull() ? QVector3D(0.35F, 0.8F, 0.45F) : dir.normalized();
  m_plant_state.params.light_direction = m_light_direction;
}

void PlantRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  (void)resources;

  const auto visible_count = Scatter::sync_filtered_state(
      m_plant_state, [](const PlantInstanceGpu& instance) -> const QVector4D& {
        return instance.pos_scale;
      });
  if (visible_count == 0 || !m_plant_state.instance_buffer) {
    return;
  }

  PlantBatchParams params = m_plant_state.params;
  params.time = renderer.get_animation_time();
  TerrainScatterCmd cmd;
  cmd.species = TerrainScatterCmd::Species::Plant;
  cmd.instance_buffer = m_plant_state.instance_buffer.get();
  cmd.instance_count = visible_count;
  cmd.plant = params;
  renderer.terrain_scatter(cmd);
}

void PlantRenderer::clear() {
  m_plant_state.reset_instances();
}

void PlantRenderer::generate_plant_instances() {
  auto& plant_instances = m_plant_state.instances;
  auto& plant_instance_count = m_plant_state.instance_count;
  auto& plant_instances_dirty = m_plant_state.instances_dirty;

  plant_instances.clear();

  {
    const float half_w = static_cast<float>(m_width) * 0.5F;
    const float half_h = static_cast<float>(m_height) * 0.5F;
    auto& terrain_service = Game::Map::TerrainService::instance();
    for (const auto& prop : m_world_props) {
      if (prop.type != Game::Map::WorldProp::Type::Plant) {
        continue;
      }
      const float wx = (prop.x - half_w) * m_tile_size;
      const float wz = (prop.z - half_h) * m_tile_size;
      const QVector3D pos =
          terrain_service.resolve_surface_world_position(wx, wz, 0.0F, 0.0F);

      uint32_t var_state = hash_coords(static_cast<int>(std::round(prop.x)),
                                       static_cast<int>(std::round(prop.z)),
                                       m_noise_seed ^ 0xC2E84B6AU);
      const float color_var = rand_01(var_state);
      const QVector3D base_color(0.26F, 0.48F, 0.22F);
      const QVector3D var_color(0.32F, 0.54F, 0.28F);
      const QVector3D tint = base_color * (1.0F - color_var) + var_color * color_var;
      const float sway_phase = rand_01(var_state) * MathConstants::k_two_pi;
      const float plant_type = std::floor(rand_01(var_state) * 4.0F);

      PlantInstanceGpu inst;
      inst.pos_scale = QVector4D(pos.x(),
                                 pos.y() + 0.05F,
                                 pos.z(),
                                 prop.scale * Game::Map::world_prop_render_scale(
                                                  Game::Map::WorldProp::Type::Plant));
      inst.color_sway = QVector4D(tint.x(), tint.y(), tint.z(), sway_phase);
      inst.type_params = QVector4D(plant_type, prop.rotation, 1.0F, 1.0F);
      plant_instances.push_back(inst);
    }
  }

  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    plant_instance_count = plant_instances.size();
    plant_instances_dirty = plant_instance_count > 0;
    return;
  }

  const auto scatter_profile = Game::Map::make_scatter_profile(m_biome_settings);
  const float plant_density = std::clamp(scatter_profile.plant_density, 0.0F, 2.0F);

  if (plant_density < 0.01F) {
    plant_instance_count = plant_instances.size();
    plant_instances_dirty = plant_instance_count > 0;
    return;
  }

  const float tile_safe = std::max(0.001F, m_tile_size);

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(
      m_height_data, m_terrain_types, m_width, m_height, m_tile_size);

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding = scatter_profile.spawn_edge_padding * k_plant_edge_padding_scale;

  SpawnValidator validator(terrain_cache, config);
  ScatterCompositionContext composition(
      terrain_cache, m_width, m_height, m_tile_size, m_biome_settings, m_world_props);

  auto add_plant = [&](float gx, float gz, uint32_t& state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    auto const scene = composition.sample_grid(gx, gz, state ^ 0xA41F2CD1U);
    if (rand_01(state) > scatter_spawn_chance(ScatterRuleSpecies::Plant, scene)) {
      return false;
    }

    float const sgx = std::clamp(gx, 0.0F, float(m_width - 1));
    float const sgz = std::clamp(gz, 0.0F, float(m_height - 1));

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    float const world_y = terrain_cache.sample_height_at(sgx, sgz);

    float const scale = remap(rand_01(state), 0.30F, 0.80F) * tile_safe *
                        scatter_scale_bias(ScatterRuleSpecies::Plant, scene);

    float plant_type = 0.0F;
    if (scene.archetype == ScatterSceneArchetype::RiverFringe ||
        scene.archetype == ScatterSceneArchetype::FertilePatch) {
      plant_type = std::floor(rand_01(state) * 2.0F);
    } else if (scene.archetype == ScatterSceneArchetype::DryClearing ||
               scene.dryness > 0.65F) {
      plant_type = 2.0F + std::floor(rand_01(state) * 2.0F);
    } else {
      plant_type = std::floor(rand_01(state) * 4.0F);
    }

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_color =
        scatter_profile.grass_primary * (0.55F + scene.fertility * 0.25F);
    QVector3D const var_color =
        scatter_profile.grass_secondary * (0.55F + (1.0F - scene.dryness) * 0.25F);
    QVector3D tint_color = base_color * (1.0F - color_var) + var_color * color_var;
    tint_color =
        tint_color * (1.0F - scene.dryness) +
        scatter_profile.grass_dry * (0.70F + scene.dryness * 0.20F) * scene.dryness;

    float const brown_mix = remap(
        rand_01(state), 0.08F + scene.dryness * 0.10F, 0.22F + scene.rockiness * 0.12F);
    QVector3D const brown_tint(0.55F, 0.50F, 0.35F);
    tint_color = tint_color * (1.0F - brown_mix) + brown_tint * brown_mix;

    float const sway_phase = rand_01(state) * MathConstants::k_two_pi;
    float const sway_strength = remap(rand_01(state), 0.6F, 1.2F);
    float const sway_speed = remap(rand_01(state), 0.8F, 1.3F);

    float const rotation = rand_01(state) * MathConstants::k_two_pi;

    PlantInstanceGpu instance;

    instance.pos_scale = QVector4D(world_x, world_y + 0.05F, world_z, scale);
    instance.color_sway =
        QVector4D(tint_color.x(), tint_color.y(), tint_color.z(), sway_phase);
    instance.type_params = QVector4D(plant_type, rotation, sway_strength, sway_speed);
    plant_instances.push_back(instance);
    return true;
  };

  for (int z = 0; z < m_height; z += k_plant_cell_span) {
    for (int x = 0; x < m_width; x += k_plant_cell_span) {
      int const sample_x = std::min(x + k_plant_cell_span / 2, m_width - 1);
      int const sample_z = std::min(z + k_plant_cell_span / 2, m_height - 1);
      int const idx = sample_z * m_width + sample_x;

      Game::Map::TerrainType const terrain_type =
          terrain_cache.get_terrain_type_at(sample_x, sample_z);
      if (terrain_type == Game::Map::TerrainType::Mountain ||
          terrain_type == Game::Map::TerrainType::River) {
        continue;
      }

      float const slope = terrain_cache.get_slope_at(sample_x, sample_z);
      if (slope > 0.65F) {
        continue;
      }

      uint32_t state =
          hash_coords(x, z, m_noise_seed ^ 0x8F3C5A7EU ^ static_cast<uint32_t>(idx));
      auto const cell_scene = composition.sample_grid(static_cast<float>(sample_x),
                                                      static_cast<float>(sample_z),
                                                      state ^ 0x4BE531F0U);

      float density_mult = 1.0F;
      if (terrain_type == Game::Map::TerrainType::Hill) {
        density_mult = 0.6F;
      }
      density_mult *= scatter_density_multiplier(ScatterRuleSpecies::Plant, cell_scene);
      float const cluster_mult = 0.55F + cell_scene.cluster_bias * 1.10F;

      float const effective_density = plant_density * density_mult * cluster_mult *
                                      0.48F * k_plant_density_area_scale;
      if (effective_density < 0.05F) {
        continue;
      }
      (void)rand_01(state);
      int plant_count = static_cast<int>(std::floor(effective_density));
      float const frac = effective_density - float(plant_count);
      if (rand_01(state) < frac) {
        plant_count += 1;
      }

      for (int i = 0; i < plant_count; ++i) {
        float const gx = float(x) + rand_01(state) * float(k_plant_cell_span);
        float const gz = float(z) + rand_01(state) * float(k_plant_cell_span);
        if (!add_plant(gx, gz, state)) {
          continue;
        }

        auto const leader_scene = composition.sample_grid(gx, gz, state ^ 0x0B35E7D4U);
        int const satellite_count = scatter_cluster_satellite_count(
            ScatterRuleSpecies::Plant, leader_scene, state);
        for (int satellite = 0; satellite < satellite_count; ++satellite) {
          float const angle = rand_01(state) * MathConstants::k_two_pi;
          float const radius_tiles = scatter_cluster_radius_tiles(
              ScatterRuleSpecies::Plant, leader_scene, state);
          add_plant(gx + std::cos(angle) * radius_tiles,
                    gz + std::sin(angle) * radius_tiles,
                    state);
        }
      }
    }
  }

  plant_instance_count = plant_instances.size();
  plant_instances_dirty = plant_instance_count > 0;
}

} // namespace Render::GL
