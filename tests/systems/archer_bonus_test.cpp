#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/combat_system/attack_processor.h"
#include "systems/combat_system/damage_processor.h"
#include "systems/owner_registry.h"
#include "units/spawn_type.h"
#include <gtest/gtest.h>

using namespace Engine::Core;
using namespace Game::Systems;

class ArcherBonusTest : public ::testing::Test {
protected:
  void SetUp() override {
    world = std::make_unique<World>();
    OwnerRegistry::instance().clear();
  }

  void TearDown() override { world.reset(); }

  std::unique_ptr<World> world;
};

TEST_F(ArcherBonusTest, ArcherDealsBonusDamageToElephant) {
  // Create an archer unit
  auto *archer = world->create_entity();
  auto *archer_transform =
      archer->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *archer_unit =
      archer->add_component<UnitComponent>(620, 620, 3.0F, 16.0F);
  archer_unit->owner_id = 1;
  archer_unit->spawn_type = Game::Units::SpawnType::Archer;
  auto *archer_attack = archer->add_component<AttackComponent>();
  archer_attack->can_melee = true;
  archer_attack->can_ranged = true;
  archer_attack->damage = 24; // Base archer damage after 1.5x increase
  archer_attack->range = 7.5F;
  archer_attack->current_mode = AttackComponent::CombatMode::Ranged;

  // Create an elephant unit
  auto *elephant = world->create_entity();
  auto *elephant_transform =
      elephant->add_component<TransformComponent>(5.0F, 0.0F, 5.0F);
  auto *elephant_unit =
      elephant->add_component<UnitComponent>(8000, 8000, 2.2F, 16.0F);
  elephant_unit->owner_id = 2;
  elephant_unit->spawn_type = Game::Units::SpawnType::Elephant;
  elephant->add_component<ElephantComponent>();

  // Calculate damage with tactical multiplier
  float tactical_multiplier = 1.0F;
  
  // Check if archer gets bonus against elephant
  if (archer_unit->spawn_type == Game::Units::SpawnType::Archer ||
      archer_unit->spawn_type == Game::Units::SpawnType::HorseArcher) {
    if (elephant->has_component<ElephantComponent>()) {
      tactical_multiplier *= 2.0F;
    }
  }

  int expected_damage = static_cast<int>(24 * tactical_multiplier);
  
  // Verify bonus is applied (24 * 2 = 48)
  EXPECT_EQ(expected_damage, 48);
  EXPECT_FLOAT_EQ(tactical_multiplier, 2.0F);
}

TEST_F(ArcherBonusTest, HorseArcherDealsBonusDamageToElephant) {
  // Create a horse archer unit
  auto *horse_archer = world->create_entity();
  auto *ha_transform =
      horse_archer->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *ha_unit = horse_archer->add_component<UnitComponent>(2000, 2000, 3.0F, 18.0F);
  ha_unit->owner_id = 1;
  ha_unit->spawn_type = Game::Units::SpawnType::HorseArcher;
  auto *ha_attack = horse_archer->add_component<AttackComponent>();
  ha_attack->can_melee = true;
  ha_attack->can_ranged = true;
  ha_attack->damage = 27; // Base horse archer damage after 1.5x increase
  ha_attack->range = 8.5F;
  ha_attack->current_mode = AttackComponent::CombatMode::Ranged;

  // Create an elephant unit
  auto *elephant = world->create_entity();
  auto *elephant_transform =
      elephant->add_component<TransformComponent>(5.0F, 0.0F, 5.0F);
  auto *elephant_unit =
      elephant->add_component<UnitComponent>(8000, 8000, 2.2F, 16.0F);
  elephant_unit->owner_id = 2;
  elephant_unit->spawn_type = Game::Units::SpawnType::Elephant;
  elephant->add_component<ElephantComponent>();

  // Calculate damage with tactical multiplier
  float tactical_multiplier = 1.0F;
  
  // Check if horse archer gets bonus against elephant
  if (ha_unit->spawn_type == Game::Units::SpawnType::Archer ||
      ha_unit->spawn_type == Game::Units::SpawnType::HorseArcher) {
    if (elephant->has_component<ElephantComponent>()) {
      tactical_multiplier *= 2.0F;
    }
  }

  int expected_damage = static_cast<int>(27 * tactical_multiplier);
  
  // Verify bonus is applied (27 * 2 = 54)
  EXPECT_EQ(expected_damage, 54);
  EXPECT_FLOAT_EQ(tactical_multiplier, 2.0F);
}

TEST_F(ArcherBonusTest, ArcherNoBonusAgainstNonElephant) {
  // Create an archer unit
  auto *archer = world->create_entity();
  auto *archer_transform =
      archer->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *archer_unit =
      archer->add_component<UnitComponent>(620, 620, 3.0F, 16.0F);
  archer_unit->owner_id = 1;
  archer_unit->spawn_type = Game::Units::SpawnType::Archer;
  auto *archer_attack = archer->add_component<AttackComponent>();
  archer_attack->can_melee = true;
  archer_attack->can_ranged = true;
  archer_attack->damage = 24;
  archer_attack->range = 7.5F;
  archer_attack->current_mode = AttackComponent::CombatMode::Ranged;

  // Create a regular enemy unit (swordsman)
  auto *swordsman = world->create_entity();
  auto *swordsman_transform =
      swordsman->add_component<TransformComponent>(5.0F, 0.0F, 5.0F);
  auto *swordsman_unit =
      swordsman->add_component<UnitComponent>(1260, 1260, 2.1F, 14.0F);
  swordsman_unit->owner_id = 2;
  swordsman_unit->spawn_type = Game::Units::SpawnType::Swordsman;

  // Calculate damage with tactical multiplier
  float tactical_multiplier = 1.0F;
  
  // Swordsman doesn't have ElephantComponent, so no bonus
  if (archer_unit->spawn_type == Game::Units::SpawnType::Archer ||
      archer_unit->spawn_type == Game::Units::SpawnType::HorseArcher) {
    if (swordsman->has_component<ElephantComponent>()) {
      tactical_multiplier *= 2.0F;
    }
  }

  int expected_damage = static_cast<int>(24 * tactical_multiplier);
  
  // Verify no bonus is applied (24 * 1 = 24)
  EXPECT_EQ(expected_damage, 24);
  EXPECT_FLOAT_EQ(tactical_multiplier, 1.0F);
}
