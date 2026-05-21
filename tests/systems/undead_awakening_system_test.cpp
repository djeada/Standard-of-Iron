#include <gtest/gtest.h>

#include "core/component.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/undead_awakening_system.h"

namespace {

auto make_test_map() -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map_definition;
  map_definition.grid.width = 32;
  map_definition.grid.height = 32;
  map_definition.grid.tile_size = 1.0F;

  Game::Map::WorldProp ruins;
  ruins.type = Game::Map::WorldProp::Type::Ruins;
  ruins.x = 16.0F;
  ruins.z = 16.0F;
  map_definition.world_props.push_back(ruins);

  Game::Map::UndeadZone zone;
  zone.id = QStringLiteral("sepulcher_zone");
  zone.anchor_type = Game::Map::WorldProp::Type::Ruins;
  zone.x = 16.0F;
  zone.z = 16.0F;
  zone.radius = 6.0F;
  zone.owner_id = 99;
  zone.team_id = 99;
  zone.awaken_on = {QStringLiteral("unit_enters_radius")};

  Game::Map::UndeadWave initial_wave;
  initial_wave.trigger = QStringLiteral("initial");
  initial_wave.units.push_back({Game::Units::SpawnType::SkeletonSwordsman, 2});
  zone.waves.push_back(initial_wave);

  map_definition.undead_zones.push_back(zone);
  return map_definition;
}

auto count_owner_units(Engine::Core::World& world, int owner_id) -> int {
  int count = 0;
  for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->owner_id == owner_id && unit->health > 0) {
      ++count;
    }
  }
  return count;
}

class UndeadAwakeningSystemTest : public ::testing::Test {
protected:
  void SetUp() override {
    auto& owners = Game::Systems::OwnerRegistry::instance();
    owners.clear();
    owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "Player");
    owners.set_owner_team(1, 1);
    owners.set_local_player_id(1);

    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    nations.initialize_defaults();
    nations.set_player_nation(1, Game::Systems::NationID::RomanRepublic);

    Game::Systems::GlobalStatsRegistry::instance().clear();
  }

  void TearDown() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }
};

TEST_F(UndeadAwakeningSystemTest, SpawnsOnlyAfterEnemyUnitEntersZone) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system;

  const Game::Map::MapDefinition map_definition = make_test_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);

  system.update(&world, 0.1F);
  EXPECT_EQ(count_owner_units(world, 99), 0);

  auto* player_entity = world.create_entity();
  ASSERT_NE(player_entity, nullptr);
  auto* transform = player_entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = player_entity->add_component<Engine::Core::UnitComponent>();
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(unit, nullptr);
  transform->position = {0.5F, 0.0F, 0.5F};
  unit->owner_id = 1;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->health = 100;
  unit->max_health = 100;

  system.update(&world, 0.1F);
  EXPECT_EQ(count_owner_units(world, 99), 2);

  for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
    auto* spawned = entity->get_component<Engine::Core::UnitComponent>();
    if (spawned == nullptr || spawned->owner_id != 99) {
      continue;
    }
    EXPECT_EQ(spawned->nation_id, Game::Systems::NationID::IronSepulcher);
  }
}

TEST_F(UndeadAwakeningSystemTest, RestoredStateDoesNotRespawnActiveWave) {
  Engine::Core::World world;
  const Game::Map::MapDefinition map_definition = make_test_map();
  Game::Map::TerrainService::instance().initialize(map_definition);

  Game::Systems::UndeadAwakeningSystem first_system;
  first_system.configure(map_definition);

  auto* player_entity = world.create_entity();
  ASSERT_NE(player_entity, nullptr);
  auto* transform = player_entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = player_entity->add_component<Engine::Core::UnitComponent>();
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(unit, nullptr);
  transform->position = {0.5F, 0.0F, 0.5F};
  unit->owner_id = 1;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->health = 100;
  unit->max_health = 100;

  first_system.update(&world, 0.1F);
  ASSERT_EQ(count_owner_units(world, 99), 2);

  Game::Systems::UndeadAwakeningSystem restored_system;
  restored_system.configure(map_definition);
  restored_system.restore_state(first_system.serialize_state());
  restored_system.update(&world, 0.1F);

  EXPECT_EQ(count_owner_units(world, 99), 2);
}

} // namespace
