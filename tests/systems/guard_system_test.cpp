#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/guard_system.h"
#include <gtest/gtest.h>

using namespace Engine::Core;
using namespace Game::Systems;

class GuardSystemTest : public ::testing::Test {
protected:
  void SetUp() override { world = std::make_unique<World>(); }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
};

TEST_F(GuardSystemTest, GuardFollowsMovingEntity) {
  // Create a unit that is guarding another unit
  auto *guard = world->create_entity();
  auto *guard_transform =
      guard->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *guard_unit = guard->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  guard_unit->owner_id = 1;
  auto *guard_movement = guard->add_component<MovementComponent>();
  auto *guard_mode = guard->add_component<GuardModeComponent>();

  // Create the unit being guarded
  auto *guarded = world->create_entity();
  auto *guarded_transform =
      guarded->add_component<TransformComponent>(5.0F, 0.0F, 5.0F);
  auto *guarded_unit =
      guarded->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  guarded_unit->owner_id = 1;

  // Setup guard mode to guard the entity
  guard_mode->active = true;
  guard_mode->guarded_entity_id = guarded->get_id();
  guard_mode->has_guard_target = true;
  guard_mode->guard_position_x = 5.0F;
  guard_mode->guard_position_z = 5.0F;

  // Run the guard system - guard should not move yet (distance is only sqrt(50)
  // ~ 7.07, threshold is 2)
  GuardSystem::update(world.get(), 0.1F);

  // Guard should have movement target set because the distance exceeds the
  // threshold
  EXPECT_TRUE(guard_movement->has_target);
  EXPECT_FLOAT_EQ(guard_movement->goal_x, 5.0F);
  EXPECT_FLOAT_EQ(guard_movement->goal_y, 5.0F); // goal_y represents Z coordinate

  // Move the guarded unit to a new position
  guarded_transform->position.x = 15.0F;
  guarded_transform->position.z = 15.0F;

  // Run the guard system again
  GuardSystem::update(world.get(), 0.1F);

  // Guard's stored position should be updated
  EXPECT_FLOAT_EQ(guard_mode->guard_position_x, 15.0F);
  EXPECT_FLOAT_EQ(guard_mode->guard_position_z, 15.0F);

  // Guard should have new movement target
  EXPECT_TRUE(guard_movement->has_target);
  EXPECT_FLOAT_EQ(guard_movement->goal_x, 15.0F);
  EXPECT_FLOAT_EQ(guard_movement->goal_y, 15.0F); // goal_y represents Z coordinate
  EXPECT_TRUE(guard_mode->returning_to_guard_position);
}

TEST_F(GuardSystemTest, GuardDoesNotFollowSmallMovements) {
  // Create a unit that is guarding another unit
  auto *guard = world->create_entity();
  auto *guard_transform =
      guard->add_component<TransformComponent>(5.0F, 0.0F, 5.0F);
  auto *guard_unit = guard->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  guard_unit->owner_id = 1;
  auto *guard_movement = guard->add_component<MovementComponent>();
  auto *guard_mode = guard->add_component<GuardModeComponent>();

  // Create the unit being guarded (very close to guard)
  auto *guarded = world->create_entity();
  auto *guarded_transform =
      guarded->add_component<TransformComponent>(5.5F, 0.0F, 5.5F);
  auto *guarded_unit =
      guarded->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  guarded_unit->owner_id = 1;

  // Setup guard mode to guard the entity
  guard_mode->active = true;
  guard_mode->guarded_entity_id = guarded->get_id();
  guard_mode->has_guard_target = true;
  guard_mode->guard_position_x = 5.5F;
  guard_mode->guard_position_z = 5.5F;

  // Run the guard system - guard should not move (distance is less than
  // threshold)
  GuardSystem::update(world.get(), 0.1F);

  // Guard should not have movement target set (distance too small)
  EXPECT_FALSE(guard_movement->has_target);
}

