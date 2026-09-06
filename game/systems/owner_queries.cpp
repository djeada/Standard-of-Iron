#include "owner_queries.h"

#include "../core/ambient_session.h"
#include "../core/world.h"
#include "troop_count_registry.h"

namespace Game::Systems {

auto troop_count_for(const Engine::Core::World& world, int owner_id) -> int {

  auto& counts = *Game::Session::services_for(world).troop_counts;
  counts.refresh_from_world(world);
  return counts.get_troop_count(owner_id);
}

} // namespace Game::Systems
