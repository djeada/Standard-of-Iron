#include <QVector3D>

#include <algorithm>
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

auto grid_of(float world) -> int {
  return static_cast<int>(std::round(world / k_tile + (k_cells * 0.5F - 0.5F)));
}

auto river_for(float river_width) -> Game::Map::RiverSegment {
  return {{-140.0F, 0.0F, 0.0F}, {140.0F, 0.0F, 0.0F}, river_width};
}

} // namespace

TEST(RiverBankWalkabilityTest, NoWalkableGroundLiesUnderTheDrawnRiver) {
  for (const float river_width : {4.2F, 7.0F, 19.6F, 26.0F}) {
    const auto terrain = build(river_width, 26.0F);
    const auto river = river_for(river_width);

    int walkable_under_water = 0;
    for (int x = 0; x < k_cells; ++x) {
      const float t =
          (world_of(x) - river.start.x()) / (river.end.x() - river.start.x());
      const auto section = Game::Map::river_drawn_cross_section(river, t);
      for (int z = 0; z < k_cells; ++z) {
        if (std::abs(world_of(z) - section.center.z()) > section.half_width) {
          continue;
        }
        if (terrain.isBridgeCell(x, z)) {
          continue;
        }
        walkable_under_water += terrain.is_walkable(x, z) ? 1 : 0;
      }
    }
    EXPECT_EQ(walkable_under_water, 0) << "river width " << river_width;
  }
}

TEST(RiverBankWalkabilityTest, BankBesideTheDrawnWaterIsWalkable) {
  for (const float river_width : {7.0F, 19.6F, 26.0F}) {
    const auto terrain = build(river_width, 8.0F);
    const auto river = river_for(river_width);

    int blocked_dry_bank = 0;
    for (int x = grid_of(-90.0F); x <= grid_of(90.0F); ++x) {
      const float t =
          (world_of(x) - river.start.x()) / (river.end.x() - river.start.x());
      const auto section = Game::Map::river_drawn_cross_section(river, t);
      const float reach = std::max(section.half_width, river_width * 0.5F) +
                          Game::Map::k_water_bank_clearance + k_tile * 1.5F;
      for (const float side : {-1.0F, 1.0F}) {
        const int z = grid_of(section.center.z() + side * reach);
        if (!terrain.isBridgeCell(x, z) && !terrain.is_walkable(x, z)) {
          ++blocked_dry_bank;
        }
      }
    }
    EXPECT_EQ(blocked_dry_bank, 0) << "river width " << river_width;
  }
}

TEST(RiverBankWalkabilityTest, BridgeDecksLandOnDryWalkableGround) {
  for (const float river_width : {4.2F, 7.0F, 19.6F, 26.0F}) {
    const auto terrain = build(river_width, 26.0F);
    const auto& bridge = terrain.get_bridges().front();
    const auto section =
        Game::Map::river_drawn_cross_section(river_for(river_width), 0.5F);

    EXPECT_LT(bridge.start.z(), section.center.z() - section.half_width)
        << "river width " << river_width;
    EXPECT_GT(bridge.end.z(), section.center.z() + section.half_width)
        << "river width " << river_width;

    const int grid_x = grid_of(0.0F);
    for (const float deck_end : {bridge.start.z(), bridge.end.z()}) {
      EXPECT_TRUE(terrain.is_walkable(grid_x, grid_of(deck_end)))
          << "river width " << river_width << " deck end " << deck_end;
    }
    // Past the deck the ground itself must carry the column on.
    for (const float step_off :
         {bridge.start.z() - 2.0F * k_tile, bridge.end.z() + 2.0F * k_tile}) {
      const int grid_z = grid_of(step_off);
      EXPECT_FALSE(terrain.isBridgeCell(grid_x, grid_z))
          << "river width " << river_width << " step-off " << step_off;
      EXPECT_TRUE(terrain.is_walkable(grid_x, grid_z))
          << "river width " << river_width << " step-off " << step_off;
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
