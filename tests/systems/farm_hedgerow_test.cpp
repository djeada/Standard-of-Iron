#include <gtest/gtest.h>

#include "game/visuals/farm_hedgerow.h"

namespace {

using Game::Visuals::farm_hedge_plant_count;
using Game::Visuals::farm_hedge_plants;

TEST(FarmHedgerowTest, TheHedgeLengthensWithEveryHarvestUpToItsFullRun) {
  EXPECT_EQ(farm_hedge_plant_count(0), Game::Visuals::k_hedge_first_plants);
  EXPECT_EQ(farm_hedge_plant_count(-3), Game::Visuals::k_hedge_first_plants);
  for (int harvests = 0; harvests < Game::Visuals::k_hedge_harvests_to_full;
       ++harvests) {
    EXPECT_EQ(farm_hedge_plant_count(harvests + 1),
              farm_hedge_plant_count(harvests) + 1);
  }
  EXPECT_EQ(farm_hedge_plant_count(Game::Visuals::k_hedge_harvests_to_full),
            Game::Visuals::k_hedge_max_plants);
  EXPECT_EQ(farm_hedge_plant_count(500), Game::Visuals::k_hedge_max_plants);
}

TEST(FarmHedgerowTest, OlderPlantsStayPutWhenTheHedgeGrows) {
  const auto young = farm_hedge_plants(1, 77U);
  const auto old = farm_hedge_plants(6, 77U);
  ASSERT_LT(young.size(), old.size());
  for (std::size_t i = 0; i < young.size(); ++i) {
    EXPECT_EQ(young[i], old[i]) << "a harvest must add plants, not move the hedge";
  }
}

TEST(FarmHedgerowTest, PlantsStandOnTheBoundaryNotInTheCropOrTheGateway) {
  for (std::uint32_t seed : {1U, 77U, 4242U, 0xFFFFFFFFU}) {
    for (const auto& plant : farm_hedge_plants(100, seed)) {
      const bool on_back = plant.z < -6.5F;
      const bool on_side = plant.x < -6.5F;
      EXPECT_TRUE(on_back || on_side) << plant.x << ", " << plant.z;
      EXPECT_LT(plant.z, 1.0F) << "the front of the farm stays open";
      EXPECT_GT(plant.x, -7.6F);
      EXPECT_GT(plant.z, -7.6F);
      EXPECT_GT(plant.scale, 0.30F);
      EXPECT_LT(plant.scale, 0.50F) << "a hedge, not a wood";
    }
  }
}

TEST(FarmHedgerowTest, TwoFarmsDoNotGrowTheSameHedge) {
  EXPECT_NE(farm_hedge_plants(6, 1U), farm_hedge_plants(6, 2U));
}

} // namespace
