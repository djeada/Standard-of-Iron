#include <cmath>
#include <gtest/gtest.h>
#include <map>
#include <tuple>
#include <vector>

#include "core/component_gameplay.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
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

auto make_two_zone_map() -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map_definition;
  map_definition.grid.width = 48;
  map_definition.grid.height = 48;
  map_definition.grid.tile_size = 1.0F;

  for (const auto& [id, x, z] : std::vector<std::tuple<QString, float, float>>{
           {QStringLiteral("north_zone"), 16.0F, 16.0F},
           {QStringLiteral("south_zone"), 32.0F, 32.0F}}) {
    Game::Map::UndeadZone zone;
    zone.id = id;
    zone.anchor_type = Game::Map::WorldProp::Type::Ruins;
    zone.x = x;
    zone.z = z;
    zone.radius = 6.0F;
    zone.owner_id = 99;
    zone.team_id = 99;
    zone.awaken_on = {QStringLiteral("unit_enters_radius")};
    map_definition.undead_zones.push_back(zone);
  }

  return map_definition;
}

auto count_shrine_props() -> int {
  int count = 0;
  for (const auto& prop : Game::Map::TerrainService::instance().world_props()) {
    if (prop.type == Game::Map::WorldProp::Type::MagicShrine) {
      ++count;
    }
  }
  return count;
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

auto undead_services() -> Game::Systems::UndeadAwakeningSystem::Services {
  auto& session = Game::Session::SessionContext::active();
  return {.terrain = session.terrain(),
          .owners = session.owners(),
          .nations = session.nations(),
          .stats = session.stats(),
          .economy = session.economy()};
}

auto count_owner_units(Engine::Core::World& world, int owner_id) -> int {
  int count = 0;
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->owner_id == owner_id && unit->health > 0 &&
        Game::Units::is_troop_spawn(unit->spawn_type)) {
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
    Game::Systems::initialize_default_content(nations);
    nations.set_player_nation(1, Game::Systems::NationID::RomanRepublic);

    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
  }

  void TearDown() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::GlobalStatsRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }
};

TEST_F(UndeadAwakeningSystemTest, SpawnsOnlyAfterEnemyUnitEntersZone) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system(undead_services());

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

  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    auto* spawned = entity->get_component<Engine::Core::UnitComponent>();
    if (spawned == nullptr || spawned->owner_id != 99) {
      continue;
    }
    EXPECT_EQ(spawned->nation_id, Game::Systems::NationID::IronSepulcher);
  }
}

TEST_F(UndeadAwakeningSystemTest, AwakeningCueBelongsToThePlayerWhoWokeTheZone) {
  auto& owners = Game::Systems::OwnerRegistry::instance();
  owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "Rival");
  owners.set_owner_team(2, 2);

  std::vector<int> cue_owners;
  auto subscription =
      Engine::Core::ScopedEventSubscription<Engine::Core::AudioCueEvent>(
          [&cue_owners](const Engine::Core::AudioCueEvent& event) {
            if (event.cue_id == "alert.undead_awakening") {
              cue_owners.push_back(event.owner_id);
            }
          });

  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system(undead_services());

  const Game::Map::MapDefinition map_definition = make_test_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);
  system.update(&world, 0.1F);
  ASSERT_TRUE(cue_owners.empty());

  auto* rival = world.create_entity();
  ASSERT_NE(rival, nullptr);
  auto* transform = rival->add_component<Engine::Core::TransformComponent>();
  auto* unit = rival->add_component<Engine::Core::UnitComponent>();
  ASSERT_NE(transform, nullptr);
  ASSERT_NE(unit, nullptr);
  transform->position = {0.5F, 0.0F, 0.5F};
  unit->owner_id = 2;
  unit->nation_id = Game::Systems::NationID::RomanRepublic;
  unit->spawn_type = Game::Units::SpawnType::Knight;
  unit->health = 100;
  unit->max_health = 100;

  system.update(&world, 0.1F);

  ASSERT_EQ(cue_owners.size(), 1U);
  EXPECT_EQ(cue_owners.front(), 2)
      << "the sepulcher woke for owner 2, so only owner 2 should hear it; "
         "owner 0 would broadcast it to every player";
}

