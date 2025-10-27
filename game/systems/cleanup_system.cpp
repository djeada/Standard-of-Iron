#include "cleanup_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include "core/entity.h"
#include <vector>

namespace Game::Systems {

void CleanupSystem::update(Engine::Core::World *world, float deltaTime) {
  removeDeadEntities(world);
}

void CleanupSystem::removeDeadEntities(Engine::Core::World *world) {
  std::vector<Engine::Core::EntityID> entities_to_remove;

  auto entities =
      world->getEntitiesWith<Engine::Core::PendingRemovalComponent>();

  entities_to_remove.reserve(entities.size());
  for (auto *entity : entities) {
    entities_to_remove.push_back(entity->getId());
  }

  for (auto entity_id : entities_to_remove) {
    world->destroyEntity(entity_id);
  }
}

} // namespace Game::Systems
