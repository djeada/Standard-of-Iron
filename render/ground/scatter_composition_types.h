#pragma once

#include "../../game/map/map_definition.h"

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
  float obstacle_influence = 0.0F;
  float cluster_bias = 0.0F;
};

} // namespace Render::Ground
