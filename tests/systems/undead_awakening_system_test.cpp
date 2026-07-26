#include <cmath>
#include <gtest/gtest.h>
#include <map>
#include <vector>

#include "core/component.h"
#include "core/event_manager.h"
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

constexpr char k_shrine_zone_id[] = "sepulcher_shrine";

auto make_shrine_map() -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map_definition = make_test_map();

  Game::Map::WorldProp shrine;
  shrine.type = Game::Map::WorldProp::Type::MagicShrine;
  shrine.x = 16.0F;
  shrine.z = 16.0F;
  map_definition.world_props.push_back(shrine);

  auto& zone = map_definition.undead_zones.front();
  zone.id = QString::fromLatin1(k_shrine_zone_id);
  zone.anchor_type = Game::Map::WorldProp::Type::MagicShrine;
  return map_definition;
}

auto make_two_wave_shrine_map(float wave_timeout) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map_definition = make_shrine_map();
  auto& zone = map_definition.undead_zones.front();
  zone.wave_timeout_seconds = wave_timeout;

  Game::Map::UndeadWave follow_up;
  follow_up.trigger = QStringLiteral("after_clear");
  follow_up.units.push_back({Game::Units::SpawnType::SkeletonArcher, 1});
  zone.waves.push_back(follow_up);
  return map_definition;
}

auto add_intruder(Engine::Core::World& world,
                  const QVector3D& position) -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  if (entity == nullptr) {
    return nullptr;
  }
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  if (transform == nullptr || unit == nullptr) {
    return nullptr;
  }
  transform->position = {position.x(), position.y(), position.z()};
  unit->owner_id = 1;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->health = 100;
  unit->max_health = 100;
  return entity;
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

TEST_F(UndeadAwakeningSystemTest, ZoneWithoutAuthoredWavesRaisesTheDefaultGarrison) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system;

  Game::Map::MapDefinition map_definition = make_test_map();
  map_definition.undead_zones.front().waves.clear();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);

  add_intruder(world, {0.5F, 0.0F, 0.5F});
  system.update(&world, 0.1F);

  std::map<Game::Units::SpawnType, int> roster;
  for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->owner_id == 99 && unit->health > 0) {
      roster[unit->spawn_type] += 1;
    }
  }

  EXPECT_EQ(roster[Game::Units::SpawnType::SkeletonSwordsman], 2);
  EXPECT_EQ(roster[Game::Units::SpawnType::SkeletonArcher], 1);
  EXPECT_EQ(roster[Game::Units::SpawnType::GravePriest], 1);
}

TEST_F(UndeadAwakeningSystemTest, MapAuthoredWavesOverrideTheDefaultGarrison) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system;

  const Game::Map::MapDefinition map_definition = make_test_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);

  add_intruder(world, {0.5F, 0.0F, 0.5F});
  system.update(&world, 0.1F);

  for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->owner_id == 99) {
      EXPECT_EQ(unit->spawn_type, Game::Units::SpawnType::SkeletonSwordsman);
    }
  }
  EXPECT_EQ(count_owner_units(world, 99), 2);
}

TEST_F(UndeadAwakeningSystemTest, WaveRisesTogetherAtDistinctSpreadPositions) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system;

  Game::Map::MapDefinition map_definition = make_test_map();
  map_definition.undead_zones.front().waves.front().units = {
      {Game::Units::SpawnType::SkeletonSwordsman, 6}};
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);

  add_intruder(world, {0.5F, 0.0F, 0.5F});
  system.update(&world, 0.1F);

  std::vector<QVector3D> positions;
  for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit != nullptr && transform != nullptr && unit->owner_id == 99) {
      positions.emplace_back(
          transform->position.x, transform->position.y, transform->position.z);
    }
  }

  ASSERT_EQ(positions.size(), 6U);
  for (std::size_t i = 0; i < positions.size(); ++i) {
    for (std::size_t j = i + 1; j < positions.size(); ++j) {
      const float dx = positions[i].x() - positions[j].x();
      const float dz = positions[i].z() - positions[j].z();
      EXPECT_GT(std::sqrt(dx * dx + dz * dz), 0.75F)
          << "guardians " << i << " and " << j << " spawned on top of each other";
    }
  }
}

