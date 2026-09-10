#include "event_manager.h"

#include "ambient_session.h"

namespace Engine::Core {

auto EventManager::process_bus() -> EventManager& {
  static auto* bus = new EventManager();
  return *bus;
}

auto EventManager::instance() -> EventManager& {
  if (const auto* services = Game::Session::ambient_services_or_null()) {
    if (services->events != nullptr) {
      return *services->events;
    }
  }
  return process_bus();
}

} // namespace Engine::Core
