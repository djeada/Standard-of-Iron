#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/command_service.h"
#include "game/systems/commander_system.h"
#include "game/systems/nation_loader.h"
#include "game/systems/nation_registry.h"
#include "game/systems/production_service.h"
#include "game/systems/troop_profile_service.h"
#include "game/units/commander_catalog.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_catalog_loader.h"
#include "game/units/troop_type.h"

namespace {

TEST(CommanderCatalogTest, DefinesThreeCommandersForEachPlayableNation) {
  auto roman = Game::Units::commander_definitions_for_nation(
      Game::Systems::NationID::RomanRepublic);
  auto carthage =
      Game::Units::commander_definitions_for_nation(Game::Systems::NationID::Carthage);

  ASSERT_EQ(roman.size(), 3U);
  ASSERT_EQ(carthage.size(), 3U);
  bool has_hannibal = false;
  bool has_health_bonus = false;
  bool has_attack_bonus = false;
  bool has_production_bonus = false;

  auto inspect_definition = [&](const Game::Units::CommanderDefinition* definition) {
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

  for (const auto* definition : roman) {
    inspect_definition(definition);
  }
  for (const auto* definition : carthage) {
    inspect_definition(definition);
  }

  EXPECT_TRUE(has_hannibal);
  EXPECT_TRUE(has_health_bonus);
  EXPECT_TRUE(has_attack_bonus);
  EXPECT_TRUE(has_production_bonus);
}

TEST(CommanderProductionTest, RejectsCommanderProductionFromBarracks) {
  Engine::Core::World world;

  auto* barracks = world.create_entity();
  auto* barracks_unit = barracks->add_component<Engine::Core::UnitComponent>();
  auto* production = barracks->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(barracks_unit, nullptr);
  ASSERT_NE(production, nullptr);
  barracks_unit->spawn_type = Game::Units::SpawnType::Barracks;
  barracks_unit->owner_id = 1;
  production->max_units = 10000;
  production->manpower_available = 1000;

  auto result =
      Game::Systems::ProductionService::start_production_for_first_selected_barracks(
          world, {barracks->get_id()}, 1, Game::Units::TroopType::RomanLegionOrganizer);
  EXPECT_EQ(result, Game::Systems::ProductionResult::CommanderNotRecruitable);
  EXPECT_FALSE(production->in_progress);
  EXPECT_EQ(production->manpower_available, 1000);
}

TEST(CommanderProductionTest, RejectsCommanderProductionEvenIfOwnerHasNoCommander) {
  Engine::Core::World world;

  auto* barracks = world.create_entity();
  auto* barracks_unit = barracks->add_component<Engine::Core::UnitComponent>();
  auto* production = barracks->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(barracks_unit, nullptr);
  ASSERT_NE(production, nullptr);
  barracks_unit->spawn_type = Game::Units::SpawnType::Barracks;
  barracks_unit->owner_id = 1;
  production->max_units = 10000;
  production->manpower_available = 1000;

  auto result =
      Game::Systems::ProductionService::start_production_for_first_selected_barracks(
          world,
          {barracks->get_id()},
          1,
          Game::Units::TroopType::CarthageMercenaryBroker);
  EXPECT_EQ(result, Game::Systems::ProductionResult::CommanderNotRecruitable);
  EXPECT_FALSE(production->in_progress);
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

  auto* first_entity = world.get_entity(first->id());
  ASSERT_NE(first_entity, nullptr);
  auto* first_unit = first_entity->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(first_unit, nullptr);
  ASSERT_NE(first_entity->get_component<Engine::Core::CommanderComponent>(), nullptr);

  params.spawn_type = Game::Units::SpawnType::RomanFieldCommander;
  auto second = registry.create(params.spawn_type, world, params);
  EXPECT_EQ(second, nullptr);

  first_unit->health = 0;
  auto replacement = registry.create(params.spawn_type, world, params);
  EXPECT_NE(replacement, nullptr);
}

TEST(UndeadSpawnTypeTest, RoundTripsAndModesMatchDesign) {
  EXPECT_EQ(Game::Units::troop_typeToString(Game::Units::TroopType::SkeletonSwordsman),
            "skeleton_swordsman");
  EXPECT_EQ(Game::Units::troop_typeToString(Game::Units::TroopType::SkeletonArcher),
            "skeleton_archer");
  EXPECT_EQ(Game::Units::troop_typeToString(Game::Units::TroopType::GravePriest),
            "grave_priest");

  EXPECT_EQ(
      Game::Units::spawn_typeFromTroopType(Game::Units::TroopType::SkeletonSwordsman),
      Game::Units::SpawnType::SkeletonSwordsman);
  EXPECT_EQ(Game::Units::spawn_typeToTroopType(Game::Units::SpawnType::SkeletonArcher),
            Game::Units::TroopType::SkeletonArcher);
  EXPECT_EQ(Game::Units::spawn_typeFromTroopType(Game::Units::TroopType::GravePriest),
            Game::Units::SpawnType::GravePriest);

  EXPECT_FALSE(
      Game::Units::is_commander_troop(Game::Units::TroopType::SkeletonSwordsman));
  EXPECT_FALSE(Game::Units::is_commander_troop(Game::Units::TroopType::SkeletonArcher));
  EXPECT_FALSE(Game::Units::is_commander_troop(Game::Units::TroopType::GravePriest));

  EXPECT_TRUE(
      Game::Units::can_use_attack_mode(Game::Units::SpawnType::SkeletonSwordsman));
  EXPECT_TRUE(Game::Units::can_use_guard_mode(Game::Units::SpawnType::SkeletonArcher));
  EXPECT_TRUE(
      Game::Units::can_use_hold_mode(Game::Units::SpawnType::SkeletonSwordsman));
  EXPECT_TRUE(Game::Units::can_use_hold_mode(Game::Units::SpawnType::SkeletonArcher));
  EXPECT_TRUE(Game::Units::can_use_hold_mode(Game::Units::SpawnType::GravePriest));
  EXPECT_TRUE(Game::Units::can_use_patrol_mode(Game::Units::SpawnType::GravePriest));
  EXPECT_FALSE(
      Game::Units::can_use_run_mode(Game::Units::SpawnType::SkeletonSwordsman));
  EXPECT_FALSE(Game::Units::can_use_run_mode(Game::Units::SpawnType::SkeletonArcher));
  EXPECT_FALSE(Game::Units::can_use_run_mode(Game::Units::SpawnType::GravePriest));
}

TEST(UndeadFactoryTest, CreatesUndeadUnitsWithoutCommanderRestrictions) {
  ASSERT_TRUE(Game::Units::TroopCatalogLoader::load_default_catalog());
  auto const nations = Game::Systems::NationLoader::load_default_nations();
  auto& nation_registry = Game::Systems::NationRegistry::instance();
  nation_registry.clear();
  nation_registry.clear_player_assignments();
  for (const auto& nation : nations) {
    nation_registry.register_nation(nation);
  }
  Game::Systems::TroopProfileService::instance().clear();

  Engine::Core::World world;
  Game::Units::UnitFactoryRegistry registry;
  Game::Units::register_built_in_units(registry);

  Game::Units::SpawnParams params;
  params.position = {0.0F, 0.0F, 0.0F};
  params.player_id = 3;
  params.nation_id = Game::Systems::NationID::IronSepulcher;
  params.ai_controlled = true;

  params.spawn_type = Game::Units::SpawnType::SkeletonSwordsman;
  auto skeleton_swordsman = registry.create(params.spawn_type, world, params);
  ASSERT_NE(skeleton_swordsman, nullptr);
  auto* skeleton_swordsman_entity = world.get_entity(skeleton_swordsman->id());
  ASSERT_NE(skeleton_swordsman_entity, nullptr);
  EXPECT_NE(skeleton_swordsman_entity->get_component<Engine::Core::UndeadComponent>(),
            nullptr);
  EXPECT_NE(skeleton_swordsman_entity->get_component<Engine::Core::AttackComponent>(),
            nullptr);

  params.spawn_type = Game::Units::SpawnType::SkeletonArcher;
  auto skeleton_archer = registry.create(params.spawn_type, world, params);
  ASSERT_NE(skeleton_archer, nullptr);
  auto* skeleton_archer_entity = world.get_entity(skeleton_archer->id());
  ASSERT_NE(skeleton_archer_entity, nullptr);
  EXPECT_NE(skeleton_archer_entity->get_component<Engine::Core::UndeadComponent>(),
            nullptr);
  EXPECT_NE(skeleton_archer_entity->get_component<Engine::Core::AttackComponent>(),
            nullptr);

  params.spawn_type = Game::Units::SpawnType::GravePriest;
  auto first_priest = registry.create(params.spawn_type, world, params);
  ASSERT_NE(first_priest, nullptr);
  auto second_priest = registry.create(params.spawn_type, world, params);
  ASSERT_NE(second_priest, nullptr);

  auto* first_priest_entity = world.get_entity(first_priest->id());
  ASSERT_NE(first_priest_entity, nullptr);
  EXPECT_NE(first_priest_entity->get_component<Engine::Core::UndeadComponent>(),
            nullptr);
  auto* priest_healer =
      first_priest_entity->get_component<Engine::Core::HealerComponent>();
  ASSERT_NE(priest_healer, nullptr);
  EXPECT_EQ(priest_healer->target_affinity,
            Engine::Core::HealerComponent::TargetAffinity::UndeadAllies);
  EXPECT_GE(priest_healer->time_since_last_heal, priest_healer->healing_cooldown);
  auto* priest_attack =
      first_priest_entity->get_component<Engine::Core::AttackComponent>();
  ASSERT_NE(priest_attack, nullptr);
  EXPECT_TRUE(priest_attack->can_ranged);
  EXPECT_EQ(priest_attack->current_mode,
            Engine::Core::AttackComponent::CombatMode::Ranged);
  EXPECT_GE(priest_attack->time_since_last, priest_attack->cooldown);
  auto* priest_special =
      first_priest_entity->get_component<Engine::Core::SpecialAttackComponent>();
  ASSERT_NE(priest_special, nullptr);
  EXPECT_TRUE(priest_special->use_projectile_system);
  EXPECT_EQ(priest_special->projectile_kind, Game::Systems::ProjectileKind::Fireball);
  EXPECT_GT(priest_special->fire_patch_duration, 0.0F);
  EXPECT_GT(priest_special->burn_damage_per_tick, 0);
  EXPECT_EQ(first_priest_entity->get_component<Engine::Core::CommanderComponent>(),
            nullptr);

  nation_registry.clear();
  nation_registry.clear_player_assignments();
  Game::Systems::TroopProfileService::instance().clear();
}

TEST(CommanderSystemTest, CommanderDeathDisablesAuraAndShocksNearbyAllies) {
  Engine::Core::World world;

  auto* commander = world.create_entity();
  auto* commander_unit = commander->add_component<Engine::Core::UnitComponent>();
  auto* commander_transform =
      commander->add_component<Engine::Core::TransformComponent>();
  auto* commander_data = commander->add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_unit, nullptr);
  ASSERT_NE(commander_transform, nullptr);
  ASSERT_NE(commander_data, nullptr);
  commander_unit->owner_id = 1;
  commander_unit->health = 0;
  commander_data->death_morale_shock = 30.0F;
  commander_data->death_shock_radius = 8.0F;

  auto* ally = world.create_entity();
  auto* ally_unit = ally->add_component<Engine::Core::UnitComponent>();
  auto* ally_transform = ally->add_component<Engine::Core::TransformComponent>();
  auto* ally_morale = ally->add_component<Engine::Core::MoraleComponent>();
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

  auto* commander = world.create_entity();
  auto* commander_unit = commander->add_component<Engine::Core::UnitComponent>();
  auto* commander_transform =
      commander->add_component<Engine::Core::TransformComponent>();
  auto* commander_data = commander->add_component<Engine::Core::CommanderComponent>();
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

  auto* ally = world.create_entity();
  auto* ally_unit = ally->add_component<Engine::Core::UnitComponent>();
  auto* ally_transform = ally->add_component<Engine::Core::TransformComponent>();
  auto* ally_attack = ally->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(ally_unit, nullptr);
  ASSERT_NE(ally_transform, nullptr);
  ASSERT_NE(ally_attack, nullptr);
  ally_unit->owner_id = 1;
  ally_unit->health = 100;
  ally_unit->spawn_type = Game::Units::SpawnType::Knight;
  ally_unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ally_transform->position = {2.0F, 0.0F, 0.0F};

  auto* barracks = world.create_entity();
  auto* barracks_unit = barracks->add_component<Engine::Core::UnitComponent>();
  auto* barracks_transform =
      barracks->add_component<Engine::Core::TransformComponent>();
  auto* barracks_prod = barracks->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(barracks_unit, nullptr);
  ASSERT_NE(barracks_transform, nullptr);
  ASSERT_NE(barracks_prod, nullptr);
  barracks_unit->owner_id = 1;
  barracks_unit->health = 100;
  barracks_unit->spawn_type = Game::Units::SpawnType::Barracks;
  barracks_transform->position = {3.0F, 0.0F, 0.0F};
  barracks_prod->in_progress = true;
  barracks_prod->time_remaining = 20.0F;

  const auto base_profile = Game::Systems::TroopProfileService::instance().get_profile(
      Game::Systems::NationID::RomanRepublic, Game::Units::TroopType::Swordsman);
  const int expected_boosted_melee = static_cast<int>(
      std::round(static_cast<float>(base_profile.combat.melee_damage) * 1.25F));

  Game::Systems::CommanderSystem system;
  system.update(&world, 1.0F);
  EXPECT_EQ(ally_attack->melee_damage, expected_boosted_melee);

  commander_data->bonus_type = "production_haste";
  commander_data->aura_bonus_value = 0.5F;
  const float before_haste = barracks_prod->time_remaining;
  system.update(&world, 1.0F);
  const float expected_haste_time = before_haste - commander_data->aura_bonus_value;
  EXPECT_FLOAT_EQ(barracks_prod->time_remaining, expected_haste_time);
}

TEST(CommanderSystemTest, AttackBoostFallsOffOutsideAura) {
  Engine::Core::World world;

  auto* commander = world.create_entity();
  auto* commander_unit = commander->add_component<Engine::Core::UnitComponent>();
  auto* commander_transform =
      commander->add_component<Engine::Core::TransformComponent>();
  auto* commander_data = commander->add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_unit, nullptr);
  ASSERT_NE(commander_transform, nullptr);
  ASSERT_NE(commander_data, nullptr);
  commander_unit->owner_id = 1;
  commander_unit->health = 100;
  commander_transform->position = {0.0F, 0.0F, 0.0F};
  commander_data->bonus_type = "attack_boost";
  commander_data->aura_bonus_value = 0.25F;
  commander_data->aura_radius = 5.0F;