TEST_F(UndeadAwakeningSystemTest, RestoredStateDoesNotRespawnActiveWave) {
  Engine::Core::World world;
  const Game::Map::MapDefinition map_definition = make_test_map();
  Game::Map::TerrainService::instance().initialize(map_definition);

  Game::Systems::UndeadAwakeningSystem first_system(undead_services());
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

  Game::Systems::UndeadAwakeningSystem restored_system(undead_services());
  restored_system.configure(map_definition);
  restored_system.restore_state(first_system.serialize_state());
  restored_system.update(&world, 0.1F);

  EXPECT_EQ(count_owner_units(world, 99), 2);
}

TEST_F(UndeadAwakeningSystemTest, ZoneWithoutAuthoredWavesRaisesTheDefaultGarrison) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system(undead_services());

  Game::Map::MapDefinition map_definition = make_test_map();
  map_definition.undead_zones.front().waves.clear();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);

  add_intruder(world, {0.5F, 0.0F, 0.5F});
  system.update(&world, 0.1F);

  std::map<Game::Units::SpawnType, int> roster;
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->owner_id == 99 && unit->health > 0 &&
        Game::Units::is_troop_spawn(unit->spawn_type)) {
      roster[unit->spawn_type] += 1;
    }
  }

  EXPECT_EQ(roster[Game::Units::SpawnType::SkeletonSwordsman], 2);
  EXPECT_EQ(roster[Game::Units::SpawnType::SkeletonArcher], 1);
  EXPECT_EQ(roster[Game::Units::SpawnType::GravePriest], 1);
}

TEST_F(UndeadAwakeningSystemTest, MapAuthoredWavesOverrideTheDefaultGarrison) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system(undead_services());

  const Game::Map::MapDefinition map_definition = make_test_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);

  add_intruder(world, {0.5F, 0.0F, 0.5F});
  system.update(&world, 0.1F);

  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit != nullptr && unit->owner_id == 99 &&
        Game::Units::is_troop_spawn(unit->spawn_type)) {
      EXPECT_EQ(unit->spawn_type, Game::Units::SpawnType::SkeletonSwordsman);
    }
  }
  EXPECT_EQ(count_owner_units(world, 99), 2);
}

TEST_F(UndeadAwakeningSystemTest, WaveRisesTogetherAtDistinctSpreadPositions) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system(undead_services());

  Game::Map::MapDefinition map_definition = make_test_map();
  map_definition.undead_zones.front().waves.front().units = {
      {Game::Units::SpawnType::SkeletonSwordsman, 6}};
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);

  add_intruder(world, {0.5F, 0.0F, 0.5F});
  system.update(&world, 0.1F);

  std::vector<QVector3D> positions;
  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit != nullptr && transform != nullptr && unit->owner_id == 99 &&
        Game::Units::is_troop_spawn(unit->spawn_type)) {
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
  Game::Systems::UndeadAwakeningSystem system(undead_services());

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

TEST_F(UndeadAwakeningSystemTest, RuinZoneAlsoRaisesAShrineBesideItsRuins) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system(undead_services());

  const Game::Map::MapDefinition map_definition = make_test_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);
  system.update(&world, 0.1F);

  const QString zone_id = QStringLiteral("sepulcher_zone");
  EXPECT_TRUE(system.has_shrine(zone_id));

  const Engine::Core::EntityID anchor_id = system.anchor_entity(zone_id);
  ASSERT_NE(anchor_id, 0U);
  auto* anchor = world.get_entity(anchor_id);
  ASSERT_NE(anchor, nullptr);
  auto* unit = anchor->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(unit, nullptr);
  EXPECT_EQ(unit->spawn_type, Game::Units::SpawnType::Barracks);
  EXPECT_EQ(unit->owner_id, 99);

  const QVector3D shrine_position = system.shrine_world_position(zone_id);
  const float dx = shrine_position.x() - 0.5F;
  const float dz = shrine_position.z() - 0.5F;
  EXPECT_GT(std::sqrt(dx * dx + dz * dz), 1.0F)
      << "the ruins occupy the centre, so the shrine has to step aside";
  EXPECT_LT(std::sqrt(dx * dx + dz * dz), map_definition.undead_zones.front().radius)
      << "the shrine still belongs to its zone";
}

