#include <gtest/gtest.h>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/arrow_system.h"
#include "systems/combat_system/combat_utils.h"
#include "systems/combat_system/damage_processor.h"
#include "systems/combat_system/siege_special_processor.h"
#include "systems/owner_registry.h"
#include "units/spawn_type.h"

using namespace Engine::Core;
using namespace Game::Systems;

class SiegeSpecialProcessorTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    world->add_system(std::make_unique<ArrowSystem>());
    OwnerRegistry::instance().clear();
  }

  void TearDown() override { world.reset(); }

  void update(float delta_time) {
    auto query_context = Combat::build_combat_query_context(world.get());
    Combat::process_siege_specials(world.get(), query_context, delta_time);
  }

  [[nodiscard]] auto arrow_count() const -> std::size_t {
    auto* arrow_sys = world->get_system<ArrowSystem>();
    if (arrow_sys == nullptr) {
      return 0;
    }
    return arrow_sys->arrows().size();
  }

  auto make_tower(float x,
                  float y,
                  float z,
                  int owner_id,
                  float range = 18.0F,
                  float height_diff = 4.0F) -> Entity* {
    auto* tower = world->create_entity();
    tower->add_component<TransformComponent>(x, y, z);
    auto* unit = tower->add_component<UnitComponent>(2000, 2000, 0.0F, 20.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::DefenseTower;
    tower->add_component<BuildingComponent>();
    auto* atk = tower->add_component<AttackComponent>(range, 30, 1.8F);
    atk->can_ranged = true;
    atk->can_melee = false;
    atk->preferred_mode = AttackComponent::CombatMode::Ranged;
    atk->current_mode = AttackComponent::CombatMode::Ranged;
    atk->max_height_difference = height_diff;
    atk->time_since_last = 999.0F;
    return tower;
  }

  auto make_enemy(float x, float y, float z, int owner_id) -> Entity* {
    auto* enemy = world->create_entity();
    enemy->add_component<TransformComponent>(x, y, z);
    auto* unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::Spearman;
    return enemy;
  }

  std::unique_ptr<World> world;
};

TEST_F(SiegeSpecialProcessorTest, TowerAttacksEnemyInRange) {
  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* enemy = make_enemy(10.0F, 0.0F, 0.0F, 2);

  update(0.1F);

  EXPECT_EQ(arrow_count(), 2U);
  auto* enemy_unit = enemy->get_component<UnitComponent>();
  ASSERT_NE(enemy_unit, nullptr);
  EXPECT_LT(enemy_unit->health, 100);
}

TEST_F(SiegeSpecialProcessorTest, TowerDoesNotAttackEnemyOutOfRange) {
  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* enemy = make_enemy(25.0F, 0.0F, 0.0F, 2);

  update(0.1F);

  EXPECT_EQ(arrow_count(), 0U);
  auto* enemy_unit = enemy->get_component<UnitComponent>();
  ASSERT_NE(enemy_unit, nullptr);
  EXPECT_EQ(enemy_unit->health, 100);
}

TEST_F(SiegeSpecialProcessorTest, TowerDoesNotAttackWhenCooldownActive) {
  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* atk = tower->get_component<AttackComponent>();
  ASSERT_NE(atk, nullptr);
  atk->time_since_last = 0.0F;

  make_enemy(10.0F, 0.0F, 0.0F, 2);

  update(0.1F);

  EXPECT_EQ(arrow_count(), 0U);
}

TEST_F(SiegeSpecialProcessorTest, TowerDoesNotAttackAlly) {
  OwnerRegistry::instance().register_owner_with_id(1, OwnerType::Player, "p1");
  OwnerRegistry::instance().register_owner_with_id(2, OwnerType::Player, "p2");
  OwnerRegistry::instance().set_owner_team(1, 1);
  OwnerRegistry::instance().set_owner_team(2, 1);

  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* ally = make_enemy(10.0F, 0.0F, 0.0F, 2);

  update(0.1F);

  EXPECT_EQ(arrow_count(), 0U);
  auto* ally_unit = ally->get_component<UnitComponent>();
  ASSERT_NE(ally_unit, nullptr);
  EXPECT_EQ(ally_unit->health, 100);
}

TEST_F(SiegeSpecialProcessorTest, TowerDoesNotAttackEnemyBeyondHeightDifference) {
  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1, 18.0F, 4.0F);
  auto* enemy = make_enemy(10.0F, 10.0F, 0.0F, 2);

  update(0.1F);

  EXPECT_EQ(arrow_count(), 0U);
  auto* enemy_unit = enemy->get_component<UnitComponent>();
  ASSERT_NE(enemy_unit, nullptr);
  EXPECT_EQ(enemy_unit->health, 100);
}

TEST_F(SiegeSpecialProcessorTest, TowerAttacksEnemyWithinHeightDifference) {
  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1, 18.0F, 4.0F);
  make_enemy(10.0F, 2.0F, 0.0F, 2);

  update(0.1F);

  EXPECT_EQ(arrow_count(), 2U);
}