  auto* ally = world.create_entity();
  auto* ally_unit = ally->add_component<Engine::Core::UnitComponent>();
  auto* ally_transform = ally->add_component<Engine::Core::TransformComponent>();
  auto* ally_attack = ally->add_component<Engine::Core::AttackComponent>();
  ASSERT_NE(ally_unit, nullptr);
  ASSERT_NE(ally_transform, nullptr);
  ASSERT_NE(ally_attack, nullptr);
  ally_unit->owner_id = 1;
  ally_unit->health = 100;
  ally_unit->spawn_type = Game::Units::SpawnType::Knight;
  ally_unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ally_transform->position = {2.0F, 0.0F, 0.0F};

  const auto base_profile = Game::Systems::TroopProfileService::instance().get_profile(
      Game::Systems::NationID::RomanRepublic, Game::Units::TroopType::Swordsman);
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

  auto* commander = world.create_entity();
  auto* commander_unit = commander->add_component<Engine::Core::UnitComponent>();
  auto* commander_transform =
      commander->add_component<Engine::Core::TransformComponent>();
  auto* commander_data = commander->add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_unit, nullptr);
  ASSERT_NE(commander_transform, nullptr);
  ASSERT_NE(commander_data, nullptr);
  commander_unit->owner_id = 1;
  commander_unit->health = 100;
  commander_transform->position = {0.0F, 0.0F, 0.0F};
  commander_data->bonus_type = "speed_boost";
  commander_data->aura_bonus_value = 0.20F;
  commander_data->aura_radius = 5.0F;

  auto* ally = world.create_entity();
  auto* ally_unit = ally->add_component<Engine::Core::UnitComponent>();
  auto* ally_transform = ally->add_component<Engine::Core::TransformComponent>();
  ASSERT_NE(ally_unit, nullptr);
  ASSERT_NE(ally_transform, nullptr);
  ally_unit->owner_id = 1;
  ally_unit->health = 100;
  ally_unit->spawn_type = Game::Units::SpawnType::Spearman;
  ally_unit->nation_id = Game::Systems::NationID::RomanRepublic;
  ally_transform->position = {2.0F, 0.0F, 0.0F};

  const auto base_profile = Game::Systems::TroopProfileService::instance().get_profile(
      Game::Systems::NationID::RomanRepublic, Game::Units::TroopType::Spearman);

  Game::Systems::CommanderSystem system;
  system.update(&world, 1.0F);
  EXPECT_FLOAT_EQ(ally_unit->speed, base_profile.combat.speed * 1.20F);

  commander_unit->health = 0;
  system.update(&world, 1.0F);

  EXPECT_FLOAT_EQ(ally_unit->speed, base_profile.combat.speed);
}

