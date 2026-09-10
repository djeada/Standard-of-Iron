#include <cmath>
#include <gtest/gtest.h>

#include "game/core/ambient_session.h"
#include "game/core/event_manager.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/map/map_transformer.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/gate_service.h"
#include "game/systems/nav_grid.h"
#include "game/systems/navigation_service.h"
#include "game/systems/pathfinding.h"
#include "game/units/factory.h"
#include "game/wildlife/bird_flock.h"
#include "game/wildlife/wildlife_config.h"

namespace {

using Game::Session::ScopedSession;
using Game::Session::SessionContext;
using Game::Systems::GateBlocker;
using Game::Systems::GateService;
using Game::Systems::NavGrid;
using Game::Systems::NavigationService;
using Game::Systems::Point;

auto grid_origin_x(int extent) -> float {
  return -((static_cast<float>(extent) * 0.5F) - 0.5F);
}

} // namespace

TEST(SessionServiceOwnershipTest, EachSessionOwnsItsOwnPathfinder) {
  SessionContext first;
  SessionContext second;

  EXPECT_NE(first.navigation().instance_id(), second.navigation().instance_id());
  EXPECT_EQ(first.navigation().pathfinder(), nullptr);

  {
    const ScopedSession scope(first);
    NavGrid::initialize(32, 32);
  }
  {
    const ScopedSession scope(second);
    NavGrid::initialize(96, 96);
  }

  ASSERT_NE(first.navigation().pathfinder(), nullptr);
  ASSERT_NE(second.navigation().pathfinder(), nullptr);
  EXPECT_NE(first.navigation().pathfinder(), second.navigation().pathfinder());
}

TEST(SessionServiceOwnershipTest, InitializingOneMatchDoesNotMoveAnotherMatchesGrid) {
  SessionContext first;
  SessionContext second;

  {
    const ScopedSession scope(first);
    NavGrid::initialize(32, 32);
    EXPECT_FLOAT_EQ(NavGrid::grid_to_world(Point{0, 0}).x(), grid_origin_x(32));
  }

  {
    const ScopedSession scope(second);
    NavGrid::initialize(96, 96);
    EXPECT_FLOAT_EQ(NavGrid::grid_to_world(Point{0, 0}).x(), grid_origin_x(96));
  }

  const ScopedSession scope(first);
  EXPECT_EQ(NavGrid::get_pathfinder(), first.navigation().pathfinder());
  EXPECT_FLOAT_EQ(NavGrid::grid_to_world(Point{0, 0}).x(), grid_origin_x(32));
}

TEST(SessionServiceOwnershipTest, GateBlockersBelongToTheSessionThatCarvedThem) {
  SessionContext first;
  SessionContext second;

  {
    const ScopedSession scope(first);
    NavGrid::initialize(32, 32);
  }
  {
    const ScopedSession scope(second);
    NavGrid::initialize(32, 32);
  }

  first.navigation().gate_blockers().push_back(GateBlocker{.min_x = -1.0F,
                                                           .max_x = 1.0F,
                                                           .min_z = -1.0F,
                                                           .max_z = 1.0F,
                                                           .owner_id = 1,
                                                           .entity_id = 7});

  {
    const ScopedSession scope(first);
    EXPECT_EQ(GateService::blockers().size(), 1U);
  }
  {
    const ScopedSession scope(second);
    EXPECT_TRUE(GateService::blockers().empty());
    GateService::clear_blockers();
  }
  {
    const ScopedSession scope(first);
    EXPECT_EQ(GateService::blockers().size(), 1U);
  }
}

TEST(SessionServiceOwnershipTest, ResettingASessionReleasesItsNavigation) {
  SessionContext session;
  {
    const ScopedSession scope(session);
    NavGrid::initialize(48, 48);
  }
  ASSERT_NE(session.navigation().pathfinder(), nullptr);

  session.reset();

  EXPECT_EQ(session.navigation().pathfinder(), nullptr);
  EXPECT_TRUE(session.navigation().gate_blockers().empty());
}

TEST(SessionServiceOwnershipTest, ArmyFormationsBelongToTheirOwnMatch) {
  SessionContext first;
  SessionContext second;

  const auto group = [](SessionContext& session) {
    const ScopedSession scope(session);
    return Game::Formation::ArmyFormationRegistry::instance().create_group(
        Game::Formation::k_neutral_doctrine,
        Game::Formation::ArmyFormationIntent::Line,
        {1, 2, 3});
  };

  const auto first_group = group(first);
  const auto second_group = group(second);

  EXPECT_NE(first.army_formations().find(first_group), nullptr);
  EXPECT_EQ(first.army_formations().find(second_group + 1000), nullptr);
  EXPECT_NE(second.army_formations().find(second_group), nullptr);

  {
    const ScopedSession scope(first);
    EXPECT_EQ(Game::Formation::ArmyFormationRegistry::instance().group_of(1),
              first_group);
  }

  second.army_formations().clear();

  {
    const ScopedSession scope(first);
    EXPECT_EQ(Game::Formation::ArmyFormationRegistry::instance().group_of(1),
              first_group);
  }
}

