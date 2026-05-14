#pragma once

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>
#include <vector>

#include "../../game/map/map_definition.h"
#include "../../game/map/terrain_service.h"
#include "ground_utils.h"
#include "spawn_validator.h"

namespace Render::Ground {

enum class ScatterSceneArchetype {
  OpenField,
  FertilePatch,
  DryClearing,
  RockyPatch,
  RiverFringe,
  CampOutskirts,
  ShelteredGrove,
};

enum class ScatterRuleSpecies {
  Stone,
  Plant,
  Pine,
  Olive,
  FireCamp,
  Tent,
  SupplyCart,
  WeaponRack,
  DeadTree,
};

enum class CampPropRole {
  Tent,
  SupplyCart,
  WeaponRack,
  DeadTree
};

struct ScatterCompositionSample {
  Game::Map::TerrainType terrain_type = Game::Map::TerrainType::Flat;
  ScatterSceneArchetype archetype = ScatterSceneArchetype::OpenField;
  float slope = 0.0F;
  float dryness = 0.0F;
  float fertility = 0.0F;
  float rockiness = 0.0F;
  float shelter = 0.0F;
  float camp_influence = 0.0F;
  float river_influence = 0.0F;
  float road_influence = 0.0F;
  float prop_influence = 0.0F;
  float cluster_bias = 0.0F;
};

namespace Detail {

struct PointAnchor {
  QVector2D position{0.0F, 0.0F};
  float radius = 1.0F;
};

struct LinearAnchor {
  QVector2D start{0.0F, 0.0F};
  QVector2D end{0.0F, 0.0F};
  float width = 1.0F;
};

inline auto distance_to_segment(const QVector2D& point,
                                const LinearAnchor& anchor) -> float {
  QVector2D const segment = anchor.end - anchor.start;
  float const length_sq = QVector2D::dotProduct(segment, segment);
  if (length_sq < 1e-4F) {
    return (point - anchor.start).length();
  }
  float const t = std::clamp(
      QVector2D::dotProduct(point - anchor.start, segment) / length_sq, 0.0F, 1.0F);
  QVector2D const closest = anchor.start + segment * t;
  return (point - closest).length();
}

inline auto distance_to_point(const QVector2D& point,
                              const PointAnchor& anchor) -> float {
  return (point - anchor.position).length();
}

inline auto point_influence(const QVector2D& point,
                            const std::vector<PointAnchor>& anchors,
                            float inner_scale,
                            float outer_scale) -> float {
  float strongest = 0.0F;
  for (const auto& anchor : anchors) {
    float const inner = std::max(0.0F, anchor.radius * inner_scale);
    float const outer = std::max(inner + 0.1F, anchor.radius * outer_scale);
    float const influence =
        1.0F - smoothstep(inner, outer, distance_to_point(point, anchor));
    strongest = std::max(strongest, influence);
  }
  return strongest;
}

inline auto line_influence(const QVector2D& point,
                           const std::vector<LinearAnchor>& anchors,
                           float inner_extra,
                           float outer_extra) -> float {
  float strongest = 0.0F;
  for (const auto& anchor : anchors) {
    float const inner = std::max(0.0F, anchor.width * 0.5F + inner_extra);
    float const outer = std::max(inner + 0.1F, inner + outer_extra);
    float const influence =
        1.0F - smoothstep(inner, outer, distance_to_segment(point, anchor));
    strongest = std::max(strongest, influence);
  }
  return strongest;
}

inline auto camp_layout_seed(float center_world_x,
                             float center_world_z) -> std::uint32_t {
  return hash_coords(static_cast<int>(std::floor(center_world_x * 4.0F)),
                     static_cast<int>(std::floor(center_world_z * 4.0F)),
                     0x6CB5E33AU);
}

inline auto camp_layout_angle(float center_world_x, float center_world_z) -> float {
  std::uint32_t state = camp_layout_seed(center_world_x, center_world_z);
  return rand_01(state) * MathConstants::k_two_pi;
}

inline auto
role_angle_offset(CampPropRole role, int slot_index, int slot_count) -> float {
  float const slot_center =
      slot_count > 1
          ? static_cast<float>(slot_index) - static_cast<float>(slot_count - 1) * 0.5F
          : 0.0F;
  switch (role) {
  case CampPropRole::Tent:
    return 0.65F + slot_center * 0.74F;
  case CampPropRole::SupplyCart:
    return MathConstants::k_two_pi * 0.5F + slot_center * 0.38F;
  case CampPropRole::WeaponRack:
    return 0.18F + slot_center * 0.52F;
  case CampPropRole::DeadTree:
    return -1.10F + slot_center * 0.62F;
  }
  return 0.0F;
}

inline auto
role_distance_range(CampPropRole role,
                    const ScatterCompositionSample& sample) -> std::pair<float, float> {
  switch (role) {
  case CampPropRole::Tent:
    return {0.84F, 0.98F + sample.dryness * 0.06F};
  case CampPropRole::SupplyCart:
    return {1.00F + sample.road_influence * 0.06F,
            1.18F + sample.road_influence * 0.10F};
  case CampPropRole::WeaponRack:
    return {0.68F, 0.82F + sample.cluster_bias * 0.06F};
  case CampPropRole::DeadTree:
    return {1.22F + sample.dryness * 0.08F, 1.52F + sample.rockiness * 0.10F};
  }
  return {0.9F, 1.1F};
}

inline auto to_species(CampPropRole role) -> ScatterRuleSpecies {
  switch (role) {
  case CampPropRole::Tent:
    return ScatterRuleSpecies::Tent;
  case CampPropRole::SupplyCart:
    return ScatterRuleSpecies::SupplyCart;
  case CampPropRole::WeaponRack:
    return ScatterRuleSpecies::WeaponRack;
  case CampPropRole::DeadTree:
    return ScatterRuleSpecies::DeadTree;
  }
  return ScatterRuleSpecies::Tent;
}

} // namespace Detail

class ScatterCompositionContext {
public:
  ScatterCompositionContext(const SpawnTerrainCache& cache,
                            int width,
                            int height,
                            float tile_size,
                            const Game::Map::BiomeSettings& biome_settings,
                            const std::vector<Game::Map::WorldProp>& world_props = {})
      : m_cache(cache)
      , m_width(width)
      , m_height(height)
      , m_tile_size(tile_size)
      , m_half_grid_width(static_cast<float>(width) * 0.5F - 0.5F)
      , m_half_grid_height(static_cast<float>(height) * 0.5F - 0.5F)
      , m_biome_settings(biome_settings)
      , m_climate_profile(Game::Map::make_climate_profile(biome_settings)) {
    build_point_anchors(world_props);
    build_linear_anchors();
  }

