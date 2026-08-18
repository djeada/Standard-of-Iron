#pragma once

#include "core/world.h"
#include "game/systems/ai_system.h"

namespace TestSupport {

inline void quiesce_ai(Engine::Core::World& world) {
  if (auto* ai_system = world.get_system<Game::Systems::AISystem>()) {
    ai_system->shutdown_workers();
  }
}

} // namespace TestSupport
