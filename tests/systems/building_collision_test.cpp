#include <gtest/gtest.h>

#include "systems/building_collision_registry.h"
#include "systems/nav_grid.h"
#include "systems/pathfinding.h"

using namespace Game::Systems;

namespace {
constexpr int k_grid_size = 32;
}

class BuildingCollisionRegistryTest : public ::testing::Test {
protected:
  void SetUp() override {
    BuildingCollisionRegistry::instance().clear();
    NavGrid::initialize(k_grid_size, k_grid_size);
  }

  void TearDown() override { BuildingCollisionRegistry::instance().clear(); }
};

TEST_F(BuildingCollisionRegistryTest, PointInsideBuilding) {
  auto& registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  EXPECT_TRUE(registry.is_point_in_building(0.0F, 0.0F));
  EXPECT_TRUE(registry.is_point_in_building(1.0F, 1.0F));
  EXPECT_FALSE(registry.is_point_in_building(10.0F, 10.0F));
}

TEST_F(BuildingCollisionRegistryTest, PointOutsideBuilding) {
  auto& registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  EXPECT_FALSE(registry.is_point_in_building(5.0F, 0.0F));
  EXPECT_FALSE(registry.is_point_in_building(0.0F, 5.0F));
  EXPECT_FALSE(registry.is_point_in_building(-5.0F, 0.0F));
  EXPECT_FALSE(registry.is_point_in_building(0.0F, -5.0F));
}

TEST_F(BuildingCollisionRegistryTest, CircleOverlappingBuilding) {
  auto& registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  EXPECT_TRUE(registry.is_circle_overlapping_building(0.0F, 0.0F, 0.5F));

  EXPECT_TRUE(registry.is_circle_overlapping_building(1.0F, 1.0F, 0.5F));

  EXPECT_TRUE(registry.is_circle_overlapping_building(2.5F, 0.0F, 1.0F));
}

TEST_F(BuildingCollisionRegistryTest, CircleNotOverlappingBuilding) {
  auto& registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  EXPECT_FALSE(registry.is_circle_overlapping_building(10.0F, 0.0F, 0.5F));
  EXPECT_FALSE(registry.is_circle_overlapping_building(0.0F, 10.0F, 0.5F));

  EXPECT_FALSE(registry.is_circle_overlapping_building(5.0F, 0.0F, 0.5F));
}

TEST_F(BuildingCollisionRegistryTest, CircleTouchingBuildingEdge) {
  auto& registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  EXPECT_TRUE(registry.is_circle_overlapping_building(2.0F, 0.0F, 0.5F));

  EXPECT_TRUE(registry.is_circle_overlapping_building(3.0F, 0.0F, 1.0F));
}

TEST_F(BuildingCollisionRegistryTest, LargeUnitRadiusPreventedFromClipping) {
  auto& registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  float const large_radius = 2.0F;

  EXPECT_TRUE(registry.is_circle_overlapping_building(3.5F, 0.0F, large_radius));

  EXPECT_TRUE(registry.is_circle_overlapping_building(0.0F, 3.5F, large_radius));

  EXPECT_FALSE(registry.is_circle_overlapping_building(5.0F, 0.0F, large_radius));
}

TEST_F(BuildingCollisionRegistryTest, IgnoreEntityId) {
  auto& registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);
  registry.register_building(2, "barracks", 10.0F, 10.0F, 0);

  EXPECT_TRUE(registry.is_circle_overlapping_building(0.0F, 0.0F, 0.5F, 0));

  EXPECT_FALSE(registry.is_circle_overlapping_building(0.0F, 0.0F, 0.5F, 1));

  EXPECT_TRUE(registry.is_circle_overlapping_building(10.0F, 10.0F, 0.5F, 1));
}

TEST_F(BuildingCollisionRegistryTest, MultipleBuildings) {
  auto& registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);
  registry.register_building(2, "barracks", 10.0F, 0.0F, 0);

  EXPECT_TRUE(registry.is_circle_overlapping_building(0.0F, 0.0F, 0.5F));
  EXPECT_TRUE(registry.is_circle_overlapping_building(10.0F, 0.0F, 0.5F));

  EXPECT_FALSE(registry.is_circle_overlapping_building(5.0F, 0.0F, 0.5F));
}

TEST_F(BuildingCollisionRegistryTest, GridPaddingAccountsForUnitRadius) {
  auto& registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  EXPECT_GE(BuildingCollisionRegistry::get_grid_padding(), 1.0F);

  const auto& buildings = registry.get_all_buildings();
  ASSERT_EQ(buildings.size(), 1);

  auto* pathfinder = NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  const auto west = NavGrid::world_to_grid(-2.5F, 0.0F);
  const auto east = NavGrid::world_to_grid(2.5F, 0.0F);
  EXPECT_FALSE(pathfinder->is_walkable(west.x, west.y));
  EXPECT_FALSE(pathfinder->is_walkable(east.x, east.y));
}

TEST_F(BuildingCollisionRegistryTest, UnitWithLargeRadiusCloseToBuilding) {
  auto& registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  float const unit_radius = 1.0F;

  EXPECT_TRUE(registry.is_circle_overlapping_building(2.5F, 0.0F, unit_radius));

  EXPECT_TRUE(registry.is_circle_overlapping_building(3.0F, 0.0F, unit_radius));

  EXPECT_FALSE(registry.is_circle_overlapping_building(3.1F, 0.0F, unit_radius));
}

TEST_F(BuildingCollisionRegistryTest, HomeUsesScaledFootprint) {
  const auto size = BuildingCollisionRegistry::get_building_size("home");

  EXPECT_FLOAT_EQ(size.width, 4.3F);
  EXPECT_FLOAT_EQ(size.depth, 4.4F);
}

TEST_F(BuildingCollisionRegistryTest, FootprintsCoverTheDrawnMesh) {
  struct Drawn {
    const char* type;
    float width;
    float depth;
  };

  for (const auto& drawn : {Drawn{"home", 4.25F, 4.36F},
                            Drawn{"marketplace", 5.46F, 5.46F},
                            Drawn{"temple", 12.28F, 9.05F}}) {
    const auto size = BuildingCollisionRegistry::get_building_size(drawn.type);
    EXPECT_GE(size.width, drawn.width) << drawn.type;
    EXPECT_GE(size.depth, drawn.depth) << drawn.type;
  }
}

TEST_F(BuildingCollisionRegistryTest, AQuarterTurnSwapsFootprintAxes) {
  const auto size = BuildingCollisionRegistry::get_building_size("temple");
  const auto turned = BuildingCollisionRegistry::axis_aligned_size(size, 90.0F);

  EXPECT_NEAR(turned.width, size.depth, 1.0e-4F);
  EXPECT_NEAR(turned.depth, size.width, 1.0e-4F);
}

TEST_F(BuildingCollisionRegistryTest, ARotatedBuildingBlocksItsCorners) {
  const auto size = BuildingCollisionRegistry::get_building_size("temple");
  const auto leaning = BuildingCollisionRegistry::axis_aligned_size(size, 30.0F);

  EXPECT_GT(leaning.width, size.width * 0.5F);
  EXPECT_GT(leaning.depth, size.depth);
}

TEST_F(BuildingCollisionRegistryTest, ARotatedTempleBlocksWhereItIsDrawn) {
  auto& registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "temple", 0.0F, 0.0F, 0, 90.0F);

  EXPECT_TRUE(registry.is_circle_overlapping_building(0.0F, 5.8F, 0.1F));
  EXPECT_FALSE(registry.is_circle_overlapping_building(0.0F, 6.4F, 0.1F));
}
