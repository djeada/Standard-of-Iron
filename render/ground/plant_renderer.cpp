#include "plant_renderer.h"
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

} // namespace

namespace Render::GL {

PlantRenderer::PlantRenderer() = default;
PlantRenderer::~PlantRenderer() = default;

void PlantRenderer::configure(const Game::Map::TerrainHeightMap &height_map,
                              const Game::Map::BiomeSettings &biome_settings) {
  m_width = height_map.getWidth();
  m_height = height_map.getHeight();
  m_tile_size = height_map.getTileSize();
  m_height_data = height_map.getHeightData();
  m_terrain_types = height_map.getTerrainTypes();
  m_biome_settings = biome_settings;
  m_noiseSeed = biome_settings.seed;

  m_plant_state.reset_instances();

  const auto profiles = Game::Map::make_biome_profiles(m_biome_settings);
  const auto &wind_profile = profiles.wind;
  auto &plant_params = m_plant_state.params;
  plant_params.light_direction = QVector3D(0.35F, 0.8F, 0.45F);
  plant_params.time = 0.0F;
  plant_params.wind_strength = wind_profile.sway_strength;
  plant_params.wind_speed = wind_profile.sway_speed;

  generate_plant_instances();
}

void PlantRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  (void)resources;

  const auto visible_count = Scatter::sync_filtered_state(
      m_plant_state, [](const PlantInstanceGpu &instance) -> const QVector4D & {
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

void PlantRenderer::clear() { m_plant_state.reset_instances(); }

void PlantRenderer::generate_plant_instances() {
  auto &plant_instances = m_plant_state.instances;
  auto &plant_instance_count = m_plant_state.instance_count;
  auto &plant_instances_dirty = m_plant_state.instances_dirty;

  plant_instances.clear();

  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    plant_instance_count = 0;
    plant_instances_dirty = false;
    return;
  }

  const auto scatter_profile =
      Game::Map::make_scatter_profile(m_biome_settings);
  const float plant_density =
      std::clamp(scatter_profile.plant_density, 0.0F, 2.0F);

  if (plant_density < 0.01F) {
    plant_instance_count = 0;
    plant_instances_dirty = false;
    return;
  }

  const float tile_safe = std::max(0.001F, m_tile_size);

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(m_height_data, m_terrain_types, m_width,
                                      m_height, m_tile_size);

  SpawnValidationConfig config = make_plant_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding = scatter_profile.spawn_edge_padding;

  SpawnValidator validator(terrain_cache, config);

  auto add_plant = [&](float gx, float gz, uint32_t &state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    float const sgx = std::clamp(gx, 0.0F, float(m_width - 1));
    float const sgz = std::clamp(gz, 0.0F, float(m_height - 1));

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    float const world_y = terrain_cache.sample_height_at(sgx, sgz);

    float const scale = remap(rand_01(state), 0.30F, 0.80F) * tile_safe;

    float const plant_type = std::floor(rand_01(state) * 4.0F);

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_color = scatter_profile.grass_primary * 0.7F;
    QVector3D const var_color = scatter_profile.grass_secondary * 0.8F;
    QVector3D tint_color =
        base_color * (1.0F - color_var) + var_color * color_var;

    float const brown_mix = remap(rand_01(state), 0.15F, 0.35F);
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
    instance.type_params =
        QVector4D(plant_type, rotation, sway_strength, sway_speed);
    plant_instances.push_back(instance);
    return true;
  };

  for (int z = 0; z < m_height; z += 3) {
    for (int x = 0; x < m_width; x += 3) {
      int const idx = z * m_width + x;

      Game::Map::TerrainType const terrain_type =
          terrain_cache.get_terrain_type_at(x, z);
      if (terrain_type == Game::Map::TerrainType::Mountain ||
          terrain_type == Game::Map::TerrainType::River) {
        continue;
      }

      float const slope = terrain_cache.get_slope_at(x, z);
      if (slope > 0.65F) {
        continue;
      }

      uint32_t state = hash_coords(
          x, z, m_noiseSeed ^ 0x8F3C5A7EU ^ static_cast<uint32_t>(idx));

      float world_x = 0.0F;
      float world_z = 0.0F;
      validator.grid_to_world(static_cast<float>(x), static_cast<float>(z),
                              world_x, world_z);

      float const cluster_noise = value_noise(world_x * 0.05F, world_z * 0.05F,
                                              m_noiseSeed ^ 0x4B9D2F1AU);

      if (cluster_noise < 0.45F) {
        continue;
      }

      float density_mult = 1.0F;
      if (terrain_type == Game::Map::TerrainType::Hill) {
        density_mult = 0.6F;
      }

      float const effective_density = plant_density * density_mult * 0.8F;
      int plant_count = static_cast<int>(std::floor(effective_density));
      float const frac = effective_density - float(plant_count);
      if (rand_01(state) < frac) {
        plant_count += 1;
      }

      for (int i = 0; i < plant_count; ++i) {
        float const gx = float(x) + rand_01(state) * 3.0F;
        float const gz = float(z) + rand_01(state) * 3.0F;
        add_plant(gx, gz, state);
      }
    }
  }

  plant_instance_count = plant_instances.size();
  plant_instances_dirty = plant_instance_count > 0;
}

} // namespace Render::GL
