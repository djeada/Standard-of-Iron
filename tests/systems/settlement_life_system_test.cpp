#include <QVector3D>

#include <gtest/gtest.h>

#include "core/component.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/order_service.h"
#include "game/systems/owner_registry.h"
#include "game/systems/settlement_life_system.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::SettlementErrand;
using Engine::Core::SettlementResidentComponent;

class SettlementLifeSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(1, Game::Systems::OwnerType::Player);
    owners.register_owner_with_id(2, Game::Systems::OwnerType::AI);
    owners.set_owner_team(1, 1);
    owners.set_owner_team(2, 2);

    Game::Map::MapDefinition map_def;
    map_def.grid.width = 96;
    map_def.grid.height = 96;
    map_def.grid.tile_size = 1.0F;
    Game::Map::TerrainService::instance().initialize(map_def);
    Game::Systems::CommandService::initialize(map_def.grid.width, map_def.grid.height);
  }

  void TearDown() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }

  static auto add_home(Engine::Core::World& world,
                       int owner_id,
                       float x,
                       float z) -> Engine::Core::Entity* {
    auto* home = world.create_entity();
    auto* transform = home->add_component<Engine::Core::TransformComponent>();
    transform->position = {x, 0.0F, z};
    auto* unit = home->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Home;
    unit->owner_id = owner_id;
    unit->health = 600;
    unit->max_health = 600;
    home->add_component<Engine::Core::BuildingComponent>();
    return home;
  }

  static auto add_villager(Engine::Core::World& world,
                           int owner_id,
                           float x,
                           float z) -> Engine::Core::Entity* {
    auto* villager = world.create_entity();
    auto* transform = villager->add_component<Engine::Core::TransformComponent>();
    transform->position = {x, 0.0F, z};
    auto* unit = villager->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Civilian;
    unit->owner_id = owner_id;
    unit->health = 35;
    unit->max_health = 35;
    unit->speed = 2.3F;
    villager->add_component<Engine::Core::MovementComponent>();
    return villager;
  }

  static auto add_spearman(Engine::Core::World& world,
                           int owner_id,
                           float x,
                           float z) -> Engine::Core::Entity* {
    auto* soldier = world.create_entity();
    auto* transform = soldier->add_component<Engine::Core::TransformComponent>();
    transform->position = {x, 0.0F, z};
    auto* unit = soldier->add_component<Engine::Core::UnitComponent>();
    unit->spawn_type = Game::Units::SpawnType::Spearman;
    unit->owner_id = owner_id;
    unit->health = 100;
    unit->max_health = 100;
    soldier->add_component<Engine::Core::MovementComponent>();
    return soldier;
  }

  static void run_for(Game::Systems::SettlementLifeSystem& system,
                      Engine::Core::World& world,
                      float seconds) {
    constexpr float k_step = 0.1F;
    for (float elapsed = 0.0F; elapsed < seconds; elapsed += k_step) {
      system.update(&world, k_step);
    }
  }
};

TEST_F(SettlementLifeSystemTest, AnIdleVillagerBesideAHomeIsGivenALifeOfItsOwn) {
  Engine::Core::World world;
  add_home(world, 1, 0.0F, 0.0F);
  auto* villager = add_villager(world, 1, 4.0F, 0.0F);

  Game::Systems::SettlementLifeSystem system;
  run_for(system, world, 3.0F);

  const auto* resident = villager->get_component<SettlementResidentComponent>();
  ASSERT_NE(resident, nullptr);
  EXPECT_TRUE(resident->hearth_assigned);
  EXPECT_FLOAT_EQ(resident->hearth_x, 0.0F);
  EXPECT_FLOAT_EQ(resident->hearth_z, 0.0F);
}

TEST_F(SettlementLifeSystemTest, AVillagerInOpenCountryIsLeftAlone) {
  Engine::Core::World world;
  auto* villager = add_villager(world, 1, 60.0F, 60.0F);

  Game::Systems::SettlementLifeSystem system;
  run_for(system, world, 3.0F);

  EXPECT_EQ(villager->get_component<SettlementResidentComponent>(), nullptr);
}

TEST_F(SettlementLifeSystemTest, AVillagerTheCommanderIsUsingIsNotAdopted) {
  Engine::Core::World world;
  add_home(world, 1, 0.0F, 0.0F);
  auto* villager = add_villager(world, 1, 4.0F, 0.0F);
  villager->add_component<Engine::Core::PlayerOrderIntentComponent>();

  Game::Systems::SettlementLifeSystem system;
  run_for(system, world, 3.0F);

  EXPECT_EQ(villager->get_component<SettlementResidentComponent>(), nullptr);
}

