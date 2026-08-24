#include "world_restore.h"

#include "../core/ambient_session.h"
#include "../core/component.h"
#include "../core/world.h"
#include "../systems/building_collision_registry.h"
#include "../systems/global_stats_registry.h"
#include "../systems/nav_grid.h"
#include "../systems/owner_registry.h"
#include "../systems/pathfinding.h"
#include "../systems/troop_count_registry.h"
#include "../systems/wall_network_service.h"
#include "../units/troop_type.h"

namespace Game::Persistence {

auto rebuild_registries_after_load(Engine::Core::World* world,
                                   int local_owner_id) -> WorldRestoreResult {
  WorldRestoreResult result;
  if (world == nullptr) {
    return result;
  }

  const auto& services = Game::Session::services_for(*world);
  auto& owner_registry = *services.owners;

  auto& troops = *services.troop_counts;
  troops.rebuild_from_world(*world);

  auto& stats_registry = *services.stats;
  stats_registry.rebuild_from_world(*world);

  const auto& all_owners = owner_registry.get_all_owners();
  for (const auto& owner : all_owners) {
    if (owner.type == Game::Systems::OwnerType::Player ||
        owner.type == Game::Systems::OwnerType::AI) {
      stats_registry.mark_game_start(owner.owner_id);
    }
  }

  rebuild_building_collisions(world);

  for (auto [entity_id, unit] : world->view<Engine::Core::UnitComponent>()) {
    if (unit.owner_id == local_owner_id) {
      result.player_unit_id = entity_id;
      break;
    }
  }

  return result;
}

void rebuild_building_collisions(Engine::Core::World* world) {
  if (world == nullptr) {
    return;
  }

  auto& registry = *Game::Session::services_for(*world).building_collision;
  registry.clear();

  for (auto [entity_id, building, unit, transform] :
       world->view<Engine::Core::BuildingComponent,
                   Engine::Core::UnitComponent,
                   Engine::Core::TransformComponent>()) {
    (void)building;
    if (unit.health <= 0 ||
        world->has<Engine::Core::PendingRemovalComponent>(entity_id)) {

      continue;
    }

    registry.register_building(entity_id,
                               Game::Units::spawn_typeToString(unit.spawn_type),
                               transform.position.x,
                               transform.position.z,
                               unit.owner_id,
                               transform.rotation.y);
  }

  Game::Systems::WallNetworkService::refresh_world(*world);

  if (auto* pathfinder = Game::Systems::NavGrid::get_pathfinder()) {
    pathfinder->mark_navigation_grid_dirty();
  }
}

} // namespace Game::Persistence
