#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/game_config.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
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
using Game::Session::SessionContext;
using Game::Systems::NavGrid;
using Game::Units::SpawnType;

constexpr int k_player = 1;
constexpr int k_enemy = 2;
constexpr int k_map = 160;

class ArmyCommandTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);
    open_field();
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  void open_field() {
    m_scope.reset();
    m_session.reset();
    Game::Systems::BuildingCollisionRegistry::instance().clear();
    Game::Map::MapDefinition map;
    map.grid.width = k_map;
    map.grid.height = k_map;
    map.grid.tile_size = 1.0F;
    map.coordSystem = Game::Map::CoordSystem::World;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;
    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(true);
    m_scope = std::make_unique<Game::Session::ScopedSession>(*m_session);
    m_session->owners().register_owner_with_id(
        k_player, Game::Systems::OwnerType::Player, "carthage");
    m_session->owners().set_owner_team(k_player, 1);
    m_session->owners().register_owner_with_id(
        k_enemy, Game::Systems::OwnerType::AI, "rome");
    m_session->owners().set_owner_team(k_enemy, 2);
    Game::Systems::initialize_default_content(m_session->nations());
    Game::Systems::register_runtime_systems(m_session->world());
    m_session->terrain().initialize(map);
    NavGrid::initialize(map.grid.width, map.grid.height);
    NavGrid::get_pathfinder()->update_navigation_grid();
  }

  auto
  spawn(SpawnType type, int owner, float x, float z, float yaw = 0.0F) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = QVector3D(x, 0.0F, z);
    params.player_id = owner;
    params.spawn_type = type;
    params.rotation_y = yaw;
    params.nation_id = owner == k_enemy ? Game::Systems::NationID::RomanRepublic
                                        : Game::Systems::NationID::Carthage;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  auto position(EntityID id) -> QVector3D {
    auto const* t = m_session->world().try_get<Engine::Core::TransformComponent>(id);
    return t == nullptr ? QVector3D() : QVector3D(t->position.x, 0.0F, t->position.z);
  }

  auto health(EntityID id) -> int {
    auto const* u = m_session->world().try_get<Engine::Core::UnitComponent>(id);
    return u == nullptr ? 0 : u->health;
  }

  auto speed(EntityID id) -> float {
    auto const* u = m_session->world().try_get<Engine::Core::UnitComponent>(id);
    return u == nullptr ? 0.0F : u->speed;
  }

  auto locked(EntityID id) -> bool {
    auto const* a = m_session->world().try_get<Engine::Core::AttackComponent>(id);
    return a != nullptr && a->in_melee_lock;
  }

  template <typename Fn>
  void run(double seconds, Fn&& each_tick) {
    double const step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds - 1e-9; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
        each_tick();
      }
    }
  }

  void run(double seconds) {
    run(seconds, [] {});
  }

  auto deploy_line(int count) -> std::vector<EntityID> {
    std::vector<EntityID> army;
    for (int i = 0; i < count; ++i) {
      army.push_back(spawn(SpawnType::Swordsman,
                           k_player,
                           -20.0F + static_cast<float>(i) * 6.0F,
                           -20.0F));
    }
    Game::Command::DeployFormation deploy;
    deploy.units = army;
    deploy.anchor = QVector3D(0.0F, 0.0F, 0.0F);
    deploy.spacing = Game::GameConfig::instance().gameplay().formation_spacing_default;
    Game::Command::submit(
        m_session->world(), Game::Command::Source::LocalPlayer, k_player, deploy);
    run(25.0);
    return army;
  }

  auto group_of(EntityID id) -> const Game::Formation::ArmyFormation* {
    auto& registry =
        Game::Formation::ArmyFormationRegistry::for_world(m_session->world());
    return registry.find(registry.group_of(id));
  }

  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
};

TEST_F(ArmyCommandTest, AnOrderedAttackPinsASquadThatIsMarchingPast) {
  struct Case {
    const char* label;
    SpawnType mine;
    SpawnType theirs;
    float lateral;
  };

  std::vector<Case> const cases{
      {"swords against marching swords",
       SpawnType::Swordsman,
       SpawnType::Swordsman,
       10.0F},
      {"cavalry against marching spears",
       SpawnType::MountedSwordsman,
       SpawnType::Spearman,
       15.0F},
  };
  for (auto const& c : cases) {
    SCOPED_TRACE(c.label);
    open_field();
    EntityID const mine = spawn(c.mine, k_player, 0.0F, -c.lateral);
    EntityID const theirs = spawn(c.theirs, k_enemy, -40.0F, 0.0F, 90.0F);
    ASSERT_NE(mine, 0U);
    ASSERT_NE(theirs, 0U);

    Game::Systems::CommandService::MoveOptions march;
    march.kind = Game::Systems::MoveOrderKind::PlannerMove;
    Game::Systems::CommandService::move_units(
        m_session->world(), {theirs}, {QVector3D(60.0F, 0.0F, 0.0F)}, march);
    run(8.0);

    Game::Command::AttackTarget attack;
    attack.units = {mine};
    attack.target = theirs;
    Game::Command::submit(
        m_session->world(), Game::Command::Source::LocalPlayer, k_player, attack);
    float const ordered_at = position(theirs).x();
    int const mine_before = health(mine);
    int const theirs_before = health(theirs);

    run(18.0);

    EXPECT_TRUE(locked(theirs)) << "the marching squad was never pinned";
    EXPECT_LT(position(theirs).x() - ordered_at, 20.0F)
        << "the enemy kept marching " << position(theirs).x() - ordered_at
        << " m after the attack order";
    EXPECT_LT(health(theirs), theirs_before) << "my squad never hurt its target";
    EXPECT_LT(health(mine), mine_before) << "the pinned squad never struck back";
  }
}

