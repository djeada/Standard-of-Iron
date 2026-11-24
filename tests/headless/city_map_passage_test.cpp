// Rome and Carthage are dense cities behind stone walls: thousands of houses,
// porticoes and towers, walls with angled legs, and gates on their straight
// ones. These tests load the shipped maps into a live world, with every
// structure on the navigation grid, and march real troops in: one through every
// gate (a gate only opens for a friendly troop that walks up to it, so the
// static grid cannot answer this), and others from the roads outside to the
// squares in the middle. With every gate shut, the walls must hold.

#include <QDir>
#include <QString>
#include <QVector3D>

#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <string>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component_core.h"
#include "game/core/component_gameplay.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/map_loader.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/session/map_session.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/default_content.h"
#include "game/systems/nation_registry.h"
#include "game/systems/navigation/nav_grid.h"
#include "game/systems/navigation/pathfinding.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::EntityID;
using Engine::Core::TransformComponent;
using Engine::Core::UnitComponent;
using Game::Session::SessionContext;
using Game::Systems::NavGrid;
using Game::Systems::Point;
using Passability = Game::Systems::Pathfinding::Passability;

constexpr int k_city_owner = 1;

struct Route {
  const char* name;
  QVector3D from;
  QVector3D to;
};

struct March {
  std::string name;
  EntityID troop{0};
  QVector3D from;
  QVector3D to;
};

class CityMapPassageTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
  }

  void TearDown() override {
    Game::Map::MapTransformer::setFactoryRegistry(nullptr);
    m_scope.reset();
    m_session.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  void load(const QString& file, Game::Systems::NationID nation) {
    m_nation = nation;
    Game::Map::MapDefinition map;
    QString error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
        QDir(QStringLiteral("assets/maps")).filePath(file), map, &error))
        << error.toStdString();
    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    auto& owners = m_session->owners();
    owners.register_owner_with_id(
        k_city_owner, Game::Systems::OwnerType::Player, "city");
    owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "besieger");
    owners.set_owner_team(k_city_owner, 1);
    owners.set_owner_team(2, 2);
    Game::Systems::initialize_default_content(m_session->nations());
    Game::Systems::register_runtime_systems(m_session->world());
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
    m_session->terrain().initialize(map);
    NavGrid::initialize(map.grid.width, map.grid.height);
    Game::Map::MapTransformer::apply_to_world(map, m_session->world());
    Game::Session::configure_map_systems(m_session->world(), map, nullptr);
    NavGrid::get_pathfinder()->update_navigation_grid();
  }

  static auto open(const Point& cell) -> bool {
    return NavGrid::get_pathfinder()->region_of(cell, Passability::Light) !=
           Game::Systems::Pathfinding::k_unreachable_region;
  }

  // The nearest open ground to a point: a road point can sit under a statue.
  static auto open_point_near(const QVector3D& at, int reach = 4) -> QVector3D {
    const Point origin = NavGrid::world_to_grid(at.x(), at.z());
    for (int radius = 0; radius <= reach; ++radius) {
      for (int dz = -radius; dz <= radius; ++dz) {
        for (int dx = -radius; dx <= radius; ++dx) {
          const Point cell{origin.x + dx, origin.y + dz};
          if (open(cell)) {
            const QVector3D world = NavGrid::grid_to_world(cell);
            return {world.x(), 0.0F, world.z()};
          }
        }
      }
    }
    return at;
  }

  auto spawn_troop(const QVector3D& at) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = at;
    params.player_id = k_city_owner;
    params.spawn_type = Game::Units::SpawnType::Spearman;
    params.nation_id = m_nation;
    auto unit = m_factory->create(params.spawn_type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  void order(EntityID troop, const QVector3D& target) {
    Game::Command::submit(m_session->world(),
                          Game::Command::Source::Script,
                          k_city_owner,
                          Game::Command::Move{.units = {troop}, .targets = {target}});
  }

  auto position_of(EntityID id) -> QVector3D {
    const auto* t = m_session->world().try_get<TransformComponent>(id);
    return t == nullptr ? QVector3D() : QVector3D(t->position.x, 0.0F, t->position.z);
  }

  void run(double seconds) {
    const double step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
      }
    }
  }

  void march(std::vector<March>& marches, double seconds, float arrival) {
    for (March& m : marches) {
      m.from = open_point_near(m.from);
      m.to = open_point_near(m.to);
      m.troop = spawn_troop(m.from);
      ASSERT_NE(m.troop, 0U) << m.name << ": no troop";
    }
    run(0.5);
    for (const March& m : marches) {
      order(m.troop, m.to);
    }
    run(seconds);
    for (const March& m : marches) {
      const QVector3D at = position_of(m.troop);
      const float left = (at - m.to).length();
      EXPECT_LE(left, arrival) << m.name << ": the troop stopped at " << at.x() << ", "
                               << at.z() << ", " << left << " m short";
    }
  }

  // A troop on one side of each gate is ordered to the other, across the wall.
  // The gates are held open: this checks the passage the map leaves through
  // every gatehouse, not when the gate system decides to swing it (planning
  // treats a shut gate as wall, so with another gate in reach a troop may
  // rightly take that one).
  auto gate_marches(int minimum_gates) -> std::vector<March> {
    for (auto [id, gate] : m_session->world().view<Engine::Core::GateComponent>()) {
      (void)id;
      gate.manual_mode = Engine::Core::GateComponent::ManualMode::ForcedOpen;
    }
    run(3.0);
    std::vector<March> marches;
    for (auto [id, unit, transform] :
         m_session->world().view<const UnitComponent, const TransformComponent>()) {
      (void)id;
      if (unit.spawn_type != Game::Units::SpawnType::WallGate) {
        continue;
      }
      const bool spans_x =
          Engine::Core::GateComponent::spans_x_axis(transform.rotation.y);
      const QVector3D across =
          spans_x ? QVector3D(0.0F, 0.0F, 1.0F) : QVector3D(1.0F, 0.0F, 0.0F);
      const QVector3D at(transform.position.x, 0.0F, transform.position.z);
      marches.push_back({"the gate at " + std::to_string(static_cast<int>(at.x())) +
                             ", " + std::to_string(static_cast<int>(at.z())),
                         0,
                         at - (across * 5.0F),
                         at + (across * 5.0F)});
    }
    EXPECT_GE(static_cast<int>(marches.size()), minimum_gates);
    return marches;
  }

  static auto route_marches(const std::vector<Route>& routes) -> std::vector<March> {
    std::vector<March> marches;
    for (const Route& route : routes) {
      marches.push_back({route.name, 0, route.from, route.to});
    }
    return marches;
  }

  void expect_walled_off(const QVector3D& outside, const QVector3D& inside) {
    const Point from = NavGrid::world_to_grid(outside.x(), outside.z());
    const Point to = NavGrid::world_to_grid(inside.x(), inside.z());
    ASSERT_TRUE(open(from));
    ASSERT_TRUE(open(to));
    EXPECT_FALSE(NavGrid::get_pathfinder()->can_reach(from, to, Passability::Light))
        << "with the gates shut, the wall can still be walked round";
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  Game::Systems::NationID m_nation{Game::Systems::NationID::RomanRepublic};
};

} // namespace

