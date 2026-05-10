#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/commander_system.h"
#include "game/systems/production_service.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/commander_catalog.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_type.h"

#include <cmath>
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

  auto inspect_definition =
      [&](const Game::Units::CommanderDefinition *definition) {
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

        has_hannibal =
            has_hannibal || definition->display_name == "Hannibal Barca";
        has_health_bonus =
            has_health_bonus || definition->bonus_type == "health_regen";
        has_attack_bonus =
            has_attack_bonus || definition->bonus_type == "attack_boost";
        has_production_bonus = has_production_bonus ||
                               definition->bonus_type == "production_haste";
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
  auto *first_unit =
      first_barracks->add_component<Engine::Core::UnitComponent>();
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

TEST(CommanderProductionTest, AllowsRecruitingReplacementCommanderAfterDeath) {
  Engine::Core::World world;

  auto *barracks = world.create_entity();
  auto *barracks_unit = barracks->add_component<Engine::Core::UnitComponent>();
  auto *production =
      barracks->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(barracks_unit, nullptr);
  ASSERT_NE(production, nullptr);
  barracks_unit->spawn_type = Game::Units::SpawnType::Barracks;
  barracks_unit->owner_id = 1;
  production->max_units = 10000;
  production->manpower_available = 1000;

  auto *commander = world.create_entity();
  auto *commander_unit =
      commander->add_component<Engine::Core::UnitComponent>();
  ASSERT_NE(commander_unit, nullptr);
  commander_unit->owner_id = 1;
  commander_unit->health = 100;
  commander_unit->spawn_type = Game::Units::SpawnType::CarthageElephantMaster;

  auto result = Game::Systems::ProductionService::
      start_production_for_first_selected_barracks(
          world, {barracks->get_id()}, 1,
          Game::Units::TroopType::CarthageMercenaryBroker);
  EXPECT_EQ(result, Game::Systems::ProductionResult::CommanderLimitReached);

  commander_unit->health = 0;
  result = Game::Systems::ProductionService::
      start_production_for_first_selected_barracks(
          world, {barracks->get_id()}, 1,
          Game::Units::TroopType::CarthageMercenaryBroker);
  EXPECT_EQ(result, Game::Systems::ProductionResult::Success);
}

TEST(CommanderFactoryTest, RefusesSecondLivingCommanderForOwner) {
  Engine::Core::World world;
  Game::Units::UnitFactoryRegistry registry;
  Game::Units::register_built_in_units(registry);

  Game::Units::SpawnParams params;
  params.position = {0.0F, 0.0F, 0.0F};
  params.player_id = 1;
  params.nation_id = Game::Systems::NationID::RomanRepublic;
  params.spawn_type = Game::Units::SpawnType::RomanVeteranConsul;

  auto first = registry.create(params.spawn_type, world, params);
  ASSERT_NE(first, nullptr);

  auto *first_entity = world.get_entity(first->id());
  ASSERT_NE(first_entity, nullptr);
  auto *first_unit = first_entity->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(first_unit, nullptr);
  ASSERT_NE(first_entity->get_component<Engine::Core::CommanderComponent>(),
            nullptr);

  params.spawn_type = Game::Units::SpawnType::RomanFieldCommander;
  auto second = registry.create(params.spawn_type, world, params);
  EXPECT_EQ(second, nullptr);

  first_unit->health = 0;
  auto replacement = registry.create(params.spawn_type, world, params);
  EXPECT_NE(replacement, nullptr);
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
  auto *ally_transform =
      ally->add_component<Engine::Core::TransformComponent>();
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
  auto *ally_transform =
      ally->add_component<Engine::Core::TransformComponent>();
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
  auto *barracks_unit = barracks->add_component<Engine::Core::UnitComponent>();
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

  const auto base_profile =
      Game::Systems::TroopProfileService::instance().get_profile(
          Game::Systems::NationID::RomanRepublic,
          Game::Units::TroopType::Swordsman);
  const int expected_boosted_melee = static_cast<int>(
      std::round(static_cast<float>(base_profile.combat.melee_damage) * 1.25F));

  Game::Systems::CommanderSystem system;
  system.update(&world, 1.0F);
  EXPECT_EQ(ally_attack->melee_damage, expected_boosted_melee);

  commander_data->bonus_type = "production_haste";
  commander_data->aura_bonus_value = 0.5F;
  const float before_haste = barracks_prod->time_remaining;
  system.update(&world, 1.0F);
  const float expected_haste_time =
      before_haste - commander_data->aura_bonus_value;
  EXPECT_FLOAT_EQ(barracks_prod->time_remaining, expected_haste_time);
}

TEST(CommanderSystemTest, AttackBoostFallsOffOutsideAura) {
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
  commander_transform->position = {0.0F, 0.0F, 0.0F};
  commander_data->bonus_type = "attack_boost";
  commander_data->aura_bonus_value = 0.25F;
  commander_data->aura_radius = 5.0F;

  auto *ally = world.create_entity();
  auto *ally_unit = ally->add_component<Engine::Core::UnitComponent>();
  auto *ally_transform =
      ally->add_component<Engine::Core::TransformComponent>();
  auto *ally_attack = ally->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(ally_unit, nullptr);
  ASSERT_NE(ally_transform, nullptr);
  ASSERT_NE(ally_attack, nullptr);
  ally_unit->owner_id = 1;
  ally_unit->health = 100;
  ally_unit->spawn_type = Game::Units::SpawnType::Knight;
  ally_unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ally_transform->position = {2.0F, 0.0F, 0.0F};

  const auto base_profile =
      Game::Systems::TroopProfileService::instance().get_profile(
          Game::Systems::NationID::RomanRepublic,
          Game::Units::TroopType::Swordsman);
  const int boosted_melee = static_cast<int>(
      std::round(static_cast<float>(base_profile.combat.melee_damage) * 1.25F));

  Game::Systems::CommanderSystem system;
  system.update(&world, 1.0F);
  EXPECT_EQ(ally_attack->melee_damage, boosted_melee);

  ally_transform->position = {10.0F, 0.0F, 0.0F};
  system.update(&world, 1.0F);

  EXPECT_EQ(ally_attack->damage, base_profile.combat.ranged_damage);
  EXPECT_EQ(ally_attack->melee_damage, base_profile.combat.melee_damage);
}

TEST(CommanderSystemTest, SpeedBoostFallsOffWhenCommanderIsWounded) {
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
  commander_transform->position = {0.0F, 0.0F, 0.0F};
  commander_data->bonus_type = "speed_boost";
  commander_data->aura_bonus_value = 0.20F;
  commander_data->aura_radius = 5.0F;

  auto *ally = world.create_entity();
  auto *ally_unit = ally->add_component<Engine::Core::UnitComponent>();
  auto *ally_transform =
      ally->add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(ally_unit, nullptr);
  ASSERT_NE(ally_transform, nullptr);
  ally_unit->owner_id = 1;
  ally_unit->health = 100;
  ally_unit->spawn_type = Game::Units::SpawnType::Spearman;
  ally_unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ally_transform->position = {2.0F, 0.0F, 0.0F};

  const auto base_profile =
      Game::Systems::TroopProfileService::instance().get_profile(
          Game::Systems::NationID::RomanRepublic,
          Game::Units::TroopType::Spearman);

  Game::Systems::CommanderSystem system;
  system.update(&world, 1.0F);
  EXPECT_FLOAT_EQ(ally_unit->speed, base_profile.combat.speed * 1.20F);

  commander_unit->health = 0;
  system.update(&world, 1.0F);

  EXPECT_FLOAT_EQ(ally_unit->speed, base_profile.combat.speed);
}

TEST(CommanderSystemTest, ManualRallyRequestRestoresNearbyWaveringAlly) {
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
  commander_transform->position = {0.0F, 0.0F, 0.0F};
  commander_data->aura_radius = 0.5F;
  commander_data->rally_range = 6.0F;
  commander_data->rally_cooldown = 12.0F;
  commander_data->rally_morale_restore = 20.0F;
  commander_data->rally_requires_manual_trigger = true;
  commander_data->rally_requested = true;

  auto *ally = world.create_entity();
  auto *ally_unit = ally->add_component<Engine::Core::UnitComponent>();
  auto *ally_transform =
      ally->add_component<Engine::Core::TransformComponent>();
  auto *ally_morale = ally->add_component<Engine::Core::MoraleComponent>();
  ASSERT_NE(ally_unit, nullptr);
  ASSERT_NE(ally_transform, nullptr);
  ASSERT_NE(ally_morale, nullptr);
  ally_unit->owner_id = 1;
  ally_unit->health = 100;
  ally_unit->spawn_type = Game::Units::SpawnType::Spearman;
  ally_transform->position = {2.0F, 0.0F, 0.0F};
  ally_morale->morale = 25.0F;
  ally_morale->wavering = true;

  Game::Systems::CommanderSystem system;
  system.update(&world, 0.1F);

  EXPECT_FALSE(commander_data->rally_requested);
  EXPECT_FLOAT_EQ(commander_data->rally_cooldown_remaining, 12.0F);
  EXPECT_FLOAT_EQ(commander_data->rally_feedback_time, 1.5F);
  EXPECT_FLOAT_EQ(ally_morale->morale, 45.0F);
  EXPECT_FALSE(ally_morale->wavering);
  EXPECT_FALSE(ally_morale->routing);
}

TEST(CommanderSystemTest, ManualRallyModeDoesNotAutoTriggerWithoutRequest) {
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
  commander_transform->position = {0.0F, 0.0F, 0.0F};
  commander_data->aura_radius = 0.5F;
  commander_data->rally_range = 6.0F;
  commander_data->rally_cooldown = 12.0F;
  commander_data->rally_morale_restore = 20.0F;
  commander_data->rally_requires_manual_trigger = true;

  auto *ally = world.create_entity();
  auto *ally_unit = ally->add_component<Engine::Core::UnitComponent>();
  auto *ally_transform =
      ally->add_component<Engine::Core::TransformComponent>();
  auto *ally_morale = ally->add_component<Engine::Core::MoraleComponent>();
  ASSERT_NE(ally_unit, nullptr);
  ASSERT_NE(ally_transform, nullptr);
  ASSERT_NE(ally_morale, nullptr);
  ally_unit->owner_id = 1;
  ally_unit->health = 100;
  ally_unit->spawn_type = Game::Units::SpawnType::Spearman;
  ally_transform->position = {2.0F, 0.0F, 0.0F};
  ally_morale->morale = 25.0F;
  ally_morale->wavering = true;

  Game::Systems::CommanderSystem system;
  system.update(&world, 0.1F);

  EXPECT_FLOAT_EQ(commander_data->rally_cooldown_remaining, 0.0F);
  EXPECT_FLOAT_EQ(commander_data->rally_feedback_time, 0.0F);
  EXPECT_FLOAT_EQ(ally_morale->morale, 25.0F);
  EXPECT_TRUE(ally_morale->wavering);
}

} // namespace
