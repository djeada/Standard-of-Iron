#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/arrow_system.h"
#include "systems/combat_system/attack_processor.h"
#include "systems/combat_system/auto_engagement.h"
#include "systems/combat_system/combat_mode_processor.h"
#include "systems/combat_system/combat_state_processor.h"
#include "systems/combat_system/combat_types.h"
#include "systems/combat_system/combat_utils.h"
#include "systems/combat_system/damage_processor.h"
#include "systems/command_service.h"
#include "systems/owner_registry.h"
#include "units/troop_config.h"
#include <algorithm>
#include <gtest/gtest.h>

using namespace Engine::Core;
using namespace Game::Systems;

class CombatModeTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    OwnerRegistry::instance().clear();
    CommandService::initialize(64, 64);
  }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
};

TEST_F(CombatModeTest, NoAttackModeWhenMovingNearEnemy) {

  auto *attacker = world->create_entity();
  auto *attacker_transform =
      attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto *attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = true;
  attacker_attack->can_ranged = false;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Auto;

  auto *enemy = world->create_entity();
  auto *enemy_transform =
      enemy->add_component<TransformComponent>(2.0F, 0.0F, 2.0F);
  auto *enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  Combat::update_combat_mode(attacker, world.get(), attacker_attack);

  EXPECT_EQ(attacker_attack->current_mode, AttackComponent::CombatMode::Melee);
  EXPECT_FALSE(attacker_attack->in_melee_lock);
}

TEST_F(CombatModeTest, AttackModeTriggersWhenEngaged) {

  auto *attacker = world->create_entity();
  auto *attacker_transform =
      attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto *attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = true;
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Auto;
  attacker_attack->melee_range = 3.0F;
  attacker_attack->range = 10.0F;

  auto *enemy = world->create_entity();
  auto *enemy_transform =
      enemy->add_component<TransformComponent>(2.0F, 0.0F, 2.0F);
  auto *enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto *attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  Combat::update_combat_mode(attacker, world.get(), attacker_attack);

  EXPECT_EQ(attacker_attack->current_mode, AttackComponent::CombatMode::Melee);
}

TEST_F(CombatModeTest, BuildingsExcludedFromCombatMode) {

  auto *attacker = world->create_entity();
  auto *attacker_transform =
      attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto *attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = true;
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Auto;
  attacker_attack->melee_range = 3.0F;
  attacker_attack->range = 10.0F;

  auto *building = world->create_entity();
  auto *building_transform =
      building->add_component<TransformComponent>(2.0F, 0.0F, 2.0F);
  auto *building_unit =
      building->add_component<UnitComponent>(500, 500, 0.0F, 12.0F);
  building_unit->owner_id = 2;
  building->add_component<BuildingComponent>();

  auto *attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = building->get_id();
  attack_target->should_chase = false;

  Combat::update_combat_mode(attacker, world.get(), attacker_attack);

  EXPECT_EQ(attacker_attack->current_mode, AttackComponent::CombatMode::Ranged);
}

TEST_F(CombatModeTest, RangedUnitUsesRangedModeWhenNotEngaged) {

  auto *attacker = world->create_entity();
  auto *attacker_transform =
      attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto *attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = false;
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Auto;

  auto *enemy = world->create_entity();
  auto *enemy_transform =
      enemy->add_component<TransformComponent>(5.0F, 0.0F, 5.0F);
  auto *enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  Combat::update_combat_mode(attacker, world.get(), attacker_attack);

  EXPECT_EQ(attacker_attack->current_mode, AttackComponent::CombatMode::Ranged);
  EXPECT_FALSE(attacker_attack->in_melee_lock);
}

TEST_F(CombatModeTest, BuildingsDoNotMoveInMeleeLock) {

  auto *unit = world->create_entity();
  auto *unit_transform =
      unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  auto *unit_attack = unit->add_component<AttackComponent>();
  unit_attack->can_melee = true;
  unit_attack->can_ranged = false;

  auto *building = world->create_entity();
  auto *building_transform =
      building->add_component<TransformComponent>(10.0F, 0.0F, 0.0F);
  auto *building_comp =
      building->add_component<UnitComponent>(500, 500, 0.0F, 12.0F);
  building_comp->owner_id = 2;
  auto *building_attack = building->add_component<AttackComponent>();
  building_attack->can_melee = false;
  building_attack->can_ranged = true;
  building->add_component<BuildingComponent>();

  float const initial_unit_x = unit_transform->position.x;
  float const initial_building_x = building_transform->position.x;

  unit_attack->in_melee_lock = true;
  unit_attack->melee_lock_target_id = building->get_id();
  building_attack->in_melee_lock = true;
  building_attack->melee_lock_target_id = unit->get_id();

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_NE(unit_transform->position.x, initial_unit_x);

  EXPECT_EQ(building_transform->position.x, initial_building_x);
}

