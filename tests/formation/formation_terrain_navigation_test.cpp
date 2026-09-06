#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/component_core.h"
#include "core/world.h"
#include "formation/army_formation_registry.h"
#include "formation/army_formation_service.h"
#include "formation/unit_layout.h"
#include "formation/unit_layout_resolver.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "systems/building_collision_registry.h"
#include "systems/command_service.h"
#include "systems/nation_registry.h"
#include "systems/nav_grid.h"
#include "systems/pathfinding.h"
#include "systems/troop_profile_service.h"

namespace {

using Game::Formation::ArmyFormationIntent;
using Game::Formation::ArmyFormationRegistry;
using Game::Formation::ArmyFormationRequest;
using Game::Formation::ArmyFormationResult;
using Game::Formation::ArmyFormationService;
using Game::Formation::SlotStatus;
using Game::Systems::CommandService;
using Game::Systems::NationID;
using Game::Systems::NavGrid;
using Game::Systems::Point;

constexpr int k_grid = 61;

auto to_grid(float world_x, float world_z) -> Point {
  return NavGrid::world_to_grid(world_x, world_z);
}

auto add_unit(Engine::Core::World& world,
              Game::Units::SpawnType spawn_type,
              NationID nation,
              float x,
              float z) -> Engine::Core::EntityID {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  transform->position = {x, 0.0F, z};
  unit->spawn_type = spawn_type;
  unit->nation_id = nation;
  unit->health = 100;
  unit->max_health = 100;
  unit->speed = 2.0F;
  return entity->get_id();
}

auto squad(Engine::Core::World& world,
           NationID nation,
           float centre_x,
           float centre_z,
           int count = 6) -> std::vector<Engine::Core::EntityID> {
  std::vector<Engine::Core::EntityID> units;
  units.reserve(static_cast<std::size_t>(count));
  for (int i = 0; i < count; ++i) {
    units.push_back(add_unit(world,
                             Game::Units::SpawnType::Knight,
                             nation,
                             centre_x + static_cast<float>(i) - (count * 0.5F),
                             centre_z));
  }
  return units;
}

auto deploy(Engine::Core::World& world,
            const std::vector<Engine::Core::EntityID>& units,
            const QVector3D& anchor,
            ArmyFormationIntent intent = ArmyFormationIntent::Line)
    -> ArmyFormationResult {
  ArmyFormationRequest request;
  request.members = units;
  request.anchor = anchor;
  request.intent = intent;
  request.spacing = 1.5F;
  return ArmyFormationService::commit(world, request);
}

auto placed_positions(const ArmyFormationResult& result) -> std::vector<QVector3D> {
  std::vector<QVector3D> placed;
  for (std::size_t i = 0; i < result.positions.size(); ++i) {
    if (result.slot_status[i] != SlotStatus::Blocked) {
      placed.push_back(result.positions[i]);
    }
  }
  return placed;
}

void expect_no_shared_positions(const std::vector<QVector3D>& positions,
                                float minimum_gap) {
  for (std::size_t i = 0; i < positions.size(); ++i) {
    for (std::size_t j = i + 1; j < positions.size(); ++j) {
      float const dx = positions[i].x() - positions[j].x();
      float const dz = positions[i].z() - positions[j].z();
      EXPECT_GT(std::sqrt(dx * dx + dz * dz), minimum_gap)
          << "slots " << i << " and " << j << " share ground";
    }
  }
}

class FormationTerrainTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    ArmyFormationRegistry::instance().clear();