TEST(CommanderSystemTest, ManualRallyRequestRestoresNearbyWaveringAlly) {
  Engine::Core::World world;

  auto* commander = world.create_entity();
  auto* commander_unit = commander->add_component<Engine::Core::UnitComponent>();
  auto* commander_transform =
      commander->add_component<Engine::Core::TransformComponent>();
  auto* commander_data = commander->add_component<Engine::Core::CommanderComponent>();
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

  auto* ally = world.create_entity();
  auto* ally_unit = ally->add_component<Engine::Core::UnitComponent>();
  auto* ally_transform = ally->add_component<Engine::Core::TransformComponent>();
  auto* ally_morale = ally->add_component<Engine::Core::MoraleComponent>();
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

  auto* commander = world.create_entity();
  auto* commander_unit = commander->add_component<Engine::Core::UnitComponent>();
  auto* commander_transform =
      commander->add_component<Engine::Core::TransformComponent>();
  auto* commander_data = commander->add_component<Engine::Core::CommanderComponent>();
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

  auto* ally = world.create_entity();
  auto* ally_unit = ally->add_component<Engine::Core::UnitComponent>();
  auto* ally_transform = ally->add_component<Engine::Core::TransformComponent>();
  auto* ally_morale = ally->add_component<Engine::Core::MoraleComponent>();
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

TEST(CommanderFlagRallyTest, FlagRallyCompletesAfterArrivalAndTimerExpires) {
  // Simulate a commander that is already at the rally position so the system
  // immediately starts the animation timer, then completes and plants the flag.
  Engine::Core::World world;
  Game::Systems::CommandService::initialize(64, 64);

  auto* commander = world.create_entity();
  auto* commander_unit = commander->add_component<Engine::Core::UnitComponent>();
  auto* commander_transform =
      commander->add_component<Engine::Core::TransformComponent>();
  auto* commander_data = commander->add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_unit, nullptr);
  ASSERT_NE(commander_transform, nullptr);
  ASSERT_NE(commander_data, nullptr);
  commander_unit->owner_id = 1;
  commander_unit->health = 100;
  commander_unit->spawn_type = Game::Units::SpawnType::RomanFieldCommander;
  commander_transform->position = {5.0F, 0.0F, 5.0F};

  // Set up the flag rally: commander is already at the pending position.
  commander_data->flag_rally_pending_x = 5.0F;
  commander_data->flag_rally_pending_z = 5.0F;
  commander_data->flag_rally_cost = 2.0F;
  commander_data->flag_rally_in_progress = true;
  commander_data->flag_rally_at_position = false;

  // Add allied troops that should receive the same formation move order as a
  // ground right-click.
  auto* ally_a = world.create_entity();
  auto* ally_a_unit = ally_a->add_component<Engine::Core::UnitComponent>();
  auto* ally_a_transform = ally_a->add_component<Engine::Core::TransformComponent>();
  auto* ally_a_stamina = ally_a->add_component<Engine::Core::StaminaComponent>();
  ally_a_unit->owner_id = 1;
  ally_a_unit->health = 100;
  ally_a_unit->spawn_type = Game::Units::SpawnType::Spearman;
  ally_a_transform->position = {0.0F, 0.0F, 0.0F};
  ally_a_stamina->run_requested = true;

  auto* ally_b = world.create_entity();
  auto* ally_b_unit = ally_b->add_component<Engine::Core::UnitComponent>();
  auto* ally_b_transform = ally_b->add_component<Engine::Core::TransformComponent>();
  auto* ally_b_stamina = ally_b->add_component<Engine::Core::StaminaComponent>();
  ally_b_unit->owner_id = 1;
  ally_b_unit->health = 100;
  ally_b_unit->spawn_type = Game::Units::SpawnType::Spearman;
  ally_b_transform->position = {1.0F, 0.0F, 0.0F};
  ally_b_stamina->run_requested = true;

  Game::Systems::CommanderSystem system;

  // First tick: commander arrives and starts the planting phase.
  system.update(&world, 0.1F);
  EXPECT_TRUE(commander_data->flag_rally_at_position);
  EXPECT_NEAR(commander_data->flag_rally_animation_timer, 2.0F, 0.01F);
  EXPECT_FALSE(commander_data->flag_rally_flag_active);

  // Advance past the full planting cost.
  system.update(&world, 2.0F);
  EXPECT_FALSE(commander_data->flag_rally_in_progress);
  EXPECT_FALSE(commander_data->flag_rally_at_position);
  EXPECT_TRUE(commander_data->flag_rally_flag_active);
  EXPECT_NEAR(commander_data->flag_rally_flag_x, 5.0F, 0.01F);
  EXPECT_NEAR(commander_data->flag_rally_flag_z, 5.0F, 0.01F);
  // issue_commands is consumed immediately in the same tick.
  EXPECT_FALSE(commander_data->flag_rally_issue_commands);

  auto* ally_a_movement = ally_a->get_component<Engine::Core::MovementComponent>();
  auto* ally_b_movement = ally_b->get_component<Engine::Core::MovementComponent>();
  ASSERT_NE(ally_a_movement, nullptr);
  ASSERT_NE(ally_b_movement, nullptr);
  EXPECT_TRUE(ally_a_movement->has_target);
  EXPECT_TRUE(ally_b_movement->has_target);
  EXPECT_FALSE(ally_a_stamina->run_requested);
  EXPECT_FALSE(ally_b_stamina->run_requested);
  EXPECT_FALSE(std::abs(ally_a_movement->goal_x - 5.0F) < 0.01F &&
               std::abs(ally_a_movement->goal_y - 5.0F) < 0.01F &&
               std::abs(ally_b_movement->goal_x - 5.0F) < 0.01F &&
               std::abs(ally_b_movement->goal_y - 5.0F) < 0.01F);
  EXPECT_NEAR(ally_a_movement->goal_y, ally_b_movement->goal_y, 0.01F);
  EXPECT_GT(std::abs(ally_a_movement->goal_x - ally_b_movement->goal_x), 0.01F);
}