  [[nodiscard]] auto sample_grid(float gx, float gz, std::uint32_t salt = 0U) const
      -> ScatterCompositionSample {
    float world_x = 0.0F;
    float world_z = 0.0F;
    grid_to_world(gx, gz, world_x, world_z);
    return sample_world(world_x, world_z, salt);
  }

  [[nodiscard]] auto sample_world(float world_x, float world_z, std::uint32_t salt = 0U)
      const -> ScatterCompositionSample {
    QVector2D const point(world_x, world_z);
    float gx = 0.0F;
    float gz = 0.0F;
    world_to_grid(world_x, world_z, gx, gz);
    int const grid_x = std::clamp(
        static_cast<int>(std::floor(gx + 0.5F)), 0, std::max(0, m_width - 1));
    int const grid_z = std::clamp(
        static_cast<int>(std::floor(gz + 0.5F)), 0, std::max(0, m_height - 1));

    ScatterCompositionSample sample;
    sample.terrain_type = m_cache.get_terrain_type_at(grid_x, grid_z);
    sample.slope = m_cache.get_slope_at(grid_x, grid_z);
    sample.camp_influence =
        Detail::point_influence(point, m_camp_anchors, 1.15F, 4.60F);
    sample.prop_influence =
        Detail::point_influence(point, m_structure_anchors, 0.55F, 3.80F);
    sample.river_influence =
        Detail::line_influence(point, m_river_anchors, 0.30F, 3.00F);
    sample.road_influence = Detail::line_influence(point, m_road_anchors, 0.60F, 5.20F);
    float const bridge_influence =
        Detail::line_influence(point, m_bridge_anchors, 0.40F, 3.60F);

    float const macro_noise = value_noise(
        world_x * 0.018F, world_z * 0.018F, m_biome_settings.seed ^ salt ^ 0x84A35F21U);
    float const micro_noise = value_noise(
        world_x * 0.043F, world_z * 0.043F, m_biome_settings.seed ^ salt ^ 0x1E4C9AB3U);
    float const ridge_noise = value_noise(
        world_x * 0.071F, world_z * 0.071F, m_biome_settings.seed ^ salt ^ 0x5C83D12FU);
    sample.cluster_bias = std::clamp(
        macro_noise * 0.55F + micro_noise * 0.30F + ridge_noise * 0.15F, 0.0F, 1.0F);

    float const terrain_rock_bonus =
        sample.terrain_type == Game::Map::TerrainType::Mountain
            ? 0.32F
            : (sample.terrain_type == Game::Map::TerrainType::Hill ? 0.14F : 0.0F);
    float const terrain_fertility_bonus =
        sample.terrain_type == Game::Map::TerrainType::Forest
            ? 0.18F
            : (sample.terrain_type == Game::Map::TerrainType::Flat ? 0.10F : 0.0F);

    sample.rockiness = std::clamp(
        m_climate_profile.rock_exposure * 0.55F + sample.slope * 0.85F +
            sample.prop_influence * 0.35F + terrain_rock_bonus + macro_noise * 0.22F -
            sample.river_influence * 0.20F + bridge_influence * 0.10F,
        0.0F,
        1.0F);
    sample.dryness = std::clamp(
        (1.0F - m_climate_profile.moisture_level) * 0.65F +
            m_climate_profile.crack_intensity * 0.55F + sample.slope * 0.35F +
            macro_noise * 0.18F + sample.road_influence * 0.08F -
            sample.river_influence * 0.50F - sample.camp_influence * 0.12F,
        0.0F,
        1.0F);
    sample.shelter = std::clamp(
        (1.0F - sample.slope) * 0.45F + sample.camp_influence * 0.18F +
            (sample.terrain_type == Game::Map::TerrainType::Forest ? 0.35F : 0.0F) +
            (1.0F - sample.cluster_bias) * 0.18F + bridge_influence * 0.05F,
        0.0F,
        1.0F);
    sample.fertility = std::clamp(
        m_climate_profile.moisture_level * 0.55F + (1.0F - sample.dryness) * 0.30F +
            sample.river_influence * 0.45F + sample.shelter * 0.25F +
            terrain_fertility_bonus - sample.rockiness * 0.25F,
        0.0F,
        1.0F);

    if (sample.camp_influence > 0.42F && sample.river_influence < 0.45F) {
      sample.archetype = ScatterSceneArchetype::CampOutskirts;
    } else if (sample.river_influence > 0.55F) {
      sample.archetype = ScatterSceneArchetype::RiverFringe;
    } else if (sample.rockiness > 0.62F || sample.prop_influence > 0.45F) {
      sample.archetype = ScatterSceneArchetype::RockyPatch;
    } else if (sample.dryness > 0.60F) {
      sample.archetype = ScatterSceneArchetype::DryClearing;
    } else if (sample.shelter > 0.58F && sample.fertility > 0.45F) {
      sample.archetype = ScatterSceneArchetype::ShelteredGrove;
    } else if (sample.fertility > 0.58F) {
      sample.archetype = ScatterSceneArchetype::FertilePatch;
    } else {
      sample.archetype = ScatterSceneArchetype::OpenField;
    }

    return sample;
  }

private:
  void build_point_anchors(const std::vector<Game::Map::WorldProp>& world_props) {
    float const half_anchor_width = static_cast<float>(m_width) * 0.5F;
    float const half_anchor_height = static_cast<float>(m_height) * 0.5F;

    for (const auto& world_prop : world_props) {
      float const world_x = (world_prop.x - half_anchor_width) * m_tile_size;
      float const world_z = (world_prop.z - half_anchor_height) * m_tile_size;
      if (world_prop.type == Game::Map::WorldProp::Type::FireCamp) {
        m_camp_anchors.push_back(
            {{world_x, world_z}, std::max(world_prop.radius, 1.5F)});
        continue;
      }
      if (world_prop.type == Game::Map::WorldProp::Type::PineTree ||
          world_prop.type == Game::Map::WorldProp::Type::OliveTree) {
        const float radius = std::max(1.5F, world_prop.scale * 2.5F);
        m_structure_anchors.push_back({{world_x, world_z}, radius});
        continue;
      }
      if (world_prop.type != Game::Map::WorldProp::Type::Boulder &&
          world_prop.type != Game::Map::WorldProp::Type::Ruins) {
        continue;
      }
      float const radius = std::max(1.0F, world_prop.scale * 1.8F);
      m_structure_anchors.push_back({{world_x, world_z}, radius});
    }
  }

