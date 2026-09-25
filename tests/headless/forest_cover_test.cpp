#include <gtest/gtest.h>
#include <memory>
#include <vector>

#include "game/core/component_combat.h"
#include "game/core/component_core.h"
#include "game/core/world.h"
#include "game/map/map_definition.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/default_content.h"
#include "game/systems/forest_cover_system.h"
#include "game/systems/nation_registry.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/runtime_system_registry.h"
#include "game/units/factory.h"
#include "game/units/spawn_type.h"

namespace {

using Engine::Core::AttackComponent;
using Engine::Core::AttackTargetComponent;
using Engine::Core::EntityID;
using Engine::Core::ForestCoverComponent;
using Engine::Core::TransformComponent;
using Game::Session::SessionContext;
using Game::Systems::NavGrid;
using Game::Systems::Point;

constexpr int k_player = 1;
constexpr int k_enemy = 2;
constexpr int k_hider = k_enemy;
constexpr int k_watcher = k_player;
constexpr int k_map = 64;
constexpr int k_wood_x = 20;
constexpr int k_wood_z = 32;

class ForestCoverTest : public ::testing::Test {
protected:
  void SetUp() override {
    Game::Systems::NationRegistry::instance().clear();
    Game::Systems::initialize_default_content(
        Game::Systems::NationRegistry::instance());
    m_factory = std::make_shared<Game::Units::UnitFactoryRegistry>();
    Game::Units::register_built_in_units(*m_factory);

    Game::Map::MapDefinition map;
    map.grid.width = k_map;
    map.grid.height = k_map;
    map.grid.tile_size = 1.0F;
    map.biome.procedural_boulders_enabled = false;
    map.biome.procedural_iron_ore_enabled = false;
    map.biome.procedural_trees_enabled = false;
    Game::Map::Forest wood;
    wood.id = QStringLiteral("test_wood");
    wood.x = static_cast<float>(k_wood_x);
    wood.z = static_cast<float>(k_wood_z);
    wood.radius = 9.0F;
    map.forests.push_back(wood);

    m_session = std::make_unique<SessionContext>();
    m_session->world().set_presentation_enabled(false);
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
    if (auto* pathfinder = NavGrid::get_pathfinder()) {
      pathfinder->mark_navigation_grid_dirty();
      pathfinder->update_navigation_grid();
    }
  }

  void TearDown() override {
    m_scope.reset();
    m_session.reset();
    Game::Map::TerrainService::instance().clear();
    Game::Systems::NationRegistry::instance().clear();
  }

  static auto world_of(int grid_x, int grid_z) -> QVector3D {
    return NavGrid::grid_to_world(Point(grid_x, grid_z));
  }

  auto
  spawn(Game::Units::SpawnType type, const QVector3D& position, int owner) -> EntityID {
    Game::Units::SpawnParams params;
    params.position = position;
    params.player_id = owner;
    params.spawn_type = type;
    params.nation_id = owner == k_player ? Game::Systems::NationID::Carthage
                                         : Game::Systems::NationID::RomanRepublic;
    auto unit = m_factory->create(type, m_session->world(), params);
    return unit ? unit->id() : 0;
  }

  void run_for(double seconds) {
    const double step = m_session->clock().tick_seconds();
    for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
      m_session->clock().advance(step);
      while (m_session->clock().consume_tick()) {
        m_session->world().update(static_cast<float>(step));
      }
    }
  }

  auto cover_of(EntityID id) -> const ForestCoverComponent* {
    auto* entity = m_session->world().get_entity(id);
    return entity != nullptr ? entity->get_component<ForestCoverComponent>() : nullptr;
  }

  auto target_of(EntityID id) -> EntityID {
    auto* entity = m_session->world().get_entity(id);
    const auto* target =
        entity != nullptr ? entity->get_component<AttackTargetComponent>() : nullptr;
    return target != nullptr ? target->target_id : 0;
  }

  void teleport(EntityID id, const QVector3D& position) {
    auto* transform =
        m_session->world().get_entity(id)->get_component<TransformComponent>();
    transform->position.x = position.x();
    transform->position.z = position.z();
  }

  std::unique_ptr<SessionContext> m_session;
  std::unique_ptr<Game::Session::ScopedSession> m_scope;
  std::shared_ptr<Game::Units::UnitFactoryRegistry> m_factory;
};

} // namespace

