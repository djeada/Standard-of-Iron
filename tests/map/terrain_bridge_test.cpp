#include "map/terrain.h"
#include <QVector3D>
#include <gtest/gtest.h>

using namespace Game::Map;

class TerrainBridgeTest : public ::testing::Test {
protected:
  static constexpr int GRID_WIDTH = 50;
  static constexpr int GRID_HEIGHT = 50;
  static constexpr float TILE_SIZE = 1.0F;
};

TEST_F(TerrainBridgeTest, BridgeCreatesWalkablePathAcrossRiver) {
  TerrainHeightMap heightMap(GRID_WIDTH, GRID_HEIGHT, TILE_SIZE);

  // Create a horizontal river across the middle of the map
  std::vector<RiverSegment> rivers;
  RiverSegment river;
  river.start = QVector3D(-20.0F, 0.0F, 0.0F);
  river.end = QVector3D(20.0F, 0.0F, 0.0F);
  river.width = 4.0F;
  rivers.push_back(river);

  heightMap.addRiverSegments(rivers);

  // Verify river cells are not walkable (before bridge)
  EXPECT_FALSE(heightMap.is_walkable(25, 25)); // Middle of map, should be river

  // Create a bridge crossing the river
  std::vector<Bridge> bridges;
  Bridge bridge;
  bridge.start = QVector3D(0.0F, 0.5F, -5.0F);
  bridge.end = QVector3D(0.0F, 0.5F, 5.0F);
  bridge.width = 3.0F;
  bridge.height = 0.5F;
  bridges.push_back(bridge);

  heightMap.addBridges(bridges);

  // Verify the bridge cells are now walkable
  // The bridge should be at grid coordinates around (25, 20) to (25, 30)
  // depending on the exact river placement
  int walkable_bridge_cells = 0;
  for (int z = 20; z <= 30; ++z) {
    if (heightMap.is_walkable(25, z)) {
      walkable_bridge_cells++;
    }
  }

  // With the connectivity margin, we should have multiple walkable cells
  // on the bridge path
  EXPECT_GT(walkable_bridge_cells, 5)
      << "Bridge should create a walkable path with connectivity margin";
}

TEST_F(TerrainBridgeTest, BridgeHasConnectivityMargin) {
  TerrainHeightMap heightMap(GRID_WIDTH, GRID_HEIGHT, TILE_SIZE);

  // Create a narrow river
  std::vector<RiverSegment> rivers;
  RiverSegment river;
  river.start = QVector3D(-20.0F, 0.0F, 0.0F);
  river.end = QVector3D(20.0F, 0.0F, 0.0F);
  river.width = 2.0F;
  rivers.push_back(river);

  heightMap.addRiverSegments(rivers);

  // Create a bridge
  std::vector<Bridge> bridges;
  Bridge bridge;
  bridge.start = QVector3D(0.0F, 0.5F, -3.0F);
  bridge.end = QVector3D(0.0F, 0.5F, 3.0F);
  bridge.width = 2.0F;
  bridge.height = 0.5F;
  bridges.push_back(bridge);

  heightMap.addBridges(bridges);

  // Count walkable cells near the bridge center
  int walkable_count = 0;
  int checked_count = 0;
  for (int z = 22; z <= 28; ++z) {
    for (int x = 24; x <= 26; ++x) {
      checked_count++;
      if (heightMap.is_walkable(x, z)) {
        walkable_count++;
      }
    }
  }

  // With the connectivity margin (0.5 grid cells), we should have more
  // walkable cells than just the exact bridge width
  float walkable_ratio =
      static_cast<float>(walkable_count) / static_cast<float>(checked_count);
  EXPECT_GT(walkable_ratio, 0.3F)
      << "Bridge should have connectivity margin for pathfinding";
}

TEST_F(TerrainBridgeTest, BridgeConvertsRiverToFlatTerrain) {
  TerrainHeightMap heightMap(GRID_WIDTH, GRID_HEIGHT, TILE_SIZE);

  // Create a river
  std::vector<RiverSegment> rivers;
  RiverSegment river;
  river.start = QVector3D(-10.0F, 0.0F, 0.0F);
  river.end = QVector3D(10.0F, 0.0F, 0.0F);
  river.width = 3.0F;
  rivers.push_back(river);

  heightMap.addRiverSegments(rivers);

  // Verify initial terrain type is River
  EXPECT_EQ(heightMap.getTerrainType(25, 25), TerrainType::River);

  // Add bridge
  std::vector<Bridge> bridges;
  Bridge bridge;
  bridge.start = QVector3D(-1.0F, 0.5F, 0.0F);
  bridge.end = QVector3D(1.0F, 0.5F, 0.0F);
  bridge.width = 3.0F;
  bridge.height = 0.5F;
  bridges.push_back(bridge);

  heightMap.addBridges(bridges);

  // Check that cells on the bridge are now Flat terrain
  bool has_flat_terrain = false;
  for (int x = 24; x <= 26; ++x) {
    if (heightMap.getTerrainType(x, 25) == TerrainType::Flat) {
      has_flat_terrain = true;
      break;
    }
  }

  EXPECT_TRUE(has_flat_terrain)
      << "Bridge should convert river cells to flat terrain";
}

