#include "core/component.h"
#include "core/world.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/pathfinding.h"
#include "game/units/troop_config.h"
#include <QVector3D>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

namespace {

class CommandServiceTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::CommandService::initialize(32, 32);
  }

  static auto
  create_unit(Engine::Core::World &world, float x, float z,
              Game::Units::SpawnType spawn_type) -> Engine::Core::Entity * {
    auto *entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    auto *transform = entity->add_component<Engine::Core::TransformComponent>();
    auto *unit = entity->add_component<Engine::Core::UnitComponent>();
    auto *movement = entity->add_component<Engine::Core::MovementComponent>();
    if (transform == nullptr || unit == nullptr || movement == nullptr) {
      return nullptr;
    }

    transform->position = {x, 0.0F, z};
    unit->spawn_type = spawn_type;
    unit->speed = 3.0F;
    return entity;
  }

  static void wait_for_path_results(
      Engine::Core::World &world,
      const std::vector<Engine::Core::MovementComponent *> &movements) {
    for (int attempt = 0; attempt < 80; ++attempt) {
      Game::Systems::CommandService::process_path_results(world);
      bool all_done = true;
      for (auto *movement : movements) {
        if (movement != nullptr && movement->path_pending) {
          all_done = false;
          break;
        }
      }
      if (all_done) {
        return;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    FAIL() << "Timed out waiting for pathfinding results";
  }
};

TEST_F(CommandServiceTest, UnitRadiusMatchesSelectionRingRadius) {
  Engine::Core::World world;
  auto *unit = create_unit(world, 0.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);

  float const expected_radius =
      Game::Units::TroopConfig::instance().get_selection_ring_size(
          Game::Units::SpawnType::Archer);

  EXPECT_FLOAT_EQ(
      Game::Systems::CommandService::get_unit_radius(world, unit->get_id()),
      expected_radius);
}

TEST_F(CommandServiceTest,
       ShortMoveNearObstacleUsesPathRequestInsteadOfUnsafeDirectLine) {
  Engine::Core::World world;
  auto *entity =
      create_unit(world, -4.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(entity, nullptr);
  auto *movement = entity->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(movement, nullptr);

  auto *pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_building_obstacles();

  Game::Systems::Point const obstacle =
      Game::Systems::CommandService::world_to_grid(-1.0F, 1.0F);
  pathfinder->set_obstacle(obstacle.x, obstacle.y, true);

  Game::Systems::CommandService::move_unit(world, entity->get_id(),
                                           QVector3D(2.0F, 0.0F, 0.0F));

  EXPECT_TRUE(movement->path_pending);
  EXPECT_NE(movement->pending_request_id, 0U);
}

TEST_F(CommandServiceTest,
       GroupMoveRejectsLeaderPathWhenFormationCannotFitThroughGap) {
  Engine::Core::World world;

  auto *left =
      create_unit(world, -10.0F, -2.0F, Game::Units::SpawnType::Archer);
  auto *center =
      create_unit(world, -10.0F, 0.0F, Game::Units::SpawnType::Archer);
  auto *right =
      create_unit(world, -10.0F, 2.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(center, nullptr);
  ASSERT_NE(right, nullptr);

  auto *left_movement = left->get_component<Engine::Core::MovementComponent>();
  auto *center_movement =
      center->get_component<Engine::Core::MovementComponent>();
  auto *right_movement =
      right->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(left_movement, nullptr);
  ASSERT_NE(center_movement, nullptr);
  ASSERT_NE(right_movement, nullptr);

  auto *pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_building_obstacles();

  for (int world_z = -15; world_z <= 15; ++world_z) {
    if (world_z == 0) {
      continue;
    }
    Game::Systems::Point const wall_cell =
        Game::Systems::CommandService::world_to_grid(
            0.0F, static_cast<float>(world_z));
    pathfinder->set_obstacle(wall_cell.x, wall_cell.y, true);
  }

  Game::Systems::CommandService::MoveOptions options;
  options.allow_direct_fallback = false;
  options.group_move = true;

  Game::Systems::CommandService::move_units(
      world, {left->get_id(), center->get_id(), right->get_id()},
      {QVector3D(10.0F, 0.0F, -2.0F), QVector3D(10.0F, 0.0F, 0.0F),
       QVector3D(10.0F, 0.0F, 2.0F)},
      options);

  wait_for_path_results(world,
                        {left_movement, center_movement, right_movement});

  EXPECT_FALSE(left_movement->has_target);
  EXPECT_FALSE(center_movement->has_target);
  EXPECT_FALSE(right_movement->has_target);
  EXPECT_TRUE(left_movement->path.empty());
  EXPECT_TRUE(center_movement->path.empty());
  EXPECT_TRUE(right_movement->path.empty());
}

TEST_F(CommandServiceTest, GroupMoveFailureNearBoundaryDoesNotTeleportUnits) {
  Game::Systems::CommandService::initialize(16, 16);
  Engine::Core::World world;

  auto *left = create_unit(world, -7.0F, -2.0F, Game::Units::SpawnType::Archer);
  auto *center =
      create_unit(world, -7.0F, 0.0F, Game::Units::SpawnType::Archer);
  auto *right = create_unit(world, -7.0F, 2.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(center, nullptr);
  ASSERT_NE(right, nullptr);

  auto *left_transform =
      left->get_component<Engine::Core::TransformComponent>();
  auto *center_transform =
      center->get_component<Engine::Core::TransformComponent>();
  auto *right_transform =
      right->get_component<Engine::Core::TransformComponent>();
  auto *left_movement = left->get_component<Engine::Core::MovementComponent>();
  auto *center_movement =
      center->get_component<Engine::Core::MovementComponent>();
  auto *right_movement =
      right->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(left_transform, nullptr);
  ASSERT_NE(center_transform, nullptr);
  ASSERT_NE(right_transform, nullptr);
  ASSERT_NE(left_movement, nullptr);
  ASSERT_NE(center_movement, nullptr);
  ASSERT_NE(right_movement, nullptr);

  auto *pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_building_obstacles();

  for (int world_z = -7; world_z <= 7; ++world_z) {
    Game::Systems::Point const wall_cell =
        Game::Systems::CommandService::world_to_grid(
            0.0F, static_cast<float>(world_z));
    pathfinder->set_obstacle(wall_cell.x, wall_cell.y, true);
  }

  Game::Systems::CommandService::MoveOptions options;
  options.allow_direct_fallback = false;
  options.group_move = true;

  Game::Systems::CommandService::move_units(
      world, {left->get_id(), center->get_id(), right->get_id()},
      {QVector3D(7.0F, 0.0F, -2.0F), QVector3D(7.0F, 0.0F, 0.0F),
       QVector3D(7.0F, 0.0F, 2.0F)},
      options);

  wait_for_path_results(world,
                        {left_movement, center_movement, right_movement});

  EXPECT_FLOAT_EQ(left_transform->position.x, -7.0F);
  EXPECT_FLOAT_EQ(left_transform->position.z, -2.0F);
  EXPECT_FLOAT_EQ(center_transform->position.x, -7.0F);
  EXPECT_FLOAT_EQ(center_transform->position.z, 0.0F);
  EXPECT_FLOAT_EQ(right_transform->position.x, -7.0F);
  EXPECT_FLOAT_EQ(right_transform->position.z, 2.0F);
}

TEST_F(CommandServiceTest,
       GroupMoveCanRetryMembersIndividuallyAfterGroupFailure) {
  Engine::Core::World world;

  auto *left =
      create_unit(world, -10.0F, -2.0F, Game::Units::SpawnType::Archer);
  auto *center =
      create_unit(world, -10.0F, 0.0F, Game::Units::SpawnType::Archer);
  auto *right =
      create_unit(world, -10.0F, 2.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(center, nullptr);
  ASSERT_NE(right, nullptr);

  auto *left_movement = left->get_component<Engine::Core::MovementComponent>();
  auto *center_movement =
      center->get_component<Engine::Core::MovementComponent>();
  auto *right_movement =
      right->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(left_movement, nullptr);
  ASSERT_NE(center_movement, nullptr);
  ASSERT_NE(right_movement, nullptr);

  auto *pathfinder = Game::Systems::CommandService::get_pathfinder();
  ASSERT_NE(pathfinder, nullptr);
  pathfinder->update_building_obstacles();

  for (int world_z = -15; world_z <= 15; ++world_z) {
    if (world_z >= -2 && world_z <= 2) {
      continue;
    }
    Game::Systems::Point const wall_cell =
        Game::Systems::CommandService::world_to_grid(
            0.0F, static_cast<float>(world_z));
    pathfinder->set_obstacle(wall_cell.x, wall_cell.y, true);
  }

  Game::Systems::CommandService::MoveOptions options;
  options.allow_direct_fallback = false;
  options.group_move = true;
  options.retry_individual_on_group_failure = true;

  Game::Systems::CommandService::move_units(
      world, {left->get_id(), center->get_id(), right->get_id()},
      {QVector3D(10.0F, 0.0F, -2.0F), QVector3D(10.0F, 0.0F, 0.0F),
       QVector3D(10.0F, 0.0F, 2.0F)},
      options);

  wait_for_path_results(world,
                        {left_movement, center_movement, right_movement});

  EXPECT_TRUE(left_movement->has_target);
  EXPECT_TRUE(center_movement->has_target);
  EXPECT_TRUE(right_movement->has_target);
  EXPECT_FALSE(left_movement->path.empty());
  EXPECT_FALSE(center_movement->path.empty());
  EXPECT_FALSE(right_movement->path.empty());
}

TEST_F(CommandServiceTest, LocalRecoveryCanRelaxRadiusToEscapeBoundaryTrap) {
  Game::Systems::CommandService::initialize(16, 16);
  Engine::Core::World world;

  auto *unit = create_unit(world, -7.0F, 0.0F, Game::Units::SpawnType::Archer);
  ASSERT_NE(unit, nullptr);

  auto *movement = unit->get_component<Engine::Core::MovementComponent>();
  auto *transform = unit->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(transform, nullptr);

  EXPECT_TRUE(Game::Systems::CommandService::try_queue_local_recovery_move(
      world, unit->get_id(),
      QVector3D(transform->position.x, 0.0F, transform->position.z),
      QVector3D(5.0F, 0.0F, 0.0F), movement));

  EXPECT_TRUE(movement->has_target);
  EXPECT_GT(movement->target_x, transform->position.x);
  EXPECT_FLOAT_EQ(movement->goal_x, 5.0F);
  EXPECT_FLOAT_EQ(movement->goal_y, 0.0F);
}

} // namespace
