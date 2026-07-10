#include <algorithm>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "core/component.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_transformer.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/command_service.h"
#include "game/systems/nation_id.h"
#include "game/systems/nation_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/wall_network_service.h"
#include "units/factory.h"
#include "units/spawn_type.h"

namespace {

constexpr float k_runtime_grid_center_offset = 0.5F;

auto runtime_world_from_grid(int coord, int grid_size) -> float {
  return static_cast<float>(coord) -
         (static_cast<float>(grid_size) * k_runtime_grid_center_offset -
          k_runtime_grid_center_offset);
}

class MapTransformerStructureTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().initialize_defaults();
    Game::Systems::OwnerRegistry::instance().clear();

    auto registry = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*registry);
    Game::Map::MapTransformer::setFactoryRegistry(std::move(registry));
    Game::Map::MapTransformer::set_local_owner_id(1);
    Game::Map::MapTransformer::clear_player_team_overrides();
  }

  void TearDown() override {
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    Game::Map::MapTransformer::clear_player_team_overrides();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::OwnerRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }
};

TEST_F(MapTransformerStructureTest, SpawnsAuthoredBuildingsAndRegistersOwners) {
  Engine::Core::World world;

  Game::Map::MapDefinition def;
  def.grid.width = 16;
  def.grid.height = 16;
  def.buildings.push_back({
      .type = QStringLiteral("defense_tower"),
      .x = runtime_world_from_grid(4, def.grid.width),
      .z = runtime_world_from_grid(6, def.grid.height),
      .player_id = 2,
      .nation = QStringLiteral("carthage"),
  });
  def.buildings.push_back({
      .type = QStringLiteral("home"),
      .x = runtime_world_from_grid(10, def.grid.width),
      .z = runtime_world_from_grid(12, def.grid.height),
      .player_id = 2,
      .nation = QStringLiteral("carthage"),
  });

  const auto runtime = Game::Map::MapTransformer::apply_to_world(def, world);
  EXPECT_TRUE(runtime.unit_ids.empty());

  int tower_count = 0;
  int home_count = 0;
  for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
    ASSERT_NE(entity, nullptr);
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    ASSERT_NE(unit, nullptr);
    ASSERT_NE(transform, nullptr);

    if (unit->spawn_type == Game::Units::SpawnType::DefenseTower) {
      ++tower_count;
      EXPECT_EQ(unit->owner_id, 2);
      EXPECT_EQ(unit->nation_id, Game::Systems::NationID::Carthage);
      EXPECT_FLOAT_EQ(transform->position.x,
                      runtime_world_from_grid(4, def.grid.width));
      EXPECT_FLOAT_EQ(transform->position.z,
                      runtime_world_from_grid(6, def.grid.height));
    }

    if (unit->spawn_type == Game::Units::SpawnType::Home) {
      ++home_count;
      EXPECT_EQ(unit->owner_id, 2);
      EXPECT_EQ(unit->nation_id, Game::Systems::NationID::Carthage);
      EXPECT_FLOAT_EQ(transform->position.x,
                      runtime_world_from_grid(10, def.grid.width));
      EXPECT_FLOAT_EQ(transform->position.z,
                      runtime_world_from_grid(12, def.grid.height));
    }
  }

  EXPECT_EQ(tower_count, 1);
  EXPECT_EQ(home_count, 1);
  EXPECT_EQ(Game::Systems::OwnerRegistry::instance().get_owner_type(2),
            Game::Systems::OwnerType::AI);
}