TEST_F(ArmyCommandTest, EveryTroopInAFormationOrderMarchesAtTheGroupPace) {
  std::vector<SpawnType> const kinds{SpawnType::Swordsman,
                                     SpawnType::Spearman,
                                     SpawnType::Archer,
                                     SpawnType::MountedSwordsman,
                                     SpawnType::Swordsman,
                                     SpawnType::HorseArcher,
                                     SpawnType::Spearman,
                                     SpawnType::Archer};
  std::vector<EntityID> army;
  float group_pace = 1.0e9F;
  for (std::size_t i = 0; i < kinds.size(); ++i) {
    float const x = -40.0F + static_cast<float>(i % 4) * 10.0F;
    float const z = -30.0F + static_cast<float>(i / 4) * 12.0F;
    army.push_back(spawn(kinds[i], k_player, x, z));
    ASSERT_NE(army.back(), 0U);
    group_pace = std::min(group_pace, speed(army.back()));
  }
  run(0.5);

  Game::Command::DeployFormation deploy;
  deploy.units = army;
  deploy.anchor = QVector3D(10.0F, 0.0F, 25.0F);
  deploy.facing = 45.0F;
  deploy.spacing = Game::GameConfig::instance().gameplay().formation_spacing_default;
  Game::Command::submit(
      m_session->world(), Game::Command::Source::LocalPlayer, k_player, deploy);

  std::vector<QVector3D> last(army.size());
  for (std::size_t i = 0; i < army.size(); ++i) {
    last[i] = position(army[i]);
  }
  std::vector<float> slowest(army.size(), 1.0e9F);
  std::vector<float> fastest(army.size(), 0.0F);
  std::vector<bool> under_way(army.size(), false);
  int tick = 0;
  constexpr int k_window = 30;
  run(30.0, [&] {
    if (++tick % k_window != 0) {
      return;
    }
    float const window_seconds =
        static_cast<float>(k_window * m_session->clock().tick_seconds());
    for (std::size_t i = 0; i < army.size(); ++i) {
      auto const* movement =
          m_session->world().try_get<Engine::Core::MovementComponent>(army[i]);
      auto const now = position(army[i]);
      float const walked = (now - last[i]).length() / window_seconds;
      last[i] = now;
      bool const marching = movement != nullptr && movement->get_has_target() &&
                            QVector3D(movement->get_goal_x() - now.x(),
                                      0.0F,
                                      movement->get_goal_y() - now.z())
                                    .length() > 3.0F;
      if (!marching) {
        continue;
      }
      if (!under_way[i]) {
        under_way[i] = true;
        continue;
      }
      slowest[i] = std::min(slowest[i], walked);
      fastest[i] = std::max(fastest[i], walked);
    }
  });

  for (std::size_t i = 0; i < army.size(); ++i) {
    if (fastest[i] <= 0.0F) {
      continue;
    }
    SCOPED_TRACE(static_cast<int>(i));
    EXPECT_GT(slowest[i], group_pace * 0.8F)
        << "a troop crawled at " << slowest[i] << " m/s against a group pace of "
        << group_pace;
    EXPECT_LT(fastest[i], group_pace * 1.1F)
        << "a troop ran ahead at " << fastest[i] << " m/s against a group pace of "
        << group_pace;
  }
}

TEST_F(ArmyCommandTest, ATroopOrderedAwayLeavesItsFormation) {
  auto const army = deploy_line(6);
  EntityID const loner = army[0];

  Game::Command::Move move;
  move.units = {loner};
  move.targets = {QVector3D(40.0F, 0.0F, 40.0F)};
  move.kind = Game::Systems::MoveOrderKind::PlayerMove;
  Game::Command::submit(
      m_session->world(), Game::Command::Source::LocalPlayer, k_player, move);
  run(30.0);

  EXPECT_EQ(group_of(loner), nullptr)
      << "a troop sent away on its own order stayed in its formation";
  auto const* rest = group_of(army[1]);
  ASSERT_NE(rest, nullptr);
  EXPECT_EQ(rest->members.size(), army.size() - 1);
  EXPECT_TRUE(rest->is_formed())
      << "the troop that left kept the rest of the formation from counting as "
         "formed (cohesion "
      << rest->cohesion << ")";

  auto const arrived = position(loner);
  m_session->world().try_get<Engine::Core::UnitComponent>(army[3])->health = 0;
  run(20.0);
  EXPECT_LT((position(loner) - arrived).length(), 1.0F)
      << "the formation pulled back a troop the player had sent away";
}

