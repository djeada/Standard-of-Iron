#include <gtest/gtest.h>
#include <set>

#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/combat_system/combat_utils.h"
#include "systems/combat_system/damage_processor.h"
#include "systems/combat_system/siege_special_processor.h"
#include "systems/owner_registry.h"
#include "systems/projectile_kind.h"
#include "systems/projectile_system.h"
#include "tests/support/movement_test_access.h"
#include "units/spawn_type.h"

using namespace Engine::Core;
using namespace Game::Systems;

class SiegeSpecialProcessorTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    world->add_system(std::make_unique<ProjectileSystem>());
    OwnerRegistry::instance().clear();
  }

  void TearDown() override { world.reset(); }

  void update(float delta_time) {
    auto query_context = Combat::build_combat_query_context(world.get());
    Combat::process_siege_specials(world.get(), query_context, delta_time);
  }

  [[nodiscard]] auto arrow_count() const -> std::size_t {
    auto* projectile_sys = world->get_system<ProjectileSystem>();
    if (projectile_sys == nullptr) {
      return 0;
    }
    return projectile_sys->projectiles().size();
  }

  void resolve_projectiles(float duration = 2.0F) {
    auto* projectile_sys = world->get_system<ProjectileSystem>();
    ASSERT_NE(projectile_sys, nullptr);
    for (float elapsed = 0.0F; elapsed < duration; elapsed += 0.05F) {
      projectile_sys->update(world.get(), 0.05F);
    }
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

  auto make_catapult(float x, float z, int owner_id) -> Entity* {
    auto* catapult = world->create_entity();
    catapult->add_component<TransformComponent>(x, 0.0F, z);
    auto* unit = catapult->add_component<UnitComponent>(420, 420, 1.0F, 20.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::Catapult;
    auto* atk = catapult->add_component<AttackComponent>(20.0F, 150, 4.5F);
    atk->can_ranged = true;
    atk->can_melee = false;
    atk->preferred_mode = AttackComponent::CombatMode::Ranged;
    atk->current_mode = AttackComponent::CombatMode::Ranged;
    atk->time_since_last = 999.0F;
    return catapult;
  }

  auto make_building(float x, float z, int owner_id) -> Entity* {
    auto* building = world->create_entity();
    building->add_component<TransformComponent>(x, 0.0F, z);
    auto* unit = building->add_component<UnitComponent>(3000, 3000, 0.0F, 20.0F);
    unit->owner_id = owner_id;
    unit->spawn_type = Game::Units::SpawnType::Barracks;
    building->add_component<BuildingComponent>();
    return building;
  }

  void aim_at(Entity& siege, Entity& target) {
    auto* attack_target = siege.get_component<AttackTargetComponent>();
    if (attack_target == nullptr) {
      attack_target = siege.add_component<AttackTargetComponent>();
    }
    attack_target->target_id = target.get_id();
  }

  [[nodiscard]] auto loaded_kind(Entity& siege) const -> ProjectileKind {
    auto const* loading = siege.get_component<CatapultLoadingComponent>();
    return loading != nullptr ? loading->loaded_projectile_kind : ProjectileKind::Arrow;
  }

  void run_until_shot_leaves(Entity& siege, float max_seconds = 6.0F) {
    for (float elapsed = 0.0F; elapsed < max_seconds; elapsed += 0.1F) {
      update(0.1F);
      if (!world->get_system<ProjectileSystem>()->projectiles().empty()) {
        return;
      }
    }
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
  make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* enemy = make_enemy(10.0F, 0.0F, 0.0F, 2);

  update(0.1F);

  EXPECT_EQ(arrow_count(), 2U);
  auto* enemy_unit = enemy->get_component<UnitComponent>();
  ASSERT_NE(enemy_unit, nullptr);
  EXPECT_EQ(enemy_unit->health, 100);
  resolve_projectiles();
  EXPECT_LT(enemy_unit->health, 100);
}

TEST_F(SiegeSpecialProcessorTest, TowerDoesNotAttackEnemyOutOfRange) {
  make_tower(0.0F, 0.0F, 0.0F, 1);
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

  make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* ally = make_enemy(10.0F, 0.0F, 0.0F, 2);

  update(0.1F);

  EXPECT_EQ(arrow_count(), 0U);
  auto* ally_unit = ally->get_component<UnitComponent>();
  ASSERT_NE(ally_unit, nullptr);
  EXPECT_EQ(ally_unit->health, 100);
}

TEST_F(SiegeSpecialProcessorTest, TowerDoesNotAttackEnemyBeyondHeightDifference) {
  make_tower(0.0F, 0.0F, 0.0F, 1, 18.0F, 4.0F);
  auto* enemy = make_enemy(10.0F, 10.0F, 0.0F, 2);

  update(0.1F);

  EXPECT_EQ(arrow_count(), 0U);
  auto* enemy_unit = enemy->get_component<UnitComponent>();
  ASSERT_NE(enemy_unit, nullptr);
  EXPECT_EQ(enemy_unit->health, 100);
}

TEST_F(SiegeSpecialProcessorTest, TowerAttacksEnemyWithinHeightDifference) {
  make_tower(0.0F, 0.0F, 0.0F, 1, 18.0F, 4.0F);
  make_enemy(10.0F, 2.0F, 0.0F, 2);

  update(0.1F);

  EXPECT_EQ(arrow_count(), 2U);
}

TEST_F(SiegeSpecialProcessorTest, TowerFiresVolleyOfTwoArrows) {
  make_tower(0.0F, 0.0F, 0.0F, 1);
  make_enemy(10.0F, 0.0F, 0.0F, 2);

  update(0.1F);

  EXPECT_EQ(arrow_count(), 2U);
}

TEST_F(SiegeSpecialProcessorTest, TowerDoesNotAttackBuildingEntities) {
  make_tower(0.0F, 0.0F, 0.0F, 1);
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
  make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* far_enemy = make_enemy(15.0F, 0.0F, 0.0F, 2);
  auto* near_enemy = make_enemy(5.0F, 0.0F, 0.0F, 2);

  update(0.1F);
  resolve_projectiles();

  auto* far_unit = far_enemy->get_component<UnitComponent>();
  auto* near_unit = near_enemy->get_component<UnitComponent>();
  ASSERT_NE(far_unit, nullptr);
  ASSERT_NE(near_unit, nullptr);
  EXPECT_EQ(far_unit->health, 100);
  EXPECT_LT(near_unit->health, 100);
}

TEST_F(SiegeSpecialProcessorTest,
       TowerArrowsVisiblyHitEnemyTowerWithoutDamagingEitherStructure) {
  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* enemy_tower = make_tower(10.0F, 0.0F, 0.0F, 2);

  update(0.1F);

  EXPECT_EQ(arrow_count(), 4U);
  auto* projectile_system = world->get_system<ProjectileSystem>();
  ASSERT_NE(projectile_system, nullptr);
  std::set<std::uint64_t> harmless_hits;
  for (float elapsed = 0.0F; elapsed < 2.0F; elapsed += 0.05F) {
    projectile_system->update(world.get(), 0.05F);
    for (auto const& impact : projectile_system->impacts()) {
      if (impact.hit_target && !impact.damage_applied) {
        harmless_hits.insert(impact.sequence);
      }
    }
  }
  auto* tower_unit = tower->get_component<UnitComponent>();
  auto* enemy_unit = enemy_tower->get_component<UnitComponent>();
  ASSERT_NE(tower_unit, nullptr);
  ASSERT_NE(enemy_unit, nullptr);
  EXPECT_EQ(tower_unit->health, 2000);
  EXPECT_EQ(enemy_unit->health, 2000);
  EXPECT_EQ(harmless_hits.size(), 4U);
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
  MovementTestAccess::set_has_target(*movement, true);
  MovementTestAccess::set_target_x(*movement, 30.0F);
  MovementTestAccess::set_target_y(*movement, 0.0F);
  intent->kind = PlayerOrderIntentKind::ManualMove;
  intent->suppress_opportunistic_combat = true;

  update(0.1F);
  resolve_projectiles();

  auto* attack_target = enemy->get_component<AttackTargetComponent>();
  ASSERT_NE(attack_target, nullptr);
  EXPECT_EQ(attack_target->target_id, tower->get_id());
  EXPECT_TRUE(attack_target->should_chase);
  EXPECT_FALSE(intent->suppress_opportunistic_combat);
  EXPECT_EQ(intent->kind, PlayerOrderIntentKind::None);
}

TEST_F(SiegeSpecialProcessorTest, ATowerBeingDismantledHoldsItsFire) {
  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* enemy = make_enemy(10.0F, 0.0F, 0.0F, 2);
  const int full_health = enemy->get_component<UnitComponent>()->health;

  tower->add_component<Engine::Core::DismantleSiteComponent>();

  update(0.1F);
  resolve_projectiles();

  EXPECT_EQ(enemy->get_component<UnitComponent>()->health, full_health)
      << "a tower being taken apart is no longer a working tower";
  auto* attack_target = enemy->get_component<AttackTargetComponent>();
  EXPECT_TRUE((attack_target == nullptr) ||
              (attack_target->target_id != tower->get_id()))
      << "and nothing should be shooting back at it";
}

TEST_F(SiegeSpecialProcessorTest, MeleeLockedUnitDoesNotRetaliateAgainstTowerAttack) {
  auto* tower = make_tower(0.0F, 0.0F, 0.0F, 1);
  auto* enemy = make_enemy(10.0F, 0.0F, 0.0F, 2);
  auto* attack = enemy->add_component<AttackComponent>(2.0F, 12, 1.0F);
  ASSERT_NE(attack, nullptr);
  attack->in_melee_lock = true;
  attack->melee_lock_target_id = 999;

  update(0.1F);
  resolve_projectiles();

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

TEST_F(SiegeSpecialProcessorTest, UnitRetaliatesAgainstAttackingEnemyUnit) {
  auto* defender = make_enemy(0.0F, 0.0F, 0.0F, 2);
  defender->add_component<AttackComponent>(2.0F, 12, 1.0F);
  auto* attacker = make_enemy(5.0F, 0.0F, 0.0F, 1);

  Combat::deal_damage(world.get(), defender, 15, attacker->get_id());

  auto* attack_target = defender->get_component<AttackTargetComponent>();
  ASSERT_NE(attack_target, nullptr);
  EXPECT_EQ(attack_target->target_id, attacker->get_id());
  EXPECT_TRUE(attack_target->should_chase);
  EXPECT_FALSE(attack_target->is_player_command);
}

TEST_F(SiegeSpecialProcessorTest, RetaliationAlertsNearbyIdleAlly) {
  auto* defender = make_enemy(0.0F, 0.0F, 0.0F, 2);
  defender->add_component<AttackComponent>(2.0F, 12, 1.0F);
  auto* ally = make_enemy(3.0F, 0.0F, 0.0F, 2);
  ally->add_component<AttackComponent>(2.0F, 12, 1.0F);
  auto* attacker = make_enemy(6.0F, 0.0F, 0.0F, 1);

  Combat::deal_damage(world.get(), defender, 15, attacker->get_id());

  auto* ally_target = ally->get_component<AttackTargetComponent>();
  ASSERT_NE(ally_target, nullptr);
  EXPECT_EQ(ally_target->target_id, attacker->get_id());
}

TEST_F(SiegeSpecialProcessorTest, RetaliationDoesNotAlertDistantAlly) {
  auto* defender = make_enemy(0.0F, 0.0F, 0.0F, 2);
  defender->add_component<AttackComponent>(2.0F, 12, 1.0F);
  auto* ally = make_enemy(40.0F, 0.0F, 0.0F, 2);
  ally->add_component<AttackComponent>(2.0F, 12, 1.0F);
  auto* attacker = make_enemy(6.0F, 0.0F, 0.0F, 1);

  Combat::deal_damage(world.get(), defender, 15, attacker->get_id());

  auto* ally_target = ally->get_component<AttackTargetComponent>();
  EXPECT_TRUE((ally_target == nullptr) || (ally_target->target_id == 0));
}

TEST_F(SiegeSpecialProcessorTest, SquadAlertReachesAnAllyUnderAManualMoveOrder) {
  auto* defender = make_enemy(0.0F, 0.0F, 0.0F, 2);
  defender->add_component<AttackComponent>(2.0F, 12, 1.0F);
  auto* ally = make_enemy(3.0F, 0.0F, 0.0F, 2);
  ally->add_component<AttackComponent>(2.0F, 12, 1.0F);
  auto* ally_move = ally->add_component<MovementComponent>();
  auto* ally_intent = ally->add_component<PlayerOrderIntentComponent>();
  MovementTestAccess::set_has_target(*ally_move, true);
  MovementTestAccess::set_target_x(*ally_move, 50.0F);
  ally_intent->kind = PlayerOrderIntentKind::ManualMove;
  ally_intent->suppress_opportunistic_combat = true;
  auto* attacker = make_enemy(6.0F, 0.0F, 0.0F, 1);

  Combat::deal_damage(world.get(), defender, 15, attacker->get_id());

  auto* ally_target = ally->get_component<AttackTargetComponent>();
  ASSERT_NE(ally_target, nullptr);
  EXPECT_EQ(ally_target->target_id, attacker->get_id())
      << "a marching ally ignored an attack on the man beside it";

  auto* defender_target = defender->get_component<AttackTargetComponent>();
  ASSERT_NE(defender_target, nullptr);
  EXPECT_EQ(defender_target->target_id, attacker->get_id());
}

TEST_F(SiegeSpecialProcessorTest, CatapultLoadsFlamingShotAgainstStructures) {
  auto* catapult = make_catapult(0.0F, 0.0F, 1);
  auto* barracks = make_building(0.0F, 12.0F, 2);
  aim_at(*catapult, *barracks);

  update(0.1F);

  EXPECT_EQ(loaded_kind(*catapult), ProjectileKind::FlamingStone);
}

TEST_F(SiegeSpecialProcessorTest, CatapultLoadsPlainShotAgainstSoldiers) {
  auto* catapult = make_catapult(0.0F, 0.0F, 1);
  auto* infantry = make_enemy(0.0F, 0.0F, 12.0F, 2);
  aim_at(*catapult, *infantry);

  update(0.1F);

  EXPECT_EQ(loaded_kind(*catapult), ProjectileKind::Stone);
}

TEST_F(SiegeSpecialProcessorTest, RetargetingFromStructureToSoldierSwapsAmmunition) {
  auto* catapult = make_catapult(0.0F, 0.0F, 1);
  auto* barracks = make_building(0.0F, 12.0F, 2);
  auto* infantry = make_enemy(4.0F, 0.0F, 10.0F, 2);
  aim_at(*catapult, *barracks);
  update(0.1F);
  ASSERT_EQ(loaded_kind(*catapult), ProjectileKind::FlamingStone);

  aim_at(*catapult, *infantry);
  update(0.1F);

  EXPECT_EQ(loaded_kind(*catapult), ProjectileKind::Stone);
  auto const* loading = catapult->get_component<CatapultLoadingComponent>();
  ASSERT_NE(loading, nullptr);
  EXPECT_EQ(loading->target_id, infantry->get_id());
}

TEST_F(SiegeSpecialProcessorTest, RetargetingFromSoldierToStructureSwapsAmmunition) {
  auto* catapult = make_catapult(0.0F, 0.0F, 1);
  auto* infantry = make_enemy(4.0F, 0.0F, 10.0F, 2);
  auto* barracks = make_building(0.0F, 12.0F, 2);
  aim_at(*catapult, *infantry);
  update(0.1F);
  ASSERT_EQ(loaded_kind(*catapult), ProjectileKind::Stone);

  aim_at(*catapult, *barracks);
  update(0.1F);

  EXPECT_EQ(loaded_kind(*catapult), ProjectileKind::FlamingStone);
}

TEST_F(SiegeSpecialProcessorTest, RetargetingKeepsTheReloadProgressAlreadyEarned) {
  auto* catapult = make_catapult(0.0F, 0.0F, 1);
  auto* barracks = make_building(0.0F, 12.0F, 2);
  auto* infantry = make_enemy(4.0F, 0.0F, 10.0F, 2);
  aim_at(*catapult, *barracks);
  for (int step = 0; step < 8; ++step) {
    update(0.1F);
  }
  auto const* loading = catapult->get_component<CatapultLoadingComponent>();
  ASSERT_NE(loading, nullptr);
  float const progress_before = loading->loading_time;
  ASSERT_GT(progress_before, 0.0F);

  aim_at(*catapult, *infantry);
  update(0.1F);

  EXPECT_GE(loading->loading_time, progress_before);
}

TEST_F(SiegeSpecialProcessorTest, CatapultFiresTheAmmunitionItLoaded) {
  auto* catapult = make_catapult(0.0F, 0.0F, 1);
  auto* barracks = make_building(0.0F, 12.0F, 2);
  aim_at(*catapult, *barracks);

  run_until_shot_leaves(*catapult);

  auto const& projectiles = world->get_system<ProjectileSystem>()->projectiles();
  ASSERT_EQ(projectiles.size(), 1U);
  EXPECT_EQ(projectiles.front()->get_kind(), ProjectileKind::FlamingStone);
}

TEST_F(SiegeSpecialProcessorTest, ShotInFlightKeepsTheKindItLaunchedWith) {
  auto* catapult = make_catapult(0.0F, 0.0F, 1);
  auto* barracks = make_building(0.0F, 12.0F, 2);
  auto* infantry = make_enemy(4.0F, 0.0F, 10.0F, 2);
  aim_at(*catapult, *barracks);
  run_until_shot_leaves(*catapult);
  auto* projectile_system = world->get_system<ProjectileSystem>();
  ASSERT_EQ(projectile_system->projectiles().size(), 1U);

  aim_at(*catapult, *infantry);
  update(0.1F);
  projectile_system->update(world.get(), 0.05F);

  ASSERT_FALSE(projectile_system->projectiles().empty());
  EXPECT_EQ(projectile_system->projectiles().front()->get_kind(),
            ProjectileKind::FlamingStone);
}

TEST_F(SiegeSpecialProcessorTest, CatapultStonesAgainstSoldiersStayPlain) {
  auto* catapult = make_catapult(0.0F, 0.0F, 1);
  auto* infantry = make_enemy(0.0F, 0.0F, 12.0F, 2);
  aim_at(*catapult, *infantry);

  run_until_shot_leaves(*catapult);

  auto const& projectiles = world->get_system<ProjectileSystem>()->projectiles();
  ASSERT_EQ(projectiles.size(), 1U);
  EXPECT_EQ(projectiles.front()->get_kind(), ProjectileKind::Stone);
}