TEST(CommanderFlagRallyTest, FlagRallyCancelledOnCommanderDeath) {
  Engine::Core::World world;

  auto* commander = world.create_entity();
  auto* commander_unit = commander->add_component<Engine::Core::UnitComponent>();
  auto* commander_transform =
      commander->add_component<Engine::Core::TransformComponent>();
  auto* commander_data = commander->add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_unit, nullptr);
  ASSERT_NE(commander_transform, nullptr);
  ASSERT_NE(commander_data, nullptr);
  commander_unit->owner_id = 1;
  commander_unit->health = 0; // commander is dead
  commander_unit->spawn_type = Game::Units::SpawnType::RomanFieldCommander;
  commander_transform->position = {5.0F, 0.0F, 5.0F};
  commander_data->flag_rally_pending_x = 5.0F;
  commander_data->flag_rally_pending_z = 5.0F;
  commander_data->flag_rally_in_progress = true;
  commander_data->flag_rally_at_position = true;
  commander_data->flag_rally_animation_timer = 1.5F;
  commander_data->flag_rally_flag_active = true;
  commander_data->flag_rally_issue_commands = true;

  Game::Systems::CommanderSystem system;
  system.update(&world, 0.1F);

  // The rally should be cancelled on death.
  EXPECT_FALSE(commander_data->flag_rally_in_progress);
  EXPECT_FALSE(commander_data->flag_rally_at_position);
  EXPECT_FALSE(commander_data->flag_rally_flag_active);
  EXPECT_FALSE(commander_data->flag_rally_issue_commands);
  EXPECT_TRUE(commander_data->wounded);
}

