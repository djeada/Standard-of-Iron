#include "systems/building_collision_registry.h"
#include <gtest/gtest.h>

using namespace Game::Systems;

class BuildingCollisionRegistryTest : public ::testing::Test {
protected:
  void SetUp() override { BuildingCollisionRegistry::instance().clear(); }

  void TearDown() override { BuildingCollisionRegistry::instance().clear(); }
};

TEST_F(BuildingCollisionRegistryTest, PointInsideBuilding) {
  auto &registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  EXPECT_TRUE(registry.is_point_in_building(0.0F, 0.0F));
  EXPECT_TRUE(registry.is_point_in_building(1.0F, 1.0F));
  EXPECT_FALSE(registry.is_point_in_building(10.0F, 10.0F));
}

TEST_F(BuildingCollisionRegistryTest, PointOutsideBuilding) {
  auto &registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  EXPECT_FALSE(registry.is_point_in_building(5.0F, 0.0F));
  EXPECT_FALSE(registry.is_point_in_building(0.0F, 5.0F));
  EXPECT_FALSE(registry.is_point_in_building(-5.0F, 0.0F));
  EXPECT_FALSE(registry.is_point_in_building(0.0F, -5.0F));
}

TEST_F(BuildingCollisionRegistryTest, CircleOverlappingBuilding) {
  auto &registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  EXPECT_TRUE(registry.is_circle_overlapping_building(0.0F, 0.0F, 0.5F));

  EXPECT_TRUE(registry.is_circle_overlapping_building(1.0F, 1.0F, 0.5F));

  EXPECT_TRUE(registry.is_circle_overlapping_building(2.5F, 0.0F, 1.0F));
}

TEST_F(BuildingCollisionRegistryTest, CircleNotOverlappingBuilding) {
  auto &registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  EXPECT_FALSE(registry.is_circle_overlapping_building(10.0F, 0.0F, 0.5F));
  EXPECT_FALSE(registry.is_circle_overlapping_building(0.0F, 10.0F, 0.5F));

  EXPECT_FALSE(registry.is_circle_overlapping_building(5.0F, 0.0F, 0.5F));
}

TEST_F(BuildingCollisionRegistryTest, CircleTouchingBuildingEdge) {
  auto &registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  EXPECT_TRUE(registry.is_circle_overlapping_building(2.0F, 0.0F, 0.5F));

  EXPECT_TRUE(registry.is_circle_overlapping_building(3.0F, 0.0F, 1.0F));
}

TEST_F(BuildingCollisionRegistryTest, LargeUnitRadiusPreventedFromClipping) {
  auto &registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  float const large_radius = 2.0F;

  EXPECT_TRUE(
      registry.is_circle_overlapping_building(3.5F, 0.0F, large_radius));

  EXPECT_TRUE(
      registry.is_circle_overlapping_building(0.0F, 3.5F, large_radius));

  EXPECT_FALSE(
      registry.is_circle_overlapping_building(5.0F, 0.0F, large_radius));
}

TEST_F(BuildingCollisionRegistryTest, IgnoreEntityId) {
  auto &registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);
  registry.register_building(2, "barracks", 10.0F, 10.0F, 0);

  EXPECT_TRUE(registry.is_circle_overlapping_building(0.0F, 0.0F, 0.5F, 0));

  EXPECT_FALSE(registry.is_circle_overlapping_building(0.0F, 0.0F, 0.5F, 1));

  EXPECT_TRUE(registry.is_circle_overlapping_building(10.0F, 10.0F, 0.5F, 1));
}

TEST_F(BuildingCollisionRegistryTest, MultipleBuildings) {
  auto &registry = BuildingCollisionRegistry::instance();

  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);
  registry.register_building(2, "barracks", 10.0F, 0.0F, 0);

  EXPECT_TRUE(registry.is_circle_overlapping_building(0.0F, 0.0F, 0.5F));
  EXPECT_TRUE(registry.is_circle_overlapping_building(10.0F, 0.0F, 0.5F));

  EXPECT_FALSE(registry.is_circle_overlapping_building(5.0F, 0.0F, 0.5F));
}
