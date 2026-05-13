#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "render/ground/scatter_composition.h"
#include "render/ground/spawn_validator.h"
#include <gtest/gtest.h>

namespace {

class ScatterCompositionTest : public ::testing::Test {
protected:
  void TearDown() override { Game::Map::TerrainService::instance().clear(); }

  auto build_context(const Game::Map::MapDefinition &map_def,
                     const std::vector<Game::Map::WorldProp> &world_props = {})
      -> Render::Ground::ScatterCompositionContext {
    auto &terrain = Game::Map::TerrainService::instance();
    terrain.initialize(map_def);
    auto const *height_map = terrain.get_height_map();
    EXPECT_NE(height_map, nullptr);

    cache.build_from_height_map(
        height_map->get_height_data(), height_map->getTerrainTypes(),
        height_map->get_width(), height_map->get_height(),
        height_map->get_tile_size());
    return {cache,
            height_map->get_width(),
            height_map->get_height(),
            height_map->get_tile_size(),
            map_def.biome,
            world_props};
  }

  Render::Ground::SpawnTerrainCache cache;
};

TEST_F(ScatterCompositionTest, ContextClassifiesCampRiverAndRockyScenes) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 20;
  map_def.grid.height = 20;
  map_def.grid.tile_size = 1.0F;
  map_def.rivers.push_back(
      {QVector3D(-6.0F, 0.0F, -8.0F), QVector3D(-6.0F, 0.0F, 8.0F), 2.0F});

  std::vector<Game::Map::WorldProp> world_props{
      {.type = Game::Map::WorldProp::Type::FireCamp,
       .x = 10.0F,
       .z = 10.0F,
       .intensity = 1.0F,
       .radius = 1.5F,
       .persistent = true},
      {.type = Game::Map::WorldProp::Type::Boulder,
       .x = 18.0F,
       .z = 10.0F,
       .scale = 1.6F,
       .rotation = 0.0F}};

  auto const context = build_context(map_def, world_props);

  auto const camp = context.sample_world(0.0F, 0.0F);
  auto const river = context.sample_world(-6.0F, 0.0F);
  auto const rocky = context.sample_world(8.0F, 0.0F);

  EXPECT_EQ(camp.archetype,
            Render::Ground::ScatterSceneArchetype::CampOutskirts);
  EXPECT_EQ(river.archetype,
            Render::Ground::ScatterSceneArchetype::RiverFringe);
  EXPECT_EQ(rocky.archetype, Render::Ground::ScatterSceneArchetype::RockyPatch);
  EXPECT_GT(camp.camp_influence, 0.8F);
  EXPECT_GT(river.river_influence, 0.8F);
  EXPECT_GT(rocky.prop_influence, 0.4F);
}

TEST(ScatterCompositionRulesTest, DensityMultipliersFavorMatchingScenes) {
  Render::Ground::ScatterCompositionSample rocky;
  rocky.archetype = Render::Ground::ScatterSceneArchetype::RockyPatch;
  rocky.rockiness = 0.9F;
  rocky.dryness = 0.7F;
  rocky.cluster_bias = 0.8F;

  Render::Ground::ScatterCompositionSample fertile;
  fertile.archetype = Render::Ground::ScatterSceneArchetype::FertilePatch;
  fertile.fertility = 0.9F;
  fertile.cluster_bias = 0.7F;

  Render::Ground::ScatterCompositionSample dry;
  dry.archetype = Render::Ground::ScatterSceneArchetype::DryClearing;
  dry.dryness = 0.85F;
  dry.rockiness = 0.65F;
  dry.cluster_bias = 0.75F;

  EXPECT_GT(Render::Ground::scatter_density_multiplier(
                Render::Ground::ScatterRuleSpecies::Stone, rocky),
            Render::Ground::scatter_density_multiplier(
                Render::Ground::ScatterRuleSpecies::Plant, rocky));
  EXPECT_GT(Render::Ground::scatter_density_multiplier(
                Render::Ground::ScatterRuleSpecies::Plant, fertile),
            Render::Ground::scatter_density_multiplier(
                Render::Ground::ScatterRuleSpecies::Stone, fertile));
  EXPECT_GT(Render::Ground::scatter_density_multiplier(
                Render::Ground::ScatterRuleSpecies::Olive, dry),
            Render::Ground::scatter_density_multiplier(
                Render::Ground::ScatterRuleSpecies::Pine, dry));
}

