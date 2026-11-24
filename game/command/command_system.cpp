#include "command_system.h"

#include "../core/world.h"
#include "../session/session_context.h"
#include "../session/simulation_clock.h"
#include "command_queue.h"

namespace Game::Command {

void CommandSystem::update(Engine::Core::World* world, float) {
  if (world == nullptr) {
    return;
  }

  auto* session = Game::Session::SessionContext::for_world(*world);
  if (session == nullptr) {
    return;
  }

  session->commands().drain(*world, session->clock().tick());
}

} // namespace Game::Command