TEST_F(SiegeSpecialProcessorTest, TowerFiresVolleyOfTwoArrows) {
  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  make_enemy(10.0F, 0.0F, 0.0F, 2);

  update(0.1F);

  EXPECT_EQ(arrow_count(), 2U);
}

TEST_F(SiegeSpecialProcessorTest, TowerDoesNotAttackBuildingEntities) {
  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* enemy_building = world->create_entity();
  enemy_building->add_component<TransformComponent>(5.0F, 0.0F, 0.0F);
  auto* unit = enemy_building->add_component<UnitComponent>(1000, 1000, 0.0F, 20.0F);
  unit->owner_id = 2;
  unit->spawn_type = Game::Units::SpawnType::Barracks;
  enemy_building->add_component<BuildingComponent>();

  update(0.1F);

  EXPECT_EQ(arrow_count(), 0U);
}

TEST_F(SiegeSpecialProcessorTest, TowerTargetsNearestEnemy) {
  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* far_enemy = make_enemy(15.0F, 0.0F, 0.0F, 2);
  auto* near_enemy = make_enemy(5.0F, 0.0F, 0.0F, 2);

  update(0.1F);

  auto* far_unit = far_enemy->get_component<UnitComponent>();
  auto* near_unit = near_enemy->get_component<UnitComponent>();
  ASSERT_NE(far_unit, nullptr);
  ASSERT_NE(near_unit, nullptr);
  EXPECT_EQ(far_unit->health, 100);
  EXPECT_LT(near_unit->health, 100);
}

TEST_F(SiegeSpecialProcessorTest, TowerCanAttackEnemyDefenseTowerInRange) {
  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* enemy_tower = make_tower(10.0F, 0.0F, 0.0F, 2);

  update(0.1F);

  EXPECT_EQ(arrow_count(), 4U);
  auto* tower_unit = tower->get_component<UnitComponent>();
  auto* enemy_unit = enemy_tower->get_component<UnitComponent>();
  ASSERT_NE(tower_unit, nullptr);
  ASSERT_NE(enemy_unit, nullptr);
  EXPECT_LT(tower_unit->health, 2000);
  EXPECT_LT(enemy_unit->health, 2000);
}

TEST_F(SiegeSpecialProcessorTest, TargetUnitRetaliatesAgainstAttackingDefenseTower) {
  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* enemy = make_enemy(10.0F, 0.0F, 0.0F, 2);
  auto* attack = enemy->add_component<AttackComponent>(2.0F, 12, 1.0F);
  auto* movement = enemy->add_component<MovementComponent>();
  auto* intent = enemy->add_component<PlayerOrderIntentComponent>();
  ASSERT_NE(attack, nullptr);
  ASSERT_NE(movement, nullptr);
  ASSERT_NE(intent, nullptr);
  movement->has_target = true;
  movement->target_x = 30.0F;
  movement->target_y = 0.0F;
  intent->kind = PlayerOrderIntentKind::ManualMove;
  intent->suppress_opportunistic_combat = true;

  update(0.1F);

  auto* attack_target = enemy->get_component<AttackTargetComponent>();
  ASSERT_NE(attack_target, nullptr);
  EXPECT_EQ(attack_target->target_id, tower->get_id());
  EXPECT_TRUE(attack_target->should_chase);
  EXPECT_FALSE(intent->suppress_opportunistic_combat);
  EXPECT_EQ(intent->kind, PlayerOrderIntentKind::None);
}

TEST_F(SiegeSpecialProcessorTest, MeleeLockedUnitDoesNotRetaliateAgainstTowerAttack) {
  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* enemy = make_enemy(10.0F, 0.0F, 0.0F, 2);
  auto* attack = enemy->add_component<AttackComponent>(2.0F, 12, 1.0F);
  ASSERT_NE(attack, nullptr);
  attack->in_melee_lock = true;
  attack->melee_lock_target_id = 999;

  update(0.1F);

  auto* attack_target = enemy->get_component<AttackTargetComponent>();
  EXPECT_TRUE((attack_target == nullptr) ||
              (attack_target->target_id != tower->get_id()));
}

TEST_F(SiegeSpecialProcessorTest, UnitDoesNotRetaliateAgainstRegularBuildingDamage) {
  auto* enemy = make_enemy(10.0F, 0.0F, 0.0F, 2);
  auto* attack = enemy->add_component<AttackComponent>(2.0F, 12, 1.0F);
  ASSERT_NE(attack, nullptr);

  auto* barracks = world->create_entity();
  barracks->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto* barracks_unit = barracks->add_component<UnitComponent>(1000, 1000, 0.0F, 20.0F);
  ASSERT_NE(barracks_unit, nullptr);
  barracks_unit->owner_id = 1;
  barracks_unit->spawn_type = Game::Units::SpawnType::Barracks;
  barracks->add_component<BuildingComponent>();

  Combat::deal_damage(world.get(), enemy, 15, barracks->get_id());

  auto* attack_target = enemy->get_component<AttackTargetComponent>();
  EXPECT_TRUE((attack_target == nullptr) || (attack_target->target_id == 0));
}