TEST(ScatterCompositionRulesTest,
     PreferredScenesProduceMoreSatelliteFollowers) {
  Render::Ground::ScatterCompositionSample rocky;
  rocky.archetype = Render::Ground::ScatterSceneArchetype::RockyPatch;
  rocky.rockiness = 0.9F;
  rocky.cluster_bias = 0.85F;

  Render::Ground::ScatterCompositionSample fertile;
  fertile.archetype = Render::Ground::ScatterSceneArchetype::FertilePatch;
  fertile.fertility = 0.9F;
  fertile.cluster_bias = 0.85F;

  int rocky_stones = 0;
  int fertile_stones = 0;
  int fertile_plants = 0;
  int rocky_plants = 0;
  for (std::uint32_t seed = 1; seed <= 64; ++seed) {
    auto stone_state_rocky = seed;
    auto stone_state_fertile = seed;
    auto plant_state_fertile = seed;
    auto plant_state_rocky = seed;
    rocky_stones += Render::Ground::scatter_cluster_satellite_count(
        Render::Ground::ScatterRuleSpecies::Stone, rocky, stone_state_rocky);
    fertile_stones += Render::Ground::scatter_cluster_satellite_count(
        Render::Ground::ScatterRuleSpecies::Stone, fertile,
        stone_state_fertile);
    fertile_plants += Render::Ground::scatter_cluster_satellite_count(
        Render::Ground::ScatterRuleSpecies::Plant, fertile,
        plant_state_fertile);
    rocky_plants += Render::Ground::scatter_cluster_satellite_count(
        Render::Ground::ScatterRuleSpecies::Plant, rocky, plant_state_rocky);
  }

  EXPECT_GT(rocky_stones, fertile_stones);
  EXPECT_GT(fertile_plants, rocky_plants);
}

TEST(ScatterCompositionRulesTest, LargeDryCampsGainRicherPropLayouts) {
  Render::Ground::ScatterCompositionSample dry_camp;
  dry_camp.archetype = Render::Ground::ScatterSceneArchetype::CampOutskirts;
  dry_camp.dryness = 0.8F;
  dry_camp.rockiness = 0.65F;
  dry_camp.road_influence = 0.35F;
  dry_camp.camp_influence = 1.0F;
  dry_camp.cluster_bias = 0.7F;

  Render::Ground::ScatterCompositionSample wet_camp;
  wet_camp.archetype = Render::Ground::ScatterSceneArchetype::CampOutskirts;
  wet_camp.dryness = 0.15F;
  wet_camp.fertility = 0.85F;
  wet_camp.camp_influence = 1.0F;
  wet_camp.road_influence = 0.10F;

  int tent_slots = 0;
  int supply_slots = 0;
  int dry_dead_trees = 0;
  int wet_dead_trees = 0;
  for (std::uint32_t seed = 10; seed < 74; ++seed) {
    auto tent_state = seed;
    auto supply_state = seed;
    auto dry_state = seed;
    auto wet_state = seed;
    tent_slots += Render::Ground::camp_prop_slot_count(
        Render::Ground::CampPropRole::Tent, dry_camp, 3.6F, tent_state);
    supply_slots += Render::Ground::camp_prop_slot_count(
        Render::Ground::CampPropRole::SupplyCart, dry_camp, 3.6F, supply_state);
    dry_dead_trees += Render::Ground::camp_prop_slot_count(
        Render::Ground::CampPropRole::DeadTree, dry_camp, 3.6F, dry_state);
    wet_dead_trees += Render::Ground::camp_prop_slot_count(
        Render::Ground::CampPropRole::DeadTree, wet_camp, 3.6F, wet_state);
  }

  EXPECT_GT(tent_slots, 64);
  EXPECT_GT(supply_slots, 32);
  EXPECT_GT(dry_dead_trees, 32);
  EXPECT_EQ(wet_dead_trees, 0);
}

} // namespace
