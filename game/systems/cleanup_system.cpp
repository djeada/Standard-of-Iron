#include "cleanup_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "core/entity.h"
#include <vector>

namespace Game::Systems {

void CleanupSystem::update(Engine::Core::World *world, float) {
  removeDeadEntities(world);
}

void CleanupSystem::remove_dead_entities(Engine::Core::World *world) {
  std::vector<Engine::Core::EntityID> entities_to_remove;

  auto entities =
      world->get_entities_with<Engine::Core::PendingRemovalComponent>();

  entities_to_remove.reserve(entities.size());
  for (auto *entity : entities) {
    entities_to_remove.push_back(entity->get_id());
  }

  for (auto entity_id : entities_to_remove) {
    world->destroy_entity(entity_id);
  }
}

} // namespace Game::Systems
