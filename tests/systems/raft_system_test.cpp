#include <QDir>
#include <QFile>
#include <QJsonObject>
#include <QTemporaryDir>

#include <cmath>
#include <cstdint>
#include <gtest/gtest.h>
#include <vector>

#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/event_manager.h"
#include "core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/terrain_service.h"
#include "game/systems/navigation/pathfinding.h"
#include "game/systems/raft_system.h"
#include "game/systems/unit_activity.h"
#include "units/spawn_type.h"

namespace {

using Game::Systems::RaftSystem;

constexpr float k_tick = 0.05F;

auto make_river_map(float river_width) -> Game::Map::MapDefinition {
  Game::Map::MapDefinition map;
  map.coordSystem = Game::Map::CoordSystem::World;
  map.grid.width = 80;
  map.grid.height = 80;
  map.grid.tile_size = 1.0F;
  map.rivers.push_back(
      {QVector3D(0.0F, 0.0F, -38.0F), QVector3D(0.0F, 0.0F, 38.0F), river_width});
  return map;
}

auto raft_at(const QString& id, float x, float z) -> Game::Map::RaftCrossing {
  Game::Map::RaftCrossing raft;
  raft.id = id;
  raft.x = x;
  raft.z = z;
  return raft;
}

auto services() -> RaftSystem::Services {
  return {.terrain = Game::Map::TerrainService::instance()};
}

auto add_troop(Engine::Core::World& world,
               const QVector3D& position,
               Game::Units::SpawnType type = Game::Units::SpawnType::Spearman)
    -> Engine::Core::Entity* {
  auto* entity = world.create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  auto* unit = entity->add_component<Engine::Core::UnitComponent>();
  entity->add_component<Engine::Core::MovementComponent>();
  transform->position = {position.x(), position.y(), position.z()};
  unit->owner_id = 1;
  unit->spawn_type = type;
  unit->health = 1000;
  unit->max_health = 1000;
  return entity;
}

auto position_of(Engine::Core::Entity* entity) -> QVector3D {
  auto const& p = entity->get_component<Engine::Core::TransformComponent>()->position;
  return {p.x, 0.0F, p.z};
}

auto riders(Engine::Core::World& world) -> int {
  int count = 0;
  world.each<Engine::Core::RaftRiderComponent>(
      [&count](Engine::Core::EntityID, Engine::Core::RaftRiderComponent&) { ++count; });
  return count;
}

auto bank_of(const RaftSystem::RaftView& raft, const QVector3D& at) -> int {
  QVector3D const centre = (raft.docks[0] + raft.docks[1]) * 0.5F;
  return QVector3D::dotProduct(at - centre, raft.across) > 0.0F ? 1 : 0;
}

class RaftSystemTest : public ::testing::Test {
protected:
  void SetUp() override { Game::Map::TerrainService::instance().clear(); }
  void TearDown() override { Game::Map::TerrainService::instance().clear(); }

  static void configure(RaftSystem& system, const Game::Map::MapDefinition& map) {
    Game::Map::TerrainService::instance().initialize(map);
    system.configure(map);
  }
};

} // namespace

