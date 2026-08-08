#include <gtest/gtest.h>

#include "systems/building_collision_registry.h"
#include "systems/command_service.h"
#include "systems/pathfinding.h"

using namespace Game::Systems;

namespace {
constexpr int k_grid_size = 32;
}

class BuildingCollisionRegistryTest : public ::testing::Test {
protected:
  void SetUp() override {
    BuildingCollisionRegistry::instance().clear();
    CommandService::initialize(k_grid_size, k_grid_size);
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

  auto* pathfinder = CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  const auto west = CommandService::world_to_grid(-2.5F, 0.0F);
  const auto east = CommandService::world_to_grid(2.5F, 0.0F);
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

  EXPECT_FLOAT_EQ(size.width, 3.0F);
  EXPECT_FLOAT_EQ(size.depth, 3.0F);
}