TEST_F(SettlementLifeSystemTest, AVillagerThePlayerMovesStopsBeingAResident) {
  Engine::Core::World world;
  add_home(world, 1, 0.0F, 0.0F);
  auto* villager = add_villager(world, 1, 4.0F, 0.0F);

  Game::Systems::SettlementLifeSystem system;
  run_for(system, world, 3.0F);
  ASSERT_NE(villager->get_component<SettlementResidentComponent>(), nullptr);

  Game::Systems::OrderService::prepare_for_move(
      villager, Game::Systems::MoveOrderKind::PlayerMove, false);
  villager->remove_component<Engine::Core::PlayerOrderIntentComponent>();
  run_for(system, world, 5.0F);

  const auto* resident = villager->get_component<SettlementResidentComponent>();
  ASSERT_NE(resident, nullptr);
  EXPECT_TRUE(resident->released);
  EXPECT_EQ(resident->errand, SettlementErrand::Settling);
}

TEST_F(SettlementLifeSystemTest, AVillagerThePlayerMovedIsNeverReadopted) {
  Engine::Core::World world;
  add_home(world, 1, 0.0F, 0.0F);
  auto* villager = add_villager(world, 1, 4.0F, 0.0F);

  Game::Systems::OrderService::prepare_for_move(
      villager, Game::Systems::MoveOrderKind::PlayerMove, false);
  villager->remove_component<Engine::Core::PlayerOrderIntentComponent>();

  Game::Systems::SettlementLifeSystem system;
  run_for(system, world, 6.0F);

  const auto* resident = villager->get_component<SettlementResidentComponent>();
  ASSERT_NE(resident, nullptr);
  EXPECT_TRUE(resident->released);
}

TEST_F(SettlementLifeSystemTest, ResidentsRunFromAnArmedEnemy) {
  Engine::Core::World world;
  add_home(world, 1, 0.0F, 0.0F);
  auto* villager = add_villager(world, 1, 4.0F, 0.0F);

  Game::Systems::SettlementLifeSystem system;
  run_for(system, world, 3.0F);
  ASSERT_NE(villager->get_component<SettlementResidentComponent>(), nullptr);

  add_spearman(world, 2, 9.0F, 0.0F);
  run_for(system, world, 1.0F);

  const auto* resident = villager->get_component<SettlementResidentComponent>();
  ASSERT_NE(resident, nullptr);
  EXPECT_EQ(resident->errand, SettlementErrand::Fleeing);

  EXPECT_LT(resident->errand_x, 4.0F);
}

TEST_F(SettlementLifeSystemTest, AFriendlyGarrisonIsNotAThreat) {
  Engine::Core::World world;
  add_home(world, 1, 0.0F, 0.0F);
  auto* villager = add_villager(world, 1, 4.0F, 0.0F);
  add_spearman(world, 1, 6.0F, 0.0F);

  Game::Systems::SettlementLifeSystem system;
  run_for(system, world, 4.0F);

  const auto* resident = villager->get_component<SettlementResidentComponent>();
  ASSERT_NE(resident, nullptr);
  EXPECT_NE(resident->errand, SettlementErrand::Fleeing);
}

TEST_F(SettlementLifeSystemTest, AVillagerAlreadyInAFightIsLeftToTheCombatSystem) {
  Engine::Core::World world;
  add_home(world, 1, 0.0F, 0.0F);
  auto* villager = add_villager(world, 1, 4.0F, 0.0F);

  Game::Systems::SettlementLifeSystem system;
  run_for(system, world, 3.0F);

  auto* raider = add_spearman(world, 2, 8.0F, 0.0F);
  auto* target = villager->add_component<Engine::Core::AttackTargetComponent>();
  target->target_id = raider->get_id();

  run_for(system, world, 1.0F);

  ASSERT_NE(villager->get_component<Engine::Core::AttackTargetComponent>(), nullptr);
  const auto* resident = villager->get_component<SettlementResidentComponent>();
  ASSERT_NE(resident, nullptr);
  EXPECT_EQ(resident->errand, SettlementErrand::Settling);
  EXPECT_FALSE(resident->is_labouring());
}

} // namespace
