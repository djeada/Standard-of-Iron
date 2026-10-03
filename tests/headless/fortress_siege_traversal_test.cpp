// The fortress assault mission rests on two engines getting where they are
// sent on its hill: a siege tower up the ramp and against the outer curtain,
// and a ram up to the south gate. Both are checked on the shipped map, with its
// walls, gates and towers standing.

#include <QDir>
#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_dispatcher.h"
#include "game/core/component_core.h"
#include "game/core/component_gameplay.h"
#include "game/core/component_structures.h"
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
#include "game/systems/movement/command_service.h"
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
using Game::Session::SessionContext;
using Game::Systems::CommandService;
using Game::Systems::NavGrid;

constexpr int k_attacker = 1;
constexpr int k_garrison = 2;

class FortressSiegeTraversalTest : public ::testing::Test {
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

  void load_fortress() {
    Game::Map::MapDefinition map;
    QString error;
    ASSERT_TRUE(Game::Map::MapLoader::load_from_json_file(
        QDir(QStringLiteral("assets/maps"))
            .filePath(QStringLiteral("map_victumulae.json")),
        map,
        &error))
        << error.toStdString();
    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(false);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    auto& owners = m_session->owners();
    owners.register_owner_with_id(
        k_attacker, Game::Systems::OwnerType::Player, "carthage");
    owners.register_owner_with_id(k_garrison, Game::Systems::OwnerType::AI, "rome");
    owners.set_owner_team(k_attacker, 1);
    owners.set_owner_team(k_garrison, 2);
    Game::Systems::initialize_default_content(m_session->nations());
    Game::Systems::register_runtime_systems(m_session->world());
    Game::Map::MapTransformer::setFactoryRegistry(m_factory);
    m_session->terrain().initialize(map);
    NavGrid::initialize(map.grid.width, map.grid.height);
    Game::Map::MapTransformer::apply_to_world(map, m_session->world());
    // As a match does: hill ramps get their stone caches, and so on.
    Game::Session::configure_map_systems(m_session->world(), map, nullptr);
    NavGrid::get_pathfinder()->update_navigation_grid();
  }

  auto spawn(Game::Units::SpawnType type, const QVector3D& at) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = at;
    params.player_id = k_attacker;
    params.spawn_type = type;
    params.nation_id = Game::Systems::NationID::Carthage;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  auto position_of(EntityID id) -> QVector3D {
    auto const* t = m_session->world().try_get<TransformComponent>(id);
    return t == nullptr ? QVector3D()
                        : QVector3D(t->position.x, t->position.y, t->position.z);
  }

  void run(double seconds) {
    double const step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
      }
    }
  }

  auto docks_within(EntityID tower, int seconds) -> bool {
    for (int second = 0; second < seconds; ++second) {
      run(1.0);
      auto const* state =
          m_session->world().try_get<Engine::Core::SiegeTowerComponent>(tower);
      if (state == nullptr) {
        return false;
      }
      if (state->state == Engine::Core::SiegeTowerComponent::State::Docked) {
        return true;
      }
    }
    return false;
  }

  auto first_garrison_wall_near(float x, float z) -> EntityID {
    EntityID best = 0;
    float best_d = 1.0e9F;
    for (auto [id, unit, transform] :
         m_session->world()
             .view<const Engine::Core::UnitComponent, const TransformComponent>()) {
      if (unit.owner_id != k_garrison ||
          unit.spawn_type != Game::Units::SpawnType::WallSegment) {
        continue;
      }
      float const d = std::hypot(transform.position.x - x, transform.position.z - z);
      if (d < best_d) {
        best_d = d;
        best = id;
      }
    }
    return best;
  }

  // Leaves the garrison its walls and nothing to fight with or recruit.
  void disarm_garrison() {
    std::vector<EntityID> doomed;
    for (auto [id, unit] :
         m_session->world().view<const Engine::Core::UnitComponent>()) {
      if (unit.owner_id == k_garrison &&
          !Game::Units::is_building_spawn(unit.spawn_type)) {
        doomed.push_back(id);
      }
    }
    for (auto const id : doomed) {
      m_session->world().destroy_entity(id);
    }
    for (auto [id, production] :
         m_session->world().view<Engine::Core::ProductionComponent>()) {
      (void)id;
      production.manpower_available = 0;
      production.max_units = 0;
    }
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

} // namespace

