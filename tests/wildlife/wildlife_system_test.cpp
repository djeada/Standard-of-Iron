#include <QJsonObject>
#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <vector>

#include "core/component.h"
#include "core/entity.h"
#include "core/ownership_constants.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/wildlife/bird_flock.h"
#include "game/wildlife/wildlife_config.h"
#include "game/wildlife/wildlife_system.h"

namespace {

using Engine::Core::Entity;
using Engine::Core::World;
using Game::Wildlife::Behavior;
using Game::Wildlife::Species;
using Game::Wildlife::WildlifeSettings;
using Game::Wildlife::WildlifeSystem;

auto make_map(int size = 64) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map_definition;
  map_definition.grid.width = size;
  map_definition.grid.height = size;
  map_definition.grid.tile_size = 1.0F;
  return map_definition;
}

auto make_settings() -> WildlifeSettings {
  WildlifeSettings settings = Game::Wildlife::default_settings();
  settings.enabled = true;
  settings.seed = 4242U;

  settings.sheep.enabled = true;
  settings.sheep.group_count = 1;
  settings.sheep.group_size_min = 4;
  settings.sheep.group_size_max = 4;
  settings.sheep.roam_radius = 8.0F;
  settings.sheep.alert_radius = 9.0F;
  settings.sheep.spawn_areas = {{0.0F, 0.0F, 2.0F}};

  settings.wolves.enabled = false;
  settings.wolves.group_count = 0;
  settings.wolves.spawn_areas = {{18.0F, 0.0F, 2.0F}};

  settings.birds.enabled = false;
  settings.birds.group_count = 0;

  return settings;
}

auto count_species(World& world, Species species) -> int {
  int count = 0;
  for (auto* entity : world.get_entities_with<Engine::Core::WildlifeComponent>()) {
    const auto* wildlife = entity->get_component<Engine::Core::WildlifeComponent>();
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (wildlife != nullptr && unit != nullptr && wildlife->species == species &&
        unit->health > 0) {
      ++count;
    }
  }
  return count;
}

auto collect_species(World& world, Species species) -> std::vector<Entity*> {
  std::vector<Entity*> result;
  for (auto* entity : world.get_entities_with<Engine::Core::WildlifeComponent>()) {
    const auto* wildlife = entity->get_component<Engine::Core::WildlifeComponent>();
    if (wildlife != nullptr && wildlife->species == species) {
      result.push_back(entity);
    }
  }
  return result;
}

auto add_troop(World& world,
               const QVector3D& position,
               Game::Units::SpawnType type = Game::Units::SpawnType::Knight)
    -> Entity* {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  transform->position = {position.x(), position.y(), position.z()};
  unit->owner_id = 1;
  unit->spawn_type = type;
  unit->health = 100;
  unit->max_health = 100;
  return entity;
}

void advance(WildlifeSystem& system, World& world, float seconds, float step = 0.1F) {
  for (float elapsed = 0.0F; elapsed < seconds; elapsed += step) {
    system.update(&world, step);
  }
}

class WildlifeSystemTest : public ::testing::Test {
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

    Game::Systems::BuildingCollisionRegistry::instance().clear();

    const auto map_definition = make_map();
    Game::Map::TerrainService::instance().initialize(map_definition);
    Game::Systems::CommandService::initialize(map_definition.grid.width,
                                              map_definition.grid.height);
  }

  void TearDown() override {
    Game::Wildlife::BirdFlockManager::instance().reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
  }
};

TEST_F(WildlifeSystemTest, SpawnsConfiguredHerdAsNeutralEntities) {
  World world;
  WildlifeSystem system;
  system.configure(make_settings(), 1U);

  system.update(&world, 0.1F);

  EXPECT_EQ(count_species(world, Species::Sheep), 4);
  ASSERT_EQ(system.groups().size(), 1U);
  EXPECT_EQ(system.groups().front().species, Species::Sheep);

  for (auto* entity : collect_species(world, Species::Sheep)) {
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* wildlife = entity->get_component<Engine::Core::WildlifeComponent>();
    ASSERT_NE(unit, nullptr);
    ASSERT_NE(wildlife, nullptr);
    EXPECT_EQ(unit->owner_id, Game::Core::NEUTRAL_OWNER_ID);
    EXPECT_EQ(unit->spawn_type, Game::Units::SpawnType::Sheep);
    EXPECT_EQ(wildlife->group_id, system.groups().front().id);
    EXPECT_TRUE(wildlife->anchor_assigned);
  }
}