TEST_F(CombatModeTest, HomeDoesNotMoveInMeleeLock) {

  auto *unit = world->create_entity();
  auto *unit_transform =
      unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  auto *unit_attack = unit->add_component<AttackComponent>();
  unit_attack->can_melee = true;
  unit_attack->can_ranged = false;

  auto *home = world->create_entity();
  auto *home_transform =
      home->add_component<TransformComponent>(10.0F, 0.0F, 0.0F);
  auto *home_comp = home->add_component<UnitComponent>(1000, 1000, 0.0F, 15.0F);
  home_comp->owner_id = 2;
  auto *home_attack = home->add_component<AttackComponent>();
  home_attack->can_melee = false;
  home_attack->can_ranged = false;
  home->add_component<BuildingComponent>();

  float const initial_unit_x = unit_transform->position.x;
  float const initial_home_x = home_transform->position.x;

  unit_attack->in_melee_lock = true;
  unit_attack->melee_lock_target_id = home->get_id();
  home_attack->in_melee_lock = true;
  home_attack->melee_lock_target_id = unit->get_id();

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_NE(unit_transform->position.x, initial_unit_x);

  EXPECT_EQ(home_transform->position.x, initial_home_x);
}

TEST_F(CombatModeTest, NearestEnemyPrefersClosestValidUnit) {
  auto *attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;

  auto *building = world->create_entity();
  building->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto *building_unit =
      building->add_component<UnitComponent>(500, 500, 0.0F, 12.0F);
  building_unit->owner_id = 2;
  building->add_component<BuildingComponent>();

  auto *pending_enemy = world->create_entity();
  pending_enemy->add_component<TransformComponent>(1.5F, 0.0F, 0.0F);
  auto *pending_unit =
      pending_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  pending_unit->owner_id = 2;
  pending_enemy->add_component<PendingRemovalComponent>();

  auto *nearest_enemy = world->create_entity();
  nearest_enemy->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto *nearest_unit =
      nearest_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  nearest_unit->owner_id = 2;

  auto *far_enemy = world->create_entity();
  far_enemy->add_component<TransformComponent>(4.0F, 0.0F, 0.0F);
  auto *far_unit =
      far_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  far_unit->owner_id = 2;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  auto *target =
      Game::Systems::Combat::find_nearest_enemy(attacker, query_context, 10.0F);

  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->get_id(), nearest_enemy->get_id());
}

TEST_F(CombatModeTest, NearestEnemyIgnoresAlliesDeadAndRemovedUnits) {
  auto &owners = OwnerRegistry::instance();
  owners.register_owner_with_id(1, OwnerType::Player, "Player");
  owners.register_owner_with_id(2, OwnerType::AI, "Ally");
  owners.register_owner_with_id(3, OwnerType::AI, "Enemy");
  owners.set_owner_team(1, 1);
  owners.set_owner_team(2, 1);
  owners.set_owner_team(3, 2);

  auto *attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;

  auto *ally = world->create_entity();
  ally->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto *ally_unit = ally->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  ally_unit->owner_id = 2;

  auto *dead_enemy = world->create_entity();
  dead_enemy->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto *dead_unit =
      dead_enemy->add_component<UnitComponent>(0, 100, 1.0F, 12.0F);
  dead_unit->owner_id = 3;

  auto *removed_enemy = world->create_entity();
  removed_enemy->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  auto *removed_unit =
      removed_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  removed_unit->owner_id = 3;
  removed_enemy->add_component<PendingRemovalComponent>();

  auto *valid_enemy = world->create_entity();
  valid_enemy->add_component<TransformComponent>(4.0F, 0.0F, 0.0F);
  auto *valid_unit =
      valid_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  valid_unit->owner_id = 3;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  auto *target =
      Game::Systems::Combat::find_nearest_enemy(attacker, query_context, 10.0F);

  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->get_id(), valid_enemy->get_id());
}

