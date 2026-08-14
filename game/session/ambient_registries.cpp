

#include "../map/terrain_service.h"
#include "../map/visibility_service.h"
#include "../systems/building_collision_registry.h"
#include "../systems/global_stats_registry.h"
#include "../systems/nation_registry.h"
#include "../systems/owner_registry.h"
#include "../systems/player_resource_registry.h"
#include "../systems/troop_count_registry.h"
#include "session_context.h"

namespace Game::Map {

auto TerrainService::instance() -> TerrainService& {
  return Game::Session::SessionContext::active().terrain();
}

auto VisibilityService::instance() -> VisibilityService& {
  return Game::Session::SessionContext::active().visibility();
}

} // namespace Game::Map

namespace Game::Systems {

auto BuildingCollisionRegistry::instance() -> BuildingCollisionRegistry& {
  return Game::Session::SessionContext::active().building_collision();
}

auto GlobalStatsRegistry::instance() -> GlobalStatsRegistry& {
  return Game::Session::SessionContext::active().stats();
}

auto NationRegistry::instance() -> NationRegistry& {
  return Game::Session::SessionContext::active().nations();
}

auto OwnerRegistry::instance() -> OwnerRegistry& {
  return Game::Session::SessionContext::active().owners();
}

auto PlayerResourceRegistry::instance() -> PlayerResourceRegistry& {
  return Game::Session::SessionContext::active().economy();
}

auto TroopCountRegistry::instance() -> TroopCountRegistry& {
  return Game::Session::SessionContext::active().troop_counts();
}

} // namespace Game::Systems