TEST_F(WildlifeSystemTest, DisabledSettingsSpawnNothing) {
  World world;
  WildlifeSystem system;
  WildlifeSettings settings = make_settings();
  settings.enabled = false;
  system.configure(settings, 1U);

  advance(system, world, 1.0F);

  EXPECT_EQ(count_species(world, Species::Sheep), 0);
  EXPECT_FALSE(system.is_enabled());
}

TEST_F(WildlifeSystemTest, HerdFleesFromApproachingTroops) {
  World world;
  WildlifeSystem system;
  system.configure(make_settings(), 1U);
  system.update(&world, 0.1F);

  add_troop(world, QVector3D(3.0F, 0.0F, 0.0F));
  advance(system, world, 1.5F);

  int fleeing = 0;
  for (auto* entity : collect_species(world, Species::Sheep)) {
    const auto* wildlife = entity->get_component<Engine::Core::WildlifeComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    ASSERT_NE(wildlife, nullptr);
    ASSERT_NE(transform, nullptr);
    if (wildlife->behavior != Behavior::Flee) {
      continue;
    }
    ++fleeing;

    float const current =
        std::hypot(transform->position.x - 3.0F, transform->position.z - 0.0F);
    float const target = std::hypot(wildlife->target_x - 3.0F, wildlife->target_z);
    EXPECT_GT(target, current);
  }
  EXPECT_GT(fleeing, 0);
  EXPECT_GT(system.stats().flee_events, 0U);
}

TEST_F(WildlifeSystemTest, CiviliansDoNotStampedeTheHerd) {
  World world;
  WildlifeSystem system;
  system.configure(make_settings(), 1U);
  system.update(&world, 0.1F);

  add_troop(world, QVector3D(3.0F, 0.0F, 0.0F), Game::Units::SpawnType::Civilian);
  advance(system, world, 1.5F);

  for (auto* entity : collect_species(world, Species::Sheep)) {
    const auto* wildlife = entity->get_component<Engine::Core::WildlifeComponent>();
    ASSERT_NE(wildlife, nullptr);
    EXPECT_NE(wildlife->behavior, Behavior::Flee);
  }
}

TEST_F(WildlifeSystemTest, WolvesHuntNearbySheep) {
  World world;
  WildlifeSystem system;
  WildlifeSettings settings = make_settings();
  settings.wolves.enabled = true;
  settings.wolves.group_count = 1;
  settings.wolves.group_size_min = 1;
  settings.wolves.group_size_max = 1;
  settings.wolves.aggression = 1.0F;
  settings.wolves.roam_radius = 12.0F;
  settings.wolves.spawn_areas = {{4.0F, 0.0F, 1.0F}};
  system.configure(settings, 1U);
  system.update(&world, 0.1F);

  ASSERT_EQ(count_species(world, Species::Wolf), 1);
  const auto sheep_before = count_species(world, Species::Sheep);
  ASSERT_GT(sheep_before, 0);

  advance(system, world, 2.0F);

  auto wolves = collect_species(world, Species::Wolf);
  ASSERT_EQ(wolves.size(), 1U);
  const auto* wolf = wolves.front()->get_component<Engine::Core::WildlifeComponent>();
  ASSERT_NE(wolf, nullptr);
  EXPECT_EQ(wolf->behavior, Behavior::Stalk);
  EXPECT_GT(system.stats().hunt_events, 0U);
}

TEST_F(WildlifeSystemTest, WolvesBackOffFromLargeFormations) {
  World world;
  WildlifeSystem system;
  WildlifeSettings settings = make_settings();
  settings.wolves.enabled = true;
  settings.wolves.group_count = 1;
  settings.wolves.group_size_min = 1;
  settings.wolves.group_size_max = 1;
  settings.wolves.aggression = 1.0F;
  settings.wolves.alert_radius = 14.0F;
  settings.wolves.spawn_areas = {{4.0F, 0.0F, 1.0F}};
  system.configure(settings, 1U);
  system.update(&world, 0.1F);

  for (int index = 0; index < 8; ++index) {
    add_troop(world, QVector3D(5.0F + static_cast<float>(index) * 0.5F, 0.0F, 2.0F));
  }
  advance(system, world, 2.0F);

  auto wolves = collect_species(world, Species::Wolf);
  ASSERT_EQ(wolves.size(), 1U);
  const auto* wolf = wolves.front()->get_component<Engine::Core::WildlifeComponent>();
  ASSERT_NE(wolf, nullptr);
  EXPECT_EQ(wolf->behavior, Behavior::Flee);
}

