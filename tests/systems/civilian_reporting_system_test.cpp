#include <QVector3D>

#include <gtest/gtest.h>

#include "game/core/component_core.h"
#include "game/core/component_economy.h"
#include "game/core/component_structures.h"
#include "game/core/world.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/economy/civilian_reporting_system.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/recruitment_rules.h"
#include "game/units/spawn_type.h"
#include "game/units/troop_config.h"
#include "game/units/troop_type.h"

namespace {

using Engine::Core::Entity;
using Engine::Core::World;

constexpr int k_owner = 1;

class CivilianReportingSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::NavGrid::initialize(96, 96);
    Game::Systems::PlayerResourceRegistry::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().ensure_owner(k_owner);
  }

  void TearDown() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::PlayerResourceRegistry::instance().clear();
  }

  static auto
  barracks(World& world, QVector3D at, int reserve, int ceiling) -> Entity* {
    auto* entity = world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>()->position = {
        at.x(), 0.0F, at.z()};
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Barracks;
    unit->owner_id = k_owner;
    unit->health = 1000;
    auto* production = entity->add_component<Engine::Core::ProductionComponent>();
    production->max_units = ceiling;
    production->manpower_available = reserve;
    return entity;
  }

  static auto home(World& world, QVector3D at, bool reports) -> Entity* {
    auto* entity = world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>()->position = {
        at.x(), 0.0F, at.z()};
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Home;
    unit->owner_id = k_owner;
    unit->health = 500;
    entity->add_component<Engine::Core::HomeComponent>()->reports_to_barracks = reports;
    auto* production = entity->add_component<Engine::Core::ProductionComponent>();
    production->product_type = Game::Units::TroopType::Civilian;
    production->build_time = 5.0F;
    const int villager_cost = Game::Units::TroopConfig::instance().get_production_cost(
        Game::Units::TroopType::Civilian);
    production->villager_cost = villager_cost;
    production->max_units = Game::Systems::k_home_lifetime_civilians;
    production->manpower_available =
        villager_cost * Game::Systems::k_home_lifetime_civilians;
    return entity;
  }

  static auto civilian(World& world, QVector3D at) -> Entity* {
    auto* entity = world.create_entity();
    entity->add_component<Engine::Core::TransformComponent>()->position = {
        at.x(), 0.0F, at.z()};
    auto* unit = entity->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Civilian;
    unit->owner_id = k_owner;
    unit->health = 35;
    entity->add_component<Engine::Core::MovementComponent>();
    return entity;
  }

  static void think(World& world) {
    Game::Systems::CivilianReportingSystem system;
    system.update(&world, Game::Systems::CivilianReportingSystem::k_think_interval);
  }
};

TEST_F(CivilianReportingSystemTest,
       ACivilianBesideAReportingHomeWalksToABarracksWithRoom) {
  World world;
  auto* hall = barracks(world, {10.0F, 0.0F, 10.0F}, 0, 120);
  home(world, {25.0F, 0.0F, 10.0F}, true);
  auto* family = civilian(world, {27.0F, 0.0F, 11.0F});

  think(world);

  const auto* delivery =
      family->get_component<Engine::Core::CivilianDeliveryComponent>();
  ASSERT_NE(delivery, nullptr) << "the civilian stayed home; nobody sent it";
  EXPECT_EQ(delivery->target_barracks_id, hall->get_id());
}

TEST_F(CivilianReportingSystemTest, AHomeSwitchedOffKeepsItsCiviliansAndRaisesNone) {
  World world;
  barracks(world, {10.0F, 0.0F, 10.0F}, 0, 120);
  auto* house = home(world, {25.0F, 0.0F, 10.0F}, false);
  auto* family = civilian(world, {27.0F, 0.0F, 11.0F});
  Game::Systems::PlayerResourceRegistry::instance().set(
      k_owner, Game::Systems::ResourceType::Food, 200);

  think(world);

  EXPECT_EQ(family->get_component<Engine::Core::CivilianDeliveryComponent>(), nullptr);
  EXPECT_FALSE(house->get_component<Engine::Core::ProductionComponent>()->in_progress);
}

TEST_F(CivilianReportingSystemTest, ACivilianThePlayerOrderedIsLeftAlone) {
  World world;
  barracks(world, {10.0F, 0.0F, 10.0F}, 0, 120);
  home(world, {25.0F, 0.0F, 10.0F}, true);
  auto* family = civilian(world, {27.0F, 0.0F, 11.0F});
  family->add_component<Engine::Core::PlayerOrderIntentComponent>();

  think(world);

  EXPECT_EQ(family->get_component<Engine::Core::CivilianDeliveryComponent>(), nullptr);
}

TEST_F(CivilianReportingSystemTest, HomesRaiseOnlyAsManyCiviliansAsTheBarracksCanTake) {
  World world;
  barracks(world,
           {10.0F, 0.0F, 10.0F},
           120 - Game::Systems::k_civilian_delivery_reserve_grant,
           120);
  auto* first = home(world, {25.0F, 0.0F, 10.0F}, true);
  auto* second = home(world, {25.0F, 0.0F, 30.0F}, true);
  Game::Systems::PlayerResourceRegistry::instance().set(
      k_owner, Game::Systems::ResourceType::Food, 200);

  think(world);

  const bool first_raises =
      first->get_component<Engine::Core::ProductionComponent>()->in_progress;
  const bool second_raises =
      second->get_component<Engine::Core::ProductionComponent>()->in_progress;
  EXPECT_TRUE(first_raises != second_raises)
      << "the barracks has room for one household, so exactly one home raises a "
         "civilian (first "
      << first_raises << ", second " << second_raises << ")";
  EXPECT_LT(Game::Systems::PlayerResourceRegistry::instance().get(
                k_owner, Game::Systems::ResourceType::Food),
            200)
      << "raising a civilian costs food";
}

TEST_F(CivilianReportingSystemTest, AFullBarracksTurnsNobodyOutAndRaisesNobody) {
  World world;
  barracks(world, {10.0F, 0.0F, 10.0F}, 120, 120);
  auto* house = home(world, {25.0F, 0.0F, 10.0F}, true);
  auto* family = civilian(world, {27.0F, 0.0F, 11.0F});
  Game::Systems::PlayerResourceRegistry::instance().set(
      k_owner, Game::Systems::ResourceType::Food, 200);

  think(world);

  EXPECT_EQ(family->get_component<Engine::Core::CivilianDeliveryComponent>(), nullptr);
  EXPECT_FALSE(house->get_component<Engine::Core::ProductionComponent>()->in_progress);
}

} // namespace