TEST_F(ArmyCommandTest, AFormationClosesRanksAfterACasualty) {
  auto const army = deploy_line(6);
  auto const* formation = group_of(army[1]);
  ASSERT_NE(formation, nullptr);
  ASSERT_TRUE(formation->is_formed());

  m_session->world().try_get<Engine::Core::UnitComponent>(army[3])->health = 0;
  run(15.0);

  formation = group_of(army[1]);
  ASSERT_NE(formation, nullptr);
  EXPECT_TRUE(formation->is_formed())
      << "one casualty left the formation unformed for good (cohesion "
      << formation->cohesion << ")";
}

TEST_F(ArmyCommandTest, AnAttackMoveCarriesTheWholeFormationToItsDestination) {
  auto const army = deploy_line(6);
  QVector3D const destination(40.0F, 0.0F, 40.0F);

  Game::Command::Move order;
  order.units = army;
  order.targets.assign(army.size(), destination);
  order.kind = Game::Systems::MoveOrderKind::AttackMove;
  Game::Command::submit(
      m_session->world(), Game::Command::Source::LocalPlayer, k_player, order);
  run(45.0);

  for (auto const troop : army) {
    EXPECT_LT((position(troop) - destination).length(), 12.0F)
        << "a troop left the attack-move and walked back to its old formation slot";
  }
}

TEST_F(ArmyCommandTest, AnArmySentAtAnEnemyDoesNotWalkBackAfterTheFight) {
  auto const army = deploy_line(6);
  std::vector<QVector3D> start_spots;
  for (auto const troop : army) {
    start_spots.push_back(position(troop));
  }
  EntityID const enemy = spawn(SpawnType::Archer, k_enemy, 30.0F, 30.0F);
  m_session->world().try_get<Engine::Core::UnitComponent>(enemy)->health = 60;

  Game::Command::AttackTarget attack;
  attack.units = army;
  attack.target = enemy;
  Game::Command::submit(
      m_session->world(), Game::Command::Source::LocalPlayer, k_player, attack);
  run(40.0);
  ASSERT_LE(health(enemy), 0) << "the army never reached its target";

  for (std::size_t i = 0; i < army.size(); ++i) {
    EXPECT_GT((position(army[i]) - start_spots[i]).length(), 15.0F)
        << "a troop walked back to where the army stood before the attack";
  }
}

TEST_F(ArmyCommandTest, ARedeployedArmySettlesInsteadOfShuttlingBetweenGroups) {
  auto const army = deploy_line(6);
  EntityID const recruit = spawn(SpawnType::Swordsman, k_player, 0.0F, -30.0F);
  std::vector<EntityID> everyone{recruit};
  everyone.insert(everyone.end(), army.begin(), army.end());

  Game::Command::DeployFormation deploy;
  deploy.units = everyone;
  deploy.anchor = QVector3D(40.0F, 0.0F, 0.0F);
  deploy.spacing = Game::GameConfig::instance().gameplay().formation_spacing_default;
  Game::Command::submit(
      m_session->world(), Game::Command::Source::LocalPlayer, k_player, deploy);
  run(30.0);

  std::vector<QVector3D> settled;
  for (auto const troop : everyone) {
    settled.push_back(position(troop));
  }
  run(15.0);
  for (std::size_t i = 0; i < everyone.size(); ++i) {
    EXPECT_LT((position(everyone[i]) - settled[i]).length(), 1.0F)
        << "a troop kept shuttling after the army had formed up";
  }
}

TEST_F(ArmyCommandTest, AnyTwoEnemiesThatTouchAreLockedInMelee) {
  struct Case {
    const char* label;
    SpawnType walker;
    SpawnType standing;
  };

  std::vector<Case> const cases{
      {"a builder crew walking through enemy swords",
       SpawnType::Builder,
       SpawnType::Swordsman},
      {"swords walking through an enemy builder crew",
       SpawnType::Swordsman,
       SpawnType::Builder},
      {"spears marching through enemy archers", SpawnType::Spearman, SpawnType::Archer},
  };
  for (auto const& c : cases) {
    SCOPED_TRACE(c.label);
    open_field();
    EntityID const walker = spawn(c.walker, k_player, -20.0F, 0.0F, 90.0F);
    EntityID const standing = spawn(c.standing, k_enemy, 0.0F, 0.0F, 270.0F);
    ASSERT_NE(walker, 0U);
    ASSERT_NE(standing, 0U);
    Game::Command::Move move;
    move.units = {walker};
    move.targets = {QVector3D(20.0F, 0.0F, 0.0F)};
    move.kind = Game::Systems::MoveOrderKind::PlayerMove;
    Game::Command::submit(
        m_session->world(), Game::Command::Source::LocalPlayer, k_player, move);
    bool both_locked = false;
    run(20.0,
        [&] { both_locked = both_locked || (locked(walker) && locked(standing)); });
    EXPECT_TRUE(both_locked) << "two enemies touched and neither was held in melee";
    EXPECT_LT(position(walker).x(), 8.0F) << "the walker passed straight through";
  }
}

} // namespace