TEST_F(TerrainBridgeTest, IsOnBridgeDetectsUnitsOnBridge) {
  TerrainHeightMap heightMap(GRID_WIDTH, GRID_HEIGHT, TILE_SIZE);

  // Create a bridge
  std::vector<Bridge> bridges;
  Bridge bridge;
  bridge.start = QVector3D(-5.0F, 0.5F, 0.0F);
  bridge.end = QVector3D(5.0F, 0.5F, 0.0F);
  bridge.width = 3.0F;
  bridge.height = 0.5F;
  bridges.push_back(bridge);

  heightMap.addBridges(bridges);

  // Test position on bridge center
  EXPECT_TRUE(heightMap.isOnBridge(0.0F, 0.0F))
      << "Position at bridge center should be detected";

  // Test position on bridge edge (within width)
  EXPECT_TRUE(heightMap.isOnBridge(0.0F, 1.4F))
      << "Position within bridge width should be detected";

  // Test position outside bridge width
  EXPECT_FALSE(heightMap.isOnBridge(0.0F, 2.5F))
      << "Position outside bridge width should not be detected";

  // Test position outside bridge length
  EXPECT_FALSE(heightMap.isOnBridge(10.0F, 0.0F))
      << "Position outside bridge length should not be detected";

  // Test position slightly off bridge (within tolerance)
  EXPECT_TRUE(heightMap.isOnBridge(0.0F, 1.8F))
      << "Position within tolerance margin should be detected";
}

TEST_F(TerrainBridgeTest, GetBridgeCenterPositionReturnsCenterPoint) {
  TerrainHeightMap heightMap(GRID_WIDTH, GRID_HEIGHT, TILE_SIZE);

  // Create a horizontal bridge
  std::vector<Bridge> bridges;
  Bridge bridge;
  bridge.start = QVector3D(-5.0F, 0.5F, 0.0F);
  bridge.end = QVector3D(5.0F, 0.5F, 0.0F);
  bridge.width = 3.0F;
  bridge.height = 0.5F;
  bridges.push_back(bridge);

  heightMap.addBridges(bridges);

  // Test getting center for a position on the side of the bridge
  auto center = heightMap.getBridgeCenterPosition(2.0F, 1.0F);
  ASSERT_TRUE(center.has_value())
      << "Should return center position for point on bridge";

  // The center should be on the bridge axis (z = 0)
  EXPECT_NEAR(center->z(), 0.0F, 0.01F) << "Center z should be on bridge axis";
  EXPECT_NEAR(center->x(), 2.0F, 0.01F)
      << "Center x should match query position along bridge";

  // Test position outside bridge
  auto outside = heightMap.getBridgeCenterPosition(10.0F, 5.0F);
  EXPECT_FALSE(outside.has_value())
      << "Should return nullopt for position outside bridge";
}

TEST_F(TerrainBridgeTest, GetBridgeCenterPositionWorksForDiagonalBridge) {
  TerrainHeightMap heightMap(GRID_WIDTH, GRID_HEIGHT, TILE_SIZE);

  // Create a diagonal bridge
  std::vector<Bridge> bridges;
  Bridge bridge;
  bridge.start = QVector3D(0.0F, 0.5F, 0.0F);
  bridge.end = QVector3D(10.0F, 0.5F, 10.0F);
  bridge.width = 3.0F;
  bridge.height = 0.5F;
  bridges.push_back(bridge);

  heightMap.addBridges(bridges);

  // Test getting center for a position on the side of the diagonal bridge
  auto center = heightMap.getBridgeCenterPosition(5.0F, 6.0F);
  ASSERT_TRUE(center.has_value())
      << "Should return center position for point on diagonal bridge";

  // The center should be on the bridge axis (diagonal line)
  // For a 45-degree diagonal bridge, x and z should be equal along the axis
  EXPECT_NEAR(center->x(), center->z(), 0.5F)
      << "Center should be on diagonal bridge axis";
}
