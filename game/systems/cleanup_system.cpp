#include "cleanup_system.h"
#include "../core/component.h"
#include "../core/world.h"
#include <vector>

namespace Game::Systems {

void CleanupSystem::update(Engine::Core::World *world, float deltaTime) {
  removeDeadEntities(world);
}

void CleanupSystem::removeDeadEntities(Engine::Core::World *world) {
  std::vector<Engine::Core::EntityID> entitiesToRemove;

  auto entities =
      world->getEntitiesWith<Engine::Core::PendingRemovalComponent>();

  for (auto entity : entities) {
    entitiesToRemove.push_back(entity->getId());
  }

  for (auto entityId : entitiesToRemove) {
    world->destroyEntity(entityId);
  }
}

} // namespace Game::Systems
