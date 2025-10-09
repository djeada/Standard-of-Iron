#pragma once

#include "entity.h"
#include "system.h"
#include <memory>
#include <unordered_map>
#include <vector>

namespace Engine::Core {

class World {
public:
  World();
  ~World();

  Entity *createEntity();
  void destroyEntity(EntityID id);
  Entity *getEntity(EntityID id);
  void clear();

  void addSystem(std::unique_ptr<System> system);
  void update(float deltaTime);

  std::vector<std::unique_ptr<System>> &systems() { return m_systems; }

  template <typename T> std::vector<Entity *> getEntitiesWith() {
    std::vector<Entity *> result;
    for (auto &[id, entity] : m_entities) {
      if (entity->hasComponent<T>()) {
        result.push_back(entity.get());
      }
    }
    return result;
  }

  std::vector<Entity *> getUnitsOwnedBy(int ownerId);
  std::vector<Entity *> getUnitsNotOwnedBy(int ownerId);

private:
  EntityID m_nextEntityId = 1;
  std::unordered_map<EntityID, std::unique_ptr<Entity>> m_entities;
  std::vector<std::unique_ptr<System>> m_systems;
};

} // namespace Engine::Core
