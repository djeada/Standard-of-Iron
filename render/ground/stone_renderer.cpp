#include "stone_renderer.h"

#include <QDebug>
#include <QVector2D>
#include <qglobal.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "../gl/buffer.h"
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
constexpr float k_reference_scatter_extent = 220.0F;

} // namespace

namespace Render::GL {

StoneRenderer::StoneRenderer() = default;
StoneRenderer::~StoneRenderer() = default;

void StoneRenderer::configure(const Game::Map::TerrainHeightMap& height_map,
                              const Game::Map::BiomeSettings& biome_settings,
                              const std::vector<Game::Map::WorldProp>& world_props) {
  configure_height_scatter_common(height_map, biome_settings, {}, world_props, false);
  auto& stone_params = m_state.params;

  stone_params.light_direction = m_light_direction;
  stone_params.time = 0.0F;

  generate_stone_instances();
}

void StoneRenderer::set_light_direction(const QVector3D& dir) {
  set_light_direction_common(dir, QVector3D(0.35F, 0.8F, 0.45F));
}

void StoneRenderer::submit(Renderer& renderer, ResourceManager* resources) {
  submit_filtered_common<false>(
      renderer,
      resources,
      TerrainScatterCmd::Species::Stone,
      [](TerrainScatterCmd& cmd, const StoneBatchParams& params) {
        cmd.stone = params;
      });
}

void StoneRenderer::clear() {
  clear_common();
}

void StoneRenderer::generate_stone_instances() {
  auto& stone_instances = m_state.instances;
  auto& stone_instance_count = m_state.instance_count;
  auto& stone_instances_dirty = m_state.instances_dirty;

  stone_instances.clear();

  if (m_width < 2 || m_height < 2 || m_height_data.empty()) {
    stone_instance_count = 0;
    stone_instances_dirty = false;
    return;
  }

  const float tile_safe = std::max(0.001F, m_tile_size);
  const auto surface_profile = Game::Map::make_surface_profile(m_biome_settings);
  const auto scatter_profile = Game::Map::make_scatter_profile(m_biome_settings);

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(
      m_height_data, m_terrain_types, m_width, m_height, m_tile_size);

  SpawnValidationConfig config = make_stone_spawn_config();
  config.grid_width = m_width;
  config.grid_height = m_height;
  config.tile_size = m_tile_size;
  config.edge_padding = scatter_profile.spawn_edge_padding;
  config.max_slope = 0.30F;
  config.allow_hill = true;

  SpawnValidator validator(terrain_cache, config);
  ScatterCompositionContext composition(
      terrain_cache, m_width, m_height, m_tile_size, m_biome_settings, m_world_props);

  auto add_stone = [&](float gx, float gz, uint32_t& state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    auto const scene = composition.sample_grid(gx, gz, state ^ 0x5D17F2A1U);
    if (scene.obstacle_influence >= 1.0F) {
      return false;
    }
    (void)rand_01(state);

    float const sgx = std::clamp(gx, 0.0F, float(m_width - 1));
    float const sgz = std::clamp(gz, 0.0F, float(m_height - 1));

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    float const world_y = terrain_cache.sample_height_at(sgx, sgz);

    float const scale = remap(rand_01(state), 0.35F, 0.90F) * tile_safe *
                        scatter_scale_bias(ScatterRuleSpecies::Stone, scene);

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_rock = surface_profile.rock_low;
    QVector3D const high_rock = surface_profile.rock_high;
    QVector3D color = base_rock * (1.0F - color_var) + high_rock * color_var;

    float const earth_mix = remap(
        rand_01(state), 0.08F + scene.dryness * 0.08F, 0.28F + scene.rockiness * 0.12F);
    QVector3D const earth_tint(0.34F, 0.31F, 0.27F);
    color = color * (1.0F - earth_mix) + earth_tint * earth_mix;
    color *= 0.84F + scene.rockiness * 0.08F;

    float const rotation = rand_01(state) * MathConstants::k_two_pi;

    StoneInstanceGpu instance;
    instance.pos_scale = QVector4D(world_x, world_y + 0.01F, world_z, scale);
    instance.color_rot = QVector4D(color.x(), color.y(), color.z(), rotation);
    stone_instances.push_back(instance);
    return true;
  };

  const float stone_density = 0.22F;

  float const area_scale = std::sqrt(
      static_cast<float>(std::max(m_width, 1) * std::max(m_height, 1)) /
      (k_reference_scatter_extent * k_reference_scatter_extent));
  int const sampling_scale = std::max(1, static_cast<int>(std::round(area_scale)));
  int const cell_span = 4 * sampling_scale;
  for (int z = 0; z < m_height; z += cell_span) {
    for (int x = 0; x < m_width; x += cell_span) {
      int const idx = z * m_width + x;

      Game::Map::TerrainType const terrain_type =
          terrain_cache.get_terrain_type_at(x, z);
      if (terrain_type == Game::Map::TerrainType::Mountain ||
          Game::Map::is_water_terrain(terrain_type)) {
        continue;
      }

      float const slope = terrain_cache.get_slope_at(x, z);
      if (slope > 0.30F) {
        continue;
      }

      uint32_t state =
          hash_coords(x, z, m_noise_seed ^ 0xABCDEF12U ^ static_cast<uint32_t>(idx));
      int const sample_x = std::min(x + cell_span / 2, m_width - 1);
      int const sample_z = std::min(z + cell_span / 2, m_height - 1);
      auto const cell_scene = composition.sample_grid(static_cast<float>(sample_x),
                                                      static_cast<float>(sample_z),
                                                      state ^ 0x3AA5B08BU);
      float density_mult =
          scatter_density_multiplier(ScatterRuleSpecies::Stone, cell_scene);
      if (terrain_type == Game::Map::TerrainType::Hill) {
        density_mult *= 1.22F;
      }
      float const cluster_mult = 0.55F + cell_scene.cluster_bias * 1.15F;
      float const effective_density =
          stone_density * density_mult * cluster_mult * 1.20F;
      if (effective_density < 0.05F) {
        continue;
      }
      (void)rand_01(state);

      int stone_count = static_cast<int>(std::floor(effective_density));
      float const frac = effective_density - float(stone_count);
      if (rand_01(state) < frac) {
        stone_count += 1;
      }

      for (int i = 0; i < stone_count; ++i) {
        float const gx = float(x) + rand_01(state) * float(cell_span);
        float const gz = float(z) + rand_01(state) * float(cell_span);
        if (!add_stone(gx, gz, state)) {
          continue;
        }

        auto const leader_scene = composition.sample_grid(gx, gz, state ^ 0x01A53C2FU);
        int const satellite_count = scatter_cluster_satellite_count(
            ScatterRuleSpecies::Stone, leader_scene, state);
        for (int satellite = 0; satellite < satellite_count; ++satellite) {
          float const angle = rand_01(state) * MathConstants::k_two_pi;
          float const radius_tiles = scatter_cluster_radius_tiles(
              ScatterRuleSpecies::Stone, leader_scene, state);
          float const satellite_gx = gx + std::cos(angle) * radius_tiles;
          float const satellite_gz = gz + std::sin(angle) * radius_tiles;
          add_stone(satellite_gx, satellite_gz, state);
        }
      }
    }
  }

  stone_instance_count = stone_instances.size();
  stone_instances_dirty = stone_instance_count > 0;
}

} // namespace Render::GL