TEST_F(RaftSystemTest, RaftsOnlyFloatOnRiversWideEnoughToCarryThem) {
  auto wide = make_river_map(9.0F);
  wide.rafts = {raft_at(QStringLiteral("ford"), 0.0F, 0.0F),
                raft_at(QStringLiteral("meadow"), 20.0F, 0.0F)};
  RaftSystem system(services());
  configure(system, wide);
  ASSERT_EQ(system.raft_count(), 1U) << "a raft authored on dry land is dropped";

  auto const raft = system.raft(0);
  EXPECT_EQ(raft.id, QStringLiteral("ford"));
  auto const& terrain = Game::Map::TerrainService::instance();
  for (int side = 0; side < 2; ++side) {
    auto const& landing = raft.landings[side];
    EXPECT_FALSE(terrain.is_forbidden_world(landing.x(), landing.z()))
        << "landing " << side << " has to be dry, walkable ground";
    EXPECT_GT(std::abs(landing.x()), 9.0F * 0.5F)
        << "landing " << side << " sits out of the water";
    EXPECT_LT(std::abs(raft.docks[side].x()), std::abs(landing.x()))
        << "the raft docks in the river, between its landings";
  }
  EXPECT_NE(bank_of(raft, raft.landings[0]), bank_of(raft, raft.landings[1]))
      << "the two landings face each other across the river";

  auto narrow = make_river_map(Game::Systems::k_raft_min_river_width - 1.5F);
  narrow.rafts = {raft_at(QStringLiteral("brook"), 0.0F, 0.0F)};
  RaftSystem brook(services());
  configure(brook, narrow);
  EXPECT_EQ(brook.raft_count(), 0U)
      << "a stream narrower than the minimum takes no raft";
  EXPECT_FALSE(
      RaftSystem::river_admits_raft(Game::Systems::k_raft_min_river_width - 0.1F));
  EXPECT_TRUE(RaftSystem::river_admits_raft(Game::Systems::k_raft_min_river_width));
}

TEST_F(RaftSystemTest, FerriesOneUnitAtATimeToTheFarBank) {
  auto map = make_river_map(9.0F);
  map.rafts = {raft_at(QStringLiteral("ford"), 0.0F, 0.0F)};
  Engine::Core::World world;
  RaftSystem system(services());
  configure(system, map);
  ASSERT_EQ(system.raft_count(), 1U);
  auto const start = system.raft(0);

  auto* first = add_troop(world, start.landings[0]);
  auto* second = add_troop(world, start.landings[0] + QVector3D(0.0F, 0.0F, 2.0F));
  auto* far = add_troop(world, start.landings[0] + QVector3D(0.0F, 0.0F, 40.0F));
  ASSERT_TRUE(system.raft_in_reach(world, first->get_id()).has_value());
  EXPECT_FALSE(system.raft_in_reach(world, far->get_id()).has_value())
      << "only troops near a landing can call the raft";

  ASSERT_TRUE(system.order_crossing(world, first->get_id()));
  ASSERT_TRUE(system.order_crossing(world, second->get_id()));
  EXPECT_FALSE(system.order_crossing(world, first->get_id()))
      << "a unit already in line cannot queue twice";

  int most_riders = 0;
  bool first_landed = false;
  bool second_landed = false;
  bool second_waited_while_first_afloat = false;
  for (float t = 0.0F; t < 60.0F && !(first_landed && second_landed); t += k_tick) {
    system.update(&world, k_tick);
    most_riders = std::max(most_riders, riders(world));
    auto const view = system.raft(0);
    if (view.passenger == first->get_id() && !view.queue.empty() &&
        view.queue.front() == second->get_id()) {
      second_waited_while_first_afloat = true;
    }
    first_landed =
        first_landed ||
        (view.passenger != first->get_id() && bank_of(view, position_of(first)) == 1 &&
         !world.has<Engine::Core::RaftRiderComponent>(first->get_id()));
    second_landed = second_landed ||
                    (view.passenger != second->get_id() &&
                     bank_of(view, position_of(second)) == 1 &&
                     !world.has<Engine::Core::RaftRiderComponent>(second->get_id()));
  }

  EXPECT_EQ(most_riders, 1) << "the raft never carries more than one unit";
  EXPECT_TRUE(second_waited_while_first_afloat)
      << "the second unit waits on the bank while the first is ferried";
  ASSERT_TRUE(first_landed) << "the first unit reaches the far bank";
  ASSERT_TRUE(second_landed) << "the raft comes back for the second unit";
  QVector3D const far_landing(start.landings[1].x(), 0.0F, start.landings[1].z());
  EXPECT_LT((position_of(first) - far_landing).length(), 1.0F)
      << "a passenger steps off at the far landing";
  EXPECT_EQ(riders(world), 0);
}

