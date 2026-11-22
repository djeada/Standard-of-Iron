#include <QDebug>
#include <QVector2D>
#include <qglobal.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/scatter_composition.h"
#include "game/map/scatter/spawn_validator.h"
#include "map/biome_settings.h"
#include "map/terrain.h"
#include "map/terrain_service.h"
#include "render/frame/decoration_gpu.h"
#include "render/ground/ground_ecology.h"
#include "render/ground/scatter/scatter_generators.h"
#include "render/ground/scatter_runtime.h"
#include "render/ground/stone_ground_fit.h"
#include "render/math/render_constants.h"

namespace Render::Ground::Scatter {

namespace {

using Render::GL::StoneInstanceGpu;
using namespace Render::GL;

using std::uint32_t;
using namespace Render::Ground;
constexpr float k_reference_scatter_extent = 220.0F;
constexpr float k_pebble_scale_min = 0.10F;
constexpr float k_pebble_scale_max = 0.26F;

} // namespace

auto generate_stones(const ScatterRequest& request) -> std::vector<StoneInstanceGpu> {
  std::vector<StoneInstanceGpu> instances;
  if (request.grid.empty()) {
    return instances;
  }
  const int grid_width = request.grid.width;
  const int grid_height = request.grid.height;
  const float tile_size = request.grid.tile_size;
  const auto& height_data = *request.grid.heights;
  const auto& terrain_types = *request.grid.types;
  const auto& world_props = request.world_props;
  const std::uint32_t noise_seed = request.biome.seed;
  const GroundEcologyField* ecology_field = request.ecology;
  auto& stone_instances = instances;

  stone_instances.clear();

  if (grid_width < 2 || grid_height < 2 || height_data.empty()) {
    return instances;
  }

  const float tile_safe = std::max(0.001F, tile_size);
  const auto surface_profile = Game::Map::make_surface_profile(request.biome);
  const auto scatter_profile = Game::Map::make_scatter_profile(request.biome);

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(
      height_data, terrain_types, grid_width, grid_height, tile_size);

  SpawnValidationConfig config = make_stone_spawn_config();
  config.grid_width = grid_width;
  config.grid_height = grid_height;
  config.tile_size = tile_size;
  config.edge_padding = scatter_profile.spawn_edge_padding;
  config.max_slope = 0.30F;
  config.allow_hill = true;

  SpawnValidator validator(terrain_cache, config);
  ScatterCompositionContext composition(
      terrain_cache, grid_width, grid_height, tile_size, request.biome, world_props);

  static const GroundEcologyField k_no_ecology;
  const GroundEcologyField& ecology = ecology_field ? *ecology_field : k_no_ecology;

  auto add_stone = [&](float gx, float gz, uint32_t& state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    auto const scene = composition.sample_grid(gx, gz, 0x3AA5B08BU);
    if (scene.obstacle_influence >= 1.0F) {
      return false;
    }
    (void)rand_01(state);

    float const sgx = std::clamp(gx, 0.0F, float(grid_width - 1));
    float const sgz = std::clamp(gz, 0.0F, float(grid_height - 1));

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    float const world_y = terrain_cache.sample_height_at(sgx, sgz);

    float const scale = remap(rand_01(state), k_pebble_scale_min, k_pebble_scale_max) *
                        tile_safe *
                        scatter_scale_bias(ScatterRuleSpecies::Stone, scene);

    QVector3D const color = stone_instance_color(surface_profile.rock_low,
                                                 surface_profile.rock_high,
                                                 scene.dryness,
                                                 scene.rockiness,
                                                 state);

    float const rotation = rand_01(state) * MathConstants::k_two_pi;
    QVector3D const ground_normal =
        sample_ground_normal(height_data, grid_width, grid_height, tile_size, sgx, sgz);

    StoneInstanceGpu instance;
    instance.pos_scale = QVector4D(world_x, world_y + 0.01F, world_z, scale);
    instance.color_rot = QVector4D(color.x(), color.y(), color.z(), rotation);
    instance.ground_fit =
        pack_stone_ground_fit(ground_normal,
                              rand_01(state),
                              scale,
                              stone_sink_fraction(ground_normal, state));
    stone_instances.push_back(instance);
    return true;
  };

  const float stone_density = 0.08F;

  float const area_scale =
      std::sqrt(static_cast<float>(std::max(grid_width, 1) * std::max(grid_height, 1)) /
                (k_reference_scatter_extent * k_reference_scatter_extent));
  int const sampling_scale = std::max(1, static_cast<int>(std::round(area_scale)));
  int const cell_span = 4 * sampling_scale;
  float const ecology_norm = ecology_density_normaliser(
      ecology,
      terrain_cache,
      grid_width,
      grid_height,
      cell_span,
      [](const GroundEcologyField& field,
         const SpawnTerrainCache& cache,
         int x,
         int z) { return stone_ecology_density(field, cache, x, z); });
  for (int z = 0; z < grid_height; z += cell_span) {
    for (int x = 0; x < grid_width; x += cell_span) {
      int const idx = z * grid_width + x;

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
          hash_coords(x, z, noise_seed ^ 0xABCDEF12U ^ static_cast<uint32_t>(idx));
      int const sample_x = std::min(x + cell_span / 2, grid_width - 1);
      int const sample_z = std::min(z + cell_span / 2, grid_height - 1);
      auto const cell_scene = composition.sample_grid(
          static_cast<float>(sample_x), static_cast<float>(sample_z), 0x3AA5B08BU);
      float density_mult =
          scatter_density_multiplier(ScatterRuleSpecies::Stone, cell_scene);
      if (terrain_type == Game::Map::TerrainType::Hill) {
        density_mult *= 1.22F;
      }
      density_mult *=
          stone_ecology_density(ecology, terrain_cache, sample_x, sample_z) *
          ecology_norm;
      float const cluster_mult = scatter_patch_multiplier(cell_scene.cluster_bias);
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

        auto const leader_scene = composition.sample_grid(gx, gz, 0x3AA5B08BU);
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
  return instances;
}

} // namespace Render::Ground::Scatter
