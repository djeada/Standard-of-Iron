#include "player_resource_registry.h"

#include "../core/ambient_session.h"

namespace Game::Systems {

auto PlayerResourceRegistry::instance() -> PlayerResourceRegistry& {
  return *Game::Session::ambient_services().economy;
}
} // namespace Game::Systems