TEST(CommanderFlagRallyTest, StartingNewFlagRallyClearsExistingPlacedFlag) {
  Engine::Core::CommanderComponent commander_data;
  commander_data.flag_rally_cost = 2.0F;
  commander_data.flag_rally_flag_x = 3.0F;
  commander_data.flag_rally_flag_z = 4.0F;
  commander_data.flag_rally_flag_active = true;
  commander_data.flag_rally_issue_commands = true;

  commander_data.begin_flag_rally(10.0F, 12.0F, false);

  EXPECT_FLOAT_EQ(commander_data.flag_rally_pending_x, 10.0F);
  EXPECT_FLOAT_EQ(commander_data.flag_rally_pending_z, 12.0F);
  EXPECT_TRUE(commander_data.flag_rally_in_progress);
  EXPECT_FALSE(commander_data.flag_rally_at_position);
  EXPECT_FLOAT_EQ(commander_data.flag_rally_animation_timer, 0.0F);
  EXPECT_FALSE(commander_data.flag_rally_flag_active);
  EXPECT_FALSE(commander_data.flag_rally_issue_commands);
}

TEST(CommanderFlagRallyTest, CommanderMovingTowardsFlagPositionDoesNotCompleteEarly) {
  Engine::Core::World world;

  auto* commander = world.create_entity();
  auto* commander_unit = commander->add_component<Engine::Core::UnitComponent>();
  auto* commander_transform =
      commander->add_component<Engine::Core::TransformComponent>();
  auto* commander_data = commander->add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_unit, nullptr);
  ASSERT_NE(commander_transform, nullptr);
  ASSERT_NE(commander_data, nullptr);
  commander_unit->owner_id = 1;
  commander_unit->health = 100;
  commander_unit->spawn_type = Game::Units::SpawnType::RomanFieldCommander;
  // Commander is far from the target — has not arrived yet.
  commander_transform->position = {0.0F, 0.0F, 0.0F};
  commander_data->flag_rally_pending_x = 50.0F;
  commander_data->flag_rally_pending_z = 50.0F;
  commander_data->flag_rally_cost = 2.0F;
  commander_data->flag_rally_in_progress = true;
  commander_data->flag_rally_at_position = false;

  Game::Systems::CommanderSystem system;
  system.update(&world, 0.1F);

  // Commander has not arrived yet — no phase transition.
  EXPECT_FALSE(commander_data->flag_rally_at_position);
  EXPECT_FALSE(commander_data->flag_rally_flag_active);
}

