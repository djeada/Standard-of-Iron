#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/home_system.h"
#include "game/systems/production_service.h"
#include "game/systems/troop_count_registry.h"
#include "game/units/barracks.h"
#include "game/units/home.h"
#include "game/units/spawn_type.h"

#include <gtest/gtest.h>
#include <vector>

namespace {

class HomeManpowerSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::TroopCountRegistry::instance().clear();
  }

  void TearDown() override {
    Game::Systems::TroopCountRegistry::instance().clear();
  }
};

TEST_F(HomeManpowerSystemTest,
       HomesIncreaseBarracksCapacityAndGenerateManpowerOverTime) {
  Engine::Core::World world;

  auto *barracks = world.create_entity();
  auto *barracks_transform =
      barracks->add_component<Engine::Core::TransformComponent>();
  auto *barracks_unit = barracks->add_component<Engine::Core::UnitComponent>();
  auto *barracks_production =
      barracks->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(barracks_transform, nullptr);
  ASSERT_NE(barracks_unit, nullptr);
  ASSERT_NE(barracks_production, nullptr);

  barracks_transform->position = {0.0F, 0.0F, 0.0F};
  barracks_unit->spawn_type = Game::Units::SpawnType::Barracks;
  barracks_unit->owner_id = 1;
  barracks_production->max_units = 100;
  barracks_production->manpower_available = 0;

  auto *home = world.create_entity();
  auto *home_transform =
      home->add_component<Engine::Core::TransformComponent>();
  auto *home_unit = home->add_component<Engine::Core::UnitComponent>();
  auto *home_component = home->add_component<Engine::Core::HomeComponent>();
  auto *home_production =
      home->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(home_transform, nullptr);
  ASSERT_NE(home_unit, nullptr);
  ASSERT_NE(home_component, nullptr);
  ASSERT_NE(home_production, nullptr);

  home_transform->position = {10.0F, 0.0F, 0.0F};
  home_unit->spawn_type = Game::Units::SpawnType::Home;
  home_unit->owner_id = 1;
  home_component->population_contribution = 50;
  home_component->update_cooldown = 0.0F;
  home_component->family_generation_interval = 8.0F;
  home_component->family_generation_cooldown = 0.0F;
  home_component->family_manpower_value = 12;

  Game::Systems::HomeSystem home_system;
  home_system.update(&world, 0.1F);

  EXPECT_EQ(home_component->nearest_barracks_id, barracks->get_id());
  EXPECT_EQ(barracks_production->max_units, 150);
  EXPECT_EQ(home_production->manpower_available, 12);
  EXPECT_FLOAT_EQ(home_component->family_generation_cooldown, 8.0F);

  home_component->update_cooldown = 0.0F;
  home_component->family_generation_cooldown = 0.0F;
  home_system.update(&world, 0.1F);

  EXPECT_EQ(barracks_production->max_units, 150);
  EXPECT_EQ(home_production->manpower_available, 24);
  EXPECT_FLOAT_EQ(home_component->family_generation_cooldown, 8.0F);
}

TEST_F(HomeManpowerSystemTest,
       BarracksProductionConsumesAvailableManpowerWhenQueued) {
  Engine::Core::World world;

  auto *barracks = world.create_entity();
  auto *unit = barracks->add_component<Engine::Core::UnitComponent>();
  auto *production =
      barracks->add_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(unit, nullptr);
  ASSERT_NE(production, nullptr);

  unit->spawn_type = Game::Units::SpawnType::Barracks;
  unit->owner_id = 1;
  production->max_units = 500;
  production->manpower_available = 40;

  const std::vector<Engine::Core::EntityID> selected = {barracks->get_id()};

  auto result = Game::Systems::ProductionService::
      start_production_for_first_selected_barracks(
          world, selected, 1, Game::Units::TroopType::Archer);
  EXPECT_EQ(result, Game::Systems::ProductionResult::InsufficientManpower);
  EXPECT_FALSE(production->in_progress);

  production->manpower_available = 60;
  result = Game::Systems::ProductionService::
      start_production_for_first_selected_barracks(
          world, selected, 1, Game::Units::TroopType::Archer);

  EXPECT_EQ(result, Game::Systems::ProductionResult::Success);
  EXPECT_TRUE(production->in_progress);
  EXPECT_EQ(production->manpower_available, 10);
}

TEST_F(HomeManpowerSystemTest,
       InitialBarracksSpawnStartsWithAuthoredManpowerReserve) {
  Engine::Core::World world;

  Game::Units::SpawnParams initial_params;
  initial_params.player_id = 1;
  initial_params.spawn_type = Game::Units::SpawnType::Barracks;
  initial_params.max_population = 180;
  initial_params.is_initial_spawn = true;

  auto initial_barracks = Game::Units::Barracks::Create(world, initial_params);
  auto *initial_entity = world.get_entity(initial_barracks->id());
  ASSERT_NE(initial_entity, nullptr);
  auto *initial_production =
      initial_entity->get_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(initial_production, nullptr);
  EXPECT_EQ(initial_production->max_units, 180);
  EXPECT_EQ(initial_production->manpower_available, 180);

  Game::Units::SpawnParams built_params;
  built_params.player_id = 1;
  built_params.spawn_type = Game::Units::SpawnType::Barracks;
  built_params.max_population = 180;
  built_params.is_initial_spawn = false;

  auto built_barracks = Game::Units::Barracks::Create(world, built_params);
  auto *built_entity = world.get_entity(built_barracks->id());
  ASSERT_NE(built_entity, nullptr);
  auto *built_production =
      built_entity->get_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(built_production, nullptr);
  EXPECT_EQ(built_production->max_units, 180);
  EXPECT_EQ(built_production->manpower_available, 0);
}

TEST_F(HomeManpowerSystemTest,
       InitialHomeSpawnStartsWithThreeCivilianRecruitsReady) {
  Engine::Core::World world;

  Game::Units::SpawnParams params;
  params.player_id = 1;
  params.spawn_type = Game::Units::SpawnType::Home;
  params.is_initial_spawn = true;

  auto home = Game::Units::Home::Create(world, params);
  auto *entity = world.get_entity(home->id());
  ASSERT_NE(entity, nullptr);
  auto *production = entity->get_component<Engine::Core::ProductionComponent>();
  ASSERT_NE(production, nullptr);

  EXPECT_EQ(production->product_type, Game::Units::TroopType::Civilian);
  EXPECT_EQ(production->max_units, 3);
  EXPECT_EQ(production->manpower_available, production->villager_cost * 3);
}

} // namespace
