#include <cmath>
#include <gtest/gtest.h>

#include "app/commander/commander_control_controller.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"

namespace {

class CommanderLockOnTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NavGrid::initialize(64, 64);
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  static auto make_unit(Engine::Core::World& world,
                        float x,
                        float z,
                        int owner) -> Engine::Core::Entity* {
    auto* entity = world.create_entity();
    if (entity == nullptr) {
      return nullptr;
    }
    entity->add_component<Engine::Core::TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    entity->add_component<Engine::Core::MovementComponent>();
    if (unit == nullptr) {
      return nullptr;
    }
    unit->health = 500;
    unit->max_health = 500;
    unit->owner_id = owner;
    unit->speed = 3.0F;
    unit->spawn_type = Game::Units::SpawnType::Knight;
    unit->render_individuals_per_unit_override = 1;
    return entity;
  }

  static auto make_commander(Engine::Core::World& world) -> Engine::Core::Entity* {
    auto* entity = make_unit(world, 0.0F, 0.0F, 1);
    if (entity == nullptr) {
      return nullptr;
    }
    auto* commander = entity->add_component<Engine::Core::CommanderComponent>();
    if (commander == nullptr) {
      return nullptr;
    }
    commander->fpv_controlled = true;
    return entity;
  }

  static void make_enemies_hostile(Engine::Core::World& world) {
    auto& owners = Game::Session::session_for(world).owners();
    owners.set_owner_team(1, 1);
    owners.set_owner_team(2, 2);
  }
};

TEST_F(CommanderLockOnTest, ALockDropsWhenTheTargetDies) {
  constexpr float k_dt = 1.0F / 60.0F;
  Engine::Core::World world;
  auto* commander = make_commander(world);
  auto* enemy = make_unit(world, 0.0F, 4.0F, 2);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);
  make_enemies_hostile(world);

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  controller.cycle_lock_on_target(world, commander->get_id(), 1);
  ASSERT_EQ(controller.locked_target_id(), enemy->get_id());

  auto* enemy_unit = enemy->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(enemy_unit, nullptr);
  enemy_unit->health = 0;

  ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));
  EXPECT_EQ(controller.locked_target_id(), 0U);
}

TEST_F(CommanderLockOnTest, CyclingWalksTargetsAcrossTheScreen) {
  Engine::Core::World world;
  auto* commander = make_commander(world);
  auto* left = make_unit(world, -3.0F, 5.0F, 2);
  auto* centre = make_unit(world, 0.0F, 5.0F, 2);
  auto* right = make_unit(world, 3.0F, 5.0F, 2);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(centre, nullptr);
  ASSERT_NE(right, nullptr);
  make_enemies_hostile(world);

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);

  controller.cycle_lock_on_target(world, commander->get_id(), 1);
  EXPECT_EQ(controller.locked_target_id(), centre->get_id())
      << "the first lock takes the target nearest the view centre";

  controller.cycle_lock_on_target(world, commander->get_id(), 1);
  EXPECT_EQ(controller.locked_target_id(), right->get_id())
      << "cycling steps to the next target to the right";

  controller.cycle_lock_on_target(world, commander->get_id(), 1);
  EXPECT_EQ(controller.locked_target_id(), left->get_id())
      << "cycling past the rightmost target wraps to the leftmost";
}

TEST_F(CommanderLockOnTest, AnOverlappingTargetDoesNotSpinTheView) {
  constexpr float k_dt = 1.0F / 60.0F;
  Engine::Core::World world;
  auto* commander = make_commander(world);
  auto* enemy = make_unit(world, 0.0F, 4.0F, 2);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);
  make_enemies_hostile(world);

  CommanderControlController controller;
  controller.set_view_yaw(0.0F);
  controller.cycle_lock_on_target(world, commander->get_id(), 1);
  ASSERT_EQ(controller.locked_target_id(), enemy->get_id());

  auto* enemy_transform = enemy->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(enemy_transform, nullptr);
  enemy_transform->position.z = -0.2F;
  enemy_transform->position.x = 0.05F;

  auto const* commander_transform =
      commander->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(commander_transform, nullptr);

  float const before = controller.view_yaw();
  for (int tick = 0; tick < 30; ++tick) {

    enemy_transform->position.x = commander_transform->position.x + 0.05F;
    enemy_transform->position.z = commander_transform->position.z - 0.20F;
    ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));
  }
  float const after = controller.view_yaw();

  EXPECT_NEAR(after, before, 1.0F)
      << "a target inside the commander's own footprint must not swing the view";
  EXPECT_EQ(controller.locked_target_id(), enemy->get_id())
      << "the lock itself survives the overlap";
}

TEST_F(CommanderLockOnTest, ManualLookOverridesTheLockSpringWithoutDroppingIt) {
  constexpr float k_dt = 1.0F / 60.0F;
  Engine::Core::World world;
  auto* commander = make_commander(world);
  auto* enemy = make_unit(world, 4.0F, 4.0F, 2);
  ASSERT_NE(commander, nullptr);
  ASSERT_NE(enemy, nullptr);
  make_enemies_hostile(world);

  CommanderControlController controller;
  controller.set_view_yaw(45.0F);
  controller.cycle_lock_on_target(world, commander->get_id(), 1);
  ASSERT_EQ(controller.locked_target_id(), enemy->get_id());

  ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));

  controller.mouse_move(120.0, 0.0);
  float const after_look = controller.view_yaw();
  ASSERT_TRUE(controller.update_simulation(world, commander->get_id(), 1, k_dt));

  EXPECT_NEAR(controller.view_yaw(), after_look, 0.5F)
      << "the lock spring must yield to manual look for its override window";
  EXPECT_EQ(controller.locked_target_id(), enemy->get_id())
      << "manual look steers, it does not unlock";
}

} // namespace