    auto& nations = Game::Systems::NationRegistry::instance();
    nations.clear();
    nations.register_nation({.id = NationID::RomanRepublic,
                             .display_name = "Roman Republic",
                             .doctrine = "rome"});
    nations.register_nation(
        {.id = NationID::Carthage, .display_name = "Carthage", .doctrine = "carthage"});
    Game::Systems::TroopProfileService::instance().clear();
  }

  void TearDown() override {
    ArmyFormationRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::TroopProfileService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  static auto base_map() -> Game::Map::MapDefinition {
    Game::Map::MapDefinition map_def;
    map_def.grid.width = k_grid;
    map_def.grid.height = k_grid;
    map_def.grid.tile_size = 1.0F;
    map_def.coordSystem = Game::Map::CoordSystem::World;
    return map_def;
  }

  static void activate(const Game::Map::MapDefinition& map_def) {
    Game::Map::TerrainService::instance().initialize(map_def);
    NavGrid::initialize(map_def.grid.width, map_def.grid.height);
    auto* pathfinder = NavGrid::get_pathfinder();
    ASSERT_NE(pathfinder, nullptr);
    pathfinder->update_navigation_grid();
  }

  static auto pathfinder() -> Game::Systems::Pathfinding* {
    return NavGrid::get_pathfinder();
  }
};

} // namespace

TEST_F(FormationTerrainTest, EveryPlacedSlotLandsOnWalkableGround) {
  auto map_def = base_map();
  map_def.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -28.0F), QVector3D(0.0F, 0.0F, 28.0F), 6.0F});
  map_def.bridges.push_back(
      {QVector3D(-5.0F, 0.0F, 0.0F), QVector3D(5.0F, 0.0F, 0.0F), 8.0F, 0.6F});
  activate(map_def);

  Engine::Core::World world;
  auto const units = squad(world, NationID::RomanRepublic, -14.0F, 0.0F, 8);

  auto const anchor = QVector3D(0.0F, 0.0F, 14.0F);
  ASSERT_FALSE(NavGrid::is_world_position_walkable(anchor))
      << "the fixture must anchor the deployment on unwalkable ground";

  auto const result = deploy(world, units, anchor);
  ASSERT_TRUE(result.valid) << result.rejection_reason;

  auto const placed = placed_positions(result);
  ASSERT_FALSE(placed.empty());
  for (const auto& position : placed) {
    EXPECT_TRUE(NavGrid::is_world_position_walkable(position))
        << position.x() << ", " << position.z();
  }
}

TEST_F(FormationTerrainTest, NoSlotIsPlacedInOpenWater) {
  auto map_def = base_map();
  map_def.rivers.push_back(
      {QVector3D(-28.0F, 0.0F, 0.0F), QVector3D(28.0F, 0.0F, 0.0F), 8.0F});
  map_def.bridges.push_back(
      {QVector3D(0.0F, 0.0F, -6.0F), QVector3D(0.0F, 0.0F, 6.0F), 8.0F, 0.6F});
  activate(map_def);

  Engine::Core::World world;
  auto const units = squad(world, NationID::RomanRepublic, 0.0F, -18.0F, 8);

  auto const result = deploy(world, units, QVector3D(0.0F, 0.0F, 0.0F));

  for (const auto& position : placed_positions(result)) {
    auto const cell = to_grid(position.x(), position.z());
    EXPECT_TRUE(pathfinder()->is_walkable(cell.x, cell.y))
        << "slot dropped into the river at " << position.x() << ", " << position.z();
  }
}

TEST_F(FormationTerrainTest, CrossingARiverRoutesThroughTheBridgeDeck) {
  auto map_def = base_map();
  map_def.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -28.0F), QVector3D(0.0F, 0.0F, 28.0F), 6.0F});
  map_def.bridges.push_back(
      {QVector3D(-5.0F, 0.0F, 0.0F), QVector3D(5.0F, 0.0F, 0.0F), 8.0F, 0.6F});
  activate(map_def);

  auto const start = to_grid(-16.0F, 0.0F);
  auto const goal = to_grid(16.0F, 0.0F);
  auto const path = pathfinder()->find_path(start, goal);
  ASSERT_FALSE(path.empty());

  auto const deck = to_grid(0.0F, 0.0F);
  bool crossed_on_deck = false;
  for (const auto& point : path) {
    if (point.x == deck.x) {
      crossed_on_deck = std::abs(point.y - deck.y) <= 4;
      if (crossed_on_deck) {
        break;
      }
    }
  }
  EXPECT_TRUE(crossed_on_deck) << "path crossed the river away from the bridge";
}

