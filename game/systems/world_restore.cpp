#include "world_restore.h"

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

  auto& owner_registry = Game::Systems::OwnerRegistry::instance();

  auto& troops = Game::Systems::TroopCountRegistry::instance();
  troops.rebuild_from_world(*world);

  auto& stats_registry = Game::Systems::GlobalStatsRegistry::instance();
  stats_registry.rebuild_from_world(*world);

  const auto& all_owners = owner_registry.get_all_owners();
  for (const auto& owner : all_owners) {
    if (owner.type == Game::Systems::OwnerType::Player ||
        owner.type == Game::Systems::OwnerType::AI) {
      stats_registry.mark_game_start(owner.owner_id);
    }
  }

  rebuild_building_collisions(world);

  auto units = world->collect_entities_with<Engine::Core::UnitComponent>();
  for (auto* entity : units) {
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    if (unit->owner_id == local_owner_id) {
      result.player_unit_id = entity->get_id();
      break;
    }
  }

  return result;
}

void rebuild_building_collisions(Engine::Core::World* world) {
  auto& registry = Game::Systems::BuildingCollisionRegistry::instance();
  registry.clear();
  if (world == nullptr) {
    return;
  }

  auto buildings = world->collect_entities_with<Engine::Core::BuildingComponent>();
  for (auto* entity : buildings) {
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((transform == nullptr) || (unit == nullptr)) {
      continue;
    }

    if (unit->health <= 0 ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {

      continue;
    }

    registry.register_building(entity->get_id(),
                               Game::Units::spawn_typeToString(unit->spawn_type),
                               transform->position.x,
                               transform->position.z,
                               unit->owner_id);
  }

  Game::Systems::WallNetworkService::refresh_world(*world);

  if (auto* pathfinder = Game::Systems::NavGrid::get_pathfinder()) {
    pathfinder->mark_navigation_grid_dirty();
  }
}

} // namespace Game::Persistence
