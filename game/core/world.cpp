#include "world.h"

#include <algorithm>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include "../systems/owner_registry.h"
#include "../systems/troop_count_registry.h"
#include "component.h"
#include "core/entity.h"
#include "core/system.h"

namespace Engine::Core {

World::World() = default;
World::~World() = default;

void World::on_component_changed(EntityID entity_id,
                                 std::type_index component_type,
                                 bool added) {

  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);

  if (added) {
    m_component_index[component_type].insert(entity_id);
  } else {
    auto it = m_component_index.find(component_type);
    if (it != m_component_index.end()) {
      it->second.erase(entity_id);
      if (it->second.empty()) {
        m_component_index.erase(it);
      }
    }
  }

  const auto observers = m_component_observers;
  for (const auto& observer : observers) {
    observer.callback(entity_id, component_type, added);
  }
}

void World::setup_entity_callback(Entity* entity) {
  entity->set_component_change_callback(
      [this](EntityID entity_id, std::type_index component_type, bool added) {
        this->on_component_changed(entity_id, component_type, added);
      });
}

auto World::create_entity() -> Entity* {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  EntityID const id = m_next_entity_id++;
  auto entity = std::make_unique<Entity>(id);
  auto* ptr = entity.get();
  setup_entity_callback(ptr);
  m_entities[id] = std::move(entity);
  return ptr;
}

auto World::create_entity_with_id(EntityID entity_id) -> Entity* {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  if (entity_id == NULL_ENTITY) {
    return nullptr;
  }

  if (m_entities.contains(entity_id)) {
    for (auto it = m_component_index.begin(); it != m_component_index.end();) {
      it->second.erase(entity_id);
      if (it->second.empty()) {
        it = m_component_index.erase(it);
      } else {
        ++it;
      }
    }
  }

  auto entity = std::make_unique<Entity>(entity_id);
  auto* ptr = entity.get();
  setup_entity_callback(ptr);
  m_entities[entity_id] = std::move(entity);

  if (entity_id >= m_next_entity_id) {
    m_next_entity_id = entity_id + 1;
  }

  return ptr;
}

void World::destroy_entity(EntityID entity_id) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);

  auto it = m_entities.find(entity_id);

  for (auto& [type, entity_set] : m_component_index) {
    entity_set.erase(entity_id);
  }

  if (it != m_entities.end()) {
    m_entities.erase(it);
  } else {
    m_entities.erase(entity_id);
  }

  const auto observers = m_entity_destroyed_observers;
  for (const auto& observer : observers) {
    observer.callback(entity_id);
  }
}

void World::clear() {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  m_entities.clear();
  m_component_index.clear();
  m_next_entity_id = 1;

  const auto observers = m_world_cleared_observers;
  for (const auto& observer : observers) {
    observer.callback();
  }
}

auto World::get_entity(EntityID entity_id) -> Entity* {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  auto it = m_entities.find(entity_id);
  return it != m_entities.end() ? it->second.get() : nullptr;
}

void World::add_system(std::unique_ptr<System> system) {
  m_systems.push_back(std::move(system));
}

void World::update(float delta_time) {
  for (auto& system : m_systems) {
    system->update(this, delta_time);
  }
}

auto World::get_units_owned_by(int owner_id) const -> std::vector<Entity*> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::vector<Entity*> result;
  result.reserve(m_entities.size());
  for (const auto& [entity_id, entity] : m_entities) {
    auto* unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    if (unit->owner_id == owner_id) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::get_units_not_owned_by(int owner_id) const -> std::vector<Entity*> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::vector<Entity*> result;
  result.reserve(m_entities.size());
  for (const auto& [entity_id, entity] : m_entities) {
    auto* unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }
    if (unit->owner_id != owner_id) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::get_allied_units(int owner_id) const -> std::vector<Entity*> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::vector<Entity*> result;
  result.reserve(m_entities.size());
  auto& owner_registry = Game::Systems::OwnerRegistry::instance();

  for (const auto& [entity_id, entity] : m_entities) {
    auto* unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (unit->owner_id == owner_id ||
        owner_registry.are_allies(owner_id, unit->owner_id)) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::get_enemy_units(int owner_id) const -> std::vector<Entity*> {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::vector<Entity*> result;
  result.reserve(m_entities.size());
  auto& owner_registry = Game::Systems::OwnerRegistry::instance();

  for (const auto& [entity_id, entity] : m_entities) {
    auto* unit = entity->get_component<UnitComponent>();
    if (unit == nullptr) {
      continue;
    }

    if (owner_registry.are_enemies(owner_id, unit->owner_id)) {
      result.push_back(entity.get());
    }
  }
  return result;
}

auto World::count_troops_for_player(int owner_id) -> int {
  return Game::Systems::TroopCountRegistry::instance().get_troop_count(owner_id);
}

auto World::get_next_entity_id() const -> EntityID {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  return m_next_entity_id;
}

void World::set_next_entity_id(EntityID next_id) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  m_next_entity_id = std::max(next_id, m_next_entity_id);
}

auto World::add_component_observer(ComponentObserverCallback callback)
    -> ObserverHandle {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  const ObserverHandle handle = m_next_observer_handle++;
  m_component_observers.push_back({handle, std::move(callback)});
  return handle;
}

auto World::add_entity_destroyed_observer(EntityDestroyedCallback callback)
    -> ObserverHandle {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  const ObserverHandle handle = m_next_observer_handle++;
  m_entity_destroyed_observers.push_back({handle, std::move(callback)});
  return handle;
}

auto World::add_world_cleared_observer(WorldClearedCallback callback)
    -> ObserverHandle {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  const ObserverHandle handle = m_next_observer_handle++;
  m_world_cleared_observers.push_back({handle, std::move(callback)});
  return handle;
}

void World::remove_component_observer(ObserverHandle handle) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::erase_if(m_component_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

void World::remove_entity_destroyed_observer(ObserverHandle handle) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::erase_if(m_entity_destroyed_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

void World::remove_world_cleared_observer(ObserverHandle handle) {
  const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
  std::erase_if(m_world_cleared_observers,
                [handle](const auto& entry) { return entry.handle == handle; });
}

} // namespace Engine::Core