TEST_F(UndeadAwakeningSystemTest, EveryZoneRaisesExactlyOneShrineOfItsOwn) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system(undead_services());

  const Game::Map::MapDefinition map_definition = make_two_zone_map();
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);
  system.update(&world, 0.1F);

  EXPECT_EQ(count_shrine_props(), 2);
  EXPECT_TRUE(system.zones_without_shrine().empty());

  const QString north = QStringLiteral("north_zone");
  const QString south = QStringLiteral("south_zone");
  EXPECT_TRUE(system.has_shrine(north));
  EXPECT_TRUE(system.has_shrine(south));
  EXPECT_NE(system.shrine_prop_id(north), system.shrine_prop_id(south));
  EXPECT_NE(system.anchor_entity(north), 0U);
  EXPECT_NE(system.anchor_entity(south), 0U);
  EXPECT_NE(system.anchor_entity(north), system.anchor_entity(south));

  for (const QString& zone_id : {north, south}) {
    auto* anchor = world.get_entity(system.anchor_entity(zone_id));
    ASSERT_NE(anchor, nullptr) << zone_id.toStdString();
    auto* unit = anchor->get_component<Engine::Core::UnitComponent>();
    ASSERT_NE(unit, nullptr);
    EXPECT_EQ(unit->spawn_type, Game::Units::SpawnType::Barracks);
    EXPECT_EQ(unit->owner_id, 99);
    EXPECT_EQ(unit->nation_id, Game::Systems::NationID::IronSepulcher);
    EXPECT_EQ(anchor->get_component<Engine::Core::ProductionComponent>(), nullptr)
        << "a shrine is captured, never recruited from";

    auto* transform = anchor->get_component<Engine::Core::TransformComponent>();
    ASSERT_NE(transform, nullptr);
    const QVector3D shrine = system.shrine_world_position(zone_id);
    EXPECT_NEAR(transform->position.x, shrine.x(), 0.01F);
    EXPECT_NEAR(transform->position.z, shrine.z(), 0.01F);
  }
}

TEST_F(UndeadAwakeningSystemTest, ReconfiguringAZoneDoesNotStampOutASecondShrine) {
  Engine::Core::World world;
  const Game::Map::MapDefinition map_definition = make_two_zone_map();
  Game::Map::TerrainService::instance().initialize(map_definition);

  Game::Systems::UndeadAwakeningSystem system(undead_services());
  system.configure(map_definition);
  const std::uint64_t first_prop_id =
      system.shrine_prop_id(QStringLiteral("north_zone"));

  system.configure(map_definition);

  EXPECT_EQ(count_shrine_props(), 2);
  EXPECT_EQ(system.shrine_prop_id(QStringLiteral("north_zone")), first_prop_id);
}

TEST_F(UndeadAwakeningSystemTest, WorldSpaceZoneKeepsItsShrinePropAndBarracksTogether) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system(undead_services());

  Game::Map::MapDefinition terrain_map;
  terrain_map.grid.width = 64;
  terrain_map.grid.height = 64;
  terrain_map.grid.tile_size = 1.0F;
  auto& terrain = Game::Map::TerrainService::instance();
  terrain.initialize(terrain_map);

  Game::Map::MapDefinition zone_map = terrain_map;
  zone_map.coordSystem = Game::Map::CoordSystem::World;
  Game::Map::UndeadZone zone;
  zone.id = QStringLiteral("world_zone");
  zone.anchor_type = Game::Map::WorldProp::Type::Ruins;
  zone.x = 0.0F;
  zone.z = 6.0F;
  zone.radius = 6.0F;
  zone.owner_id = 99;
  zone.team_id = 99;
  zone_map.undead_zones.push_back(zone);

  system.configure(zone_map);
  system.update(&world, 0.1F);

  ASSERT_TRUE(system.has_shrine(zone.id));
  const QVector3D shrine = system.shrine_world_position(zone.id);
  EXPECT_LE(std::hypot(shrine.x() - zone.x, shrine.z() - zone.z), zone.radius)
      << "a world-space zone must not read its centre as grid coordinates";

  const std::uint64_t prop_id = system.shrine_prop_id(zone.id);
  ASSERT_NE(prop_id, 0U);
  const Game::Map::WorldProp* prop = nullptr;
  for (const auto& candidate : terrain.world_props()) {
    if (candidate.id == prop_id) {
      prop = &candidate;
    }
  }
  ASSERT_NE(prop, nullptr);

  const QVector3D prop_world = terrain.world_prop_world_position(*prop);
  EXPECT_NEAR(prop_world.x(), shrine.x(), 0.01F)
      << "the shrine mesh must stand where its barracks does";
  EXPECT_NEAR(prop_world.z(), shrine.z(), 0.01F);

  auto* anchor = world.get_entity(system.anchor_entity(zone.id));
  ASSERT_NE(anchor, nullptr);
  auto* transform = anchor->get_component<Engine::Core::TransformComponent>();
  ASSERT_NE(transform, nullptr);
  EXPECT_NEAR(transform->position.x, shrine.x(), 0.01F);
  EXPECT_NEAR(transform->position.z, shrine.z(), 0.01F);
}