TEST_F(UndeadAwakeningSystemTest, ShrineZoneGarrisonsACapturableSepulcherBarracks) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system;

  const Game::Map::MapDefinition map_definition = make_shrine_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);
  system.update(&world, 0.1F);

  const Engine::Core::EntityID anchor_id = system.anchor_entity(k_shrine_zone_id);
  ASSERT_NE(anchor_id, 0U);
  auto* anchor = world.get_entity(anchor_id);
  ASSERT_NE(anchor, nullptr);

  auto* unit = anchor->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  EXPECT_EQ(unit->spawn_type, Game::Units::SpawnType::Barracks);
  EXPECT_EQ(unit->owner_id, 99);
  EXPECT_EQ(unit->nation_id, Game::Systems::NationID::IronSepulcher);
  EXPECT_EQ(anchor->get_component<Engine::Core::ProductionComponent>(), nullptr);

  system.update(&world, 0.1F);
  EXPECT_EQ(system.anchor_entity(k_shrine_zone_id), anchor_id);
}

TEST_F(UndeadAwakeningSystemTest, RuinZoneKeepsItsAnchorDecorative) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system;

  const Game::Map::MapDefinition map_definition = make_test_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);
  system.update(&world, 0.1F);

  EXPECT_EQ(system.anchor_entity(QStringLiteral("sepulcher_zone")), 0U);
  EXPECT_EQ(count_owner_units(world, 99), 0);
}

TEST_F(UndeadAwakeningSystemTest, LosingTheShrineBreaksTheGarrisonAndClearsTheZone) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system;

  const Game::Map::MapDefinition map_definition = make_shrine_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);

  add_intruder(world, {0.5F, 0.0F, 0.5F});
  system.update(&world, 0.1F);
  ASSERT_GT(count_owner_units(world, 99), 1);
  EXPECT_FALSE(system.is_zone_cleared(k_shrine_zone_id));

  auto* anchor = world.get_entity(system.anchor_entity(k_shrine_zone_id));
  ASSERT_NE(anchor, nullptr);
  anchor->get_component<Engine::Core::UnitComponent>()->health = 0;

  system.update(&world, 0.1F);

  EXPECT_EQ(count_owner_units(world, 99), 0);
  EXPECT_TRUE(system.is_zone_cleared(k_shrine_zone_id));
  EXPECT_TRUE(system.is_shrine_purified(k_shrine_zone_id));
}

TEST_F(UndeadAwakeningSystemTest, BrokenGarrisonSurvivesASaveLoadRoundTrip) {
  Engine::Core::World world;
  const Game::Map::MapDefinition map_definition = make_shrine_map();
  Game::Map::TerrainService::instance().initialize(map_definition);

  Game::Systems::UndeadAwakeningSystem first_system;
  first_system.configure(map_definition);
  add_intruder(world, {0.5F, 0.0F, 0.5F});
  first_system.update(&world, 0.1F);
  world.get_entity(first_system.anchor_entity(k_shrine_zone_id))
      ->get_component<Engine::Core::UnitComponent>()
      ->health = 0;
  first_system.update(&world, 0.1F);
  ASSERT_TRUE(first_system.is_zone_cleared(k_shrine_zone_id));

  Game::Systems::UndeadAwakeningSystem restored_system;
  restored_system.configure(map_definition);
  restored_system.restore_state(first_system.serialize_state());
  restored_system.update(&world, 0.1F);

  EXPECT_TRUE(restored_system.is_zone_cleared(k_shrine_zone_id));
  EXPECT_EQ(count_owner_units(world, 99), 0)
      << "a restored broken zone must not raise a fresh garrison or shrine";
}

TEST_F(UndeadAwakeningSystemTest, EveryWaveAnnouncesItsNumberOutOfTheTotal) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system;

  std::vector<QString> announcements;
  Engine::Core::ScopedEventSubscription<Engine::Core::MissionAnnouncementEvent> const
      subscription([&announcements](const Engine::Core::MissionAnnouncementEvent& e) {
        announcements.push_back(e.text);
      });

  const Game::Map::MapDefinition map_definition = make_two_wave_shrine_map(0.0F);
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);

  add_intruder(world, {0.5F, 0.0F, 0.5F});
  system.update(&world, 0.1F);
  ASSERT_EQ(announcements.size(), 1U);
  EXPECT_TRUE(announcements.front().contains(QStringLiteral("Wave 1/2")))
      << announcements.front().toStdString();

  for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->owner_id == 99 &&
        Game::Units::is_troop_spawn(unit->spawn_type)) {
      unit->health = 0;
    }
  }

  for (int tick = 0; tick < 40; ++tick) {
    system.update(&world, 0.1F);
  }

  ASSERT_GE(announcements.size(), 2U);
  EXPECT_TRUE(announcements[1].contains(QStringLiteral("Wave 2/2")))
      << announcements[1].toStdString();
}

