#include <cstdint>
#include <gtest/gtest.h>
#include <string>
#include <vector>

#include "game/core/system_schedule.h"
#include "game/core/world.h"
#include "game/session/session_context.h"
#include "game/systems/runtime_system_registry.h"

namespace {

using Engine::Core::SystemPhase;
using Game::Session::ScopedSession;
using Game::Session::SessionContext;

TEST(RuntimeSystemPhaseOrderTest, PhasesNeverGoBackwardsThroughTheRegistry) {
  SessionContext session;
  const ScopedSession scope(session);
  Game::Systems::register_runtime_systems(session.world());

  const auto phases = session.world().system_phases();
  ASSERT_FALSE(phases.empty());
  ASSERT_EQ(phases.size(), session.world().systems().size());

  for (std::size_t i = 1; i < phases.size(); ++i) {
    EXPECT_LE(static_cast<int>(phases[i - 1]), static_cast<int>(phases[i]))
        << "system " << i << " runs in phase " << Engine::Core::phase_name(phases[i])
        << " after phase " << Engine::Core::phase_name(phases[i - 1]);
  }
}

TEST(RuntimeSystemPhaseOrderTest, TheMatchUsesEveryPhase) {
  SessionContext session;
  const ScopedSession scope(session);
  Game::Systems::register_runtime_systems(session.world());

  std::vector<bool> seen(static_cast<std::size_t>(SystemPhase::_Count), false);
  for (const SystemPhase phase : session.world().system_phases()) {
    seen[static_cast<std::size_t>(phase)] = true;
  }

  for (std::size_t raw = 0; raw < seen.size(); ++raw) {
    EXPECT_TRUE(seen[raw]) << "no system runs in phase "
                           << Engine::Core::phase_name(static_cast<SystemPhase>(raw));
  }
}

TEST(RuntimeSystemPhaseOrderTest, NoSystemHasDeclaredWhatItTouchesYet) {
  SessionContext session;
  const ScopedSession scope(session);
  Game::Systems::register_runtime_systems(session.world());

  std::size_t systems_in_phases = 0;
  std::size_t batches = 0;
  for (std::uint8_t raw = 0; raw < static_cast<std::uint8_t>(SystemPhase::_Count);
       ++raw) {
    const auto plan =
        session.world().plan_phase_schedule(static_cast<SystemPhase>(raw));
    batches += plan.size();
    for (const auto& batch : plan) {
      systems_in_phases += batch.size();
    }
  }

  EXPECT_EQ(systems_in_phases, session.world().systems().size());
  EXPECT_EQ(batches, systems_in_phases);
}

TEST(RuntimeSystemPhaseOrderTest, AnEmptyWorldTicksWithoutSystems) {
  Engine::Core::World world;
  world.update(0.016F);
  EXPECT_EQ(world.tick_id(), 1U);
}

} // namespace