TEST_F(CombatModeTest, CombatQueryContextSkipsRemovedAndDeadSpatialEntries) {
  auto *attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;

  auto *removed_enemy = world->create_entity();
  removed_enemy->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto *removed_unit =
      removed_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  removed_unit->owner_id = 2;
  removed_enemy->add_component<PendingRemovalComponent>();

  auto *dead_enemy = world->create_entity();
  dead_enemy->add_component<TransformComponent>(1.5F, 0.0F, 0.0F);
  auto *dead_unit =
      dead_enemy->add_component<UnitComponent>(0, 100, 1.0F, 12.0F);
  dead_unit->owner_id = 2;

  auto *building = world->create_entity();
  building->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto *building_unit =
      building->add_component<UnitComponent>(500, 500, 0.0F, 12.0F);
  building_unit->owner_id = 2;
  building->add_component<BuildingComponent>();

  auto *valid_enemy = world->create_entity();
  valid_enemy->add_component<TransformComponent>(2.5F, 0.0F, 0.0F);
  auto *valid_unit =
      valid_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  valid_unit->owner_id = 2;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());

  EXPECT_EQ(query_context.entities_by_id.count(removed_enemy->get_id()), 0U);
  EXPECT_EQ(query_context.entities_by_id.count(dead_enemy->get_id()), 0U);
  EXPECT_FALSE(
      std::any_of(query_context.units.begin(), query_context.units.end(),
                  [removed_enemy, dead_enemy](Entity *entity) {
                    return entity == removed_enemy || entity == dead_enemy;
                  }));

  std::uint64_t scan_iterations = 0;
  auto *target = Game::Systems::Combat::find_nearest_enemy(
      attacker, query_context, 10.0F, &scan_iterations);

  ASSERT_NE(target, nullptr);
  EXPECT_EQ(target->get_id(), valid_enemy->get_id());
  EXPECT_EQ(scan_iterations, 2U);
}

TEST_F(CombatModeTest, EnemyValidityHelperAllowsBuildingsOnlyWhenRequested) {
  auto *attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;

  auto *building = world->create_entity();
  building->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto *building_unit =
      building->add_component<UnitComponent>(500, 500, 0.0F, 12.0F);
  building_unit->owner_id = 2;
  building->add_component<BuildingComponent>();

  EXPECT_FALSE(Game::Systems::Combat::is_valid_enemy_unit(attacker_unit,
                                                          building, false));
  EXPECT_TRUE(Game::Systems::Combat::is_valid_enemy_unit(attacker_unit,
                                                         building, true));
}

TEST_F(CombatModeTest, AutoEngagementUsesNearestEnemyInsideGuardRadius) {
  auto *attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Spearman;
  auto *attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  auto *guard_mode = attacker->add_component<GuardModeComponent>();
  guard_mode->active = true;
  guard_mode->guard_position_x = 0.0F;
  guard_mode->guard_position_z = 0.0F;
  guard_mode->guard_radius = 3.0F;

  auto *inside_enemy = world->create_entity();
  inside_enemy->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto *inside_unit =
      inside_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  inside_unit->owner_id = 2;

  auto *outside_enemy = world->create_entity();
  outside_enemy->add_component<TransformComponent>(5.0F, 0.0F, 0.0F);
  auto *outside_unit =
      outside_enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  outside_unit->owner_id = 2;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::AutoEngagement auto_engagement;
  auto_engagement.process(world.get(), query_context, 0.016F);

  auto *attack_target = attacker->get_component<AttackTargetComponent>();
  ASSERT_NE(attack_target, nullptr);
  EXPECT_EQ(attack_target->target_id, inside_enemy->get_id());
}

TEST_F(CombatModeTest, AutoEngagementIgnoresCloserBuildingAndTargetsEnemyUnit) {
  auto *attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Spearman;
  auto *attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;

  auto *building = world->create_entity();
  building->add_component<TransformComponent>(2.0F, 0.0F, 0.0F);
  auto *building_unit =
      building->add_component<UnitComponent>(500, 500, 0.0F, 12.0F);
  building_unit->owner_id = 2;
  building->add_component<BuildingComponent>();

  auto *enemy = world->create_entity();
  enemy->add_component<TransformComponent>(4.0F, 0.0F, 0.0F);
  auto *enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::AutoEngagement auto_engagement;
  auto_engagement.process(world.get(), query_context, 0.016F);

  auto *attack_target = attacker->get_component<AttackTargetComponent>();
  ASSERT_NE(attack_target, nullptr);
  EXPECT_EQ(attack_target->target_id, enemy->get_id());
  EXPECT_TRUE(attack_target->should_chase);
}

