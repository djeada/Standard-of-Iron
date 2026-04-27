#include "game/map/terrain.h"

#include <gtest/gtest.h>

namespace {

TEST(TerrainProfilesTest, SplitsBiomeIntoSurfaceAndScatterViews) {
  Game::Map::BiomeSettings biome;
  Game::Map::apply_ground_type_defaults(biome,
                                        Game::Map::GroundType::SoilFertile);

  const auto surface = Game::Map::make_surface_profile(biome);
  const auto scatter = Game::Map::make_scatter_profile(biome);
  const auto climate = Game::Map::make_climate_profile(biome);
  const auto wind = Game::Map::make_wind_profile(biome);
  const auto profiles = Game::Map::make_biome_profiles(biome);

  EXPECT_EQ(surface.ground_type, biome.ground_type);
  EXPECT_EQ(surface.soil_color, biome.soil_color);
  EXPECT_FLOAT_EQ(surface.terrain_macro_noise_scale,
                  biome.terrain_macro_noise_scale);
  EXPECT_EQ(scatter.ground_type, biome.ground_type);
  EXPECT_EQ(scatter.grass_primary, biome.grass_primary);
  EXPECT_FLOAT_EQ(scatter.patch_density, biome.patch_density);
  EXPECT_FLOAT_EQ(scatter.spawn_edge_padding, biome.spawn_edge_padding);
  EXPECT_FLOAT_EQ(climate.moisture_level, biome.moisture_level);
  EXPECT_EQ(climate.snow_color, biome.snow_color);
  EXPECT_FLOAT_EQ(wind.sway_strength, biome.sway_strength);
  EXPECT_FLOAT_EQ(wind.background_sway_variance,
                  biome.background_sway_variance);
  EXPECT_EQ(profiles.surface.rock_high, biome.rock_high);
  EXPECT_FLOAT_EQ(profiles.scatter.background_scatter_radius,
                  biome.background_scatter_radius);
  EXPECT_FLOAT_EQ(profiles.climate.soil_roughness, biome.soil_roughness);
  EXPECT_FLOAT_EQ(profiles.wind.sway_speed, biome.sway_speed);
}

TEST(TerrainProfilesTest, ScatterRulesCentralizeDryGroundTreeChoice) {
  const auto dry_rules =
      Game::Map::make_scatter_rules(Game::Map::GroundType::GrassDry);
  EXPECT_FALSE(dry_rules.allow_pines);
  EXPECT_TRUE(dry_rules.allow_olives);
  EXPECT_FLOAT_EQ(dry_rules.olive_base_density, 0.12F);
  EXPECT_FLOAT_EQ(dry_rules.olive_density_scale, 0.15F);
  EXPECT_FLOAT_EQ(dry_rules.olive_scale_min, 3.2F);
  EXPECT_FLOAT_EQ(dry_rules.olive_scale_max, 6.5F);

  const auto forest_rules =
      Game::Map::make_scatter_rules(Game::Map::GroundType::ForestMud);
  EXPECT_TRUE(forest_rules.allow_pines);
  EXPECT_FALSE(forest_rules.allow_olives);
  EXPECT_FLOAT_EQ(forest_rules.olive_base_density, 0.05F);
  EXPECT_FLOAT_EQ(forest_rules.olive_density_scale, 0.08F);
}

} // namespace
