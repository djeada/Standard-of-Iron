#include "map/map_definition.h"
#include "map/minimap/minimap_generator.h"
#include <QImage>
#include <gtest/gtest.h>

using namespace Game::Map;
using namespace Game::Map::Minimap;

class MinimapGeneratorTest : public ::testing::Test {
protected:
  // No additional setup needed; the global test environment handles
  // QGuiApplication.

  void SetUp() override {
    // Create a simple test map
    test_map.name = "Test Map";
    test_map.grid.width = 50;
    test_map.grid.height = 50;
    test_map.grid.tile_size = 1.0F;

    // Set up biome with default colors
    test_map.biome.grass_primary = QVector3D(0.3F, 0.6F, 0.28F);
    test_map.biome.grass_secondary = QVector3D(0.44F, 0.7F, 0.32F);
    test_map.biome.soil_color = QVector3D(0.28F, 0.24F, 0.18F);
  }

  MapDefinition test_map;
};

TEST_F(MinimapGeneratorTest, GeneratesValidImage) {
  MinimapGenerator generator;
  QImage result = generator.generate(test_map);

  // Check that image was created
  EXPECT_FALSE(result.isNull());
  EXPECT_GT(result.width(), 0);
  EXPECT_GT(result.height(), 0);
}

TEST_F(MinimapGeneratorTest, ImageDimensionsMatchGrid) {
  MinimapGenerator::Config config;
  config.pixels_per_tile = 2.0F;
  MinimapGenerator generator(config);

  QImage result = generator.generate(test_map);

  const int expected_width = test_map.grid.width * config.pixels_per_tile;
  const int expected_height = test_map.grid.height * config.pixels_per_tile;

  EXPECT_EQ(result.width(), expected_width);
  EXPECT_EQ(result.height(), expected_height);
}

TEST_F(MinimapGeneratorTest, RendersRivers) {
  // Add a river to the map
  RiverSegment river;
  river.start = QVector3D(10.0F, 0.0F, 10.0F);
  river.end = QVector3D(40.0F, 0.0F, 40.0F);
  river.width = 3.0F;
  test_map.rivers.push_back(river);

  MinimapGenerator generator;
  QImage result = generator.generate(test_map);

  EXPECT_FALSE(result.isNull());
  // River should have been rendered (we can't easily verify pixels, but check
  // image is valid)
}

TEST_F(MinimapGeneratorTest, RendersTerrainFeatures) {
  // Add a hill
  TerrainFeature hill;
  hill.type = TerrainType::Hill;
  hill.center_x = 25.0F;
  hill.center_z = 25.0F;
  hill.width = 10.0F;
  hill.depth = 10.0F;
  hill.height = 3.0F;
  test_map.terrain.push_back(hill);

  MinimapGenerator generator;
  QImage result = generator.generate(test_map);

  EXPECT_FALSE(result.isNull());
}

TEST_F(MinimapGeneratorTest, RendersForestFeatures) {
  // Add a forest
  TerrainFeature forest;
  forest.type = TerrainType::Forest;
  forest.center_x = 30.0F;
  forest.center_z = 30.0F;
  forest.width = 8.0F;
  forest.depth = 8.0F;
  forest.height = 2.0F;
  test_map.terrain.push_back(forest);

  MinimapGenerator generator;
  QImage result = generator.generate(test_map);

  EXPECT_FALSE(result.isNull());
}

TEST_F(MinimapGeneratorTest, RendersRoads) {
  // Add a road
  RoadSegment road;
  road.start = QVector3D(5.0F, 0.0F, 5.0F);
  road.end = QVector3D(45.0F, 0.0F, 45.0F);
  road.width = 3.0F;
  road.style = "default";
  test_map.roads.push_back(road);

  MinimapGenerator generator;
  QImage result = generator.generate(test_map);

  EXPECT_FALSE(result.isNull());
}

TEST_F(MinimapGeneratorTest, RendersStructures) {
  // Add a barracks spawn
  UnitSpawn barracks;
  barracks.type = Game::Units::SpawnType::Barracks;
  barracks.x = 25.0F;
  barracks.z = 25.0F;
  barracks.player_id = 1;
  test_map.spawns.push_back(barracks);

  MinimapGenerator generator;
  QImage result = generator.generate(test_map);

  EXPECT_FALSE(result.isNull());
}

TEST_F(MinimapGeneratorTest, HandlesEmptyMap) {
  // Empty map with no features
  MapDefinition empty_map;
  empty_map.grid.width = 10;
  empty_map.grid.height = 10;
  empty_map.grid.tile_size = 1.0F;
  empty_map.biome.grass_primary = QVector3D(0.3F, 0.6F, 0.28F);

  MinimapGenerator generator;
  QImage result = generator.generate(empty_map);

  EXPECT_FALSE(result.isNull());
  EXPECT_GT(result.width(), 0);
  EXPECT_GT(result.height(), 0);
}