TEST_F(WildlifeSystemTest, LostHerdMembersRespawnAfterTheConfiguredDelay) {
  World world;
  WildlifeSystem system;
  WildlifeSettings settings = make_settings();
  settings.sheep.respawn = true;
  settings.sheep.respawn_delay = 2.0F;
  system.configure(settings, 1U);
  system.update(&world, 0.1F);
  ASSERT_EQ(count_species(world, Species::Sheep), 4);

  auto sheep = collect_species(world, Species::Sheep);
  ASSERT_FALSE(sheep.empty());
  sheep.front()->get_component<Engine::Core::UnitComponent>()->health = 0;
  world.destroy_entity(sheep.front()->get_id());

  ASSERT_EQ(count_species(world, Species::Sheep), 3);
  advance(system, world, 1.0F);
  EXPECT_EQ(count_species(world, Species::Sheep), 3);

  advance(system, world, 2.0F);
  EXPECT_EQ(count_species(world, Species::Sheep), 4);
  EXPECT_GT(system.stats().respawns, 0U);
}

TEST_F(WildlifeSystemTest, RespawnCanBeDisabled) {
  World world;
  WildlifeSystem system;
  WildlifeSettings settings = make_settings();
  settings.sheep.respawn = false;
  system.configure(settings, 1U);
  system.update(&world, 0.1F);

  auto sheep = collect_species(world, Species::Sheep);
  ASSERT_FALSE(sheep.empty());
  world.destroy_entity(sheep.front()->get_id());

  advance(system, world, 5.0F);
  EXPECT_EQ(count_species(world, Species::Sheep), 3);
}

TEST_F(WildlifeSystemTest, DistantAnimalsStopThinking) {
  World world;
  WildlifeSystem system;
  WildlifeSettings settings = make_settings();
  settings.near_simulation_radius = 12.0F;
  settings.far_simulation_radius = 24.0F;
  system.configure(settings, 1U);
  system.update(&world, 0.1F);

  system.set_focus(400.0F, 400.0F);
  advance(system, world, 2.0F);

  EXPECT_GT(system.stats().dormant_skips, 0U);
  EXPECT_EQ(system.stats().near_thinks, 0U);

  system.set_focus(0.0F, 0.0F);
  advance(system, world, 2.0F);
  EXPECT_GT(system.stats().near_thinks, 0U);
}

TEST_F(WildlifeSystemTest, MoveTargetsStayOnWalkableGround) {
  Game::Map::MapDefinition map_definition = make_map();
  Game::Map::Lake lake;
  lake.center = QVector3D(6.0F, 0.0F, 0.0F);
  lake.width = 14.0F;
  lake.depth = 14.0F;
  map_definition.lakes.push_back(lake);
  Game::Map::TerrainService::instance().initialize(map_definition);

  World world;
  WildlifeSystem system;
  WildlifeSettings settings = make_settings();
  settings.sheep.roam_radius = 12.0F;
  system.configure(settings, 1U);
  advance(system, world, 3.0F);

  const auto& terrain = Game::Map::TerrainService::instance();
  ASSERT_TRUE(terrain.is_forbidden_world(lake.center.x(), lake.center.z()));
  for (auto* entity : collect_species(world, Species::Sheep)) {
    const auto* wildlife = entity->get_component<Engine::Core::WildlifeComponent>();
    ASSERT_NE(wildlife, nullptr);
    EXPECT_FALSE(terrain.is_forbidden_world(wildlife->target_x, wildlife->target_z));
  }
}

