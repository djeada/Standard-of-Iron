#include <gtest/gtest.h>
#include <memory>

#include "core/component_gameplay.h"
#include "core/entity.h"
#include "core/world.h"
#include "game/map/terrain_service.h"
#include "systems/building_collision_registry.h"
#include "systems/navigation/nav_grid.h"
#include "systems/owner_registry.h"
#include "systems/skirmish_screen_system.h"
#include "units/spawn_type.h"

using namespace Engine::Core;
using namespace Game::Systems;
using Game::Units::SpawnType;

class SkirmishScreenSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    NavGrid::initialize(64, 64);
    world = std::make_unique<World>();
    system = std::make_unique<SkirmishScreenSystem>(
        SkirmishScreenSystem::Services{.owners = owners});
  }

  void TearDown() override {
    system.reset();
    world.reset();
    Game::Map::TerrainService::instance().clear();
    BuildingCollisionRegistry::instance().clear();
  }

  auto spawn(SpawnType type, int owner, float x, float z, bool melee) -> Entity* {
    auto* entity = world->create_entity();
    entity->add_component<TransformComponent>(x, 0.0F, z);
    auto* unit = entity->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = owner;
    unit->spawn_type = type;
    entity->add_component<MovementComponent>();
    auto* attack = entity->add_component<AttackComponent>();
    attack->can_ranged = !melee;
    attack->current_mode = melee ? AttackComponent::CombatMode::Melee
                                 : AttackComponent::CombatMode::Ranged;
    return entity;
  }

  OwnerRegistry owners;
  std::unique_ptr<World> world;
  std::unique_ptr<SkirmishScreenSystem> system;
};

TEST_F(SkirmishScreenSystemTest, WithdrawsBehindTheNearestFriendlyLine) {
  auto* velites = spawn(SpawnType::Velites, 1, 20.0F, 20.0F, false);
  spawn(SpawnType::Swordsman, 1, 20.0F, 14.0F, true);
  spawn(SpawnType::Spearman, 2, 20.0F, 23.0F, true);

  system->update(world.get(), 0.1F);

  auto const* movement = velites->get_component<MovementComponent>();
  ASSERT_TRUE(movement->get_has_target());
  EXPECT_NEAR(movement->get_requested_goal_x(), 20.0F, 0.01F);
  EXPECT_NEAR(movement->get_requested_goal_z(),
              14.0F - SkirmishScreenSystem::k_behind_line_distance,
              0.01F);
}

TEST_F(SkirmishScreenSystemTest, FallsBackAwayFromTheThreatWithNoLineNearby) {
  auto* slingers = spawn(SpawnType::Slinger, 1, 20.0F, 20.0F, false);
  spawn(SpawnType::Swordsman, 2, 20.0F, 23.0F, true);

  system->update(world.get(), 0.1F);

  auto const* movement = slingers->get_component<MovementComponent>();
  ASSERT_TRUE(movement->get_has_target());
  EXPECT_NEAR(movement->get_requested_goal_z(),
              20.0F - SkirmishScreenSystem::k_open_ground_retreat,
              0.01F);
}

TEST_F(SkirmishScreenSystemTest, StandsAgainstMissileTroopsAndDistantFoot) {
  auto* slingers = spawn(SpawnType::Slinger, 1, 20.0F, 20.0F, false);
  spawn(SpawnType::Archer, 2, 20.0F, 23.0F, false);
  spawn(SpawnType::Swordsman, 2, 20.0F, 32.0F, true);

  system->update(world.get(), 0.1F);

  EXPECT_FALSE(slingers->get_component<MovementComponent>()->get_has_target());
}

TEST_F(SkirmishScreenSystemTest, LeavesOrderedAndOtherTroopsAlone) {
  auto* line = spawn(SpawnType::Archer, 1, 24.0F, 20.0F, false);
  auto* velites = spawn(SpawnType::Velites, 1, 20.0F, 20.0F, false);
  velites->get_component<MovementComponent>()->engage_manual_move(30.0F, 30.0F);
  spawn(SpawnType::Spearman, 2, 22.0F, 22.0F, true);

  system->update(world.get(), 0.1F);

  auto const* movement = velites->get_component<MovementComponent>();
  EXPECT_FLOAT_EQ(movement->get_goal_x(), 30.0F);
  EXPECT_FLOAT_EQ(movement->get_goal_y(), 30.0F);
  EXPECT_FALSE(line->get_component<MovementComponent>()->get_has_target());
}