TEST_F(RaftSystemTest, RaftComesBackEmptyToFetchFromTheFarBank) {
  auto map = make_river_map(9.0F);
  map.rafts = {raft_at(QStringLiteral("ford"), 0.0F, 0.0F)};
  Engine::Core::World world;
  RaftSystem system(services());
  configure(system, map);
  auto const start = system.raft(0);
  ASSERT_EQ(start.side, 0);

  auto* troop = add_troop(world, start.landings[1]);
  ASSERT_TRUE(system.order_crossing(world, troop->get_id()));
  system.update(&world, k_tick);
  EXPECT_EQ(system.raft(0).phase, RaftSystem::Phase::Crossing);
  EXPECT_EQ(system.raft(0).passenger, 0U) << "the raft crosses empty to fetch the unit";

  for (float t = 0.0F; t < 40.0F && bank_of(system.raft(0), position_of(troop)) != 0;
       t += k_tick) {
    system.update(&world, k_tick);
  }
  EXPECT_EQ(bank_of(system.raft(0), position_of(troop)), 0)
      << "the unit is ferried to the raft's home bank";
}

TEST_F(RaftSystemTest, SiegeEnginesAndElephantsStayAshore) {
  auto map = make_river_map(9.0F);
  map.rafts = {raft_at(QStringLiteral("ford"), 0.0F, 0.0F)};
  Engine::Core::World world;
  RaftSystem system(services());
  configure(system, map);
  auto const landing = system.raft(0).landings[0];
  auto* ram = add_troop(world, landing, Game::Units::SpawnType::Ram);
  auto* elephant = add_troop(world, landing, Game::Units::SpawnType::Elephant);
  EXPECT_FALSE(system.order_crossing(world, ram->get_id()));
  EXPECT_FALSE(system.order_crossing(world, elephant->get_id()));
}

TEST_F(RaftSystemTest, AQueuedUnitSentElsewhereLeavesTheLine) {
  auto map = make_river_map(9.0F);
  map.rafts = {raft_at(QStringLiteral("ford"), 0.0F, 0.0F)};
  Engine::Core::World world;
  RaftSystem system(services());
  configure(system, map);
  auto const landing = system.raft(0).landings[1];
  auto* troop = add_troop(world, landing);
  ASSERT_TRUE(system.order_crossing(world, troop->get_id()));
  ASSERT_EQ(system.raft(0).queue.size(), 1U);

  auto* movement = troop->get_component<Engine::Core::MovementComponent>();
  movement->engage_manual_move(landing.x() + 25.0F, landing.z());
  movement->stop();
  troop->get_component<Engine::Core::TransformComponent>()->position.x +=
      Game::Systems::k_raft_call_radius * 2.0F;
  system.update(&world, k_tick);
  EXPECT_TRUE(system.raft(0).queue.empty()) << "a unit that walked off is forgotten";
}

TEST_F(RaftSystemTest, StateSurvivesASaveMidCrossing) {
  auto map = make_river_map(9.0F);
  map.rafts = {raft_at(QStringLiteral("ford"), 0.0F, 0.0F)};
  Engine::Core::World world;
  RaftSystem system(services());
  configure(system, map);
  auto* troop = add_troop(world, system.raft(0).landings[0]);
  ASSERT_TRUE(system.order_crossing(world, troop->get_id()));
  for (int i = 0; i < 40; ++i) {
    system.update(&world, k_tick);
  }
  auto const before = system.raft(0);
  ASSERT_EQ(before.phase, RaftSystem::Phase::Crossing);
  ASSERT_EQ(before.passenger, troop->get_id());
  EXPECT_EQ(Game::Systems::classify_unit_activity(world, troop->get_id()).kind,
            Game::Systems::ActivityKind::Ferry)
      << "the unit card says the troops are on the raft, not idle";

  QJsonObject const saved = system.serialize_state();
  RaftSystem restored(services());
  restored.configure(map);
  restored.restore_state(saved);
  auto const after = restored.raft(0);
  EXPECT_EQ(after.phase, before.phase);
  EXPECT_EQ(after.passenger, before.passenger);
  EXPECT_EQ(after.side, before.side);
  EXPECT_LT((after.position - before.position).length(), 1.0e-3F);
  EXPECT_EQ(restored.serialize_state(), saved);
}

