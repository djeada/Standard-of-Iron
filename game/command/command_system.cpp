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

  auto* recorder = session->replay_recorder();
  auto* replay = session->replay_player();
  if (recorder != nullptr || replay != nullptr) {
    const auto digests = Game::Session::subsystem_digests(*session);
    if (recorder != nullptr) {
      recorder->record_digest(tick, digests);
    }
    if (replay != nullptr && !replay->check(tick, digests)) {
      const auto& divergence = *replay->divergence();
      if (divergence.tick == tick) {
        std::fprintf(stderr,
                     "replay: digest diverged at tick %llu in %s (recorded %llu, "
                     "observed %llu)\n",
                     static_cast<unsigned long long>(tick),
                     divergence.subsystem != nullptr ? divergence.subsystem
                                                     : "an unrecorded subsystem",
                     static_cast<unsigned long long>(divergence.recorded),
                     static_cast<unsigned long long>(divergence.observed));
      }
    }
  }
  if (replay != nullptr) {
    replay->feed(tick, session->commands());
  }
  session->commands().drain(*world, tick);
}

} // namespace Game::Command
