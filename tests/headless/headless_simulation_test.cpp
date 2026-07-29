

#include <gtest/gtest.h>

#include "game/command/command.h"
#include "game/command/command_queue.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/session/deterministic_rng.h"
#include "game/session/session_context.h"
#include "game/session/simulation_clock.h"
#include "game/systems/owner_registry.h"
#include "game/systems/runtime_system_registry.h"

namespace {

using Engine::Core::EntityID;
using Game::Session::ScopedSession;
using Game::Session::SessionContext;

auto spawn(SessionContext& session, int owner_id, float x, float z) -> EntityID {
  auto* entity = session.world().create_entity();
  auto* transform = entity->add_component<Engine::Core::TransformComponent>();
  transform->position.x = x;
  transform->position.z = z;
  auto* unit = entity->add_component<Engine::Core::UnitComponent>(100, 100, 2.0F, 6.0F);
  unit->owner_id = owner_id;
  return entity->get_id();
}

void run_for(SessionContext& session, double seconds) {
  const double step = session.clock().tick_seconds();
  for (double elapsed = 0.0; elapsed < seconds; elapsed += step) {
    session.clock().advance(step);
    while (session.clock().consume_tick()) {
      session.world().update(static_cast<float>(step));
    }
  }
}

auto make_match() -> std::unique_ptr<SessionContext> {
  auto session = std::make_unique<SessionContext>();
  auto& owners = session->owners();
  owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "blue");
  owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "red");
  owners.set_owner_team(1, 1);
  owners.set_owner_team(2, 2);
  Game::Systems::register_runtime_systems(session->world());
  return session;
}

TEST(HeadlessSimulation, RunsWithoutAnyRenderingSetUp) {
  auto session = make_match();
  const ScopedSession scope(*session);

  const EntityID mine = spawn(*session, 1, 0.0F, 0.0F);
  spawn(*session, 2, 20.0F, 20.0F);

  run_for(*session, 1.0);

  EXPECT_GT(session->clock().tick(), 0U);
  EXPECT_TRUE(session->world().is_alive(mine));
}

TEST(HeadlessSimulation, OrdersReachTheSimulationThroughTheCommandSystem) {
  auto session = make_match();
  const ScopedSession scope(*session);

  const EntityID mine = spawn(*session, 1, 0.0F, 0.0F);

  Game::Command::submit(
      session->world(),
      Game::Command::Source::Script,
      1,
      Game::Command::Move{.units = {mine}, .targets = {QVector3D(6.0F, 0.0F, 0.0F)}});

  EXPECT_EQ(session->commands().pending(), 1U);

  run_for(*session, 0.05);

  EXPECT_EQ(session->commands().pending(), 0U);
  EXPECT_EQ(session->commands().accepted_count(), 1U);

  auto* entity = session->world().get_entity(mine);
  ASSERT_NE(entity, nullptr);
  EXPECT_NE(entity->get_component<Engine::Core::MovementComponent>(), nullptr);
}

TEST(HeadlessSimulation, SameSeedAndOrdersProduceTheSameOutcome) {

  const auto run_once = [](std::uint64_t seed) {
    SessionContext::Config config;
    config.rng_seed = seed;
    auto session = std::make_unique<SessionContext>(config);
    auto& owners = session->owners();
    owners.register_owner_with_id(1, Game::Systems::OwnerType::Player, "blue");
    owners.register_owner_with_id(2, Game::Systems::OwnerType::AI, "red");
    owners.set_owner_team(1, 1);
    owners.set_owner_team(2, 2);
    Game::Systems::register_runtime_systems(session->world());

    const ScopedSession scope(*session);
    const EntityID mine = spawn(*session, 1, 0.0F, 0.0F);
    spawn(*session, 2, 4.0F, 0.0F);

    Game::Command::submit(
        session->world(),
        Game::Command::Source::Script,
        1,
        Game::Command::Move{.units = {mine}, .targets = {QVector3D(4.0F, 0.0F, 0.0F)}});
    run_for(*session, 2.0);

    auto* entity = session->world().get_entity(mine);
    const auto* transform =
        entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    struct Outcome {
      float x;
      float z;
      std::uint64_t tick;
    };
    return Outcome{transform != nullptr ? transform->position.x : 0.0F,
                   transform != nullptr ? transform->position.z : 0.0F,
                   session->clock().tick()};
  };

  const auto first = run_once(4242);
  const auto second = run_once(4242);

  EXPECT_FLOAT_EQ(first.x, second.x);
  EXPECT_FLOAT_EQ(first.z, second.z);
  EXPECT_EQ(first.tick, second.tick);
}

TEST(HeadlessSimulation, TwoMatchesRunSideBySideInOneProcess) {

  auto first = make_match();
  auto second = make_match();

  {
    const ScopedSession scope(*first);
    spawn(*first, 1, 0.0F, 0.0F);
    run_for(*first, 0.5);
  }
  {
    const ScopedSession scope(*second);
    spawn(*second, 1, 0.0F, 0.0F);
    spawn(*second, 1, 1.0F, 1.0F);
    run_for(*second, 0.25);
  }

  EXPECT_EQ(first->world().entity_count(), 1U);
  EXPECT_EQ(second->world().entity_count(), 2U);
  EXPECT_GT(first->clock().tick(), second->clock().tick());
}

} // namespace
