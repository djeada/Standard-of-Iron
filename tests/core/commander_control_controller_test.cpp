#include <gtest/gtest.h>

#include "app/core/commander_control_controller.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/movement_system.h"
#include "game/systems/pathfinding.h"
#include "render/gl/camera.h"

namespace {

class CommanderControlControllerTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::CommandService::initialize(32, 32);
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static auto create_commander(Engine::Core::World& world,
                               float x,
                               float z) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }

    auto* transform =
        entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    auto* movement = entity->add_component<Engine::Core::MovementComponent>();
    auto* commander = entity->add_component<Engine::Core::CommanderComponent>();
    if (transform == nullptr || unit == nullptr || movement == nullptr ||
        commander == nullptr) {
      return nullptr;
    }

    unit->health = 100;
    unit->max_health = 100;
    unit->owner_id = 1;
    unit->speed = 3.0F;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    return entity;
  }
};

TEST_F(CommanderControlControllerTest, JumpForwardBypassesBlockedGroundCells) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  auto const blocked = Game::Systems::CommandService::world_to_grid(0.0F, 1.0F);
  pathfinder->set_obstacle(blocked.x, blocked.y, true);

  CommanderControlController controller;
  controller.input().forward = true;
  controller.request_jump();

  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.4F));

  EXPECT_GT(transform->position.z, 0.9F);
  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_data, nullptr);
  EXPECT_TRUE(commander_data->jump_active);
}

TEST_F(CommanderControlControllerTest,
       JumpLandingSnapsBackWhenNoWalkableLandingExists) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  for (float z : {1.0F, 2.0F, 3.0F}) {
    auto const blocked = Game::Systems::CommandService::world_to_grid(0.0F, z);
    pathfinder->set_obstacle(blocked.x, blocked.y, true);
  }

  CommanderControlController controller;
  controller.input().forward = true;
  controller.request_jump();

  Render::GL::Camera camera;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.2F));
  float const first_safe_z = transform->position.z;
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.2F));
  ASSERT_TRUE(controller.update(world, commander->get_id(), 1, camera, 0.2F));

  EXPECT_NEAR(transform->position.z, first_safe_z, 0.0001F);
}

TEST_F(CommanderControlControllerTest,
       AirborneCommanderSkipsMovementRollbackAndRecovery) {
  Engine::Core::World world;
  auto* commander = create_commander(world, 0.0F, 0.0F);
  ASSERT_NE(commander, nullptr);

  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  auto* movement = commander->get_component<Engine::Core::MovementComponent>();
  auto* commander_data = commander->get_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(commander_data, nullptr);

  auto* pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  auto const blocked = Game::Systems::CommandService::world_to_grid(
      transform->position.x, transform->position.z);
  pathfinder->set_obstacle(blocked.x, blocked.y, true);

  movement->has_target = true;
  movement->target_x = 0.0F;
  movement->target_y = 3.0F;
  movement->goal_x = 0.0F;
  movement->goal_y = 3.0F;
  movement->vx = 1.5F;
  movement->vz = 1.5F;
  commander_data->jump_active = true;

  Game::Systems::MovementSystem movement_system;
  movement_system.update(&world, 0.25F);

  EXPECT_FLOAT_EQ(transform->position.x, 0.0F);
  EXPECT_FLOAT_EQ(transform->position.z, 0.0F);
  EXPECT_TRUE(movement->has_target);
  EXPECT_FLOAT_EQ(movement->vx, 1.5F);
  EXPECT_FLOAT_EQ(movement->vz, 1.5F);
}

} // namespace
