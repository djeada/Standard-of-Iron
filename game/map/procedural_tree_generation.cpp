#include "procedural_tree_generation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "../../game/map/scatter/scatter_composition.h"
#include "../../game/map/scatter/spawn_validator.h"
#include "../../game/map/scatter/tree_scatter_walk.h"
#include "terrain.h"

namespace Game::Map {

namespace {

using namespace Render::Ground;
using std::uint32_t;

constexpr int k_boulder_cell_span = 6;
constexpr int k_iron_ore_cell_span = 18;
constexpr int k_min_runtime_tree_map_size = 32;
constexpr float k_min_tile_size = 0.0001F;

auto authored_world_to_grid(float world_coord,
                            int grid_size,
                            float tile_size) -> float {
  float const safe_tile_size = std::max(tile_size, k_min_tile_size);
  return world_coord / safe_tile_size + (static_cast<float>(grid_size) * 0.5F - 0.5F);
}

auto generated_prop_scale(WorldProp::Type type, float rendered_scale) -> float {
  float const base_scale = world_prop_render_scale(type);
  if (base_scale <= k_min_tile_size) {
    return rendered_scale;
  }
  return rendered_scale / base_scale;
}

void append_generated_world_prop(std::vector<WorldProp>& out,
                                 WorldProp::Type type,
                                 const TerrainHeightMap& height_map,
                                 CoordSystem coord_system,
                                 float world_x,
                                 float world_z,
                                 float rendered_scale,
                                 float rotation) {
  WorldProp prop;
  prop.type = type;
  prop.scale = generated_prop_scale(type, rendered_scale);
  prop.rotation = rotation;

  if (coord_system == CoordSystem::World) {
    prop.x = world_x;
    prop.z = world_z;
  } else {
    prop.x = authored_world_to_grid(
        world_x, height_map.get_width(), height_map.get_tile_size());
    prop.z = authored_world_to_grid(
        world_z, height_map.get_height(), height_map.get_tile_size());
  }

  out.push_back(prop);
}

void append_generated_trees(std::vector<WorldProp>& out,
                            const TerrainHeightMap& height_map,
                            const BiomeSettings& biome_settings,
                            CoordSystem coord_system,
                            const std::vector<WorldProp>& anchor_world_props,
                            TreeSpecies species) {
  const auto scatter_profile = make_scatter_profile(biome_settings);
  const auto scatter_rules = make_scatter_rules(scatter_profile.ground_type);
  const auto& rule = scatter_rules.tree(species);
  if (!rule.allowed) {
    return;
  }

  const int width = height_map.get_width();
  const int height = height_map.get_height();
  const float tile_size = height_map.get_tile_size();

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(height_map.get_height_data(),
                                      height_map.getTerrainTypes(),
                                      width,
                                      height,
                                      tile_size);

  const auto& profile = tree_scatter_profile(species);

  TreeScatterWalkInput input;
  input.terrain_cache = &terrain_cache;
  input.width = width;
  input.height = height;
  input.tile_size = tile_size;
  input.noise_seed = biome_settings.seed;
  input.density = tree_scatter_density(scatter_rules, scatter_profile, species);
  input.scale_min = rule.scale_min;
  input.scale_max = rule.scale_max;

  const SpawnValidationConfig config = make_tree_scatter_spawn_config(
      profile, input, scatter_profile.spawn_edge_padding);
  SpawnValidator validator(terrain_cache, config);
  ScatterCompositionContext composition(
      terrain_cache, width, height, tile_size, biome_settings, anchor_world_props);
  input.validator = &validator;
  input.composition = &composition;

  walk_tree_scatter(
      profile,
      input,
      [&](const TreeScatterSample& sample, const ScatterCompositionSample&) {
        append_generated_world_prop(out,
                                    profile.prop_type,
                                    height_map,
                                    coord_system,
                                    sample.world_x,
                                    sample.world_z,
                                    sample.scale,
                                    sample.rotation);
        return true;
      });
}

void append_generated_boulders(std::vector<WorldProp>& out,
                               const TerrainHeightMap& height_map,
                               const BiomeSettings& biome_settings,
                               CoordSystem coord_system,
                               const std::vector<WorldProp>& anchor_world_props) {
  const int width = height_map.get_width();
  const int height = height_map.get_height();
  const float tile_size = height_map.get_tile_size();

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(height_map.get_height_data(),
                                      height_map.getTerrainTypes(),
                                      width,
                                      height,
                                      tile_size);

  SpawnValidationConfig config = make_stone_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = biome_settings.spawn_edge_padding * 0.45F;
  config.max_slope = 0.38F;
  config.allow_hill = true;
  config.road_clearance = 1.8F;
  config.river_clearance = 1.4F;

  SpawnValidator validator(terrain_cache, config);
  ScatterCompositionContext composition(
      terrain_cache, width, height, tile_size, biome_settings, anchor_world_props);

  auto add_boulder = [&](float gx,
                         float gz,
                         float scale_min,
                         float scale_max,
                         uint32_t& state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    auto const scene = composition.sample_grid(gx, gz, state ^ 0xE148D9A7U);
    if (scene.obstacle_influence >= 1.0F) {
      return false;
    }
    float const support = std::max(
        scene.rockiness, std::max(scene.prop_influence * 0.90F, scene.dryness * 0.42F));
    if (support < 0.16F && scene.archetype != ScatterSceneArchetype::RockyPatch &&
        scene.archetype != ScatterSceneArchetype::DryClearing &&
        scene.prop_influence < 0.28F) {
      return false;
    }
    (void)rand_01(state);

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    (void)remap(rand_01(state), 0.0F, 1.0F);
    (void)remap(rand_01(state), 0.08F, 0.28F + scene.dryness * 0.12F);
    float const scale = remap(rand_01(state), scale_min, scale_max) *
                        scatter_scale_bias(ScatterRuleSpecies::Stone, scene);
    float const rotation = rand_01(state) * MathConstants::k_two_pi;

    append_generated_world_prop(out,
                                WorldProp::Type::Boulder,
                                height_map,
                                coord_system,
                                world_x,
                                world_z,
                                scale,
                                rotation);
    return true;
  };

  float const base_density = std::clamp(0.055F + biome_settings.rock_exposure * 0.080F -
                                            biome_settings.moisture_level * 0.015F,
                                        0.035F,
                                        0.125F);
  for (int z = 0; z < height; z += k_boulder_cell_span) {
    for (int x = 0; x < width; x += k_boulder_cell_span) {
      int const sample_x = std::min(x + k_boulder_cell_span / 2, width - 1);
      int const sample_z = std::min(z + k_boulder_cell_span / 2, height - 1);
      uint32_t state = hash_coords(x, z, biome_settings.seed ^ 0x6D1A75B3U);
      auto const scene = composition.sample_grid(static_cast<float>(sample_x),
                                                 static_cast<float>(sample_z),
                                                 state ^ 0xA28C52EFU);
      float const density =
          base_density * scatter_density_multiplier(ScatterRuleSpecies::Stone, scene) *
          (0.72F + scene.cluster_bias * 1.40F);
      if (rand_01(state) > density) {
        continue;
      }
      float const gx =
          static_cast<float>(x) + rand_01(state) * float(k_boulder_cell_span);
      float const gz =
          static_cast<float>(z) + rand_01(state) * float(k_boulder_cell_span);
      add_boulder(gx, gz, 0.90F, 1.85F, state);
    }
  }
}

void append_generated_iron_ore(std::vector<WorldProp>& out,
                               const TerrainHeightMap& height_map,
                               const BiomeSettings& biome_settings,
                               CoordSystem coord_system,
                               const std::vector<WorldProp>& anchor_world_props) {
  const int width = height_map.get_width();
  const int height = height_map.get_height();
  const float tile_size = height_map.get_tile_size();

  SpawnTerrainCache terrain_cache;
  terrain_cache.build_from_height_map(height_map.get_height_data(),
                                      height_map.getTerrainTypes(),
                                      width,
                                      height,
                                      tile_size);

  SpawnValidationConfig config = make_stone_spawn_config();
  config.grid_width = width;
  config.grid_height = height;
  config.tile_size = tile_size;
  config.edge_padding = biome_settings.spawn_edge_padding * 1.15F;
  config.max_slope = 0.20F;
  config.allow_flat = true;
  config.allow_hill = true;
  config.allow_mountain = false;
  config.road_clearance = 3.0F;
  config.river_clearance = 3.0F;

  SpawnValidator validator(terrain_cache, config);
  ScatterCompositionContext composition(
      terrain_cache, width, height, tile_size, biome_settings, anchor_world_props);

  auto add_ore = [&](float gx,
                     float gz,
                     float scale_min,
                     float scale_max,
                     uint32_t& state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    auto const scene = composition.sample_grid(gx, gz, state ^ 0xD64E21B7U);
    if (scene.obstacle_influence >= 1.0F) {
      return false;
    }
    float const flat_support = 1.0F - std::clamp(scene.slope / 0.20F, 0.0F, 1.0F);
    float const deposit_support =
        std::max(scene.rockiness * 0.72F,
                 std::max(scene.prop_influence * 0.55F, flat_support * 0.34F));
    if (deposit_support < 0.22F) {
      return false;
    }

    float world_x = 0.0F;
    float world_z = 0.0F;
    validator.grid_to_world(gx, gz, world_x, world_z);
    float const scale = remap(rand_01(state), scale_min, scale_max) *
                        remap(deposit_support, 0.92F, 1.18F);
    float const rotation = rand_01(state) * MathConstants::k_two_pi;

    append_generated_world_prop(out,
                                WorldProp::Type::IronOre,
                                height_map,
                                coord_system,
                                world_x,
                                world_z,
                                scale,
                                rotation);
    return true;
  };

  float const climate_bias =
      biome_settings.rock_exposure * 0.014F +
      std::max(0.0F, 0.42F - biome_settings.moisture_level) * 0.010F;
  float const base_density = std::clamp(0.010F + climate_bias, 0.006F, 0.030F);

  for (int z = 0; z < height; z += k_iron_ore_cell_span) {
    for (int x = 0; x < width; x += k_iron_ore_cell_span) {
      int const sample_x = std::min(x + k_iron_ore_cell_span / 2, width - 1);
      int const sample_z = std::min(z + k_iron_ore_cell_span / 2, height - 1);
      uint32_t state = hash_coords(x, z, biome_settings.seed ^ 0x92E7B54CU);
      auto const scene = composition.sample_grid(static_cast<float>(sample_x),
                                                 static_cast<float>(sample_z),
                                                 state ^ 0x3C18A6D1U);
      float const flat_support = 1.0F - std::clamp(scene.slope / 0.20F, 0.0F, 1.0F);
      float const density = base_density *
                            (0.75F + scene.rockiness * 2.15F +
                             scene.prop_influence * 1.20F + flat_support * 0.80F) *
                            (0.70F + scene.cluster_bias * 0.85F);
      if (rand_01(state) > density) {
        continue;
      }

      float const gx =
          static_cast<float>(x) + rand_01(state) * float(k_iron_ore_cell_span);
      float const gz =
          static_cast<float>(z) + rand_01(state) * float(k_iron_ore_cell_span);
      if (!add_ore(gx, gz, 1.25F, 2.10F, state)) {
        continue;
      }

      if (rand_01(state) < 0.18F) {
        float const angle = rand_01(state) * MathConstants::k_two_pi;
        float const radius_tiles = remap(rand_01(state), 1.8F, 3.6F);
        (void)add_ore(gx + std::cos(angle) * radius_tiles,
                      gz + std::sin(angle) * radius_tiles,
                      0.72F,
                      1.18F,
                      state);
      }
    }
  }
}

} // namespace

auto generate_procedural_world_props(const TerrainHeightMap& height_map,
                                     const BiomeSettings& biome_settings,
                                     CoordSystem coord_system,
                                     const std::vector<WorldProp>& anchor_world_props)
    -> std::vector<WorldProp> {
  if (height_map.get_width() < k_min_runtime_tree_map_size ||
      height_map.get_height() < k_min_runtime_tree_map_size) {
    return {};
  }

  std::vector<WorldProp> generated;
  generated.reserve(
      static_cast<size_t>(height_map.get_width() * height_map.get_height() / 8));
  if (biome_settings.procedural_boulders_enabled) {
    append_generated_boulders(
        generated, height_map, biome_settings, coord_system, anchor_world_props);
  }
  if (biome_settings.procedural_iron_ore_enabled) {
    append_generated_iron_ore(
        generated, height_map, biome_settings, coord_system, anchor_world_props);
  }
  if (biome_settings.procedural_trees_enabled) {
    for (std::size_t i = 0; i < k_tree_species_count; ++i) {
      append_generated_trees(generated,
                             height_map,
                             biome_settings,
                             coord_system,
                             anchor_world_props,
                             static_cast<TreeSpecies>(i));
    }
  }
  return generated;
}

} // namespace Game::Map
