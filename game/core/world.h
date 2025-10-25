#pragma once

#include "entity.h"
#include "system.h"
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace Engine::Core {

class World {
public:
  World();
  ~World();

  World(const World &) = delete;
  World(World &&) = delete;
  World &operator=(const World &) = delete;
  World &operator=(World &&) = delete;

  Entity *createEntity();
  Entity *createEntityWithId(EntityID entityId);
  void destroyEntity(EntityID entityId);
  Entity *getEntity(EntityID entityId);
  void clear();

  void addSystem(std::unique_ptr<System> system);
  void update(float deltaTime);

  std::vector<std::unique_ptr<System>> &systems() { return m_systems; }

  template <typename T> T *getSystem() {
    for (auto &system : m_systems) {
      if (auto *ptr = dynamic_cast<T *>(system.get())) {
        return ptr;
      }
    }
    return nullptr;
  }

  template <typename T> std::vector<Entity *> getEntitiesWith() {
    const std::lock_guard<std::recursive_mutex> lock(m_entityMutex);
    std::vector<Entity *> result;
    for (auto &[entityId, entity] : m_entities) {
      if (entity->template hasComponent<T>()) {
        result.push_back(entity.get());
      }
    }
    return result;
  }

  std::vector<Entity *> getUnitsOwnedBy(int ownerId) const;
  std::vector<Entity *> getUnitsNotOwnedBy(int ownerId) const;
  std::vector<Entity *> getAlliedUnits(int ownerId) const;
  std::vector<Entity *> getEnemyUnits(int ownerId) const;
  int countTroopsForPlayer(int ownerId) const;

  const std::unordered_map<EntityID, std::unique_ptr<Entity>> &
  getEntities() const {
    return m_entities;
  }

  EntityID getNextEntityId() const;
  void setNextEntityId(EntityID nextId);

  std::recursive_mutex &getEntityMutex() { return m_entityMutex; }

private:
  EntityID m_nextEntityId = 1;
  std::unordered_map<EntityID, std::unique_ptr<Entity>> m_entities;
  std::vector<std::unique_ptr<System>> m_systems;
  mutable std::recursive_mutex m_entityMutex;
};

} // namespace Engine::Core