TEST(SessionServiceOwnershipTest, AWorldWithNoSessionIsCountedAsAnUnboundLookup) {
  Engine::Core::World detached;

  Game::Session::reset_unbound_world_lookups();
  ASSERT_EQ(Game::Session::unbound_world_lookups(), 0U);

  SessionContext session;
  const ScopedSession scope(session);
  (void)Game::Session::services_for(detached);

  EXPECT_EQ(Game::Session::unbound_world_lookups(), 1U);
  EXPECT_NE(Game::Session::services_for_or_null(session.world()), nullptr);
  EXPECT_EQ(Game::Session::services_for_or_null(detached), nullptr);

  Game::Session::reset_unbound_world_lookups();
}

TEST(SessionServiceOwnershipTest, TwoSessionsDoNotOverhearEachOther) {
  SessionContext first;
  SessionContext second;

  int heard_by_first = 0;
  int heard_by_second = 0;

  const auto listen = [](SessionContext& session, int& counter) {
    const ScopedSession scope(session);
    return Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent>(
        [&counter](const Engine::Core::UnitDiedEvent&) { ++counter; });
  };

  auto first_ear = listen(first, heard_by_first);
  auto second_ear = listen(second, heard_by_second);

  {
    const ScopedSession scope(first);
    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitDiedEvent(7, 1, Game::Units::SpawnType::Spearman));
  }

  EXPECT_EQ(heard_by_first, 1);
  EXPECT_EQ(heard_by_second, 0)
      << "the second match must not be told about a death in the first";

  {
    const ScopedSession scope(second);
    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitDiedEvent(9, 2, Game::Units::SpawnType::Spearman));
  }

  EXPECT_EQ(heard_by_first, 1);
  EXPECT_EQ(heard_by_second, 1);
}

TEST(SessionServiceOwnershipTest, CodeWithNoSessionStillHasABusToTalkOn) {
  Engine::Core::EventManager::process_bus().clear_all_subscriptions();

  int heard = 0;
  const Engine::Core::ScopedEventSubscription<Engine::Core::UnitDiedEvent> ear(
      [&heard](const Engine::Core::UnitDiedEvent&) { ++heard; });

  Engine::Core::EventManager::instance().publish(
      Engine::Core::UnitDiedEvent(1, 1, Game::Units::SpawnType::Spearman));
  EXPECT_EQ(heard, 1);

  SessionContext session;
  {
    const ScopedSession scope(session);
    Engine::Core::EventManager::instance().publish(
        Engine::Core::UnitDiedEvent(2, 1, Game::Units::SpawnType::Spearman));
  }
  EXPECT_EQ(heard, 1) << "a match must not reach a listener that never joined one";
}

TEST(SessionServiceOwnershipTest, MatchSetupDoesNotFollowThePlayerIntoTheNextMatch) {
  Game::Map::MapTransformer::setFactoryRegistry(nullptr);

  SessionContext first;
  SessionContext second;

  {
    const ScopedSession scope(first);
    Game::Map::MapTransformer::setFactoryRegistry(
        std::make_shared<Game::Units::UnitFactoryRegistry>());
    EXPECT_NE(Game::Map::MapTransformer::get_factory_registry(), nullptr);
  }

  {
    const ScopedSession scope(second);
    EXPECT_EQ(Game::Map::MapTransformer::get_factory_registry(), nullptr)
        << "the second match was handed the first match's unit factories";
  }

  EXPECT_NE(first.units(), nullptr);
  EXPECT_EQ(second.units(), nullptr);
}

TEST(SessionServiceOwnershipTest, EachMatchKeepsItsOwnSky) {
  SessionContext first;
  SessionContext second;

  Game::Wildlife::SpeciesConfig config = Game::Wildlife::default_bird_config();
  config.group_count = 1;
  config.group_size_min = 4;
  config.group_size_max = 4;
  config.spawn_areas = {{0.0F, 0.0F, 4.0F}};
  config.flyover_interval_min = 0.0F;
  config.flyover_interval_max = 0.0F;

  {
    const ScopedSession scope(first);
    Game::Wildlife::BirdFlockManager::instance().configure(config, 5U, 40.0F, 80.0F);
  }

  EXPECT_EQ(first.birds().birds().size(), 4U);
  EXPECT_TRUE(second.birds().birds().empty())
      << "a match that never authored birds inherited another match's flock";

  {
    const ScopedSession scope(second);
    EXPECT_TRUE(Game::Wildlife::BirdFlockManager::instance().birds().empty());
  }
}

TEST(SessionServiceOwnershipDeathTest, StrictBindingRefusesAWorldWithNoSession) {
  GTEST_FLAG_SET(death_test_style, "threadsafe");
  EXPECT_DEATH(
      {
        SessionContext session;
        const ScopedSession scope(session);
        Game::Session::set_strict_world_binding(true);
        Engine::Core::World detached;
        (void)Game::Session::services_for(detached);
      },
      "strict world binding");
  EXPECT_FALSE(Game::Session::strict_world_binding())
      << "the switch must not leak out of the death test's child process";
}
