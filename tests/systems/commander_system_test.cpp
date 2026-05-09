#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/commander_system.h"
#include "game/systems/production_service.h"
#include "game/units/commander_catalog.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

#include <gtest/gtest.h>
#include <vector>

namespace {

TEST(CommanderCatalogTest, DefinesThreeCommandersForEachPlayableNation) {
  auto roman = Game::Units::commander_definitions_for_nation(
      Game::Systems::NationID::RomanRepublic);
  auto carthage = Game::Units::commander_definitions_for_nation(
      Game::Systems::NationID::Carthage);

  ASSERT_EQ(roman.size(), 3U);
  ASSERT_EQ(carthage.size(), 3U);
  bool has_hannibal = false;
  bool has_health_bonus = false;
  bool has_attack_bonus = false;
  bool has_production_bonus = false;

  auto inspect_definition = [&](const Game::Units::CommanderDefinition *definition) {
    ASSERT_NE(definition, nullptr);
    EXPECT_FALSE(definition->strategic_identity.empty());
    EXPECT_FALSE(definition->recruitment_effect.empty());
    EXPECT_FALSE(definition->battlefield_role.empty());
    EXPECT_FALSE(definition->strengths.empty());
    EXPECT_FALSE(definition->weaknesses.empty());
    EXPECT_FALSE(definition->visual_requirements.empty());
    EXPECT_FALSE(definition->bonus_type.empty());
    EXPECT_FALSE(definition->bonus_summary.empty());
    EXPECT_TRUE(Game::Units::is_commander_troop(definition->troop_type));

    has_hannibal = has_hannibal || definition->display_name == "Hannibal Barca";
    has_health_bonus = has_health_bonus || definition->bonus_type == "health_regen";
    has_attack_bonus = has_attack_bonus || definition->bonus_type == "attack_boost";
    has_production_bonus =
        has_production_bonus || definition->bonus_type == "production_haste";
  };

  for (const auto *definition : roman) {
    inspect_definition(definition);
  }
  for (const auto *definition : carthage) {
    inspect_definition(definition);
  }

  EXPECT_TRUE(has_hannibal);
  EXPECT_TRUE(has_health_bonus);
  EXPECT_TRUE(has_attack_bonus);
  EXPECT_TRUE(has_production_bonus);
}

TEST(CommanderProductionTest, ReservesOneCommanderPerOwnerWhenQueued) {
  Engine::Core::World world;

  auto *first_barracks = world.create_entity();
  auto *first_unit = first_barracks->add_component<Engine::Core::UnitComponent>();
  auto *first_production =
      first_barracks->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(first_unit, nullptr);
  ASSERT_NE(first_production, nullptr);
  first_unit->spawn_type = Game::Units::SpawnType::Barracks;
  first_unit->owner_id = 1;
  first_production->max_units = 10000;
  first_production->manpower_available = 1000;

  auto *second_barracks = world.create_entity();
  auto *second_unit =
      second_barracks->add_component<Engine::Core::UnitComponent>();
  auto *second_production =
      second_barracks->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(second_unit, nullptr);
  ASSERT_NE(second_production, nullptr);
  second_unit->spawn_type = Game::Units::SpawnType::Barracks;
  second_unit->owner_id = 1;
  second_production->max_units = 10000;
  second_production->manpower_available = 1000;

  auto result = Game::Systems::ProductionService::
      start_production_for_first_selected_barracks(
          world, {first_barracks->get_id()}, 1,
          Game::Units::TroopType::RomanLegionOrganizer);
  EXPECT_EQ(result, Game::Systems::ProductionResult::Success);
  EXPECT_TRUE(first_production->commander_committed);

  result = Game::Systems::ProductionService::
      start_production_for_first_selected_barracks(
          world, {second_barracks->get_id()}, 1,
          Game::Units::TroopType::RomanVeteranConsul);
  EXPECT_EQ(result, Game::Systems::ProductionResult::CommanderLimitReached);
  EXPECT_FALSE(second_production->commander_committed);
  EXPECT_FALSE(second_production->in_progress);
}

TEST(CommanderSystemTest, CommanderDeathDisablesAuraAndShocksNearbyAllies) {
  Engine::Core::World world;

  auto *commander = world.create_entity();
  auto *commander_unit =
      commander->add_component<Engine::Core::UnitComponent>();
  auto *commander_transform =
      commander->add_component<Engine::Core::TransformComponent>();
  auto *commander_data =
      commander->add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_unit, nullptr);
  ASSERT_NE(commander_transform, nullptr);
  ASSERT_NE(commander_data, nullptr);
  commander_unit->owner_id = 1;
  commander_unit->health = 0;
  commander_data->death_morale_shock = 30.0F;
  commander_data->death_shock_radius = 8.0F;

