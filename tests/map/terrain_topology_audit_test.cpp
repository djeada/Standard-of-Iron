#include <gtest/gtest.h>

#include "game/map/terrain_topology_audit.h"

TEST(TerrainTopologyAuditTest, AcceptsConnectedRoutesBoundaryWaterAndDefendedHill) {
  Game::Map::MapDefinition map;
  map.grid = {.width = 101, .height = 101, .tile_size = 1.0F};
  map.roads = {{{-50, 0, 0}, {0, 0, 0}, 3.0F}, {{0, 0, 0}, {50, 0, 20}, 3.0F}};
  map.rivers = {{{-50, 0, -20}, {0, 0, -18}, 4.0F}, {{0, 0, -18}, {50, 0, -10}, 4.0F}};
  Game::Map::TerrainFeature hill;
  hill.type = Game::Map::TerrainType::Hill;
  hill.width = 24.0F;
  hill.depth = 18.0F;
  hill.entrances = {{-12, 0, 0}, {12, 0, 0}};
  map.terrain.push_back(hill);

  const auto audit = Game::Map::audit_terrain_topology(map);
  EXPECT_TRUE(audit.passed()) << audit.issues.join("; ").toStdString();
  EXPECT_EQ(audit.road_components, 1);
  EXPECT_EQ(audit.invalid_river_endpoints, 0);
  EXPECT_EQ(audit.hills_without_two_approaches, 0);
}

TEST(TerrainTopologyAuditTest, RejectsPatchworkRoutesAndInteriorWaterEnds) {
  Game::Map::MapDefinition map;
  map.grid = {.width = 101, .height = 101, .tile_size = 1.0F};
  map.roads = {{{-40, 0, -20}, {-20, 0, -20}, 3.0F}, {{20, 0, 20}, {40, 0, 20}, 3.0F}};
  map.rivers = {{{-10, 0, 0}, {10, 0, 0}, 4.0F}};

  const auto audit = Game::Map::audit_terrain_topology(map);
  EXPECT_FALSE(audit.passed());
  EXPECT_EQ(audit.road_components, 2);
  EXPECT_EQ(audit.invalid_river_endpoints, 2);
}

TEST(TerrainTopologyAuditTest, AcceptsLakeShoreEndpointButRejectsSubmergedEndpoint) {
  Game::Map::MapDefinition map;
  map.grid = {.width = 101, .height = 101, .tile_size = 1.0F};
  map.lakes = {{{0, 0, 0}, 30.0F, 20.0F}};
  map.rivers = {{{-50, 0, 0}, {-15, 0, 0}, 3.0F}};

  auto audit = Game::Map::audit_terrain_topology(map);
  EXPECT_EQ(audit.invalid_river_endpoints, 0);

  map.rivers[0].end = {0, 0, 0};
  audit = Game::Map::audit_terrain_topology(map);
  EXPECT_EQ(audit.invalid_river_endpoints, 1);
}

TEST(TerrainTopologyAuditTest, AcceptsTributaryEndingAtReceivingRiverbank) {
  Game::Map::MapDefinition map;
  map.grid = {.width = 101, .height = 101, .tile_size = 1.0F};
  map.rivers = {
      {{-50, 0, 0}, {50, 0, 0}, 10.0F},
      {{0, 0, 50}, {0, 0, 5}, 4.0F},
  };

  const auto audit = Game::Map::audit_terrain_topology(map);
  EXPECT_EQ(audit.river_components, 1);
  EXPECT_EQ(audit.invalid_river_endpoints, 0);
}
