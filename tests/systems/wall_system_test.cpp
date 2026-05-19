#include <gtest/gtest.h>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/ai_system/ai_strategy.h"
#include "systems/ai_system/ai_types.h"
#include "systems/building_collision_registry.h"
#include "systems/wall_system.h"
#include "units/spawn_type.h"

using namespace Engine::Core;
using namespace Game::Systems;

class WallSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    BuildingCollisionRegistry::instance().clear();
  }

  void TearDown() override {
    BuildingCollisionRegistry::instance().clear();
    world.reset();
  }

  auto make_wall(float x, float y, float z, int owner_id, int health = 800)
      -> Entity* {
    auto* wall = world->create_entity();
    wall->add_component<TransformComponent>(x, y, z);
    auto* unit = wall->add_component<UnitComponent>(health, 800, 0.0F, 0.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::WallSegment;
    wall->add_component<BuildingComponent>();
    BuildingCollisionRegistry::instance().register_building(
        wall->get_id(), "wall_segment", x, z, owner_id);
    return wall;
  }

  std::unique_ptr<World> world;
  WallSystem wall_system;
};

TEST_F(WallSystemTest, HealthyWallIsNotRemoved) {
  auto* wall = make_wall(0.0F, 0.0F, 0.0F, 1);

  wall_system.update(world.get(), 0.1F);

  EXPECT_FALSE(wall->has_component<PendingRemovalComponent>());
}

TEST_F(WallSystemTest, DestroyedWallIsMarkedForRemoval) {
  auto* wall = make_wall(0.0F, 0.0F, 0.0F, 1, 0);

  wall_system.update(world.get(), 0.1F);

  EXPECT_TRUE(wall->has_component<PendingRemovalComponent>());
}

TEST_F(WallSystemTest, DestroyedWallIsUnregisteredFromCollision) {
  make_wall(5.0F, 0.0F, 0.0F, 1, 0);

  EXPECT_TRUE(
      BuildingCollisionRegistry::instance().is_point_in_building(5.0F, 0.0F));

  wall_system.update(world.get(), 0.1F);

  EXPECT_FALSE(
      BuildingCollisionRegistry::instance().is_point_in_building(5.0F, 0.0F));
}

TEST_F(WallSystemTest, DamagedButAliveWallIsNotRemoved) {
  auto* wall = make_wall(0.0F, 0.0F, 0.0F, 1, 400);

  wall_system.update(world.get(), 0.1F);

  EXPECT_FALSE(wall->has_component<PendingRemovalComponent>());
}

TEST_F(WallSystemTest, OnlyWallSegmentsAreProcessed) {
  auto* tower = world->create_entity();
  tower->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* unit = tower->add_component<UnitComponent>(0, 2000, 0.0F, 20.0F);
  unit->owner_id = 1;
  unit->spawn_type = Game::Units::SpawnType::DefenseTower;
  tower->add_component<BuildingComponent>();

  wall_system.update(world.get(), 0.1F);

  EXPECT_FALSE(tower->has_component<PendingRemovalComponent>());
}

TEST_F(WallSystemTest, WallSegmentBlocksPoint) {
  make_wall(3.0F, 0.0F, 0.0F, 1);

  EXPECT_TRUE(
      BuildingCollisionRegistry::instance().is_point_in_building(3.0F, 0.0F));
  EXPECT_FALSE(
      BuildingCollisionRegistry::instance().is_point_in_building(10.0F, 0.0F));
}

// ---- Builder production dispatch ----

TEST(WallBuilderProductionTest, WallSegmentSpawnTypeExists) {
  // Verify WallSegment is a distinct spawn type (compile-time check via cast)
  constexpr auto wall_type = Game::Units::SpawnType::WallSegment;
  EXPECT_NE(wall_type, Game::Units::SpawnType::DefenseTower);
  EXPECT_NE(wall_type, Game::Units::SpawnType::Barracks);
  EXPECT_NE(wall_type, Game::Units::SpawnType::Home);
}

TEST(WallBuilderProductionTest, BuilderProductionComponentProductTypeStorable) {
  BuilderProductionComponent comp;
  comp.product_type = "wall_segment";
  EXPECT_EQ(comp.product_type, "wall_segment");
}

// ---- AI strategy wall counts ----

TEST(AIWallStrategyTest, DefensiveStrategyWantsWalls) {
  auto config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Defensive);
  EXPECT_GT(config.desired_wall_segment_count, 0);
}

TEST(AIWallStrategyTest, RusherStrategyWantsNoWalls) {
  auto config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Rusher);
  EXPECT_EQ(config.desired_wall_segment_count, 0);
}

TEST(AIWallStrategyTest, AggressiveStrategyWantsNoWalls) {
  auto config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Aggressive);
  EXPECT_EQ(config.desired_wall_segment_count, 0);
}

TEST(AIWallStrategyTest, BalancedStrategyWantsAtLeastOneWall) {
  auto config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Balanced);
  EXPECT_GE(config.desired_wall_segment_count, 1);
}

TEST(AIWallStrategyTest, HighDefensePersonalityBoostsWallCount) {
  auto config = Game::Systems::AI::AIStrategyFactory::create_config(
      Game::Systems::AI::AIStrategy::Balanced);
  const int baseline = config.desired_wall_segment_count;
  Game::Systems::AI::AIStrategyFactory::apply_personality(config, 0.5F, 0.8F, 0.5F);
  EXPECT_GE(config.desired_wall_segment_count, baseline);
}

// ---- AIContext wall_segment_count field ----

TEST(AIWallContextTest, AIContextHasWallSegmentCount) {
  Game::Systems::AI::AIContext ctx;
  ctx.wall_segment_count = 5;
  EXPECT_EQ(ctx.wall_segment_count, 5);
}

TEST(AIWallContextTest, MacroTargetsHasWallSegmentCount) {
  Game::Systems::AI::AIContext::MacroTargets targets;
  targets.wall_segment_count = 3;
  EXPECT_EQ(targets.wall_segment_count, 3);
}