TEST(CommanderFlagRallyTest, DivergentMoveOrderCancelsFlagRally) {
  Engine::Core::World world;

  auto* commander = world.create_entity();
  auto* commander_unit = commander->add_component<Engine::Core::UnitComponent>();
  auto* commander_transform =
      commander->add_component<Engine::Core::TransformComponent>();
  auto* commander_movement =
      commander->add_component<Engine::Core::MovementComponent>();
  auto* commander_data = commander->add_component<Engine::Core::CommanderComponent>();
  ASSERT_NE(commander_unit, nullptr);
  ASSERT_NE(commander_transform, nullptr);
  ASSERT_NE(commander_movement, nullptr);
  ASSERT_NE(commander_data, nullptr);
  commander_unit->owner_id = 1;
  commander_unit->health = 100;
  commander_unit->spawn_type = Game::Units::SpawnType::RomanFieldCommander;
  commander_transform->position = {0.0F, 0.0F, 0.0F};
  commander_data->flag_rally_pending_x = 10.0F;
  commander_data->flag_rally_pending_z = 0.0F;
  commander_data->flag_rally_in_progress = true;
  commander_movement->has_target = true;
  commander_movement->goal_x = 25.0F;
  commander_movement->goal_y = 0.0F;
  commander_movement->target_x = 25.0F;
  commander_movement->target_y = 0.0F;

  Game::Systems::CommanderSystem system;
  system.update(&world, 0.1F);

  EXPECT_FALSE(commander_data->flag_rally_in_progress);
  EXPECT_FALSE(commander_data->flag_rally_at_position);
  EXPECT_FALSE(commander_data->flag_rally_flag_active);
}

