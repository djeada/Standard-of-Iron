#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "ground_utils.h"
#include "scatter_composition_types.h"

namespace Render::Ground {

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

} // namespace Render::Ground