TEST_F(CityMapPassageTest, EveryRomanGateLetsATroopThrough) {
  load(QStringLiteral("map_rome.json"), Game::Systems::NationID::RomanRepublic);
  auto marches = gate_marches(8);
  march(marches, 40.0, 4.0F);
}

TEST_F(CityMapPassageTest, RomesRoadsLeadToTheForum) {
  load(QStringLiteral("map_rome.json"), Game::Systems::NationID::RomanRepublic);
  const QVector3D forum(-50.0F, 0.0F, 2.0F);
  auto marches = route_marches({
      {"Via Appia", QVector3D(100.0F, 0.0F, 262.0F), forum},
      {"Via Salaria", QVector3D(40.0F, 0.0F, -290.0F), forum},
      {"Via Praenestina", QVector3D(260.0F, 0.0F, -50.0F), forum},
      {"Via Ostiensis", QVector3D(-142.0F, 0.0F, 230.0F), forum},
      {"Trastevere, over the river", QVector3D(-262.0F, 0.0F, 50.0F), forum},
      {"the Subura", QVector3D(33.0F, 0.0F, -62.0F), forum},
  });
  march(marches, 420.0, 12.0F);
}

TEST_F(CityMapPassageTest, EveryPunicGateLetsATroopThrough) {
  load(QStringLiteral("map_carthage.json"), Game::Systems::NationID::Carthage);
  auto marches = gate_marches(8);
  march(marches, 40.0, 4.0F);
}

TEST_F(CityMapPassageTest, CarthagesRoadsLeadToTheAgora) {
  load(QStringLiteral("map_carthage.json"), Game::Systems::NationID::Carthage);
  const QVector3D agora(109.0F, 0.0F, 122.0F);
  auto marches = route_marches({
      {"the Tunis road, through the triple wall",
       QVector3D(-320.0F, 0.0F, 128.0F),
       agora},
      {"Megara, through the garden gate", QVector3D(-40.0F, 0.0F, -250.0F), agora},
      {"down from the Byrsa", QVector3D(-58.0F, 0.0F, 30.0F), agora},
      {"from the round harbour", QVector3D(95.0F, 0.0F, 148.0F), agora},
  });
  march(marches, 420.0, 12.0F);
}
