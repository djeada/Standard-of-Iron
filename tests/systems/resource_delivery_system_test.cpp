#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <memory>

#include "core/component.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/movement_pipeline.h"
#include "game/systems/nav_grid.h"
#include "game/systems/pathfinding.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_delivery_system.h"
#include "game/systems/resource_stockpile.h"
#include "game/units/spawn_type.h"

namespace {

using Game::Systems::ResourceType;

class ResourceDeliverySystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::NavGrid::initialize(64, 64);
  }

  void TearDown() override {
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static auto add_barracks(Engine::Core::World& world,
                           int owner_id,
                           float x,
                           float z,
                           float yaw_degrees = 0.0F) -> Engine::Core::Entity* {
    auto* barracks = world.create_entity();
    auto* transform = barracks->add_component<Engine::Core::TransformComponent>();
    transform->position = {x, 0.0F, z};
    transform->rotation = {0.0F, yaw_degrees, 0.0F};
    auto* unit = barracks->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Barracks;
    unit->owner_id = owner_id;
    unit->health = 2000;
    unit->max_health = 2000;
    return barracks;
  }

  static auto add_hauler(Engine::Core::World& world,
                         int owner_id,
                         float x,
                         float z,
                         ResourceType type,
                         int amount) -> Engine::Core::Entity* {
    auto* hauler = world.create_entity();
    auto* transform = hauler->add_component<Engine::Core::TransformComponent>();
    transform->position = {x, 0.0F, z};
    auto* unit = hauler->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Builder;
    unit->owner_id = owner_id;
    unit->health = 100;
    unit->max_health = 100;
    unit->speed = 3.0F;
    hauler->add_component<Engine::Core::MovementComponent>();
    hauler->add_component<Engine::Core::BuilderProductionComponent>();
    auto* carry = hauler->add_component<Engine::Core::ResourceCarryComponent>();
    carry->amounts.add(type, amount);
    return hauler;
  }
};

TEST_F(ResourceDeliverySystemTest,
       CarriedGoodsStayOffTheCountersUntilTheyReachTheYard) {
  Engine::Core::World world;
  add_barracks(world, 1, 0.0F, 0.0F);
  auto* hauler = add_hauler(world, 1, 24.0F, 18.0F, ResourceType::Wood, 40);

  Game::Systems::ResourceDeliverySystem system;
  system.update(&world, 0.1F);

  EXPECT_EQ(
      Game::Systems::PlayerResourceRegistry::instance().get(1, ResourceType::Wood), 0);
  EXPECT_TRUE(hauler->has_component<Engine::Core::ResourceCarryComponent>());
}

TEST_F(ResourceDeliverySystemTest, AHaulerIsWalkedTowardsTheStockpile) {
  Engine::Core::World world;
  add_barracks(world, 1, 0.0F, 0.0F);
  auto* hauler = add_hauler(world, 1, 24.0F, 18.0F, ResourceType::Stone, 35);

  Game::Systems::ResourceDeliverySystem system;
  system.update(&world, 0.1F);

  const auto* movement = hauler->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_TRUE(movement->get_has_target());

  auto const drop = Game::Systems::stockpile_drop_point(0.0F, 0.0F, 0.0F);
  float const dx = movement->get_goal_x() - drop.x;
  float const dz = movement->get_goal_y() - drop.z;
  EXPECT_LT(std::sqrt((dx * dx) + (dz * dz)), 3.0F)
      << "the hauler should be sent to the yard beside the barracks";
}

TEST_F(ResourceDeliverySystemTest, UnloadingAtTheYardCreditsTheOwnerOnce) {
  Engine::Core::World world;
  auto* barracks = add_barracks(world, 1, 0.0F, 0.0F);
  auto const drop = Game::Systems::stockpile_drop_point(0.0F, 0.0F, 0.0F);
  auto* hauler = add_hauler(world, 1, drop.x, drop.z, ResourceType::Iron, 30);

  Game::Systems::ResourceDeliverySystem system;
  system.update(&world, 0.1F);

  EXPECT_EQ(
      Game::Systems::PlayerResourceRegistry::instance().get(1, ResourceType::Iron), 30);
  EXPECT_FALSE(hauler->has_component<Engine::Core::ResourceCarryComponent>());

  const auto* stockpile = barracks->get_component<Engine::Core::StockpileComponent>();
  ASSERT_NE(stockpile, nullptr);
  EXPECT_GT(stockpile->deposit_flash, 0.0F);

  system.update(&world, 0.1F);
  EXPECT_EQ(
      Game::Systems::PlayerResourceRegistry::instance().get(1, ResourceType::Iron), 30);
}

TEST_F(ResourceDeliverySystemTest, TheYardIsFoundThroughARotatedBarracks) {
  Engine::Core::World world;
  add_barracks(world, 1, 12.0F, -6.0F, 90.0F);
  auto const drop = Game::Systems::stockpile_drop_point(12.0F, -6.0F, 90.0F);
  auto* hauler = add_hauler(world, 1, drop.x, drop.z, ResourceType::Wood, 40);

  EXPECT_NEAR(drop.x, 12.0F, 0.01F);
  EXPECT_NEAR(drop.z, -6.0F - Game::Systems::k_stockpile_drop_x, 0.01F);

  Game::Systems::ResourceDeliverySystem system;
  system.update(&world, 0.1F);

  EXPECT_EQ(
      Game::Systems::PlayerResourceRegistry::instance().get(1, ResourceType::Wood), 40);
  EXPECT_FALSE(hauler->has_component<Engine::Core::ResourceCarryComponent>());
}