TEST_F(FormationTerrainTest, SlotsNeverLandInsideARegisteredBuildingFootprint) {
  auto map_def = base_map();
  activate(map_def);

  auto& buildings = Game::Systems::BuildingCollisionRegistry::instance();
  buildings.register_building(9001U, "barracks", 0.0F, 0.0F, 1);
  buildings.register_building(9002U, "barracks", 8.0F, 0.0F, 1);
  pathfinder()->update_navigation_grid();

  Engine::Core::World world;
  auto const units = squad(world, NationID::RomanRepublic, -18.0F, -18.0F, 8);

  auto const result = deploy(world, units, QVector3D(2.0F, 0.0F, 0.0F));

  for (const auto& position : placed_positions(result)) {
    auto const cell = to_grid(position.x(), position.z());
    EXPECT_TRUE(pathfinder()->is_walkable(cell.x, cell.y))
        << "slot placed inside a building at " << position.x() << ", " << position.z();
  }
}

TEST_F(FormationTerrainTest, SlotsAvoidWallSegmentsAndStayOnOneSide) {
  auto map_def = base_map();
  activate(map_def);

  for (int z = -10; z <= 10; ++z) {
    auto const cell = to_grid(0.0F, static_cast<float>(z));
    pathfinder()->set_obstacle(cell.x, cell.y, true);
  }

  Engine::Core::World world;
  auto const units = squad(world, NationID::RomanRepublic, -12.0F, 0.0F, 8);

  auto const result = deploy(world, units, QVector3D(-2.0F, 0.0F, 0.0F));

  auto const wall_column = to_grid(0.0F, 0.0F).x;
  for (const auto& position : placed_positions(result)) {
    auto const cell = to_grid(position.x(), position.z());
    EXPECT_NE(cell.x, wall_column)
        << "slot placed on top of the wall line at " << position.x();
    EXPECT_TRUE(pathfinder()->is_walkable(cell.x, cell.y));
  }
}

TEST_F(FormationTerrainTest, HillFlanksDoNotSwallowFormationSlots) {
  auto map_def = base_map();
  Game::Map::TerrainFeature hill;
  hill.type = Game::Map::TerrainType::Hill;
  hill.center_x = 0.0F;
  hill.center_z = 0.0F;
  hill.width = 18.0F;
  hill.depth = 18.0F;
  hill.height = 5.0F;
  hill.entrances.emplace_back(-9.0F, 0.0F, 0.0F);
  map_def.terrain.push_back(hill);
  activate(map_def);

  Engine::Core::World world;
  auto const units = squad(world, NationID::RomanRepublic, -20.0F, 0.0F, 8);

  auto const result = deploy(world, units, QVector3D(0.0F, 0.0F, 0.0F));

  for (const auto& position : placed_positions(result)) {
    auto const cell = to_grid(position.x(), position.z());
    EXPECT_TRUE(pathfinder()->is_walkable(cell.x, cell.y))
        << "slot placed on an impassable hill flank";
  }
  expect_no_shared_positions(placed_positions(result), 0.4F);
}

TEST_F(FormationTerrainTest, ObstructedGroundNeverStacksTwoUnitsOnOneTile) {
  auto map_def = base_map();
  activate(map_def);

  auto const centre = to_grid(0.0F, 0.0F);
  for (int dz = -3; dz <= 3; ++dz) {
    for (int dx = -3; dx <= 3; ++dx) {
      if (dx == 0 && dz == 0) {
        continue;
      }
      pathfinder()->set_obstacle(centre.x + dx, centre.y + dz, true);
    }
  }

  Engine::Core::World world;
  auto const units = squad(world, NationID::RomanRepublic, -20.0F, -20.0F, 10);

  auto const result = deploy(world, units, QVector3D(0.0F, 0.0F, 0.0F));
  expect_no_shared_positions(placed_positions(result), 0.4F);
}