TEST_F(RaftSystemTest, MapJsonDeclaresRafts) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  QString const path = dir.filePath(QStringLiteral("river.json"));
  QFile file(path);
  ASSERT_TRUE(file.open(QIODevice::WriteOnly));
  file.write(R"({
    "name": "Rhodanus",
    "grid": {"width": 40, "height": 40, "tile_size": 1.0},
    "rafts": [
      {"id": "upstream", "position": [20, 8], "speed": 2.5},
      {"x": 20, "z": 30},
      {"id": "broken"}
    ]
  })");
  file.close();

  Game::Map::MapDefinition map;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(path, map, &error))
      << error.toStdString();
  ASSERT_EQ(map.rafts.size(), 2U) << "a raft without a position is skipped";
  EXPECT_EQ(map.rafts[0].id, QStringLiteral("upstream"));
  EXPECT_FLOAT_EQ(map.rafts[0].x, 20.0F);
  EXPECT_FLOAT_EQ(map.rafts[0].z, 8.0F);
  EXPECT_FLOAT_EQ(map.rafts[0].speed, 2.5F);
  EXPECT_EQ(map.rafts[1].id, QStringLiteral("raft_2"));
  EXPECT_FLOAT_EQ(map.rafts[1].z, 30.0F);
}

TEST_F(RaftSystemTest, EveryRaftOnAShippedMapFindsItsRiver) {
  QDir const maps(QStringLiteral("assets/maps"));
  ASSERT_TRUE(maps.exists()) << "run from the repository root";
  int authored = 0;
  for (auto const& name : maps.entryList({QStringLiteral("*.json")}, QDir::Files)) {
    Game::Map::MapDefinition map;
    QString error;
    ASSERT_TRUE(
        Game::Map::MapLoader::load_from_json_file(maps.filePath(name), map, &error))
        << name.toStdString() << ": " << error.toStdString();
    if (map.rafts.empty()) {
      continue;
    }
    authored += static_cast<int>(map.rafts.size());
    RaftSystem system(services());
    configure(system, map);
    EXPECT_EQ(system.raft_count(), map.rafts.size())
        << name.toStdString()
        << " authors a raft off a river, on one too narrow, or with no dry landing";
  }
  EXPECT_GT(authored, 0) << "the Rhone crossing ships with rafts";
}

namespace {

auto reachable_cells(const Game::Systems::Pathfinding& pathfinding,
                     int width,
                     int height,
                     Game::Systems::Point from) -> std::vector<std::uint8_t> {
  std::vector<std::uint8_t> seen(static_cast<std::size_t>(width * height), 0U);
  std::vector<Game::Systems::Point> open{from};
  seen[static_cast<std::size_t>(from.y * width + from.x)] = 1U;
  while (!open.empty()) {
    auto const cell = open.back();
    open.pop_back();
    for (int dz = -1; dz <= 1; ++dz) {
      for (int dx = -1; dx <= 1; ++dx) {
        int const x = cell.x + dx;
        int const z = cell.y + dz;
        if (x < 0 || z < 0 || x >= width || z >= height) {
          continue;
        }
        auto& mark = seen[static_cast<std::size_t>(z * width + x)];
        if (mark == 0U && pathfinding.is_walkable(x, z)) {
          mark = 1U;
          open.push_back({x, z});
        }
      }
    }
  }
  return seen;
}

} // namespace

