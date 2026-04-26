#include "game/map/terrain.h"

#include <gtest/gtest.h>

namespace {

TEST(TerrainProfilesTest, SplitsBiomeIntoSurfaceAndScatterViews) {
  Game::Map::BiomeSettings biome;
  Game::Map::apply_ground_type_defaults(biome, Game::Map::GroundType::SoilFertile);

  const auto surface = Game::Map::make_surface_profile(biome);
  const auto scatter = Game::Map::make_scatter_profile(biome);

  EXPECT_EQ(surface.ground_type, biome.ground_type);
  EXPECT_EQ(surface.soil_color, biome.soil_color);
  EXPECT_FLOAT_EQ(surface.terrain_macro_noise_scale,
                  biome.terrain_macro_noise_scale);
  EXPECT_EQ(scatter.ground_type, biome.ground_type);
  EXPECT_EQ(scatter.grass_primary, biome.grass_primary);
  EXPECT_FLOAT_EQ(scatter.patch_density, biome.patch_density);
  EXPECT_FLOAT_EQ(scatter.spawn_edge_padding, biome.spawn_edge_padding);
}

TEST(TerrainProfilesTest, ScatterRulesCentralizeDryGroundTreeChoice) {
  const auto dry_rules =
      Game::Map::make_scatter_rules(Game::Map::GroundType::GrassDry);
  EXPECT_FALSE(dry_rules.allow_pines);
  EXPECT_TRUE(dry_rules.allow_olives);

  const auto forest_rules =
      Game::Map::make_scatter_rules(Game::Map::GroundType::ForestMud);
  EXPECT_TRUE(forest_rules.allow_pines);
  EXPECT_FALSE(forest_rules.allow_olives);
}

} // namespace