TEST_F(GuardSystemTest, GuardDoesNotFollowWhileAttacking) {
  // Create a unit that is guarding another unit
  auto *guard = world->create_entity();
  auto *guard_transform =
      guard->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *guard_unit = guard->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  guard_unit->owner_id = 1;
  auto *guard_movement = guard->add_component<MovementComponent>();
  auto *guard_mode = guard->add_component<GuardModeComponent>();

  // Create the unit being guarded
  auto *guarded = world->create_entity();
  auto *guarded_transform =
      guarded->add_component<TransformComponent>(20.0F, 0.0F, 20.0F);
  auto *guarded_unit =
      guarded->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  guarded_unit->owner_id = 1;

  // Setup guard mode to guard the entity
  guard_mode->active = true;
  guard_mode->guarded_entity_id = guarded->get_id();
  guard_mode->has_guard_target = true;
  guard_mode->guard_position_x = 5.0F;
  guard_mode->guard_position_z = 5.0F;

  // Create an enemy that the guard is attacking
  auto *enemy = world->create_entity();
  enemy->add_component<TransformComponent>(2.0F, 0.0F, 2.0F);
  auto *enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 10.0F);
  enemy_unit->owner_id = 2;

  // Guard is attacking the enemy
  auto *attack_target = guard->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();

  // Run the guard system
  GuardSystem::update(world.get(), 0.1F);

  // Guard should NOT follow the guarded unit while attacking
  EXPECT_FALSE(guard_movement->has_target);
  // Guard position should not have been updated
  EXPECT_FLOAT_EQ(guard_mode->guard_position_x, 5.0F);
  EXPECT_FLOAT_EQ(guard_mode->guard_position_z, 5.0F);
}

TEST_F(GuardSystemTest, GuardReturnsToPositionWhenGuardingLocation) {
  // Create a unit that is guarding a position (not an entity)
  auto *guard = world->create_entity();
  auto *guard_transform =
      guard->add_component<TransformComponent>(5.0F, 0.0F, 5.0F);
  auto *guard_unit = guard->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  guard_unit->owner_id = 1;
  auto *guard_movement = guard->add_component<MovementComponent>();
  auto *guard_mode = guard->add_component<GuardModeComponent>();

  // Setup guard mode to guard a position (no entity)
  guard_mode->active = true;
  guard_mode->guarded_entity_id = 0; // No entity
  guard_mode->has_guard_target = true;
  guard_mode->guard_position_x = 10.0F;
  guard_mode->guard_position_z = 10.0F;
  guard_mode->returning_to_guard_position = false;

  // Run the guard system - guard should move back to guard position
  GuardSystem::update(world.get(), 0.1F);

  // Guard should have movement target set to return to guard position
  EXPECT_TRUE(guard_movement->has_target);
  EXPECT_FLOAT_EQ(guard_movement->goal_x, 10.0F);
  EXPECT_FLOAT_EQ(guard_movement->goal_y, 10.0F); // goal_y represents Z coordinate
  EXPECT_TRUE(guard_mode->returning_to_guard_position);
}

TEST_F(GuardSystemTest, GuardDoesNotMoveWhenAlreadyAtPosition) {
  // Create a unit that is guarding a position
  auto *guard = world->create_entity();
  auto *guard_transform =
      guard->add_component<TransformComponent>(10.0F, 0.0F, 10.0F);
  auto *guard_unit = guard->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  guard_unit->owner_id = 1;
  auto *guard_movement = guard->add_component<MovementComponent>();
  auto *guard_mode = guard->add_component<GuardModeComponent>();

  // Setup guard mode to guard a position (already at position)
  guard_mode->active = true;
  guard_mode->guarded_entity_id = 0; // No entity
  guard_mode->has_guard_target = true;
  guard_mode->guard_position_x = 10.0F;
  guard_mode->guard_position_z = 10.0F;
  guard_mode->returning_to_guard_position = false;

  // Run the guard system
  GuardSystem::update(world.get(), 0.1F);

  // Guard should not move (already at position)
  EXPECT_FALSE(guard_movement->has_target);
  EXPECT_FALSE(guard_mode->returning_to_guard_position);
}
