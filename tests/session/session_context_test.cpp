#include <gtest/gtest.h>
#include <thread>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/session/deterministic_rng.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/global_stats_registry.h"
#include "game/systems/owner_registry.h"
#include "game/systems/player_resource_registry.h"
#include "game/systems/resource_types.h"

namespace {

using Game::Session::ScopedSession;
using Game::Session::ScopedThreadSession;
using Game::Session::SessionContext;

TEST(SessionContextTest, TwoMatchesCoexistWithoutSharingState) {
  SessionContext first;
  SessionContext second;

  {
    const ScopedSession scope(first);
    Game::Systems::OwnerRegistry::instance().register_owner_with_id(
        1, Game::Systems::OwnerType::Player, "first-player");
    Game::Systems::PlayerResourceRegistry::instance().set(
        1, Game::Systems::ResourceType::Gold, 500);
  }

  {
    const ScopedSession scope(second);
    EXPECT_TRUE(Game::Systems::OwnerRegistry::instance().get_all_owners().empty());
    EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                  1, Game::Systems::ResourceType::Gold),
              0);

    Game::Systems::PlayerResourceRegistry::instance().set(
        1, Game::Systems::ResourceType::Gold, 100);
  }

  const ScopedSession scope(first);
  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Gold),
            500);
  EXPECT_EQ(Game::Systems::OwnerRegistry::instance().get_all_owners().size(), 1U);
}

TEST(SessionContextTest, WorldsAreSessionScoped) {
  SessionContext first;
  SessionContext second;

  auto* entity = first.world().create_entity();
  ASSERT_NE(entity, nullptr);
  entity->add_component<Engine::Core::TransformComponent>();

  EXPECT_EQ(first.world().get_entities_with<Engine::Core::TransformComponent>().size(),
            1U);
  EXPECT_TRUE(
      second.world().get_entities_with<Engine::Core::TransformComponent>().empty());
}

TEST(SessionContextTest, WorldResolvesBackToItsOwningSession) {
  SessionContext first;
  SessionContext second;

  EXPECT_EQ(SessionContext::for_world(first.world()), &first);
  EXPECT_EQ(SessionContext::for_world(second.world()), &second);

  Engine::Core::World orphan;
  EXPECT_EQ(SessionContext::for_world(orphan), nullptr);
}

TEST(SessionContextTest, ScopedSessionRestoresPreviousBinding) {
  SessionContext outer;
  SessionContext inner;

  const ScopedSession outer_scope(outer);
  EXPECT_EQ(SessionContext::active_or_null(), &outer);
  {
    const ScopedSession inner_scope(inner);
    EXPECT_EQ(SessionContext::active_or_null(), &inner);
  }
  EXPECT_EQ(SessionContext::active_or_null(), &outer);
}

TEST(SessionContextTest, ThreadBindingDoesNotDisturbTheMainSession) {
  SessionContext match;
  SessionContext hypothetical;

  const ScopedSession scope(match);
  Game::Systems::PlayerResourceRegistry::instance().set(
      1, Game::Systems::ResourceType::Gold, 42);

  std::thread worker([&hypothetical] {
    const ScopedThreadSession thread_scope(hypothetical);

    EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                  1, Game::Systems::ResourceType::Gold),
              0);
    Game::Systems::PlayerResourceRegistry::instance().set(
        1, Game::Systems::ResourceType::Gold, 999);
  });
  worker.join();

  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Gold),
            42);
}

TEST(SessionContextTest, ResetClearsEveryPerMatchStore) {
  SessionContext session;
  const ScopedSession scope(session);

  session.world().create_entity();
  Game::Systems::OwnerRegistry::instance().register_owner_with_id(
      1, Game::Systems::OwnerType::Player, "p1");
  Game::Systems::PlayerResourceRegistry::instance().set(
      1, Game::Systems::ResourceType::Gold, 7);
  session.clock().advance(1.0);
  while (session.clock().consume_tick()) {
  }
  session.rng().next_u64();

  session.reset();

  EXPECT_EQ(session.world().entity_count(), 0U);
  EXPECT_TRUE(Game::Systems::OwnerRegistry::instance().get_all_owners().empty());
  EXPECT_EQ(Game::Systems::PlayerResourceRegistry::instance().get(
                1, Game::Systems::ResourceType::Gold),
            0);
  EXPECT_EQ(session.clock().tick(), 0U);
  EXPECT_EQ(session.rng().draw_count(), 0U);
}

