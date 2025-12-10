#include "world.h"
#include "../systems/owner_registry.h"
#include "../systems/troop_count_registry.h"
#include "component.h"
#include "core/entity.h"
#include "core/system.h"
#include <algorithm>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace Engine::Core {

World::World() = default;
World::~World() = default;

auto World::createEntity() -> Entity * {
  const std::lock_guard<std::recursive_mutex> lock(m_entityMutex);
  EntityID const id = m_nextEntityId++;
  auto entity = std::make_unique<Entity>(id);
  auto *ptr = entity.get();
  m_entities[id] = std::move(entity);
  return ptr;
}

auto World::createEntityWithId(EntityID entity_id) -> Entity * {
  const std::lock_guard<std::recursive_mutex> lock(m_entityMutex);
  if (entity_id == NULL_ENTITY) {
    return nullptr;
  }

  auto entity = std::make_unique<Entity>(entity_id);
  auto *ptr = entity.get();
  m_entities[entity_id] = std::move(entity);

  if (entity_id >= m_nextEntityId) {
    m_nextEntityId = entity_id + 1;
  }

  return ptr;
}

void World::destroyEntity(EntityID entity_id) {
  const std::lock_guard<std::recursive_mutex> lock(m_entityMutex);
  m_entities.erase(entity_id);
}

void World::clear() {
  const std::lock_guard<std::recursive_mutex> lock(m_entityMutex);
  m_entities.clear();
  m_nextEntityId = 1;
}

auto World::getEntity(EntityID entity_id) -> Entity * {
  const std::lock_guard<std::recursive_mutex> lock(m_entityMutex);
  auto it = m_entities.find(entity_id);
  return it != m_entities.end() ? it->second.get() : nullptr;
}

void World::addSystem(std::unique_ptr<System> system) {
  m_systems.push_back(std::move(system));
}

void World::update(float delta_time) {
  for (auto &system : m_systems) {
    system->update(this, delta_time);
  }
}

auto World::getUnitsOwnedBy(int owner_id) const -> std::vector<Entity *> {
  const std::lock_guard<std::recursive_mutex> lock(m_entityMutex);
  std::vector<Entity *> result;
  result.reserve(m_entities.size());
  for (const auto &[entity_id, entity] : m_entities) {
    auto *unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    if (unit->owner_id == owner_id) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::getUnitsNotOwnedBy(int owner_id) const -> std::vector<Entity *> {
  const std::lock_guard<std::recursive_mutex> lock(m_entityMutex);
  std::vector<Entity *> result;
  result.reserve(m_entities.size());
  for (const auto &[entity_id, entity] : m_entities) {
    auto *unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    if (unit->owner_id != owner_id) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::getAlliedUnits(int owner_id) const -> std::vector<Entity *> {
  const std::lock_guard<std::recursive_mutex> lock(m_entityMutex);
  std::vector<Entity *> result;
  result.reserve(m_entities.size());
  auto &owner_registry = Game::Systems::OwnerRegistry::instance();

  for (const auto &[entity_id, entity] : m_entities) {
    auto *unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->owner_id == owner_id ||
        owner_registry.areAllies(owner_id, unit->owner_id)) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::getEnemyUnits(int owner_id) const -> std::vector<Entity *> {
  const std::lock_guard<std::recursive_mutex> lock(m_entityMutex);
  std::vector<Entity *> result;
  result.reserve(m_entities.size());
  auto &owner_registry = Game::Systems::OwnerRegistry::instance();

  for (const auto &[entity_id, entity] : m_entities) {
    auto *unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (owner_registry.areEnemies(owner_id, unit->owner_id)) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::countTroopsForPlayer(int owner_id) -> int {
  return Game::Systems::TroopCountRegistry::instance().getTroopCount(owner_id);
}

auto World::getNextEntityId() const -> EntityID {
  const std::lock_guard<std::recursive_mutex> lock(m_entityMutex);
  return m_nextEntityId;
}

void World::setNextEntityId(EntityID next_id) {
  const std::lock_guard<std::recursive_mutex> lock(m_entityMutex);
  m_nextEntityId = std::max(next_id, m_nextEntityId);
}

} // namespace Engine::Core
