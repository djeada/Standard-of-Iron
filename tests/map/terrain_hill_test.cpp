#include "map/terrain.h"
#include <QVector3D>
#include <gtest/gtest.h>

using namespace Game::Map;

class TerrainHillTest : public ::testing::Test {
protected:
  static constexpr int GRID_WIDTH = 100;
  static constexpr int GRID_HEIGHT = 100;
  static constexpr float TILE_SIZE = 1.0F;
};

TEST_F(TerrainHillTest, HillPlateauIsWalkable) {
  TerrainHeightMap heightMap(GRID_WIDTH, GRID_HEIGHT, TILE_SIZE);

  // Create a hill with entrances
  std::vector<TerrainFeature> features;
  TerrainFeature hill;
  hill.type = TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.width = 20.0F;
  hill.depth = 20.0F;
  hill.height = 4.0F;
  hill.rotationDeg = 0.0F;

  // Add entrance from the south
  hill.entrances.push_back(QVector3D(50, 0.0F, 40));

  features.push_back(hill);
  heightMap.buildFromFeatures(features);

  // Check that the plateau center is walkable
  EXPECT_TRUE(heightMap.is_walkable(50, 50))
      << "Hill plateau center should be walkable";
}

TEST_F(TerrainHillTest, HillSteepSlopeIsNotWalkable) {
  TerrainHeightMap heightMap(GRID_WIDTH, GRID_HEIGHT, TILE_SIZE);

  // Create a hill with a single entrance on the south side
  std::vector<TerrainFeature> features;
  TerrainFeature hill;
  hill.type = TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.width = 20.0F;
  hill.depth = 20.0F;
  hill.height = 4.0F;
  hill.rotationDeg = 0.0F;

  // Only one entrance from the south
  hill.entrances.push_back(QVector3D(50, 0.0F, 40));

  features.push_back(hill);
  heightMap.buildFromFeatures(features);

  // Check that steep slopes (without entrance) are NOT walkable
  // North side (opposite of entrance)
  EXPECT_FALSE(heightMap.is_walkable(50, 60))
      << "Hill north slope (no entrance) should not be walkable";

  // East side (no entrance)
  EXPECT_FALSE(heightMap.is_walkable(60, 50))
      << "Hill east slope (no entrance) should not be walkable";

  // West side (no entrance)
  EXPECT_FALSE(heightMap.is_walkable(40, 50))
      << "Hill west slope (no entrance) should not be walkable";
}

TEST_F(TerrainHillTest, HillEntranceIsWalkable) {
  TerrainHeightMap heightMap(GRID_WIDTH, GRID_HEIGHT, TILE_SIZE);

  // Create a hill with an entrance
  std::vector<TerrainFeature> features;
  TerrainFeature hill;
  hill.type = TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.width = 20.0F;
  hill.depth = 20.0F;
  hill.height = 4.0F;
  hill.rotationDeg = 0.0F;

  // Entrance from south
  hill.entrances.push_back(QVector3D(50, 0.0F, 40));

  features.push_back(hill);
  heightMap.buildFromFeatures(features);

  // Check that the entrance point is walkable
  EXPECT_TRUE(heightMap.is_walkable(50, 40))
      << "Hill entrance should be walkable";

  // Check that the path from entrance to plateau is walkable
  int walkable_cells = 0;
  for (int z = 40; z <= 50; ++z) {
    if (heightMap.is_walkable(50, z)) {
      walkable_cells++;
    }
  }

  EXPECT_GT(walkable_cells, 5)
      << "Should have multiple walkable cells along entrance path";
}

TEST_F(TerrainHillTest, HillEntranceIsMarkedCorrectly) {
  TerrainHeightMap heightMap(GRID_WIDTH, GRID_HEIGHT, TILE_SIZE);

  // Create a hill with multiple entrances
  std::vector<TerrainFeature> features;
  TerrainFeature hill;
  hill.type = TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.width = 20.0F;
  hill.depth = 20.0F;
  hill.height = 4.0F;
  hill.rotationDeg = 0.0F;

  // Add multiple entrances
  hill.entrances.push_back(QVector3D(50, 0.0F, 40)); // South
  hill.entrances.push_back(QVector3D(40, 0.0F, 50)); // West
  hill.entrances.push_back(QVector3D(60, 0.0F, 50)); // East

  features.push_back(hill);
  heightMap.buildFromFeatures(features);

  // Check that entrance points are marked
  EXPECT_TRUE(heightMap.isHillEntrance(50, 40))
      << "South entrance should be marked";
  EXPECT_TRUE(heightMap.isHillEntrance(40, 50))
      << "West entrance should be marked";
  EXPECT_TRUE(heightMap.isHillEntrance(60, 50))
      << "East entrance should be marked";

  // Check that non-entrance points are not marked
  EXPECT_FALSE(heightMap.isHillEntrance(50, 60))
      << "North side (no entrance) should not be marked as entrance";
}

TEST_F(TerrainHillTest, HillSteepSlopeAreaIsNotWalkableEvenNearEntrance) {
  TerrainHeightMap heightMap(GRID_WIDTH, GRID_HEIGHT, TILE_SIZE);

  // Create a hill with one entrance
  std::vector<TerrainFeature> features;
  TerrainFeature hill;
  hill.type = TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.width = 20.0F;
  hill.depth = 20.0F;
  hill.height = 4.0F;
  hill.rotationDeg = 0.0F;

  // Only south entrance
  hill.entrances.push_back(QVector3D(50, 0.0F, 40));

  features.push_back(hill);
  heightMap.buildFromFeatures(features);

  // Check that steep areas perpendicular to entrance are not walkable
  // These are on the sides of the hill, not near the plateau
  EXPECT_FALSE(heightMap.is_walkable(45, 45))
      << "Steep slope to the side of entrance should not be walkable";
  EXPECT_FALSE(heightMap.is_walkable(55, 45))
      << "Steep slope to the side of entrance should not be walkable";
}

TEST_F(TerrainHillTest, MultipleEntrancesAllowMultiplePaths) {
  TerrainHeightMap heightMap(GRID_WIDTH, GRID_HEIGHT, TILE_SIZE);

  // Create a hill with three entrances
  std::vector<TerrainFeature> features;
  TerrainFeature hill;
  hill.type = TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.width = 20.0F;
  hill.depth = 20.0F;
  hill.height = 4.0F;
  hill.rotationDeg = 0.0F;

  hill.entrances.push_back(QVector3D(50, 0.0F, 40)); // South
  hill.entrances.push_back(QVector3D(40, 0.0F, 50)); // West
  hill.entrances.push_back(QVector3D(60, 0.0F, 50)); // East

  features.push_back(hill);
  heightMap.buildFromFeatures(features);

  // Verify each entrance path is walkable
  EXPECT_TRUE(heightMap.is_walkable(50, 40)) << "South entrance walkable";
  EXPECT_TRUE(heightMap.is_walkable(40, 50)) << "West entrance walkable";
  EXPECT_TRUE(heightMap.is_walkable(60, 50)) << "East entrance walkable";

  // But the side without entrance should still not be walkable
  EXPECT_FALSE(heightMap.is_walkable(50, 60))
      << "North side (no entrance) should not be walkable";
}