TEST_F(ForestCoverTest, AnEnemyBeyondSpottingDistanceCannotPickOutAUnitInTheWood) {
  const EntityID hidden =
      spawn(Game::Units::SpawnType::Builder, world_of(k_wood_x, k_wood_z), k_hider);
  const EntityID watcher = spawn(
      Game::Units::SpawnType::Archer, world_of(k_wood_x + 7, k_wood_z), k_watcher);
  ASSERT_NE(hidden, 0U);
  ASSERT_NE(watcher, 0U);
  run_for(6.0);

  const auto* cover = cover_of(hidden);
  ASSERT_NE(cover, nullptr) << "a unit standing in a forest must be given cover";
  EXPECT_TRUE(cover->concealed);
  EXPECT_TRUE(cover->hidden_from(k_watcher));
  EXPECT_FALSE(cover->hidden_from(k_hider)) << "a side always sees its own men";
  EXPECT_NE(target_of(watcher), hidden)
      << "an archer in bow range shot at a man it cannot see among the trees";
}

TEST_F(ForestCoverTest, CloseEnoughToLookIntoTheWoodTheEnemyFindsHim) {
  const EntityID hidden =
      spawn(Game::Units::SpawnType::Archer, world_of(k_wood_x, k_wood_z), k_hider);
  const EntityID watcher = spawn(
      Game::Units::SpawnType::Archer, world_of(k_wood_x + 4, k_wood_z), k_watcher);
  ASSERT_NE(hidden, 0U);
  ASSERT_NE(watcher, 0U);
  run_for(6.0);

  const auto* cover = cover_of(hidden);
  EXPECT_TRUE(cover == nullptr || !cover->hidden_from(k_watcher))
      << "within " << Game::Systems::k_forest_spot_distance
      << " m the trees no longer hide him";
  EXPECT_EQ(target_of(watcher), hidden);
}

TEST_F(ForestCoverTest, LeavingTheWoodGivesTheCoverUp) {
  const EntityID hidden =
      spawn(Game::Units::SpawnType::Archer, world_of(k_wood_x, k_wood_z), k_hider);
  const EntityID watcher = spawn(
      Game::Units::SpawnType::Archer, world_of(k_wood_x + 16, k_wood_z), k_watcher);
  ASSERT_NE(hidden, 0U);
  ASSERT_NE(watcher, 0U);
  run_for(6.0);
  ASSERT_NE(cover_of(hidden), nullptr);
  ASSERT_TRUE(cover_of(hidden)->hidden_from(k_watcher));

  teleport(hidden, world_of(k_wood_x + 11, k_wood_z));
  run_for(2.0);
  EXPECT_FALSE(cover_of(hidden)->in_forest);
  EXPECT_FALSE(cover_of(hidden)->concealed);
  EXPECT_EQ(target_of(watcher), hidden)
      << "out on open ground the archer should find him at once";
}

TEST_F(ForestCoverTest, LoosingAVolleyGivesAnArcherAwayForAFewSeconds) {
  const EntityID archer =
      spawn(Game::Units::SpawnType::Archer, world_of(k_wood_x, k_wood_z), k_hider);
  ASSERT_NE(archer, 0U);
  run_for(6.0);
  ASSERT_NE(cover_of(archer), nullptr);
  ASSERT_TRUE(cover_of(archer)->concealed);

  m_session->world()
      .get_entity(archer)
      ->get_component<AttackComponent>()
      ->time_since_last = 0.0F;
  run_for(0.5);
  EXPECT_FALSE(cover_of(archer)->concealed)
      << "an archer who has just shot is marked by his own arrows";
  EXPECT_TRUE(cover_of(archer)->in_forest) << "though the trees still stand round him";
  run_for(Game::Systems::k_forest_reveal_after_strike + 0.5);
  EXPECT_TRUE(cover_of(archer)->concealed) << "and melts back into the trees after";
}

TEST_F(ForestCoverTest, TreesStopAShareOfTheArrowsLoosedIntoTheWood) {
  auto damage_taken = [this](int standoff_from_wood_centre) {
    const EntityID target =
        spawn(Game::Units::SpawnType::Builder,
              world_of(k_wood_x + standoff_from_wood_centre, k_wood_z),
              k_hider);
    const EntityID shooter =
        spawn(Game::Units::SpawnType::Archer,
              world_of(k_wood_x + standoff_from_wood_centre + 4, k_wood_z),
              k_watcher);
    auto* unit = m_session->world()
                     .get_entity(target)
                     ->get_component<Engine::Core::UnitComponent>();
    const int before = unit->health;
    run_for(8.0);
    const int taken = before - unit->health;
    m_session->world().destroy_entity(target);
    m_session->world().destroy_entity(shooter);
    run_for(0.5);
    return taken;
  };

  const int in_the_open = damage_taken(14);
  const int among_the_trees = damage_taken(0);
  ASSERT_GT(in_the_open, 0) << "the archer never shot, so this proves nothing";
  EXPECT_LT(among_the_trees, in_the_open * 3 / 4)
      << "open ground took " << in_the_open << ", the wood " << among_the_trees;
}
