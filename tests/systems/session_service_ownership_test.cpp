#include <cmath>
#include <gtest/gtest.h>

#include "game/core/ambient_session.h"
#include "game/core/world.h"
#include "game/formation/army_formation_registry.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/gate_service.h"
#include "game/systems/nav_grid.h"
#include "game/systems/navigation_service.h"
#include "game/systems/pathfinding.h"

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