TEST_F(WildlifeSystemTest, SerializedStateRestoresGroupsAndFlocks) {
  World world;
  WildlifeSystem system;
  WildlifeSettings settings = make_settings();
  settings.birds.enabled = true;
  settings.birds.group_count = 1;
  settings.birds.group_size_min = 3;
  settings.birds.group_size_max = 3;
  settings.birds.spawn_areas = {{0.0F, 0.0F, 3.0F}};
  system.configure(settings, 1U);
  advance(system, world, 2.0F);

  const QJsonObject state = system.serialize_state();
  ASSERT_TRUE(state.contains("groups"));
  ASSERT_TRUE(state.contains("birds"));

  const auto expected_groups = system.groups();
  const auto expected_birds = Game::Wildlife::BirdFlockManager::instance().birds();

  WildlifeSystem restored;
  restored.configure(settings, 1U);
  restored.restore_state(state);

  ASSERT_EQ(restored.groups().size(), expected_groups.size());
  for (std::size_t index = 0; index < expected_groups.size(); ++index) {
    EXPECT_EQ(restored.groups()[index].id, expected_groups[index].id);
    EXPECT_EQ(restored.groups()[index].species, expected_groups[index].species);
    EXPECT_EQ(restored.groups()[index].desired_size,
              expected_groups[index].desired_size);
    EXPECT_FLOAT_EQ(restored.groups()[index].home_x, expected_groups[index].home_x);
  }

  const auto& restored_birds = Game::Wildlife::BirdFlockManager::instance().birds();
  ASSERT_EQ(restored_birds.size(), expected_birds.size());
  for (std::size_t index = 0; index < restored_birds.size(); ++index) {
    EXPECT_FLOAT_EQ(restored_birds[index].x, expected_birds[index].x);
    EXPECT_FLOAT_EQ(restored_birds[index].z, expected_birds[index].z);
  }
}

TEST_F(WildlifeSystemTest, RestoredSystemDoesNotDuplicateThePopulation) {
  World world;
  WildlifeSystem system;
  system.configure(make_settings(), 1U);
  advance(system, world, 1.0F);
  const QJsonObject state = system.serialize_state();

  WildlifeSystem restored;
  restored.configure(make_settings(), 1U);
  restored.restore_state(state);
  advance(restored, world, 1.0F);

  EXPECT_EQ(count_species(world, Species::Sheep), 4);
}

TEST_F(WildlifeSystemTest, AMapThatNeverAuthoredWildlifeStillGetsAPopulation) {
  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QStringLiteral("assets/maps/map_rivers.json"), map_definition, &error))
      << error.toStdString();
  ASSERT_TRUE(map_definition.wildlife.enabled);

  Game::Map::TerrainService::instance().initialize(map_definition);
  Game::Systems::CommandService::initialize(map_definition.grid.width,
                                            map_definition.grid.height);

  World world;
  WildlifeSystem system;
  system.configure(map_definition);
  advance(system, world, 1.0F);

  EXPECT_GT(count_species(world, Species::Sheep), 0)
      << "derived spawn areas landed on ground the spawner refuses";
  EXPECT_GT(count_species(world, Species::Wolf), 0);

  const auto& terrain = Game::Map::TerrainService::instance();
  for (auto* entity : world.get_entities_with<Engine::Core::WildlifeComponent>()) {
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    ASSERT_NE(transform, nullptr);
    EXPECT_FALSE(
        terrain.is_forbidden_world(transform->position.x, transform->position.z));
  }
}

TEST_F(WildlifeSystemTest, ShippedForestMapPopulatesEverySpeciesItEnables) {
  Game::Map::MapDefinition map_definition;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QStringLiteral("assets/maps/map_forest.json"), map_definition, &error))
      << error.toStdString();
  ASSERT_TRUE(map_definition.wildlife.enabled);

  Game::Map::TerrainService::instance().initialize(map_definition);
  Game::Systems::CommandService::initialize(map_definition.grid.width,
                                            map_definition.grid.height);

  World world;
  WildlifeSystem system;
  system.configure(map_definition);
  advance(system, world, 1.0F);

  EXPECT_GT(count_species(world, Species::Sheep), 0);
  EXPECT_GT(count_species(world, Species::Wolf), 0);

  bool birds_seen = false;
  for (int slice = 0; slice < 60 && !birds_seen; ++slice) {
    advance(system, world, 2.0F);
    birds_seen = !Game::Wildlife::BirdFlockManager::instance().birds().empty();
  }
  EXPECT_TRUE(birds_seen) << "no flyover crossed the map in two minutes";

  const auto& terrain = Game::Map::TerrainService::instance();
  for (auto* entity : world.get_entities_with<Engine::Core::WildlifeComponent>()) {
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    ASSERT_NE(transform, nullptr);
    EXPECT_FALSE(
        terrain.is_forbidden_world(transform->position.x, transform->position.z));
  }
}

} // namespace
