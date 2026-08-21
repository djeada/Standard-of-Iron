#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>

#include "game/map/terrain.h"

namespace {

constexpr int k_cells = 192;
constexpr float k_tile = 1.0F;

auto build(float river_width, float bridge_width) -> Game::Map::TerrainHeightMap {
  Game::Map::TerrainHeightMap terrain(k_cells, k_cells, k_tile);
  terrain.build_from_features({});
  terrain.add_river_segments({Game::Map::RiverSegment{
      {-140.0F, 0.0F, 0.0F}, {140.0F, 0.0F, 0.0F}, river_width}});
  terrain.add_bridges({Game::Map::Bridge{
      {0.0F, 0.0F, -16.0F}, {0.0F, 0.0F, 16.0F}, bridge_width, 0.5F}});
  return terrain;
}

auto world_of(int grid) -> float {
  return (static_cast<float>(grid) - (k_cells * 0.5F - 0.5F)) * k_tile;
}

} // namespace

TEST(RiverBankWalkabilityTest, NoWalkableGroundLiesUnderTheDrawnRiver) {
  for (const float river_width : {4.2F, 7.0F, 19.6F, 26.0F}) {
    const auto terrain = build(river_width, 26.0F);
    const float drawn_half = Game::Map::river_drawn_half_width(river_width);

    int walkable_under_water = 0;
    for (int z = 0; z < k_cells; ++z) {
      if (std::abs(world_of(z)) > drawn_half) {
        continue;
      }
      for (int x = 0; x < k_cells; ++x) {
        if (terrain.isBridgeCell(x, z)) {
          continue;
        }
        walkable_under_water += terrain.is_walkable(x, z) ? 1 : 0;
      }
    }
    EXPECT_EQ(walkable_under_water, 0) << "river width " << river_width;
  }
}

TEST(RiverBankWalkabilityTest, BridgeDecksLandOnDryWalkableGround) {
  for (const float river_width : {4.2F, 7.0F, 19.6F, 26.0F}) {
    const auto terrain = build(river_width, 26.0F);
    const auto& bridge = terrain.get_bridges().front();
    const float blocked_half = Game::Map::river_bank_standing_half_width(river_width);

    EXPECT_GT(std::abs(bridge.start.z()), blocked_half)
        << "river width " << river_width;
    EXPECT_GT(std::abs(bridge.end.z()), blocked_half) << "river width " << river_width;

    for (const float deck_end : {bridge.start.z(), bridge.end.z()}) {
      const int grid_z =
          static_cast<int>(std::round(deck_end + (k_cells * 0.5F - 0.5F)));
      const int grid_x = static_cast<int>(std::round(k_cells * 0.5F - 0.5F));
      EXPECT_TRUE(terrain.is_walkable(grid_x, grid_z))
          << "river width " << river_width << " deck end " << deck_end;
    }
  }
}

TEST(RiverBankWalkabilityTest, DeckWalkableWidthIsInsetFromTheParapet) {
  const auto terrain = build(26.0F, 26.0F);
  const int centre_z = static_cast<int>(std::round(k_cells * 0.5F - 0.5F));

  int walkable_span = 0;
  int deck_span = 0;
  for (int x = 0; x < k_cells; ++x) {
    deck_span += terrain.isBridgeCell(x, centre_z) ? 1 : 0;
    walkable_span += terrain.is_walkable(x, centre_z) ? 1 : 0;
  }

  EXPECT_GT(deck_span, walkable_span);
  EXPECT_GT(walkable_span, 0);
  EXPECT_NEAR(static_cast<float>(walkable_span),
              Game::Map::bridge_walkable_half_width(26.0F) * 2.0F,
              2.0F);
}
