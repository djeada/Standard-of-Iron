#include "command_system.h"

#include <cstdio>

#include "../core/world.h"
#include "../session/session_context.h"
#include "../session/simulation_clock.h"
#include "../session/world_digest.h"
#include "command_queue.h"
#include "replay.h"

namespace Game::Command {

void CommandSystem::update(Engine::Core::World* world, float) {
  if (world == nullptr) {
    return;
  }

  auto* session = Game::Session::SessionContext::for_world(*world);
  if (session == nullptr) {
    return;
  }

  const auto tick = session->clock().tick();

  if (auto* recorder = session->replay_recorder()) {
    recorder->record_digest(tick, Game::Session::session_digest(*session));
  }
  if (auto* replay = session->replay_player()) {
    if (!replay->check(tick, Game::Session::session_digest(*session))) {
      const auto& divergence = *replay->divergence();
      if (divergence.tick == tick) {
        std::fprintf(
            stderr,
            "replay: digest diverged at tick %llu (recorded %llu, observed %llu)\n",
            static_cast<unsigned long long>(tick),
            static_cast<unsigned long long>(divergence.recorded),
            static_cast<unsigned long long>(divergence.observed));
      }
    }
    replay->feed(tick, session->commands());
  }
  session->commands().drain(*world, tick);
}

} // namespace Game::Command