  void build_linear_anchors() {
    auto& terrain_service = Game::Map::TerrainService::instance();
    for (const auto& road : terrain_service.road_segments()) {
      m_road_anchors.push_back(
          {{road.start.x(), road.start.z()}, {road.end.x(), road.end.z()}, road.width});
    }

    auto const* height_map = terrain_service.get_height_map();
    if (height_map == nullptr) {
      return;
    }

    for (const auto& river : height_map->get_river_segments()) {
      m_river_anchors.push_back({{river.start.x(), river.start.z()},
                                 {river.end.x(), river.end.z()},
                                 river.width});
    }
    for (const auto& bridge : height_map->get_bridges()) {
      m_bridge_anchors.push_back({{bridge.start.x(), bridge.start.z()},
                                  {bridge.end.x(), bridge.end.z()},
                                  bridge.width});
    }
  }

  void grid_to_world(float gx, float gz, float& out_world_x, float& out_world_z) const {
    out_world_x = (gx - m_half_grid_width) * m_tile_size;
    out_world_z = (gz - m_half_grid_height) * m_tile_size;
  }

  void world_to_grid(float world_x, float world_z, float& out_gx, float& out_gz) const {
    out_gx = world_x / m_tile_size + m_half_grid_width;
    out_gz = world_z / m_tile_size + m_half_grid_height;
  }