TEST(SimulationClockTest, ConvertsVariableFramesIntoWholeFixedTicks) {
  Game::Session::SimulationClock clock(1.0 / 60.0);

  EXPECT_EQ(clock.advance(1.0 / 60.0), 1);
  EXPECT_TRUE(clock.consume_tick());
  EXPECT_FALSE(clock.consume_tick());
  EXPECT_EQ(clock.tick(), 1U);

  EXPECT_EQ(clock.advance(1.0 / 240.0), 0);
  EXPECT_EQ(clock.advance(1.0 / 240.0), 0);
  EXPECT_EQ(clock.advance(1.0 / 240.0), 0);
  EXPECT_EQ(clock.advance(1.0 / 240.0), 1);
}

TEST(SimulationClockTest, PausedAndScaledTimeDivergeFromWallClock) {
  Game::Session::SimulationClock clock(1.0 / 60.0);

  clock.set_paused(true);
  EXPECT_EQ(clock.advance(1.0), 0);
  EXPECT_DOUBLE_EQ(clock.now_seconds(), 0.0);

  clock.set_paused(false);
  clock.set_time_scale(2.0);
  EXPECT_EQ(clock.advance(1.0 / 60.0), 2);
}

TEST(SimulationClockTest, ClampsStallsInsteadOfCatchingUp) {
  Game::Session::SimulationClock clock(1.0 / 60.0);

  const int ticks = clock.advance(10.0);
  EXPECT_LE(
      ticks,
      static_cast<int>(Game::Session::SimulationClock::k_max_frame_seconds * 60.0) + 1);
}

TEST(DeterministicRngTest, SameSeedProducesSameStream) {
  Game::Session::DeterministicRng first(1234);
  Game::Session::DeterministicRng second(1234);

  for (int i = 0; i < 32; ++i) {
    EXPECT_EQ(first.next_u64(), second.next_u64());
  }
}

TEST(DeterministicRngTest, RestoreResumesTheStreamAtTheSavedPosition) {
  Game::Session::DeterministicRng live(99);
  for (int i = 0; i < 10; ++i) {
    live.next_u64();
  }
  const std::uint64_t expected = live.next_u64();

  Game::Session::DeterministicRng restored(0);
  restored.restore(99, 10);
  EXPECT_EQ(restored.next_u64(), expected);
}

TEST(SessionStatsTest, PlayTimeFollowsSimulationTimeNotWallClock) {
  SessionContext session;
  const ScopedSession scope(session);

  auto& stats = session.stats();
  stats.mark_game_start(1);

  for (int i = 0; i < 120; ++i) {
    session.clock().advance(1.0 / 60.0);
    while (session.clock().consume_tick()) {
    }
  }

  stats.mark_game_end(1);
  const auto* player = stats.get_stats(1);
  ASSERT_NE(player, nullptr);
  EXPECT_NEAR(player->play_time_sec, 2.0F, 0.02F);
}

TEST(SessionStatsTest, PausedTimeDoesNotCountAsPlayTime) {
  SessionContext session;
  const ScopedSession scope(session);

  auto& stats = session.stats();
  stats.mark_game_start(1);

  session.clock().set_paused(true);
  for (int i = 0; i < 120; ++i) {
    session.clock().advance(1.0 / 60.0);
    while (session.clock().consume_tick()) {
    }
  }

  stats.mark_game_end(1);
  const auto* player = stats.get_stats(1);
  ASSERT_NE(player, nullptr);
  EXPECT_FLOAT_EQ(player->play_time_sec, 0.0F);
}

TEST(SessionContextTest, TerrainIsSessionScoped) {
  SessionContext first;
  SessionContext second;

  Game::Map::MapDefinition map_def;
  map_def.grid.width = 8;
  map_def.grid.height = 8;
  first.terrain().initialize(map_def);

  EXPECT_TRUE(first.terrain().is_initialized());
  EXPECT_FALSE(second.terrain().is_initialized());
}

} // namespace