TEST_F(ResourceDeliverySystemTest, AnEnemyBarracksIsNotAValidDropOff) {
  Engine::Core::World world;
  add_barracks(world, 2, 0.0F, 0.0F);
  auto const drop = Game::Systems::stockpile_drop_point(0.0F, 0.0F, 0.0F);
  auto* hauler = add_hauler(world, 1, drop.x, drop.z, ResourceType::Wood, 40);

  Game::Systems::ResourceDeliverySystem system;
  system.update(&world, 0.1F);

  EXPECT_EQ(
      Game::Systems::PlayerResourceRegistry::instance().get(2, ResourceType::Wood), 0);
  EXPECT_EQ(
      Game::Systems::PlayerResourceRegistry::instance().get(1, ResourceType::Wood), 40);
  EXPECT_FALSE(hauler->has_component<Engine::Core::ResourceCarryComponent>());
}

TEST_F(ResourceDeliverySystemTest, AHaulerWalksTheWholeWayHomeBeforeTheCountersMove) {
  Game::Map::MapDefinition map_def;
  map_def.grid.width = 64;
  map_def.grid.height = 64;
  map_def.grid.tile_size = 1.0F;
  Game::Map::TerrainService::instance().initialize(map_def);
  auto* pathfinder = Game::Systems::NavGrid::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_navigation_grid();

  Engine::Core::World world;
  add_barracks(world, 1, 0.0F, 0.0F);
  auto* hauler = add_hauler(world, 1, 12.0F, 8.0F, ResourceType::Wood, 40);
  const auto* transform = hauler->get_component<Engine::Core::TransformComponent>();

  Game::Systems::ResourceDeliverySystem delivery;
  Game::Systems::MovementPipeline movement;

  bool credited = false;
  for (int tick = 0; tick < 900 && !credited; ++tick) {
    delivery.update(&world, 1.0F / 30.0F);
    movement.update(&world, 1.0F / 30.0F);
    credited = Game::Systems::PlayerResourceRegistry::instance().get(
                   1, ResourceType::Wood) > 0;
    if (!credited) {
      EXPECT_TRUE(hauler->has_component<Engine::Core::ResourceCarryComponent>());
    }
  }

  ASSERT_TRUE(credited) << "the hauler never reached the stockpile";
  EXPECT_EQ(
      Game::Systems::PlayerResourceRegistry::instance().get(1, ResourceType::Wood), 40);

  auto const drop = Game::Systems::stockpile_drop_point(0.0F, 0.0F, 0.0F);
  float const dx = transform->position.x - drop.x;
  float const dz = transform->position.z - drop.z;
  EXPECT_LT(std::sqrt((dx * dx) + (dz * dz)),
            Game::Systems::k_stockpile_drop_radius + 0.5F)
      << "the load should only be handed over on the yard itself";

  Game::Map::TerrainService::instance().clear();
}

TEST_F(ResourceDeliverySystemTest, PileHeightsFollowTheOwnersStores) {
  Engine::Core::World world;
  auto* barracks = add_barracks(world, 1, 0.0F, 0.0F);
  Game::Systems::PlayerResourceRegistry::instance().set(
      1, ResourceType::Wood, Game::Systems::k_stockpile_wood_display_cap * 2);
  Game::Systems::PlayerResourceRegistry::instance().set(
      1, ResourceType::Iron, Game::Systems::k_stockpile_iron_display_cap / 4);

  Game::Systems::ResourceDeliverySystem system;
  for (int tick = 0; tick < 60; ++tick) {
    system.update(&world, 0.1F);
  }

  const auto* stockpile = barracks->get_component<Engine::Core::StockpileComponent>();
  ASSERT_NE(stockpile, nullptr);
  EXPECT_FLOAT_EQ(stockpile->wood_fill, 1.0F);
  EXPECT_FLOAT_EQ(stockpile->stone_fill, 0.0F);
  EXPECT_NEAR(stockpile->iron_fill, 0.25F, 0.01F);
  EXPECT_FLOAT_EQ(stockpile->deposit_flash, 0.0F);
}

TEST_F(ResourceDeliverySystemTest, EachResourceKeepsItsOwnDisplayScale) {
  using Game::Systems::stockpile_fill_ratio;

  EXPECT_FLOAT_EQ(stockpile_fill_ratio(0, ResourceType::Wood), 0.0F);
  EXPECT_FLOAT_EQ(stockpile_fill_ratio(Game::Systems::k_stockpile_wood_display_cap,
                                       ResourceType::Wood),
                  1.0F);

  EXPECT_LT(stockpile_fill_ratio(200, ResourceType::Wood),
            stockpile_fill_ratio(200, ResourceType::Stone))
      << "stone reads fuller than wood at the same count, because a yard holds "
         "less of it";
}

} // namespace