TEST_F(MapTransformerStructureTest,
       SpawnsWallLinesUsingMapGridEvenWhenCommandGridIsStale) {
  Engine::Core::World world;

  Game::Systems::CommandService::initialize(8, 8);

  Game::Map::MapDefinition def;
  def.grid.width = 16;
  def.grid.height = 16;
  def.wall_lines.push_back({
      .start = QVector3D(runtime_world_from_grid(4, def.grid.width),
                         0.0F,
                         runtime_world_from_grid(8, def.grid.height)),
      .end = QVector3D(runtime_world_from_grid(8, def.grid.width),
                       0.0F,
                       runtime_world_from_grid(8, def.grid.height)),
      .width = 2.0F,
      .player_id = 2,
      .nation = QStringLiteral("carthage"),
  });

  Game::Map::MapTransformer::apply_to_world(def, world);

  Game::Systems::CommandService::initialize(def.grid.width, def.grid.height);
  Game::Systems::WallNetworkService::refresh_world(world);

  struct WallSnapshot {
    float x = 0.0F;
    float z = 0.0F;
    std::uint8_t connection_mask = 0;
    int owner_id = 0;
    Game::Systems::NationID nation_id = Game::Systems::NationID::RomanRepublic;
  };

  std::vector<WallSnapshot> walls;
  for (auto* entity : world.get_entities_with<Engine::Core::WallSegmentComponent>()) {
    ASSERT_NE(entity, nullptr);
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    const auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    const auto* wall = entity->get_component<Engine::Core::WallSegmentComponent>();
    ASSERT_NE(unit, nullptr);
    ASSERT_NE(transform, nullptr);
    ASSERT_NE(wall, nullptr);
    walls.push_back({
        .x = transform->position.x,
        .z = transform->position.z,
        .connection_mask = wall->connection_mask,
        .owner_id = unit->owner_id,
        .nation_id = unit->nation_id,
    });
  }

  ASSERT_EQ(walls.size(), 3U);
  std::sort(
      walls.begin(), walls.end(), [](const WallSnapshot& lhs, const WallSnapshot& rhs) {
        return lhs.x < rhs.x;
      });

  EXPECT_FLOAT_EQ(walls[0].x, runtime_world_from_grid(4, def.grid.width));
  EXPECT_FLOAT_EQ(walls[1].x, runtime_world_from_grid(6, def.grid.width));
  EXPECT_FLOAT_EQ(walls[2].x, runtime_world_from_grid(8, def.grid.width));
  EXPECT_FLOAT_EQ(walls[0].z, runtime_world_from_grid(8, def.grid.height));
  EXPECT_FLOAT_EQ(walls[1].z, runtime_world_from_grid(8, def.grid.height));
  EXPECT_FLOAT_EQ(walls[2].z, runtime_world_from_grid(8, def.grid.height));

  for (const auto& wall : walls) {
    EXPECT_EQ(wall.owner_id, 2);
    EXPECT_EQ(wall.nation_id, Game::Systems::NationID::Carthage);
  }

  EXPECT_EQ(walls[0].connection_mask,
            Game::Systems::WallNetworkService::k_connection_east);
  EXPECT_EQ(walls[1].connection_mask,
            Game::Systems::WallNetworkService::k_connection_east |
                Game::Systems::WallNetworkService::k_connection_west);
  EXPECT_EQ(walls[2].connection_mask,
            Game::Systems::WallNetworkService::k_connection_west);
}

TEST_F(MapTransformerStructureTest, AuthoredGuardSpawnIsScenarioControlled) {
  Engine::Core::World world;

  Game::Map::MapDefinition def;
  def.grid.width = 16;
  def.grid.height = 16;
  def.spawns.push_back({
      .type = Game::Units::SpawnType::Spearman,
      .x = 4,
      .z = 6,
      .player_id = 2,
      .team_id = 1,
      .max_population = 100,
      .nation = Game::Systems::NationID::Carthage,
      .behavior = QStringLiteral("guard"),
      .guard_radius = 16.0F,
  });
  def.spawns.push_back({
      .type = Game::Units::SpawnType::Archer,
      .x = 8,
      .z = 6,
      .player_id = 2,
      .team_id = 1,
      .max_population = 100,
      .nation = Game::Systems::NationID::Carthage,
  });

  Game::Map::MapTransformer::apply_to_world(def, world);

  Engine::Core::Entity* guard_entity = nullptr;
  Engine::Core::Entity* strategic_entity = nullptr;
  for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
    ASSERT_NE(entity, nullptr);
    const auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    ASSERT_NE(unit, nullptr);
    if (unit->spawn_type == Game::Units::SpawnType::Spearman) {
      guard_entity = entity;
    } else if (unit->spawn_type == Game::Units::SpawnType::Archer) {
      strategic_entity = entity;
    }
  }

  ASSERT_NE(guard_entity, nullptr);
  EXPECT_EQ(guard_entity->get_component<Engine::Core::AIControlledComponent>(),
            nullptr);
  const auto* guard = guard_entity->get_component<Engine::Core::GuardModeComponent>();
  ASSERT_NE(guard, nullptr);
  EXPECT_TRUE(guard->active);
  EXPECT_TRUE(guard->has_guard_target);
  EXPECT_FLOAT_EQ(guard->guard_radius, 16.0F);
  EXPECT_FLOAT_EQ(guard->guard_position_x, runtime_world_from_grid(4, def.grid.width));
  EXPECT_FLOAT_EQ(guard->guard_position_z, runtime_world_from_grid(6, def.grid.height));

  ASSERT_NE(strategic_entity, nullptr);
  EXPECT_NE(strategic_entity->get_component<Engine::Core::AIControlledComponent>(),
            nullptr);
}

} // namespace
