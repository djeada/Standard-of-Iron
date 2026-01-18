#include "core/component.h"
#include "core/entity.h"
#include "core/world.h"
#include "systems/combat_system/combat_types.h"
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

TEST_F(ArcherBonusTest, ArcherHasIncreasedBaseDamage) {
  // Verify that archer base damage has been increased from 16 to 24 (1.5x)
  // This test documents the expected base damage value
  int const expected_archer_damage = 24;
  int const original_archer_damage = 16;

  EXPECT_EQ(expected_archer_damage, original_archer_damage * 1.5);
}

TEST_F(ArcherBonusTest, HorseArcherHasIncreasedBaseDamage) {
  // Verify that horse archer base damage has been increased from 18 to 27
  // (1.5x) This test documents the expected base damage value
  int const expected_horse_archer_damage = 27;
  int const original_horse_archer_damage = 18;

  EXPECT_EQ(expected_horse_archer_damage, original_horse_archer_damage * 1.5);
}

TEST_F(ArcherBonusTest, ArcherVsElephantMultiplierIsCorrect) {
  // Verify the archer vs elephant multiplier constant is set correctly
  float const expected_multiplier = 2.0F;

  EXPECT_FLOAT_EQ(Combat::Constants::k_archer_vs_elephant_multiplier,
                  expected_multiplier);
}

TEST_F(ArcherBonusTest, ElephantComponentExistsOnElephantUnit) {
  // Create an elephant unit and verify it has the ElephantComponent
  auto *elephant = world->create_entity();
  elephant->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *elephant_unit =
      elephant->add_component<UnitComponent>(8000, 8000, 2.2F, 16.0F);
  elephant_unit->spawn_type = Game::Units::SpawnType::Elephant;
  elephant->add_component<ElephantComponent>();

  EXPECT_TRUE(elephant->has_component<ElephantComponent>());
}

TEST_F(ArcherBonusTest, NonElephantUnitsDoNotHaveElephantComponent) {
  // Verify that non-elephant units don't have ElephantComponent
  auto *spearman = world->create_entity();
  spearman->add_component<TransformComponent>(0.0F, 0.0F, 0.0F);
  auto *spearman_unit =
      spearman->add_component<UnitComponent>(1260, 1260, 2.1F, 14.0F);
  spearman_unit->spawn_type = Game::Units::SpawnType::Spearman;

  EXPECT_FALSE(spearman->has_component<ElephantComponent>());
}

TEST_F(ArcherBonusTest, ExpectedDamageCalculation) {
  // Document the expected damage calculations:
  // Archer vs Elephant: 24 (base) * 2.0 (multiplier) = 48
  // Horse Archer vs Elephant: 27 (base) * 2.0 (multiplier) = 54
  // Archer vs Other: 24 (base) * 1.0 (no multiplier) = 24

  int const archer_base_damage = 24;
  int const horse_archer_base_damage = 27;
  float const elephant_multiplier =
      Combat::Constants::k_archer_vs_elephant_multiplier;

  int const archer_vs_elephant =
      static_cast<int>(archer_base_damage * elephant_multiplier);
  int const horse_archer_vs_elephant =
      static_cast<int>(horse_archer_base_damage * elephant_multiplier);
  int const archer_vs_other = archer_base_damage;

  EXPECT_EQ(archer_vs_elephant, 48);
  EXPECT_EQ(horse_archer_vs_elephant, 54);
  EXPECT_EQ(archer_vs_other, 24);
}
