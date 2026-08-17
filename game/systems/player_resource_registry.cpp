#include "player_resource_registry.h"

#include "../core/ambient_session.h"

namespace Game::Systems {

auto PlayerResourceRegistry::instance() -> PlayerResourceRegistry& {
  return *Game::Session::ambient_services().economy;
}

void grant_resources(int owner_id, const ResourceAmounts& amounts) {
  auto& resources = PlayerResourceRegistry::instance();
  for (const auto resource_type : k_all_resource_types) {
    if (const int amount = amounts.get(resource_type); amount > 0) {
      resources.add(owner_id, resource_type, amount);
    }
  }
}
} // namespace Game::Systems