TEST_F(FormationTerrainTest, TwoFormationsDeployedSideBySideDoNotOverlap) {
  auto map_def = base_map();
  activate(map_def);

  Engine::Core::World world;
  auto const left = squad(world, NationID::RomanRepublic, -20.0F, -12.0F, 6);
  auto const right = squad(world, NationID::Carthage, 20.0F, -12.0F, 6);

  auto const left_result = deploy(world, left, QVector3D(-9.0F, 0.0F, 6.0F));
  auto const right_result = deploy(world, right, QVector3D(9.0F, 0.0F, 6.0F));
  ASSERT_TRUE(left_result.valid);
  ASSERT_TRUE(right_result.valid);

  auto combined = placed_positions(left_result);
  auto const right_positions = placed_positions(right_result);
  combined.insert(combined.end(), right_positions.begin(), right_positions.end());
  expect_no_shared_positions(combined, 0.4F);
}

TEST_F(FormationTerrainTest, EveryPlacedSlotIsReachableFromTheStartingGround) {
  auto map_def = base_map();
  map_def.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -28.0F), QVector3D(0.0F, 0.0F, 28.0F), 6.0F});
  map_def.bridges.push_back(
      {QVector3D(-5.0F, 0.0F, 0.0F), QVector3D(5.0F, 0.0F, 0.0F), 8.0F, 0.6F});
  activate(map_def);

  Engine::Core::World world;
  auto const units = squad(world, NationID::RomanRepublic, -16.0F, 0.0F, 8);

  auto const result = deploy(world, units, QVector3D(16.0F, 0.0F, 0.0F));
  ASSERT_TRUE(result.valid) << result.rejection_reason;

  auto const start = to_grid(-16.0F, 0.0F);
  for (const auto& position : placed_positions(result)) {
    auto const goal = to_grid(position.x(), position.z());
    auto const path = pathfinder()->find_path(start, goal);
    ASSERT_FALSE(path.empty())
        << "no route to slot at " << position.x() << ", " << position.z();
    auto const& arrival = path.back();
    EXPECT_LE(std::abs(arrival.x - goal.x) + std::abs(arrival.y - goal.y), 2)
        << "route stops short of the slot";
  }
}

TEST_F(FormationTerrainTest, SoldierOffsetsStayInsideTheUnitFootprint) {
  auto map_def = base_map();
  activate(map_def);

  using Game::Formation::UnitLayoutLibrary;
  using Game::Formation::UnitLayoutSystem;

  for (const auto* doctrine : {"rome", "carthage", "iron_sepulcher"}) {
    for (const auto* style :
         {"close_order_infantry", "spear_ranks", "loose_order_ranged"}) {
      auto const id = UnitLayoutLibrary::instance().resolve(doctrine, style);
      auto const offsets = UnitLayoutSystem::instance().compute(id, 24, 6, 1.0F, 7U);

      float radius = 0.0F;
      for (const auto& offset : offsets) {
        radius = std::max(radius, std::hypot(offset.offset_x, offset.offset_z));
      }
      EXPECT_LT(radius, 12.0F) << doctrine << "/" << style;
    }
  }
}

TEST_F(FormationTerrainTest, DeploymentAcrossABridgeKeepsSlotsOffTheRiverbank) {
  auto map_def = base_map();
  map_def.rivers.push_back(
      {QVector3D(-28.0F, 0.0F, 0.0F), QVector3D(28.0F, 0.0F, 0.0F), 6.0F});
  map_def.bridges.push_back(
      {QVector3D(0.0F, 0.0F, -5.0F), QVector3D(0.0F, 0.0F, 5.0F), 8.0F, 0.6F});
  activate(map_def);

  Engine::Core::World world;
  auto const units = squad(world, NationID::RomanRepublic, 0.0F, -16.0F, 8);

  auto const result =
      deploy(world, units, QVector3D(0.0F, 0.0F, 12.0F), ArmyFormationIntent::Column);
  ASSERT_TRUE(result.valid) << result.rejection_reason;

  for (const auto& position : placed_positions(result)) {
    EXPECT_TRUE(NavGrid::is_world_position_walkable(position));
  }
  expect_no_shared_positions(placed_positions(result), 0.4F);
}
