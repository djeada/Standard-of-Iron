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
  auto operator=(const World &) -> World & = delete;
  auto operator=(World &&) -> World & = delete;

  auto createEntity() -> Entity *;
  auto createEntityWithId(EntityID entity_id) -> Entity *;
  void destroyEntity(EntityID entity_id);
  auto getEntity(EntityID entity_id) -> Entity *;
  void clear();

  void addSystem(std::unique_ptr<System> system);
  void update(float delta_time);

  auto systems() -> std::vector<std::unique_ptr<System>> & { return m_systems; }

  template <typename T> auto getSystem() -> T * {
    for (auto &system : m_systems) {
      if (auto *ptr = dynamic_cast<T *>(system.get())) {
        return ptr;
      }
    }
    return nullptr;
  }

  template <typename T> auto getEntitiesWith() -> std::vector<Entity *> {
    const std::lock_guard<std::recursive_mutex> lock(m_entityMutex);
    std::vector<Entity *> result;
    for (auto &[entity_id, entity] : m_entities) {
      if (entity->template hasComponent<T>()) {
        result.push_back(entity.get());
      }
    }
    return result;
  }

  auto getUnitsOwnedBy(int owner_id) const -> std::vector<Entity *>;
  auto getUnitsNotOwnedBy(int owner_id) const -> std::vector<Entity *>;
  auto getAlliedUnits(int owner_id) const -> std::vector<Entity *>;
  auto getEnemyUnits(int owner_id) const -> std::vector<Entity *>;
  static auto countTroopsForPlayer(int owner_id) -> int;

  auto getEntities() const
      -> const std::unordered_map<EntityID, std::unique_ptr<Entity>> & {
    return m_entities;
  }

  auto getNextEntityId() const -> EntityID;
  void setNextEntityId(EntityID next_id);

  auto getEntityMutex() -> std::recursive_mutex & { return m_entityMutex; }

private:
  EntityID m_nextEntityId = 1;
  std::unordered_map<EntityID, std::unique_ptr<Entity>> m_entities;
  std::vector<std::unique_ptr<System>> m_systems;
  mutable std::recursive_mutex m_entityMutex;
};

} // namespace Engine::Core