TEST_F(RaftSystemTest, RhoneRiverTownCanOnlyBeReachedByRaft) {
  Game::Map::MapDefinition map;
  QString error;
  ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
      QStringLiteral("assets/maps/map_crossing_rhone.json"), map, &error))
      << error.toStdString();
  Game::Map::TerrainService::instance().initialize(map);
  Game::Systems::Pathfinding pathfinding(map.grid.width, map.grid.height);
  pathfinding.set_grid_offset(-(static_cast<float>(map.grid.width) * 0.5F - 0.5F),
                              -(static_cast<float>(map.grid.height) * 0.5F - 0.5F));
  pathfinding.update_navigation_grid();

  int const width = map.grid.width;
  auto const cell_of_world = [&map](const QVector3D& at) {
    float const half_w = static_cast<float>(map.grid.width) * 0.5F - 0.5F;
    float const half_h = static_cast<float>(map.grid.height) * 0.5F - 0.5F;
    return Game::Systems::Point{static_cast<int>(std::round(at.x() + half_w)),
                                static_cast<int>(std::round(at.z() + half_h))};
  };
  auto const reached = [&](const std::vector<std::uint8_t>& seen,
                           Game::Systems::Point cell) {
    return seen[static_cast<std::size_t>(cell.y * width + cell.x)] != 0U;
  };

  auto const nearest_walkable = [&pathfinding](Game::Systems::Point at) {
    for (int ring = 0; ring < 30; ++ring) {
      for (int dz = -ring; dz <= ring; ++dz) {
        for (int dx = -ring; dx <= ring; ++dx) {
          if (std::max(std::abs(dx), std::abs(dz)) == ring &&
              pathfinding.is_walkable(at.x + dx, at.y + dz)) {
            return Game::Systems::Point{at.x + dx, at.y + dz};
          }
        }
      }
    }
    return at;
  };
  Game::Systems::Point const hannibal_camp{36, 173};
  auto const hill_fort_yard = nearest_walkable({376, 44});
  auto const river_town_yard = nearest_walkable({566, 545});
  ASSERT_TRUE(pathfinding.is_walkable(hannibal_camp.x, hannibal_camp.y));
  auto const from_camp =
      reachable_cells(pathfinding, width, map.grid.height, hannibal_camp);
  EXPECT_TRUE(reached(from_camp, hill_fort_yard))
      << "the hill fort is the camp the army can march to over a bridge";
  EXPECT_FALSE(reached(from_camp, river_town_yard))
      << "no bridge may lead to the river town: the rafts are the only way in";

  auto const from_town =
      reachable_cells(pathfinding, width, map.grid.height, river_town_yard);
  RaftSystem system(services());
  system.configure(map);
  ASSERT_EQ(system.raft_count(), map.rafts.size());
  for (std::size_t index = 0; index < system.raft_count(); ++index) {
    auto const raft = system.raft(index);
    auto const near = cell_of_world(raft.landings[0]);
    auto const far = cell_of_world(raft.landings[1]);
    bool const army_side = reached(from_camp, near) || reached(from_camp, far);
    bool const town_side = reached(from_town, near) || reached(from_town, far);
    EXPECT_TRUE(army_side && town_side)
        << raft.id.toStdString()
        << " has to link ground the army holds with the river town's bank";
  }
}

TEST_F(RaftSystemTest, TheHudIsToldOnlyWhenSomeoneComesWithinReach) {
  auto map = make_river_map(9.0F);
  map.rafts = {raft_at(QStringLiteral("ford"), 0.0F, 0.0F)};
  Engine::Core::World world;
  RaftSystem system(services());
  configure(system, map);

  int changes = 0;
  Engine::Core::ScopedEventSubscription<Engine::Core::ContextActionsChangedEvent>
      counter(
          [&changes](const Engine::Core::ContextActionsChangedEvent&) { ++changes; });

  auto const landing = system.raft(0).landings[0];
  auto* troop = add_troop(world, landing + QVector3D(0.0F, 0.0F, 60.0F));
  for (int i = 0; i < 20; ++i) {
    system.update(&world, k_tick);
  }
  EXPECT_EQ(changes, 0) << "nobody near the raft: the HUD hears nothing";

  troop->get_component<Engine::Core::TransformComponent>()->position.z = landing.z();
  world.spatial_index().invalidate();
  for (int i = 0; i < 20; ++i) {
    system.update(&world, k_tick);
  }
  EXPECT_EQ(changes, 1) << "one push when the troop walks into reach, not one a tick";
}