TEST(CommanderFlagRallyTest, CombatInterruptCancelsPlantingPhase) {
  Engine::Core::World world;

  auto* commander = world.create_entity();
  auto* commander_unit = commander->add_component<Engine::Core::UnitComponent>();
  auto* commander_transform =
      commander->add_component<Engine::Core::TransformComponent>();
  auto* commander_data = commander->add_component<Engine::Core::CommanderComponent>();
  auto* commander_action =
      commander->add_component<Engine::Core::RpgCommanderActionComponent>();
  ASSERT_NE(commander_unit, nullptr);
  ASSERT_NE(commander_transform, nullptr);
  ASSERT_NE(commander_data, nullptr);
  ASSERT_NE(commander_action, nullptr);
  commander_unit->owner_id = 1;
  commander_unit->health = 100;
  commander_unit->spawn_type = Game::Units::SpawnType::RomanFieldCommander;
  commander_transform->position = {5.0F, 0.0F, 5.0F};
  commander_data->flag_rally_pending_x = 5.0F;
  commander_data->flag_rally_pending_z = 5.0F;
  commander_data->flag_rally_in_progress = true;
  commander_data->flag_rally_at_position = true;
  commander_data->flag_rally_animation_timer = 1.5F;
  commander_action->phase = Engine::Core::RpgCommanderActionPhase::Ability;

  Game::Systems::CommanderSystem system;
  system.update(&world, 0.1F);

  EXPECT_FALSE(commander_data->flag_rally_in_progress);
  EXPECT_FALSE(commander_data->flag_rally_at_position);
  EXPECT_FALSE(commander_data->flag_rally_flag_active);
}

} // namespace
