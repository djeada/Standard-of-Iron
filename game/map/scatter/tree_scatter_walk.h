#pragma once

#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

#include "game/map/scatter/ground_utils.h"
#include "game/map/scatter/scatter_composition_context.h"
#include "game/map/scatter/scatter_rules.h"
#include "game/map/scatter/spawn_validator.h"
#include "game/map/terrain.h"

namespace Render::Ground {

inline constexpr int k_tree_cell_span = 4;
inline constexpr float k_tree_density_area_scale = 16.0F / 36.0F;
inline constexpr float k_tree_edge_padding_scale = 0.35F;

enum class SceneChannel : std::uint8_t {
  None = 0,
  Dryness,
  Fertility,
  Rockiness,
  Shelter,
  ClusterBias,
  RiverInfluence,
  RoadInfluence
};

[[nodiscard]] inline auto
scene_channel(SceneChannel channel, const ScatterCompositionSample& sample) -> float {
  switch (channel) {
  case SceneChannel::None:
    return 0.0F;
  case SceneChannel::Dryness:
    return sample.dryness;
  case SceneChannel::Fertility:
    return sample.fertility;
  case SceneChannel::Rockiness:
    return sample.rockiness;
  case SceneChannel::Shelter:
    return sample.shelter;
  case SceneChannel::ClusterBias:
    return sample.cluster_bias;
  case SceneChannel::RiverInfluence:
    return sample.river_influence;
  case SceneChannel::RoadInfluence:
    return sample.road_influence;
  }
  return 0.0F;
}

struct TreeTintTerm {
  QVector3D offset;
  std::array<SceneChannel, 3> channel{
      SceneChannel::None, SceneChannel::None, SceneChannel::None};
  QVector3D weight;

  [[nodiscard]] auto
  resolve(const ScatterCompositionSample& sample) const -> QVector3D {
    return {offset.x() + scene_channel(channel[0], sample) * weight.x(),
            offset.y() + scene_channel(channel[1], sample) * weight.y(),
            offset.z() + scene_channel(channel[2], sample) * weight.z()};
  }
};

struct TreeTintBlend {
  QVector3D tint;
  float low_offset = 0.0F;
  SceneChannel low_channel = SceneChannel::None;
  float low_weight = 0.0F;
  float high_offset = 0.0F;
  SceneChannel high_channel = SceneChannel::None;
  float high_weight = 0.0F;

  [[nodiscard]] auto low(const ScatterCompositionSample& sample) const -> float {
    return low_offset + scene_channel(low_channel, sample) * low_weight;
  }
  [[nodiscard]] auto high(const ScatterCompositionSample& sample) const -> float {
    return high_offset + scene_channel(high_channel, sample) * high_weight;
  }
};

enum class TreeRandomOrder : std::uint8_t {
  ScaleFirst,
  ScaleLast
};

struct TreeScatterProfile {
  Game::Map::TreeSpecies species = Game::Map::TreeSpecies::Pine;
  ScatterRuleSpecies rule_species = ScatterRuleSpecies::Pine;
  Game::Map::WorldProp::Type prop_type = Game::Map::WorldProp::Type::PineTree;
  TreeRandomOrder random_order = TreeRandomOrder::ScaleFirst;

  float cell_slope_limit = 0.75F;
  float spawn_max_slope = 0.0F;

  std::uint32_t cell_salt = 0U;
  std::uint32_t cell_scene_salt = 0U;
  std::uint32_t sample_salt = 0U;
  std::uint32_t leader_salt = 0U;
  std::uint32_t macro_salt = 0U;
  std::uint32_t mid_salt = 0U;
  std::uint32_t prop_salt = 0U;

  float hill_density = 1.0F;
  float mountain_density = 1.0F;
  float forest_density = 1.0F;

  float cluster_base = 0.45F;
  float cluster_gain = 1.75F;
  float cluster_bias_gain = 1.10F;

  TreeTintTerm base_tint;
  TreeTintTerm var_tint;
  TreeTintBlend blend;
  float value_min = 0.78F;
  float value_max = 1.24F;

