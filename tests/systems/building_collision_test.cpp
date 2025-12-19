#include "systems/building_collision_registry.h"
#include <gtest/gtest.h>

using namespace Game::Systems;

class BuildingCollisionRegistryTest : public ::testing::Test {
protected:
  void SetUp() override {
    BuildingCollisionRegistry::instance().clear();
  }

  void TearDown() override {
    BuildingCollisionRegistry::instance().clear();
  }
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

TEST_F(BuildingCollisionRegistryTest, SmallUnitShouldNotClipIntoBuilding) {
  auto &registry = BuildingCollisionRegistry::instance();
  
  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  float const small_radius = 1.0F;
  float const building_half_width = 2.0F;
  
  EXPECT_TRUE(registry.is_circle_overlapping_building(
      building_half_width + small_radius - 0.1F, 0.0F, small_radius));
  
  EXPECT_FALSE(registry.is_circle_overlapping_building(
      building_half_width + small_radius + 0.1F, 0.0F, small_radius));
}

TEST_F(BuildingCollisionRegistryTest, MediumUnitShouldNotClipIntoBuilding) {
  auto &registry = BuildingCollisionRegistry::instance();
  
  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  float const medium_radius = 1.5F;
  float const building_half_width = 2.0F;
  
  EXPECT_TRUE(registry.is_circle_overlapping_building(
      building_half_width + medium_radius - 0.1F, 0.0F, medium_radius));
  
  EXPECT_FALSE(registry.is_circle_overlapping_building(
      building_half_width + medium_radius + 0.1F, 0.0F, medium_radius));
}

TEST_F(BuildingCollisionRegistryTest, LargeUnitShouldNotClipIntoBuilding) {
  auto &registry = BuildingCollisionRegistry::instance();
  
  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  float const large_radius = 2.0F;
  float const building_half_width = 2.0F;
  
  EXPECT_TRUE(registry.is_circle_overlapping_building(
      building_half_width + large_radius - 0.1F, 0.0F, large_radius));
  
  EXPECT_FALSE(registry.is_circle_overlapping_building(
      building_half_width + large_radius + 0.1F, 0.0F, large_radius));
}

TEST_F(BuildingCollisionRegistryTest, UnitAdjacentToBuildingEdge) {
  auto &registry = BuildingCollisionRegistry::instance();
  
  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  float const unit_radius = 1.2F;
  float const building_half_width = 2.0F;
  float const safe_distance = building_half_width + unit_radius;
  
  EXPECT_FALSE(registry.is_circle_overlapping_building(
      safe_distance, 0.0F, unit_radius));
  EXPECT_FALSE(registry.is_circle_overlapping_building(
      0.0F, safe_distance, unit_radius));
  EXPECT_FALSE(registry.is_circle_overlapping_building(
      -safe_distance, 0.0F, unit_radius));
  EXPECT_FALSE(registry.is_circle_overlapping_building(
      0.0F, -safe_distance, unit_radius));
}

TEST_F(BuildingCollisionRegistryTest, AllCommonUnitSizesRespected) {
  auto &registry = BuildingCollisionRegistry::instance();
  
  registry.register_building(1, "barracks", 0.0F, 0.0F, 0);

  float const building_half_width = 2.0F;
  
  std::vector<float> const common_ring_sizes = {
      1.0F, 1.1F, 1.2F, 1.4F, 1.8F, 1.9F, 2.0F};
  
  for (float const ring_size : common_ring_sizes) {
    float const safe_distance = building_half_width + ring_size;
    
    EXPECT_FALSE(registry.is_circle_overlapping_building(
        safe_distance, 0.0F, ring_size))
        << "Ring size " << ring_size
        << " should not overlap at safe distance";
    
    EXPECT_TRUE(registry.is_circle_overlapping_building(
        safe_distance - 0.5F, 0.0F, ring_size))
        << "Ring size " << ring_size
        << " should overlap when too close";
  }
}
