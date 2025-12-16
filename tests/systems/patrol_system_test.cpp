#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/patrol_system.h"
#include <gtest/gtest.h>

using namespace Engine::Core;
using namespace Game::Systems;

// Detection range from patrol_system.cpp (line 66: if (dist_sq < 25.0F))
constexpr float kPatrolDetectionRangeSq = 25.0F;

class PatrolSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    patrol_system = std::make_unique<PatrolSystem>();
  }

  void TearDown() override {
    patrol_system.reset();
    world.reset();
  }

  std::unique_ptr<World> world;
  std::unique_ptr<PatrolSystem> patrol_system;
};

TEST_F(PatrolSystemTest, PatrollingUnitIgnoresEnemyBuildings) {
  // Create a patrolling unit
  auto *unit = world->create_entity();
  auto *transform = unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  // UnitComponent(health, max_health, speed, vision_range)
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  auto *movement = unit->add_component<MovementComponent>();
  auto *patrol = unit->add_component<PatrolComponent>();

  // Setup patrol waypoints
  patrol->waypoints.push_back({10.0F, 0.0F});
  patrol->waypoints.push_back({10.0F, 10.0F});
  patrol->patrolling = true;
  patrol->current_waypoint = 0;

  // Create an enemy building nearby (within detection range of 25.0F dist_sq)
  // Distance of 3.0F units from origin results in dist_sq = 9.0F, which is
  // < 25.0F
  auto *enemy_building = world->create_entity();
  auto *enemy_transform =
      enemy_building->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  // UnitComponent(health, max_health, speed, vision_range)
  auto *enemy_unit_comp =
      enemy_building->add_component<UnitComponent>(100, 100, 0.0F, 10.0F);
  enemy_unit_comp->owner_id = 2;                      // Different owner
  enemy_building->add_component<BuildingComponent>(); // Mark as building

  // Run the patrol system
  patrol_system->update(world.get(), 0.1F);

  // Verify that the unit did NOT auto-attack the building
  auto *attack_target = unit->get_component<AttackTargetComponent>();
  EXPECT_EQ(attack_target, nullptr)
      << "Patrolling unit should not auto-attack enemy buildings";
}

TEST_F(PatrolSystemTest, PatrollingUnitAttacksEnemyTroops) {
  // Create a patrolling unit
  auto *unit = world->create_entity();
  auto *transform = unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  // UnitComponent(health, max_health, speed, vision_range)
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  auto *movement = unit->add_component<MovementComponent>();
  auto *patrol = unit->add_component<PatrolComponent>();

  // Setup patrol waypoints
  patrol->waypoints.push_back({10.0F, 0.0F});
  patrol->waypoints.push_back({10.0F, 10.0F});
  patrol->patrolling = true;
  patrol->current_waypoint = 0;

  // Create an enemy troop nearby (within detection range)
  auto *enemy_troop = world->create_entity();
  auto *enemy_transform =
      enemy_troop->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  // UnitComponent(health, max_health, speed, vision_range)
  auto *enemy_unit_comp =
      enemy_troop->add_component<UnitComponent>(100, 100, 1.0F, 10.0F);
  enemy_unit_comp->owner_id = 2; // Different owner
  // No BuildingComponent - this is a regular troop

  // Run the patrol system
  patrol_system->update(world.get(), 0.1F);

  // Verify that the unit DID auto-attack the enemy troop
  auto *attack_target = unit->get_component<AttackTargetComponent>();
  ASSERT_NE(attack_target, nullptr)
      << "Patrolling unit should auto-attack enemy troops";
  EXPECT_EQ(attack_target->target_id, enemy_troop->get_id());
  EXPECT_FALSE(attack_target->should_chase) << "Patrol attack should not chase";
}

TEST_F(PatrolSystemTest, PatrollingUnitIgnoresFriendlyUnits) {
  // Create a patrolling unit
  auto *unit = world->create_entity();
  auto *transform = unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  // UnitComponent(health, max_health, speed, vision_range)
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  auto *movement = unit->add_component<MovementComponent>();
  auto *patrol = unit->add_component<PatrolComponent>();

  // Setup patrol waypoints
  patrol->waypoints.push_back({10.0F, 0.0F});
  patrol->waypoints.push_back({10.0F, 10.0F});
  patrol->patrolling = true;
  patrol->current_waypoint = 0;

  // Create a friendly unit nearby
  auto *friendly_unit = world->create_entity();
  auto *friendly_transform =
      friendly_unit->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  // UnitComponent(health, max_health, speed, vision_range)
  auto *friendly_unit_comp =
      friendly_unit->add_component<UnitComponent>(100, 100, 1.0F, 10.0F);
  friendly_unit_comp->owner_id = 1; // Same owner

  // Run the patrol system
  patrol_system->update(world.get(), 0.1F);

  // Verify that the unit did NOT attack the friendly unit
  auto *attack_target = unit->get_component<AttackTargetComponent>();
  EXPECT_EQ(attack_target, nullptr)
      << "Patrolling unit should not attack friendly units";
}

TEST_F(PatrolSystemTest, PatrollingUnitIgnoresDeadEnemies) {
  // Create a patrolling unit
  auto *unit = world->create_entity();
  auto *transform = unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  // UnitComponent(health, max_health, speed, vision_range)
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  auto *movement = unit->add_component<MovementComponent>();
  auto *patrol = unit->add_component<PatrolComponent>();

  // Setup patrol waypoints
  patrol->waypoints.push_back({10.0F, 0.0F});
  patrol->waypoints.push_back({10.0F, 10.0F});
  patrol->patrolling = true;
  patrol->current_waypoint = 0;

  // Create a dead enemy troop nearby
  auto *dead_enemy = world->create_entity();
  auto *enemy_transform =
      dead_enemy->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  // UnitComponent(health=0, max_health, speed, vision_range)
  auto *enemy_unit_comp =
      dead_enemy->add_component<UnitComponent>(0, 100, 1.0F, 10.0F); // 0 health
  enemy_unit_comp->owner_id = 2; // Different owner

  // Run the patrol system
  patrol_system->update(world.get(), 0.1F);

  // Verify that the unit did NOT attack the dead enemy
  auto *attack_target = unit->get_component<AttackTargetComponent>();
  EXPECT_EQ(attack_target, nullptr)
      << "Patrolling unit should not attack dead enemies";
}