TEST_F(CombatModeTest, ChasingUnitDoesNotRequestNewPathEveryFrame) {
  auto *attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto *attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->cooldown = 0.0F;
  attack->melee_cooldown = 0.0F;

  auto *enemy = world->create_entity();
  enemy->add_component<TransformComponent>(6.0F, 0.0F, 0.0F);
  auto *enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto *attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  auto const first_query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), first_query_context,
                                         0.016F);

  auto *movement = attacker->get_component<MovementComponent>();
  ASSERT_NE(movement, nullptr);
  EXPECT_TRUE(movement->path_pending);
  std::uint64_t const first_request_id = movement->pending_request_id;
  ASSERT_NE(first_request_id, 0U);

  auto const second_query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), second_query_context,
                                         0.016F);

  EXPECT_TRUE(movement->path_pending);
  EXPECT_EQ(movement->pending_request_id, first_request_id);
}

TEST_F(CombatModeTest, ChasingUnitRequestsNewPathWhenTargetMovesFarEnough) {
  auto *attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto *attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->cooldown = 0.0F;
  attack->melee_cooldown = 0.0F;

  auto *enemy = world->create_entity();
  auto *enemy_transform =
      enemy->add_component<TransformComponent>(6.0F, 0.0F, 0.0F);
  auto *enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto *attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  auto const first_query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), first_query_context,
                                         0.016F);

  auto *movement = attacker->get_component<MovementComponent>();
  ASSERT_NE(movement, nullptr);
  std::uint64_t const first_request_id = movement->pending_request_id;
  ASSERT_NE(first_request_id, 0U);

  movement->path_pending = false;
  movement->has_target = true;
  movement->target_x = movement->goal_x;
  movement->target_y = movement->goal_y;

  enemy_transform->position.x = 10.0F;
  enemy_transform->position.z = 0.0F;

  auto const second_query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), second_query_context,
                                         0.016F);

  EXPECT_TRUE(movement->path_pending);
  EXPECT_NE(movement->pending_request_id, first_request_id);
}

TEST_F(CombatModeTest, PendingPathDoesNotSubmitDuplicateEquivalentRequest) {
  auto *attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto *attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->cooldown = 0.0F;
  attack->melee_cooldown = 0.0F;

  auto *enemy = world->create_entity();
  enemy->add_component<TransformComponent>(7.0F, 0.0F, 0.0F);
  auto *enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto *attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  auto const first_query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), first_query_context,
                                         0.016F);

  auto *movement = attacker->get_component<MovementComponent>();
  ASSERT_NE(movement, nullptr);
  std::uint64_t const first_request_id = movement->pending_request_id;
  ASSERT_NE(first_request_id, 0U);

  auto const second_query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), second_query_context,
                                         0.016F);

  EXPECT_EQ(movement->pending_request_id, first_request_id);
}

TEST_F(CombatModeTest, UnitStillAttacksWhenTargetReachesRange) {
  auto *attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  auto *attack = attacker->add_component<AttackComponent>();
  attack->can_melee = true;
  attack->can_ranged = false;
  attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attack->melee_range = 2.0F;
  attack->damage = 10;
  attack->melee_damage = 10;
  attack->cooldown = 0.0F;
  attack->melee_cooldown = 0.0F;

  auto *enemy = world->create_entity();
  enemy->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto *enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto *attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_LT(enemy_unit->health, 100);
  auto *movement = attacker->get_component<MovementComponent>();
  EXPECT_TRUE((movement == nullptr) || !movement->path_pending);
}