TEST_F(UndeadAwakeningSystemTest, ZoneDrownedByALakeReportsItsShrinePlacementFailure) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system(undead_services());

  Game::Map::MapDefinition map_definition = make_test_map();
  map_definition.world_props.clear();
  map_definition.lakes.push_back(Game::Map::Lake{
      .center = QVector3D(0.5F, 0.0F, 0.5F), .width = 40.0F, .depth = 40.0F});
  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);
  system.update(&world, 0.1F);

  const QString zone_id = QStringLiteral("sepulcher_zone");
  EXPECT_FALSE(system.has_shrine(zone_id));
  EXPECT_EQ(system.anchor_entity(zone_id), 0U);
  EXPECT_EQ(count_shrine_props(), 0);

  const auto unplaced = system.zones_without_shrine();
  ASSERT_EQ(unplaced.size(), 1U);
  EXPECT_EQ(unplaced.front(), zone_id);
}

TEST_F(UndeadAwakeningSystemTest, ShrineHealthAndZoneAssociationSurviveASaveLoad) {
  Engine::Core::World world;
  const Game::Map::MapDefinition map_definition = make_two_zone_map();
  Game::Map::TerrainService::instance().initialize(map_definition);

  Game::Systems::UndeadAwakeningSystem first_system(undead_services());
  first_system.configure(map_definition);
  first_system.update(&world, 0.1F);

  const QString zone_id = QStringLiteral("north_zone");
  const Engine::Core::EntityID anchor_id = first_system.anchor_entity(zone_id);
  ASSERT_NE(anchor_id, 0U);
  auto* anchor_unit =
      world.get_entity(anchor_id)->get_component<Engine::Core::UnitComponent>();
  ASSERT_NE(anchor_unit, nullptr);
  anchor_unit->health = 640;

  Game::Systems::UndeadAwakeningSystem restored_system(undead_services());
  restored_system.configure(map_definition);
  restored_system.restore_state(first_system.serialize_state());
  restored_system.update(&world, 0.1F);

  EXPECT_EQ(restored_system.anchor_entity(zone_id), anchor_id)
      << "the restored zone must keep the shrine it already owns";
  EXPECT_EQ(anchor_unit->health, 640) << "a reload must not heal the shrine";
  EXPECT_EQ(count_shrine_props(), 2) << "a reload must not plant a second shrine";
  EXPECT_TRUE(restored_system.has_shrine(zone_id));
}

TEST_F(UndeadAwakeningSystemTest, LosingTheShrineBreaksTheGarrisonAndClearsTheZone) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system(undead_services());

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

