#include "player_resource_registry.h"

#include "../session/session_context.h"

namespace Game::Systems {

auto PlayerResourceRegistry::instance() -> PlayerResourceRegistry& {
  return Game::Session::SessionContext::active().economy();
}

} // namespace Game::Systems