// The outer curtain runs east-west at z = -3.5 either side of the south gate;
// the camp lies far to the south across the ramp at z = 12.5.
const QVector3D k_camp_yard(26.0F, 0.0F, 46.0F);
const QVector3D k_before_west_curtain(-7.5F, 0.0F, -1.2F);

TEST_F(FortressSiegeTraversalTest, SiegeTowerClimbsTheRampAndDocksOnTheCurtain) {
  load_fortress();
  auto const tower = spawn(Game::Units::SpawnType::SiegeTower, k_camp_yard);
  ASSERT_NE(tower, 0U);
  run(0.5);
  CommandService::move_units(m_session->world(), {tower}, {k_before_west_curtain});
  EXPECT_TRUE(docks_within(tower, 180))
      << "the tower stopped at (" << position_of(tower).x() << ", "
      << position_of(tower).z() << ")";
}

TEST_F(FortressSiegeTraversalTest, SiegeTowerGetsPastItsOwnCompanies) {
  load_fortress();
  disarm_garrison();
  auto const tower = spawn(Game::Units::SpawnType::SiegeTower, k_camp_yard);
  ASSERT_NE(tower, 0U);
  // The escort waits on the crown, right where the tower has to roll through.
  for (QVector3D const at : {QVector3D(-7.5F, 0.0F, 7.0F),
                             QVector3D(-4.5F, 0.0F, 6.0F),
                             QVector3D(-1.5F, 0.0F, 8.0F)}) {
    ASSERT_NE(spawn(Game::Units::SpawnType::Swordsman, at), 0U);
  }
  run(0.5);
  CommandService::move_units(m_session->world(), {tower}, {k_before_west_curtain});
  EXPECT_TRUE(docks_within(tower, 180))
      << "the tower never docked; it stopped at (" << position_of(tower).x() << ", "
      << position_of(tower).z() << ")";
}

TEST_F(FortressSiegeTraversalTest, AttackOrderOnAWallSendsTheTowerToDockAgainstIt) {
  load_fortress();
  auto const tower = spawn(Game::Units::SpawnType::SiegeTower, k_camp_yard);
  ASSERT_NE(tower, 0U);
  auto const wall = first_garrison_wall_near(-7.5F, -3.5F);
  ASSERT_NE(wall, 0U);
  run(0.5);
  // What a right-click on the curtain with the tower selected sends.
  Game::Command::dispatch(
      m_session->world(),
      Game::Command::Command{
          .owner_id = k_attacker,
          .payload = Game::Command::AttackTarget{.units = {tower}, .target = wall}});
  EXPECT_TRUE(docks_within(tower, 180))
      << "the tower stopped at (" << position_of(tower).x() << ", "
      << position_of(tower).z() << ")";
}

TEST_F(FortressSiegeTraversalTest, RamClimbsTheRampToTheSouthGate) {
  load_fortress();
  auto const ram = spawn(Game::Units::SpawnType::Ram, {32.0F, 0.0F, 62.0F});
  ASSERT_NE(ram, 0U);
  run(0.5);
  QVector3D const before_gate(0.5F, 0.0F, 2.5F);
  CommandService::move_units(m_session->world(), {ram}, {before_gate});
  float closest = 1.0e9F;
  for (int second = 0; second < 150; ++second) {
    run(1.0);
    auto const p = position_of(ram);
    closest =
        std::min(closest, std::hypot(p.x() - before_gate.x(), p.z() - before_gate.z()));
  }
  EXPECT_LT(closest, 3.0F) << "the ram never reached the south gate";
}

TEST_F(FortressSiegeTraversalTest, PlayerMoveOrderTakesTheTowerToTheCurtain) {
  load_fortress();
  auto const tower = spawn(Game::Units::SpawnType::SiegeTower, k_camp_yard);
  ASSERT_NE(tower, 0U);
  run(0.5);
  auto const plan = CommandService::plan_ground_move(
      m_session->world(), {tower}, k_before_west_curtain, false);
  std::vector<QVector3D> targets;
  for (auto const& slot : plan.member_slots) {
    targets.push_back(slot.position);
  }
  ASSERT_EQ(targets.size(), 1U);
  Game::Command::dispatch(
      m_session->world(),
      Game::Command::Command{
          .owner_id = k_attacker,
          .payload = Game::Command::Move{.units = {tower}, .targets = targets}});
  EXPECT_TRUE(docks_within(tower, 180))
      << "planned to (" << targets.front().x() << ", " << targets.front().z()
      << "); the tower stopped at (" << position_of(tower).x() << ", "
      << position_of(tower).z() << ")";
}