TEST_F(UndeadAwakeningSystemTest, BreakingTheGarrisonPaysTheAuthoredClearReward) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system(undead_services());

  Game::Map::MapDefinition map_definition = make_shrine_map();
  ASSERT_FALSE(map_definition.undead_zones.empty());
  auto& reward = map_definition.undead_zones.front().clear_reward;
  reward.set(Game::Systems::ResourceType::Stone, 120);
  reward.set(Game::Systems::ResourceType::Iron, 60);

  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);

  auto& resources = Game::Systems::PlayerResourceRegistry::instance();
  const int stone_before = resources.get(1, Game::Systems::ResourceType::Stone);
  const int iron_before = resources.get(1, Game::Systems::ResourceType::Iron);
  const int gold_before = resources.get(1, Game::Systems::ResourceType::Gold);

  add_intruder(world, {0.5F, 0.0F, 0.5F});
  system.update(&world, 0.1F);
  ASSERT_FALSE(system.is_zone_cleared(k_shrine_zone_id));
  EXPECT_EQ(resources.get(1, Game::Systems::ResourceType::Stone), stone_before)
      << "an unbroken garrison must not pay out";

  world.get_entity(system.anchor_entity(k_shrine_zone_id))
      ->get_component<Engine::Core::UnitComponent>()
      ->health = 0;
  system.update(&world, 0.1F);
  ASSERT_TRUE(system.is_zone_cleared(k_shrine_zone_id));

  EXPECT_EQ(resources.get(1, Game::Systems::ResourceType::Stone), stone_before + 120);
  EXPECT_EQ(resources.get(1, Game::Systems::ResourceType::Iron), iron_before + 60);
  EXPECT_EQ(resources.get(1, Game::Systems::ResourceType::Gold), gold_before)
      << "unlisted resources must not be granted";

  system.update(&world, 0.1F);
  EXPECT_EQ(resources.get(1, Game::Systems::ResourceType::Stone), stone_before + 120)
      << "the hoard must only be paid once";
}

TEST_F(UndeadAwakeningSystemTest, ZoneWithoutAClearRewardPaysNothing) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system(undead_services());

  const Game::Map::MapDefinition map_definition = make_shrine_map();
  ASSERT_TRUE(map_definition.undead_zones.front().clear_reward.empty());

  Game::Map::TerrainService::instance().initialize(map_definition);
  system.configure(map_definition);

  auto& resources = Game::Systems::PlayerResourceRegistry::instance();
  const int gold_before = resources.get(1, Game::Systems::ResourceType::Gold);

  add_intruder(world, {0.5F, 0.0F, 0.5F});
  system.update(&world, 0.1F);
  world.get_entity(system.anchor_entity(k_shrine_zone_id))
      ->get_component<Engine::Core::UnitComponent>()
      ->health = 0;
  system.update(&world, 0.1F);
  ASSERT_TRUE(system.is_zone_cleared(k_shrine_zone_id));

  EXPECT_EQ(resources.get(1, Game::Systems::ResourceType::Gold), gold_before);
}

TEST_F(UndeadAwakeningSystemTest, BrokenGarrisonSurvivesASaveLoadRoundTrip) {
  Engine::Core::World world;
  const Game::Map::MapDefinition map_definition = make_shrine_map();
  Game::Map::TerrainService::instance().initialize(map_definition);

  Game::Systems::UndeadAwakeningSystem first_system(undead_services());
  first_system.configure(map_definition);
  add_intruder(world, {0.5F, 0.0F, 0.5F});
  first_system.update(&world, 0.1F);
  world.get_entity(first_system.anchor_entity(k_shrine_zone_id))
      ->get_component<Engine::Core::UnitComponent>()
      ->health = 0;
  first_system.update(&world, 0.1F);
  ASSERT_TRUE(first_system.is_zone_cleared(k_shrine_zone_id));

  Game::Systems::UndeadAwakeningSystem restored_system(undead_services());
  restored_system.configure(map_definition);
  restored_system.restore_state(first_system.serialize_state());
  restored_system.update(&world, 0.1F);

  EXPECT_TRUE(restored_system.is_zone_cleared(k_shrine_zone_id));
  EXPECT_EQ(count_owner_units(world, 99), 0)
      << "a restored broken zone must not raise a fresh garrison or shrine";
}

TEST_F(UndeadAwakeningSystemTest, EveryWaveAnnouncesItsNumberOutOfTheTotal) {
  Engine::Core::World world;
  Game::Systems::UndeadAwakeningSystem system(undead_services());

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

  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
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
  Game::Systems::UndeadAwakeningSystem system(undead_services());

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
  Game::Systems::UndeadAwakeningSystem system(undead_services());

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
  Game::Systems::UndeadAwakeningSystem system(undead_services());

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

  for (auto* entity : world.collect_entities_with<Engine::Core::UnitComponent>()) {
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
  Game::Systems::UndeadAwakeningSystem system(undead_services());

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
