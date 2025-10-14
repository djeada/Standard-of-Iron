#include "world.h"
#include "../systems/owner_registry.h"
#include "../systems/troop_count_registry.h"
#include "component.h"

namespace Engine::Core {

World::World() = default;
World::~World() = default;

Entity *World::createEntity() {
  EntityID id = m_nextEntityId++;
  auto entity = std::make_unique<Entity>(id);
  auto ptr = entity.get();
  m_entities[id] = std::move(entity);
  return ptr;
}

void World::destroyEntity(EntityID id) { m_entities.erase(id); }

void World::clear() {

  m_entities.clear();
  m_nextEntityId = 1;
}

Entity *World::getEntity(EntityID id) {
  auto it = m_entities.find(id);
  return it != m_entities.end() ? it->second.get() : nullptr;
}

void World::addSystem(std::unique_ptr<System> system) {
  m_systems.push_back(std::move(system));
}

void World::update(float deltaTime) {
  for (auto &system : m_systems) {
    system->update(this, deltaTime);
  }
}

std::vector<Entity *> World::getUnitsOwnedBy(int ownerId) const {
  std::vector<Entity *> result;
  result.reserve(m_entities.size());
  for (auto &[id, entity] : m_entities) {
    auto *unit = entity->getComponent<UnitComponent>();
    if (!unit)
      continue;
    if (unit->ownerId == ownerId) {
      result.push_back(entity.get());
    }
  }
  return result;
}

std::vector<Entity *> World::getUnitsNotOwnedBy(int ownerId) const {
  std::vector<Entity *> result;
  result.reserve(m_entities.size());
  for (auto &[id, entity] : m_entities) {
    auto *unit = entity->getComponent<UnitComponent>();
    if (!unit)
      continue;
    if (unit->ownerId != ownerId) {
      result.push_back(entity.get());
    }
  }
  return result;
}

std::vector<Entity *> World::getAlliedUnits(int ownerId) const {
  std::vector<Entity *> result;
  result.reserve(m_entities.size());
  auto &ownerRegistry = Game::Systems::OwnerRegistry::instance();

  for (auto &[id, entity] : m_entities) {
    auto *unit = entity->getComponent<UnitComponent>();
    if (!unit)
      continue;

    if (unit->ownerId == ownerId ||
        ownerRegistry.areAllies(ownerId, unit->ownerId)) {
      result.push_back(entity.get());
    }
  }
  return result;
}

std::vector<Entity *> World::getEnemyUnits(int ownerId) const {
  std::vector<Entity *> result;
  result.reserve(m_entities.size());
  auto &ownerRegistry = Game::Systems::OwnerRegistry::instance();

  for (auto &[id, entity] : m_entities) {
    auto *unit = entity->getComponent<UnitComponent>();
    if (!unit)
      continue;

    if (ownerRegistry.areEnemies(ownerId, unit->ownerId)) {
      result.push_back(entity.get());
    }
  }
  return result;
}

int World::countTroopsForPlayer(int ownerId) const {
  return Game::Systems::TroopCountRegistry::instance().getTroopCount(ownerId);
}

} // namespace Engine::Core