  auto *ally = world.create_entity();
  auto *ally_unit = ally->add_component<Engine::Core::UnitComponent>();
  auto *ally_transform = ally->add_component<Engine::Core::TransformComponent>();
  auto *ally_morale = ally->add_component<Engine::Core::MoraleComponent>();
  ASSERT_NE(ally_unit, nullptr);
  ASSERT_NE(ally_transform, nullptr);
  ASSERT_NE(ally_morale, nullptr);
  ally_unit->owner_id = 1;
  ally_unit->health = 100;
  ally_transform->position = {3.0F, 0.0F, 0.0F};
  ally_morale->morale = 50.0F;

  Game::Systems::CommanderSystem system;
  system.update(&world, 0.1F);

  EXPECT_TRUE(commander_data->wounded);
  EXPECT_FALSE(commander_data->aura_active);
  EXPECT_FLOAT_EQ(ally_morale->morale, 20.0F);
  EXPECT_TRUE(ally_morale->wavering);
}

TEST(CommanderSystemTest, AuraAppliesAttackAndProductionBonusesByType) {
  Engine::Core::World world;

  auto *commander = world.create_entity();
  auto *commander_unit =
      commander->add_component<Engine::Core::UnitComponent>();
  auto *commander_transform =
      commander->add_component<Engine::Core::TransformComponent>();
  auto *commander_data =
      commander->add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_unit, nullptr);
  ASSERT_NE(commander_transform, nullptr);
  ASSERT_NE(commander_data, nullptr);
  commander_unit->owner_id = 1;
  commander_unit->health = 100;
  commander_unit->nation_id = Game::Systems::NationID::RomanRepublic;
  commander_transform->position = {0.0F, 0.0F, 0.0F};
  commander_data->bonus_type = "attack_boost";
  commander_data->aura_bonus_value = 0.25F;
  commander_data->aura_radius = 10.0F;

  auto *ally = world.create_entity();
  auto *ally_unit = ally->add_component<Engine::Core::UnitComponent>();
  auto *ally_transform = ally->add_component<Engine::Core::TransformComponent>();
  auto *ally_attack = ally->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(ally_unit, nullptr);
  ASSERT_NE(ally_transform, nullptr);
  ASSERT_NE(ally_attack, nullptr);
  ally_unit->owner_id = 1;
  ally_unit->health = 100;
  ally_unit->spawn_type = Game::Units::SpawnType::Knight;
  ally_unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ally_transform->position = {2.0F, 0.0F, 0.0F};

  auto *barracks = world.create_entity();
  auto *barracks_unit =
      barracks->add_component<Engine::Core::UnitComponent>();
  auto *barracks_transform =
      barracks->add_component<Engine::Core::TransformComponent>();
  auto *barracks_prod =
      barracks->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(barracks_unit, nullptr);
  ASSERT_NE(barracks_transform, nullptr);
  ASSERT_NE(barracks_prod, nullptr);
  barracks_unit->owner_id = 1;
  barracks_unit->health = 100;
  barracks_unit->spawn_type = Game::Units::SpawnType::Barracks;
  barracks_transform->position = {3.0F, 0.0F, 0.0F};
  barracks_prod->in_progress = true;
  barracks_prod->time_remaining = 20.0F;

  Game::Systems::CommanderSystem system;
  system.update(&world, 1.0F);
  EXPECT_GT(ally_attack->melee_damage, 22);

  commander_data->bonus_type = "production_haste";
  commander_data->aura_bonus_value = 0.5F;
  const float before_haste = barracks_prod->time_remaining;
  system.update(&world, 1.0F);
  EXPECT_LT(barracks_prod->time_remaining, before_haste - 0.45F);
}

} // namespace