TEST_F(CombatModeTest,
       RepeatedMeleeLockDoesNotResetExistingAttackAnimationSeed) {
  auto *attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Spearman;
  auto *attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = true;
  attacker_attack->can_ranged = false;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Melee;
  attacker_attack->current_mode = AttackComponent::CombatMode::Melee;
  attacker_attack->cooldown = 0.0F;
  attacker_attack->melee_cooldown = 0.0F;
  attacker_attack->time_since_last = 1.0F;
  attacker_attack->in_melee_lock = true;

  auto *enemy = world->create_entity();
  enemy->add_component<TransformComponent>(0.5F, 0.0F, 0.0F);
  auto *enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;
  auto *enemy_attack = enemy->add_component<AttackComponent>();
  enemy_attack->in_melee_lock = true;
  enemy_attack->melee_lock_target_id = attacker->get_id();

  attacker_attack->melee_lock_target_id = enemy->get_id();

  auto *attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = false;

  auto *combat_state = attacker->add_component<CombatStateComponent>();
  combat_state->animation_state = CombatAnimationState::Idle;
  combat_state->attack_offset = 0.123F;
  combat_state->attack_variant = 2;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  EXPECT_FLOAT_EQ(combat_state->attack_offset, 0.123F);
  EXPECT_EQ(combat_state->attack_variant, 2);
}

TEST_F(CombatModeTest, RangedVolleyVisualsAreClampedWithoutChangingDamage) {
  struct RefreshTroopConfigOnExit {
    ~RefreshTroopConfigOnExit() {
      Game::Units::TroopConfig::instance().refresh_from_catalog();
    }
  } refresh_guard;

  world->add_system(std::make_unique<Game::Systems::ArrowSystem>());
  Game::Units::TroopConfig::instance().register_troop_type(
      Game::Units::TroopType::Archer, 60);

  auto *attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Archer;
  auto *attacker_attack = attacker->add_component<AttackComponent>();
  attacker_attack->can_melee = false;
  attacker_attack->can_ranged = true;
  attacker_attack->preferred_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->current_mode = AttackComponent::CombatMode::Ranged;
  attacker_attack->range = 10.0F;
  attacker_attack->damage = 17;
  attacker_attack->cooldown = 0.0F;
  attacker_attack->time_since_last = 1.0F;

  auto *enemy = world->create_entity();
  enemy->add_component<TransformComponent>(3.0F, 0.0F, 0.0F);
  auto *enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  auto *attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = false;

  auto const query_context =
      Game::Systems::Combat::build_combat_query_context(world.get());
  Game::Systems::Combat::process_attacks(world.get(), query_context, 0.016F);

  auto *arrow_system = world->get_system<Game::Systems::ArrowSystem>();
  ASSERT_NE(arrow_system, nullptr);
  EXPECT_EQ(
      arrow_system->arrows().size(),
      static_cast<std::size_t>(
          Game::Systems::Combat::Constants::k_max_visual_arrows_per_volley));
  EXPECT_EQ(enemy_unit->health, 83);
}

TEST_F(CombatModeTest, HitPauseUsesPerUnitCombatAnimationConstant) {
  auto *attacker = world->create_entity();
  attacker->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *attacker_unit =
      attacker->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  attacker_unit->owner_id = 1;
  attacker_unit->spawn_type = Game::Units::SpawnType::Spearman;

  auto *target = world->create_entity();
  target->add_component<TransformComponent>(1.0F, 0.0F, 0.0F);
  auto *target_unit =
      target->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  target_unit->owner_id = 2;
  auto *combat_state = target->add_component<CombatStateComponent>();
  combat_state->animation_state = CombatAnimationState::WindUp;
  combat_state->state_duration = 1.0F;
  combat_state->state_time = 0.25F;

  Game::Systems::Combat::deal_damage(world.get(), target, 5,
                                     attacker->get_id());

  EXPECT_TRUE(combat_state->is_hit_paused);
  EXPECT_FLOAT_EQ(combat_state->hit_pause_remaining,
                  CombatStateComponent::k_combat_animation_hit_pause_duration);

  Game::Systems::Combat::process_combat_state(
      world.get(),
      CombatStateComponent::k_combat_animation_hit_pause_duration * 0.5F);
  EXPECT_TRUE(combat_state->is_hit_paused);
  EXPECT_FLOAT_EQ(combat_state->state_time, 0.25F);

  Game::Systems::Combat::process_combat_state(
      world.get(),
      CombatStateComponent::k_combat_animation_hit_pause_duration * 0.5F);
  EXPECT_FALSE(combat_state->is_hit_paused);
  EXPECT_FLOAT_EQ(combat_state->hit_pause_remaining, 0.0F);

  Game::Systems::Combat::process_combat_state(world.get(), 0.1F);
  EXPECT_GT(combat_state->state_time, 0.25F);
}
