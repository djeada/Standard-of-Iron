#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/arrow_system.h"
#include "systems/defense_tower_system.h"
#include "systems/owner_registry.h"
#include "units/spawn_type.h"
#include <gtest/gtest.h>

using namespace Engine::Core;
using namespace Game::Systems;

class DefenseTowerSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    world->add_system(std::make_unique<ArrowSystem>());
    OwnerRegistry::instance().clear();
  }

  void TearDown() override { world.reset(); }

  auto arrow_count() const -> std::size_t {
    auto *arrow_sys = world->get_system<ArrowSystem>();
    if (arrow_sys == nullptr) {
      return 0;
    }
    return arrow_sys->arrows().size();
  }

  auto make_tower(float x, float y, float z, int owner_id, float range = 18.0F,
                  float height_diff = 4.0F) -> Entity * {
    auto *tower = world->create_entity();
    tower->add_component<TransformComponent>(x, y, z);
    auto *unit = tower->add_component<UnitComponent>(2000, 2000, 0.0F, 20.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::DefenseTower;
    tower->add_component<BuildingComponent>();
    auto *atk = tower->add_component<AttackComponent>(range, 30, 1.8F);
    atk->can_ranged = true;
    atk->can_melee = false;
    atk->preferred_mode = AttackComponent::CombatMode::Ranged;
    atk->current_mode = AttackComponent::CombatMode::Ranged;
    atk->max_height_difference = height_diff;
    atk->time_since_last = 999.0F;
    return tower;
  }

  auto make_enemy(float x, float y, float z, int owner_id) -> Entity * {
    auto *enemy = world->create_entity();
    enemy->add_component<TransformComponent>(x, y, z);
    auto *unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::Spearman;
    return enemy;
  }

  std::unique_ptr<World> world;
  DefenseTowerSystem tower_system;
};

TEST_F(DefenseTowerSystemTest, TowerAttacksEnemyInRange) {
  auto *tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto *enemy = make_enemy(10.0F, 0.0F, 0.0F, 2);

  tower_system.update(world.get(), 0.1F);

  EXPECT_EQ(arrow_count(), 2u);
  auto *enemy_unit = enemy->get_component<UnitComponent>();
  ASSERT_NE(enemy_unit, nullptr);
  EXPECT_LT(enemy_unit->health, 100);
}

TEST_F(DefenseTowerSystemTest, TowerDoesNotAttackEnemyOutOfRange) {
  auto *tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto *enemy = make_enemy(25.0F, 0.0F, 0.0F, 2);

  tower_system.update(world.get(), 0.1F);

  EXPECT_EQ(arrow_count(), 0u);
  auto *enemy_unit = enemy->get_component<UnitComponent>();
  ASSERT_NE(enemy_unit, nullptr);
  EXPECT_EQ(enemy_unit->health, 100);
}

TEST_F(DefenseTowerSystemTest, TowerDoesNotAttackWhenCooldownActive) {
  auto *tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto *atk = tower->get_component<AttackComponent>();
  ASSERT_NE(atk, nullptr);
  atk->time_since_last = 0.0F;

  make_enemy(10.0F, 0.0F, 0.0F, 2);

  tower_system.update(world.get(), 0.1F);

  EXPECT_EQ(arrow_count(), 0u);
}

TEST_F(DefenseTowerSystemTest, TowerDoesNotAttackAlly) {
  OwnerRegistry::instance().register_owner_with_id(1, OwnerType::Player, "p1");
  OwnerRegistry::instance().register_owner_with_id(2, OwnerType::Player, "p2");
  OwnerRegistry::instance().set_owner_team(1, 1);
  OwnerRegistry::instance().set_owner_team(2, 1);

  auto *tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto *ally = make_enemy(10.0F, 0.0F, 0.0F, 2);

  tower_system.update(world.get(), 0.1F);

  EXPECT_EQ(arrow_count(), 0u);
  auto *ally_unit = ally->get_component<UnitComponent>();
  ASSERT_NE(ally_unit, nullptr);
  EXPECT_EQ(ally_unit->health, 100);
}

TEST_F(DefenseTowerSystemTest, TowerDoesNotAttackEnemyBeyondHeightDifference) {
  auto *tower = make_tower(0.0F, 0.0F, 0.0F, 1, 18.0F, 4.0F);
  auto *enemy = make_enemy(10.0F, 10.0F, 0.0F, 2);

  tower_system.update(world.get(), 0.1F);

  EXPECT_EQ(arrow_count(), 0u);
  auto *enemy_unit = enemy->get_component<UnitComponent>();
  ASSERT_NE(enemy_unit, nullptr);
  EXPECT_EQ(enemy_unit->health, 100);
}

TEST_F(DefenseTowerSystemTest, TowerAttacksEnemyWithinHeightDifference) {
  auto *tower = make_tower(0.0F, 0.0F, 0.0F, 1, 18.0F, 4.0F);
  make_enemy(10.0F, 2.0F, 0.0F, 2);

  tower_system.update(world.get(), 0.1F);

  EXPECT_EQ(arrow_count(), 2u);
}

TEST_F(DefenseTowerSystemTest, TowerFiresVolleyOfTwoArrows) {
  auto *tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  make_enemy(10.0F, 0.0F, 0.0F, 2);

  tower_system.update(world.get(), 0.1F);

  EXPECT_EQ(arrow_count(), 2u);
}

TEST_F(DefenseTowerSystemTest, TowerDoesNotAttackBuildingEntities) {
  auto *tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto *enemy_building = world->create_entity();
  enemy_building->add_component<TransformComponent>(5.0F, 0.0F, 0.0F);
  auto *unit =
      enemy_building->add_component<UnitComponent>(1000, 1000, 0.0F, 20.0F);
  unit->owner_id = 2;
  unit->spawn_type = Game::Units::SpawnType::Barracks;
  enemy_building->add_component<BuildingComponent>();

  tower_system.update(world.get(), 0.1F);

  EXPECT_EQ(arrow_count(), 0u);
}

TEST_F(DefenseTowerSystemTest, TowerTargetsNearestEnemy) {
  auto *tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto *far_enemy = make_enemy(15.0F, 0.0F, 0.0F, 2);
  auto *near_enemy = make_enemy(5.0F, 0.0F, 0.0F, 2);

  tower_system.update(world.get(), 0.1F);

  auto *far_unit = far_enemy->get_component<UnitComponent>();
  auto *near_unit = near_enemy->get_component<UnitComponent>();
  ASSERT_NE(far_unit, nullptr);
  ASSERT_NE(near_unit, nullptr);
  EXPECT_EQ(far_unit->health, 100);
  EXPECT_LT(near_unit->health, 100);
}