  const SpawnTerrainCache& m_cache;
  int m_width = 0;
  int m_height = 0;
  float m_tile_size = 1.0F;
  float m_half_grid_width = 0.0F;
  float m_half_grid_height = 0.0F;
  Game::Map::BiomeSettings m_biome_settings;
  Game::Map::ClimateProfile m_climate_profile;
  std::vector<Detail::PointAnchor> m_camp_anchors;
  std::vector<Detail::PointAnchor> m_structure_anchors;
  std::vector<Detail::LinearAnchor> m_road_anchors;
  std::vector<Detail::LinearAnchor> m_river_anchors;
  std::vector<Detail::LinearAnchor> m_bridge_anchors;
};

[[nodiscard]] inline auto
scatter_density_multiplier(ScatterRuleSpecies species,
                           const ScatterCompositionSample& sample) -> float {
  float scene_multiplier = 1.0F;
  switch (species) {
  case ScatterRuleSpecies::Stone:
    switch (sample.archetype) {
    case ScatterSceneArchetype::RockyPatch:
      scene_multiplier = 1.70F;
      break;
    case ScatterSceneArchetype::DryClearing:
      scene_multiplier = 1.25F;
      break;
    case ScatterSceneArchetype::RiverFringe:
      scene_multiplier = 0.35F;
      break;
    case ScatterSceneArchetype::CampOutskirts:
      scene_multiplier = 0.45F;
      break;
    case ScatterSceneArchetype::FertilePatch:
      scene_multiplier = 0.60F;
      break;
    case ScatterSceneArchetype::ShelteredGrove:
      scene_multiplier = 0.75F;
      break;
    case ScatterSceneArchetype::OpenField:
      scene_multiplier = 1.00F;
      break;
    }
    scene_multiplier *= 0.75F + sample.rockiness * 0.85F + sample.cluster_bias * 0.35F -
                        sample.fertility * 0.20F;
    break;
  case ScatterRuleSpecies::Plant:
    switch (sample.archetype) {
    case ScatterSceneArchetype::RiverFringe:
      scene_multiplier = 1.55F;
      break;
    case ScatterSceneArchetype::FertilePatch:
      scene_multiplier = 1.45F;
      break;
    case ScatterSceneArchetype::ShelteredGrove:
      scene_multiplier = 1.25F;
      break;
    case ScatterSceneArchetype::RockyPatch:
      scene_multiplier = 0.50F;
      break;
    case ScatterSceneArchetype::DryClearing:
      scene_multiplier = 0.65F;
      break;
    case ScatterSceneArchetype::CampOutskirts:
      scene_multiplier = 0.75F;
      break;
    case ScatterSceneArchetype::OpenField:
      scene_multiplier = 1.00F;
      break;
    }
    scene_multiplier *= 0.60F + sample.fertility * 0.90F + sample.cluster_bias * 0.40F -
                        sample.rockiness * 0.25F;
    break;
  case ScatterRuleSpecies::Pine:
    switch (sample.archetype) {
    case ScatterSceneArchetype::ShelteredGrove:
      scene_multiplier = 1.60F;
      break;
    case ScatterSceneArchetype::RockyPatch:
      scene_multiplier = 1.05F;
      break;
    case ScatterSceneArchetype::DryClearing:
      scene_multiplier = 0.28F;
      break;
    case ScatterSceneArchetype::RiverFringe:
      scene_multiplier = 0.45F;
      break;
    case ScatterSceneArchetype::CampOutskirts:
      scene_multiplier = 0.55F;
      break;
    case ScatterSceneArchetype::FertilePatch:
      scene_multiplier = 0.85F;
      break;
    case ScatterSceneArchetype::OpenField:
      scene_multiplier = 1.00F;
      break;
    }
    scene_multiplier *= 0.55F + sample.shelter * 0.95F + sample.rockiness * 0.20F -
                        sample.dryness * 0.35F;
    break;
  case ScatterRuleSpecies::Olive:
    switch (sample.archetype) {
    case ScatterSceneArchetype::DryClearing:
      scene_multiplier = 1.70F;
      break;
    case ScatterSceneArchetype::RockyPatch:
      scene_multiplier = 1.30F;
      break;
    case ScatterSceneArchetype::RiverFringe:
      scene_multiplier = 0.25F;
      break;
    case ScatterSceneArchetype::ShelteredGrove:
      scene_multiplier = 0.55F;
      break;
    case ScatterSceneArchetype::FertilePatch:
      scene_multiplier = 0.65F;
      break;
    case ScatterSceneArchetype::CampOutskirts:
      scene_multiplier = 0.70F;
      break;
    case ScatterSceneArchetype::OpenField:
      scene_multiplier = 1.00F;
      break;
    }
    scene_multiplier *= 0.55F + sample.dryness * 0.95F + sample.rockiness * 0.30F -
                        sample.river_influence * 0.40F;
    break;
  case ScatterRuleSpecies::FireCamp:
    switch (sample.archetype) {
    case ScatterSceneArchetype::DryClearing:
      scene_multiplier = 1.40F;
      break;
    case ScatterSceneArchetype::OpenField:
      scene_multiplier = 1.00F;
      break;
    case ScatterSceneArchetype::RockyPatch:
      scene_multiplier = 0.70F;
      break;
    case ScatterSceneArchetype::RiverFringe:
      scene_multiplier = 0.15F;
      break;
    case ScatterSceneArchetype::CampOutskirts:
      scene_multiplier = 0.25F;
      break;
    case ScatterSceneArchetype::FertilePatch:
      scene_multiplier = 0.65F;
      break;
    case ScatterSceneArchetype::ShelteredGrove:
      scene_multiplier = 0.55F;
      break;
    }
    scene_multiplier *= 0.70F + sample.dryness * 0.45F + sample.cluster_bias * 0.20F -
                        sample.river_influence * 0.45F;
    break;
  case ScatterRuleSpecies::Tent:
    scene_multiplier =
        sample.archetype == ScatterSceneArchetype::CampOutskirts ? 1.30F : 0.75F;
    scene_multiplier *= 0.80F + sample.dryness * 0.20F + sample.camp_influence * 0.30F;
    break;
  case ScatterRuleSpecies::SupplyCart:
    scene_multiplier =
        sample.archetype == ScatterSceneArchetype::CampOutskirts ? 1.20F : 0.70F;
    scene_multiplier *=
        0.75F + sample.road_influence * 0.35F + sample.camp_influence * 0.25F;
    break;
  case ScatterRuleSpecies::WeaponRack:
    scene_multiplier =
        sample.archetype == ScatterSceneArchetype::CampOutskirts ? 1.25F : 0.65F;
    scene_multiplier *=
        0.80F + sample.cluster_bias * 0.20F + sample.camp_influence * 0.35F;
    break;
  case ScatterRuleSpecies::DeadTree:
    switch (sample.archetype) {
    case ScatterSceneArchetype::DryClearing:
      scene_multiplier = 1.35F;
      break;
    case ScatterSceneArchetype::RockyPatch:
      scene_multiplier = 1.15F;
      break;
    case ScatterSceneArchetype::RiverFringe:
      scene_multiplier = 0.25F;
      break;
    case ScatterSceneArchetype::CampOutskirts:
      scene_multiplier = 0.90F;
      break;
    case ScatterSceneArchetype::OpenField:
      scene_multiplier = 0.70F;
      break;
    case ScatterSceneArchetype::FertilePatch:
      scene_multiplier = 0.35F;
      break;
    case ScatterSceneArchetype::ShelteredGrove:
      scene_multiplier = 0.45F;
      break;
    }
    scene_multiplier *= 0.65F + sample.dryness * 0.60F + sample.rockiness * 0.25F -
                        sample.river_influence * 0.25F;
    break;
  }
  return std::clamp(scene_multiplier, 0.0F, 2.6F);
}

[[nodiscard]] inline auto
scatter_spawn_chance(ScatterRuleSpecies species,
                     const ScatterCompositionSample& sample) -> float {
  float const density = scatter_density_multiplier(species, sample);
  float chance = 0.20F + density * 0.30F;
  switch (species) {
  case ScatterRuleSpecies::Stone:
    chance += sample.rockiness * 0.15F - sample.fertility * 0.10F;
    break;
  case ScatterRuleSpecies::Plant:
    chance += sample.fertility * 0.14F - sample.rockiness * 0.10F;
    break;
  case ScatterRuleSpecies::Pine:
    chance += sample.shelter * 0.18F - sample.dryness * 0.10F;
    break;
  case ScatterRuleSpecies::Olive:
    chance += sample.dryness * 0.18F - sample.river_influence * 0.18F;
    break;
  case ScatterRuleSpecies::FireCamp:
    chance += sample.dryness * 0.10F - sample.camp_influence * 0.25F -
              sample.river_influence * 0.25F;
    break;
  case ScatterRuleSpecies::Tent:
  case ScatterRuleSpecies::SupplyCart:
  case ScatterRuleSpecies::WeaponRack:
    chance += sample.camp_influence * 0.15F;
    break;
  case ScatterRuleSpecies::DeadTree:
    chance += sample.dryness * 0.12F - sample.fertility * 0.12F;
    break;
  }
  return std::clamp(chance, 0.05F, 0.95F);
}

[[nodiscard]] inline auto
scatter_scale_bias(ScatterRuleSpecies species,
                   const ScatterCompositionSample& sample) -> float {
  switch (species) {
  case ScatterRuleSpecies::Stone:
    return std::clamp(
        0.90F + sample.rockiness * 0.22F + sample.dryness * 0.10F, 0.80F, 1.30F);
  case ScatterRuleSpecies::Plant:
    return std::clamp(
        0.84F + sample.fertility * 0.22F - sample.dryness * 0.08F, 0.75F, 1.20F);
  case ScatterRuleSpecies::Pine:
    return std::clamp(
        0.86F + sample.shelter * 0.18F + sample.cluster_bias * 0.10F, 0.80F, 1.18F);
  case ScatterRuleSpecies::Olive:
    return std::clamp(
        0.88F + sample.dryness * 0.18F + sample.rockiness * 0.08F, 0.80F, 1.22F);
  case ScatterRuleSpecies::FireCamp:
    return std::clamp(0.92F + sample.dryness * 0.12F, 0.85F, 1.12F);
  case ScatterRuleSpecies::Tent:
  case ScatterRuleSpecies::SupplyCart:
  case ScatterRuleSpecies::WeaponRack:
    return std::clamp(0.92F + sample.camp_influence * 0.10F, 0.85F, 1.15F);
  case ScatterRuleSpecies::DeadTree:
    return std::clamp(
        0.92F + sample.dryness * 0.18F + sample.rockiness * 0.08F, 0.85F, 1.18F);
  }
  return 1.0F;
}

[[nodiscard]] inline auto
scatter_cluster_satellite_count(ScatterRuleSpecies species,
                                const ScatterCompositionSample& sample,
                                std::uint32_t& state) -> int {
  int min_satellites = 0;
  int max_satellites = 0;
  switch (species) {
  case ScatterRuleSpecies::Stone:
    if (sample.archetype == ScatterSceneArchetype::RockyPatch) {
      min_satellites = sample.cluster_bias > 0.45F ? 1 : 0;
      max_satellites = 3;
    } else if (sample.archetype == ScatterSceneArchetype::DryClearing) {
      max_satellites = 2;
    } else {
      max_satellites = 1;
    }
    break;
  case ScatterRuleSpecies::Plant:
    if (sample.archetype == ScatterSceneArchetype::RiverFringe ||
        sample.archetype == ScatterSceneArchetype::FertilePatch) {
      min_satellites = sample.cluster_bias > 0.35F ? 1 : 0;
      max_satellites = 3;
    } else if (sample.archetype == ScatterSceneArchetype::ShelteredGrove) {
      max_satellites = 2;
    } else {
      max_satellites = 1;
    }
    break;
  case ScatterRuleSpecies::Pine:
    max_satellites = sample.archetype == ScatterSceneArchetype::ShelteredGrove ? 2 : 1;
    break;
  case ScatterRuleSpecies::Olive:
    max_satellites = (sample.archetype == ScatterSceneArchetype::DryClearing ||
                      sample.archetype == ScatterSceneArchetype::RockyPatch)
                         ? 2
                         : 1;
    break;
  case ScatterRuleSpecies::FireCamp:
  case ScatterRuleSpecies::Tent:
  case ScatterRuleSpecies::SupplyCart:
  case ScatterRuleSpecies::WeaponRack:
  case ScatterRuleSpecies::DeadTree:
    return 0;
  }
  if (max_satellites <= 0) {
    return 0;
  }
  return min_satellites +
         static_cast<int>(std::floor(
             rand_01(state) * static_cast<float>(max_satellites - min_satellites + 1)));
}

[[nodiscard]] inline auto
scatter_cluster_radius_tiles(ScatterRuleSpecies species,
                             const ScatterCompositionSample& sample,
                             std::uint32_t& state) -> float {
  float min_radius = 0.35F;
  float max_radius = 1.00F;
  switch (species) {
  case ScatterRuleSpecies::Stone:
    min_radius = 0.55F;
    max_radius = 1.80F;
    break;
  case ScatterRuleSpecies::Plant:
    min_radius = 0.25F;
    max_radius = 0.95F;
    break;
  case ScatterRuleSpecies::Pine:
  case ScatterRuleSpecies::Olive:
    min_radius = 1.50F;
    max_radius = 3.20F;
    break;
  case ScatterRuleSpecies::FireCamp:
  case ScatterRuleSpecies::Tent:
  case ScatterRuleSpecies::SupplyCart:
  case ScatterRuleSpecies::WeaponRack:
  case ScatterRuleSpecies::DeadTree:
    return 0.0F;
  }
  float const radius = remap(rand_01(state), min_radius, max_radius) *
                       (0.85F + sample.cluster_bias * 0.35F);
  return radius;
}

[[nodiscard]] inline auto
camp_prop_slot_count(CampPropRole role,
                     const ScatterCompositionSample& camp_sample,
                     float camp_radius,
                     std::uint32_t& state) -> int {
  float const size_factor = smoothstep(1.8F, 4.2F, camp_radius);
  switch (role) {
  case CampPropRole::Tent: {
    int count = 1 + (size_factor > 0.35F ? 1 : 0);
    if (size_factor > 0.80F && rand_01(state) < 0.45F) {
      ++count;
    }
    return count;
  }
  case CampPropRole::SupplyCart: {
    int count = (camp_radius > 1.7F || camp_sample.road_influence > 0.25F) ? 1 : 0;
    if (count > 0 && size_factor > 0.85F && rand_01(state) < 0.35F) {
      ++count;
    }
    return count;
  }
  case CampPropRole::WeaponRack: {
    if (camp_sample.fertility > 0.75F && camp_sample.dryness < 0.35F) {
      return 0;
    }
    int count = 1;
    if (size_factor > 0.45F && rand_01(state) < 0.60F) {
      ++count;
    }
    return count;
  }
  case CampPropRole::DeadTree: {
    if (camp_sample.dryness < 0.45F && camp_sample.rockiness < 0.55F) {
      return 0;
    }
    int count = 1;
    if (size_factor > 0.70F && rand_01(state) < 0.30F) {
      ++count;
    }
    return count;
  }
  }
  return 0;
}

inline auto
find_contextual_camp_prop_spawn_position(const SpawnValidator& validator,
                                         const ScatterCompositionContext& context,
                                         CampPropRole role,
                                         float center_world_x,
                                         float center_world_z,
                                         float preferred_distance,
                                         const ScatterCompositionSample& camp_sample,
                                         int slot_index,
                                         int slot_count,
                                         std::uint32_t& state,
                                         float& out_world_x,
                                         float& out_world_z) -> bool {
  float const base_angle = Detail::camp_layout_angle(center_world_x, center_world_z) +
                           Detail::role_angle_offset(role, slot_index, slot_count);
  auto const [min_scale, max_scale] = Detail::role_distance_range(role, camp_sample);

  for (int attempt = 0; attempt < 12; ++attempt) {
    float const angle = base_angle + remap(rand_01(state), -0.22F, 0.22F) +
                        static_cast<float>(attempt) * 0.08F;
    float const distance_scale = remap(rand_01(state), min_scale, max_scale) +
                                 static_cast<float>(attempt) * 0.03F;
    float const distance = preferred_distance * distance_scale;
    float const candidate_x = center_world_x + std::cos(angle) * distance;
    float const candidate_z = center_world_z + std::sin(angle) * distance;
    if (!validator.can_spawn_at_world(candidate_x, candidate_z)) {
      continue;
    }

    auto const sample =
        context.sample_world(candidate_x, candidate_z, state ^ 0x4D92F1B7U);
    if (rand_01(state) > scatter_spawn_chance(Detail::to_species(role), sample)) {
      continue;
    }
    out_world_x = candidate_x;
    out_world_z = candidate_z;
    return true;
  }
  return false;
}

} // namespace Render::Ground
