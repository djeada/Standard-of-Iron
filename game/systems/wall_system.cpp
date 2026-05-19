#include "wall_system.h"

#include "../core/component.h"
#include "../core/world.h"
#include "../units/spawn_type.h"
#include "building_collision_registry.h"

namespace Game::Systems {

void WallSystem::update(Engine::Core::World* world, float /*delta_time*/) {
  auto entities = world->get_entities_with<Engine::Core::UnitComponent>();

  for (auto* wall : entities) {
    if (wall->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto* wall_unit = wall->get_component<Engine::Core::UnitComponent>();
    if (wall_unit == nullptr) {
      continue;
    }

    if (wall_unit->spawn_type != Game::Units::SpawnType::WallSegment) {
      continue;
    }

    if (!wall->has_component<Engine::Core::BuildingComponent>()) {
      continue;
    }

    if (wall_unit->health <= 0) {
      BuildingCollisionRegistry::instance().unregister_building(wall->get_id());
      wall->add_component<Engine::Core::PendingRemovalComponent>();
    }
  }
}

} // namespace Game::Systems
