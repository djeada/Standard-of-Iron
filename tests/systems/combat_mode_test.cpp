#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/combat_system/combat_mode_processor.h"
#include "systems/combat_system/attack_processor.h"
#include "systems/owner_registry.h"
#include <gtest/gtest.h>

using namespace Engine::Core;
using namespace Game::Systems;

class CombatModeTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    OwnerRegistry::instance().clear();
  }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
};

TEST_F(CombatModeTest, NoAttackModeWhenMovingNearEnemy) {
  // Create an attacker unit
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

  // Create an enemy unit nearby
  auto *enemy = world->create_entity();
  auto *enemy_transform =
      enemy->add_component<TransformComponent>(2.0F, 0.0F, 2.0F);
  auto *enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  // Update combat mode - should not trigger attack mode because unit is not
  // engaged
  Combat::update_combat_mode(attacker, world.get(), attacker_attack);

  // Combat mode should remain in default (Melee for melee-only units) and not
  // switch based on proximity
  EXPECT_EQ(attacker_attack->current_mode, AttackComponent::CombatMode::Melee);
  EXPECT_FALSE(attacker_attack->in_melee_lock);
}

TEST_F(CombatModeTest, AttackModeTriggersWhenEngaged) {
  // Create an attacker unit
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

  // Create an enemy unit nearby
  auto *enemy = world->create_entity();
  auto *enemy_transform =
      enemy->add_component<TransformComponent>(2.0F, 0.0F, 2.0F);
  auto *enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  // Engage in combat by adding attack target
  auto *attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = enemy->get_id();
  attack_target->should_chase = true;

  // Update combat mode - should now calculate proper mode since engaged
  Combat::update_combat_mode(attacker, world.get(), attacker_attack);

  // Since enemy is close (within melee range), should be in melee mode
  EXPECT_EQ(attacker_attack->current_mode, AttackComponent::CombatMode::Melee);
}

TEST_F(CombatModeTest, BuildingsExcludedFromCombatMode) {
  // Create an attacker unit
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

  // Create an enemy building nearby
  auto *building = world->create_entity();
  auto *building_transform =
      building->add_component<TransformComponent>(2.0F, 0.0F, 2.0F);
  auto *building_unit =
      building->add_component<UnitComponent>(500, 500, 0.0F, 12.0F);
  building_unit->owner_id = 2;
  building->add_component<BuildingComponent>();

  // Engage with the building by adding attack target
  auto *attack_target = attacker->add_component<AttackTargetComponent>();
  attack_target->target_id = building->get_id();
  attack_target->should_chase = false;

  // Update combat mode - buildings should be excluded from calculation
  Combat::update_combat_mode(attacker, world.get(), attacker_attack);

  // Since building is excluded, should use ranged mode (default for
  // ranged-capable units)
  EXPECT_EQ(attacker_attack->current_mode, AttackComponent::CombatMode::Ranged);
}

TEST_F(CombatModeTest, RangedUnitUsesRangedModeWhenNotEngaged) {
  // Create a ranged attacker unit
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

  // Create an enemy unit nearby
  auto *enemy = world->create_entity();
  auto *enemy_transform =
      enemy->add_component<TransformComponent>(5.0F, 0.0F, 5.0F);
  auto *enemy_unit = enemy->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  enemy_unit->owner_id = 2;

  // Update combat mode - should keep ranged mode as default
  Combat::update_combat_mode(attacker, world.get(), attacker_attack);

  // Ranged unit should be in ranged mode
  EXPECT_EQ(attacker_attack->current_mode, AttackComponent::CombatMode::Ranged);
  EXPECT_FALSE(attacker_attack->in_melee_lock);
}

TEST_F(CombatModeTest, BuildingsDoNotMoveInMeleeLock) {
  // Create a regular unit
  auto *unit = world->create_entity();
  auto *unit_transform =
      unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  auto *unit_attack = unit->add_component<AttackComponent>();
  unit_attack->can_melee = true;
  unit_attack->can_ranged = false;

  // Create a building (defense tower) at distance
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

  // Store initial positions
  float const initial_unit_x = unit_transform->position.x;
  float const initial_building_x = building_transform->position.x;

  // Simulate melee lock initiation
  unit_attack->in_melee_lock = true;
  unit_attack->melee_lock_target_id = building->get_id();
  building_attack->in_melee_lock = true;
  building_attack->melee_lock_target_id = unit->get_id();

  // Process attacks which includes melee lock processing
  Game::Systems::Combat::process_attacks(world.get(), 0.016F);

  // Regular unit should have moved toward the building
  EXPECT_NE(unit_transform->position.x, initial_unit_x);

  // Building should NOT have moved
  EXPECT_EQ(building_transform->position.x, initial_building_x);
}

TEST_F(CombatModeTest, HomeDoesNotMoveInMeleeLock) {
  // Create a regular unit
  auto *unit = world->create_entity();
  auto *unit_transform =
      unit->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *unit_comp = unit->add_component<UnitComponent>(100, 100, 1.0F, 12.0F);
  unit_comp->owner_id = 1;
  auto *unit_attack = unit->add_component<AttackComponent>();
  unit_attack->can_melee = true;
  unit_attack->can_ranged = false;

  // Create a home building at distance
  auto *home = world->create_entity();
  auto *home_transform =
      home->add_component<TransformComponent>(10.0F, 0.0F, 0.0F);
  auto *home_comp = home->add_component<UnitComponent>(1000, 1000, 0.0F, 15.0F);
  home_comp->owner_id = 2;
  auto *home_attack = home->add_component<AttackComponent>();
  home_attack->can_melee = false;
  home_attack->can_ranged = false;
  home->add_component<BuildingComponent>();

  // Store initial positions
  float const initial_unit_x = unit_transform->position.x;
  float const initial_home_x = home_transform->position.x;

  // Simulate melee lock initiation
  unit_attack->in_melee_lock = true;
  unit_attack->melee_lock_target_id = home->get_id();
  home_attack->in_melee_lock = true;
  home_attack->melee_lock_target_id = unit->get_id();

  // Process attacks which includes melee lock processing
  Game::Systems::Combat::process_attacks(world.get(), 0.016F);

  // Regular unit should have moved toward the home
  EXPECT_NE(unit_transform->position.x, initial_unit_x);

  // Home building should NOT have moved
  EXPECT_EQ(home_transform->position.x, initial_home_x);
}