  QVector3D prop_base_color;
  QVector3D prop_var_color;
  float prop_value_min = 0.80F;
  float prop_value_max = 1.22F;
};

struct TreeScatterSample {
  float world_x = 0.0F;
  float world_z = 0.0F;
  float grid_x = 0.0F;
  float grid_z = 0.0F;
  float scale = 1.0F;
  float rotation = 0.0F;
  QVector3D tint;
  float sway_phase = 0.0F;
  float silhouette_seed = 0.0F;
  float leaf_seed = 0.0F;
  float bark_seed = 0.0F;
};

struct TreeScatterWalkInput {
  const SpawnTerrainCache* terrain_cache = nullptr;
  const SpawnValidator* validator = nullptr;
  const ScatterCompositionContext* composition = nullptr;
  int width = 0;
  int height = 0;
  float tile_size = 1.0F;
  std::uint32_t noise_seed = 0U;
  float density = 0.0F;
  float scale_min = 1.0F;
  float scale_max = 2.0F;
};

[[nodiscard]] inline auto
make_tree_scatter_spawn_config(const TreeScatterProfile& profile,
                               const TreeScatterWalkInput& input,
                               float spawn_edge_padding) -> SpawnValidationConfig {
  SpawnValidationConfig config = make_tree_spawn_config();
  config.grid_width = input.width;
  config.grid_height = input.height;
  config.tile_size = input.tile_size;
  config.edge_padding = spawn_edge_padding * k_tree_edge_padding_scale;
  if (profile.spawn_max_slope > 0.0F) {
    config.max_slope = profile.spawn_max_slope;
  }
  return config;
}

[[nodiscard]] inline auto
tree_scatter_density(const Game::Map::TerrainScatterRules& rules,
                     const Game::Map::TerrainScatterProfile& scatter_profile,
                     Game::Map::TreeSpecies species) -> float {
  const auto& rule = rules.tree(species);
  if (scatter_profile.plant_density > 0.0F) {
    return scatter_profile.plant_density * rule.density_scale;
  }
  return rule.base_density;
}

template <typename Emit>
void walk_tree_scatter(const TreeScatterProfile& profile,
                       const TreeScatterWalkInput& input,
                       Emit&& on_tree) {
  if (input.terrain_cache == nullptr || input.validator == nullptr ||
      input.composition == nullptr || input.width < 2 || input.height < 2) {
    return;
  }

  const SpawnTerrainCache& terrain_cache = *input.terrain_cache;
  const SpawnValidator& validator = *input.validator;
  const ScatterCompositionContext& composition = *input.composition;
  const float tile_safe = std::max(0.1F, input.tile_size);

  auto add_tree = [&](float gx, float gz, std::uint32_t& state) -> bool {
    if (!validator.can_spawn_at_grid(gx, gz)) {
      return false;
    }

    auto const scene = composition.sample_grid(gx, gz, state ^ profile.sample_salt);
    if (scene.obstacle_influence >= 1.0F) {
      return false;
    }
    if (rand_01(state) > scatter_spawn_chance(profile.rule_species, scene)) {
      return false;
    }

    TreeScatterSample sample;
    sample.grid_x = gx;
    sample.grid_z = gz;
    validator.grid_to_world(gx, gz, sample.world_x, sample.world_z);

    auto draw_scale = [&] {
      sample.scale = remap(rand_01(state), input.scale_min, input.scale_max) *
                     tile_safe * scatter_scale_bias(profile.rule_species, scene);
    };

    if (profile.random_order == TreeRandomOrder::ScaleFirst) {
      draw_scale();
    }

    float const color_var = remap(rand_01(state), 0.0F, 1.0F);
    QVector3D const base_color = profile.base_tint.resolve(scene);
    QVector3D const var_color = profile.var_tint.resolve(scene);
    QVector3D tint = base_color * (1.0F - color_var) + var_color * color_var;

    float const blend_mix =
        remap(rand_01(state), profile.blend.low(scene), profile.blend.high(scene));
    tint = tint * (1.0F - blend_mix) + profile.blend.tint * blend_mix;
    tint *= remap(rand_01(state), profile.value_min, profile.value_max);
    sample.tint = tint;

    sample.sway_phase = rand_01(state) * MathConstants::k_two_pi;
    sample.rotation = rand_01(state) * MathConstants::k_two_pi;
    sample.silhouette_seed = rand_01(state);
    sample.leaf_seed = rand_01(state);
    sample.bark_seed = rand_01(state);

    if (profile.random_order == TreeRandomOrder::ScaleLast) {
      draw_scale();
    }

    return on_tree(sample, scene);
  };

  for (int z = 0; z < input.height; z += k_tree_cell_span) {
    for (int x = 0; x < input.width; x += k_tree_cell_span) {
      int const sample_x = std::min(x + k_tree_cell_span / 2, input.width - 1);
      int const sample_z = std::min(z + k_tree_cell_span / 2, input.height - 1);
      int const idx = sample_z * input.width + sample_x;

      if (terrain_cache.get_slope_at(sample_x, sample_z) > profile.cell_slope_limit) {
        continue;
      }

      std::uint32_t state = hash_coords(
          x, z, input.noise_seed ^ profile.cell_salt ^ static_cast<std::uint32_t>(idx));
      auto const cell_scene = composition.sample_grid(static_cast<float>(sample_x),
                                                      static_cast<float>(sample_z),
                                                      state ^ profile.cell_scene_salt);

      float density_mult = 1.0F;
      switch (terrain_cache.get_terrain_type_at(sample_x, sample_z)) {
      case Game::Map::TerrainType::Hill:
        density_mult = profile.hill_density;
        break;
      case Game::Map::TerrainType::Mountain:
        density_mult = profile.mountain_density;
        break;
      case Game::Map::TerrainType::Forest:
        density_mult = profile.forest_density;
        break;
      default:
        break;
      }

      std::uint32_t cls_state =
          hash_coords(x / 8, z / 8, input.noise_seed ^ profile.macro_salt);
      float const macro_noise = rand_01(cls_state);
      std::uint32_t mid_state =
          hash_coords(x / 4, z / 4, input.noise_seed ^ profile.mid_salt);
      float const mid_noise = rand_01(mid_state);
      float const cluster_noise = macro_noise * 0.65F + mid_noise * 0.35F;
      float const cluster_mult =
          profile.cluster_base + cluster_noise * cluster_noise * profile.cluster_gain;
      density_mult *= scatter_density_multiplier(profile.rule_species, cell_scene);

      float const effective_density =
          input.density * density_mult *
          (0.70F + cell_scene.cluster_bias * profile.cluster_bias_gain) *
          k_tree_density_area_scale * cluster_mult;
      if (effective_density < 0.04F) {
        continue;
      }
      if (rand_01(state) > scatter_spawn_chance(profile.rule_species, cell_scene)) {
        continue;
      }

      int tree_count = static_cast<int>(std::floor(effective_density));
      float const frac = effective_density - static_cast<float>(tree_count);
      if (rand_01(state) < frac) {
        tree_count += 1;
      }

      for (int i = 0; i < tree_count; ++i) {
        float const gx = static_cast<float>(x) +
                         rand_01(state) * static_cast<float>(k_tree_cell_span);
        float const gz = static_cast<float>(z) +
                         rand_01(state) * static_cast<float>(k_tree_cell_span);
        if (!add_tree(gx, gz, state)) {
          continue;
        }

        auto const leader_scene =
            composition.sample_grid(gx, gz, state ^ profile.leader_salt);
        int const satellite_count =
            scatter_cluster_satellite_count(profile.rule_species, leader_scene, state);
        for (int satellite = 0; satellite < satellite_count; ++satellite) {
          float const angle = rand_01(state) * MathConstants::k_two_pi;
          float const radius_tiles =
              scatter_cluster_radius_tiles(profile.rule_species, leader_scene, state);
          add_tree(gx + std::cos(angle) * radius_tiles,
                   gz + std::sin(angle) * radius_tiles,
                   state);
        }
      }
    }
  }
}

[[nodiscard]] inline auto
tree_world_prop_look(const TreeScatterProfile& profile,
                     std::uint32_t& state) -> TreeScatterSample {
  TreeScatterSample sample;
  float const color_var = rand_01(state);
  sample.tint =
      profile.prop_base_color * (1.0F - color_var) + profile.prop_var_color * color_var;
  sample.tint *= remap(rand_01(state), profile.prop_value_min, profile.prop_value_max);
  sample.sway_phase = rand_01(state) * MathConstants::k_two_pi;
  sample.silhouette_seed = rand_01(state);
  sample.leaf_seed = rand_01(state);
  sample.bark_seed = rand_01(state);
  return sample;
}

[[nodiscard]] auto
tree_scatter_profile(Game::Map::TreeSpecies species) -> const TreeScatterProfile&;

} // namespace Render::Ground