TEST_F(UndeadAwakeningSystemTest, UnclearedWaveIsFollowedByTheNextOnceItTimesOut) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system;

  const Game::Map::MapDefinition map_definition = make_two_wave_shrine_map(5.0F);
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);

  add_intruder(world, {0.5F, 0.0F, 0.5F});
  system.update(&world, 0.1F);
  const int first_wave_size = count_owner_units(world, 99);
  ASSERT_GT(first_wave_size, 0);

  for (int tick = 0; tick < 20; ++tick) {
    system.update(&world, 0.5F);
  }

  EXPECT_GT(count_owner_units(world, 99), first_wave_size)
      << "a wave the player cannot finish in time must still escalate";
}

TEST_F(UndeadAwakeningSystemTest, ZeroWaveTimeoutWaitsForTheWaveToBeCleared) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system;

  const Game::Map::MapDefinition map_definition = make_two_wave_shrine_map(0.0F);
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);

  add_intruder(world, {0.5F, 0.0F, 0.5F});
  system.update(&world, 0.1F);
  const int first_wave_size = count_owner_units(world, 99);

  for (int tick = 0; tick < 40; ++tick) {
    system.update(&world, 0.5F);
  }

  EXPECT_EQ(count_owner_units(world, 99), first_wave_size);
}

TEST_F(UndeadAwakeningSystemTest, ShrineCaptureIsLockedWhileAnyGuardianStands) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system;

  const Game::Map::MapDefinition map_definition = make_two_wave_shrine_map(0.0F);
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);
  system.update(&world, 0.1F);

  auto* anchor = world.get_entity(system.anchor_entity(k_shrine_zone_id));
  ASSERT_NE(anchor, nullptr);
  auto* capture = anchor->get_component<Engine::Core::CaptureComponent>();
  ASSERT_NE(capture, nullptr);
  EXPECT_FALSE(capture->capture_blocked) << "a dormant shrine is free to take";

  add_intruder(world, {0.5F, 0.0F, 0.5F});
  system.update(&world, 0.1F);
  EXPECT_TRUE(capture->capture_blocked);

  for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->owner_id == 99 &&
        Game::Units::is_troop_spawn(unit->spawn_type)) {
      unit->health = 0;
    }
  }
  system.update(&world, 0.1F);
  EXPECT_FALSE(capture->capture_blocked)
      << "the flag becomes takeable in the gap between waves";
}

TEST_F(UndeadAwakeningSystemTest, AwakeningAndDefeatEachAnnounceExactlyOnce) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system;

  std::vector<QString> announcements;
  Engine::Core::ScopedEventSubscription<Engine::Core::MissionAnnouncementEvent> const
      subscription([&announcements](const Engine::Core::MissionAnnouncementEvent& e) {
        announcements.push_back(e.text);
      });

  const Game::Map::MapDefinition map_definition = make_shrine_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);

  system.update(&world, 0.1F);
  EXPECT_TRUE(announcements.empty());

  add_intruder(world, {0.5F, 0.0F, 0.5F});
  system.update(&world, 0.1F);
  system.update(&world, 0.1F);
  ASSERT_EQ(announcements.size(), 1U);
  EXPECT_TRUE(announcements.front().contains(QStringLiteral("wakes")));
  EXPECT_TRUE(announcements.front().contains(QStringLiteral("Wave 1/1")));

  world.get_entity(system.anchor_entity(k_shrine_zone_id))
      ->get_component<Engine::Core::UnitComponent>()
      ->health = 0;
  system.update(&world, 0.1F);
  system.update(&world, 0.1F);

  ASSERT_EQ(announcements.size(), 2U);
  EXPECT_TRUE(announcements.back().contains(QStringLiteral("shrine")));
}

} // namespace
