#pragma once

#include "entity.h"
#include "system.h"
#include <memory>
#include <mutex>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
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

  auto create_entity() -> Entity *;
  auto create_entity_with_id(EntityID entity_id) -> Entity *;
  void destroy_entity(EntityID entity_id);
  auto get_entity(EntityID entity_id) -> Entity *;
  void clear();

  void add_system(std::unique_ptr<System> system);
  void update(float delta_time);

  auto systems() -> std::vector<std::unique_ptr<System>> & { return m_systems; }

  template <typename T> auto get_system() -> T * {
    for (auto &system : m_systems) {
      if (auto *ptr = dynamic_cast<T *>(system.get())) {
        return ptr;
      }
    }
    return nullptr;
  }

  template <typename T> auto get_entities_with() -> std::vector<Entity *> {
    const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
    std::type_index const type_idx = std::type_index(typeid(T));

    auto it = m_component_index.find(type_idx);
    if (it == m_component_index.end()) {
      return {};
    }

    std::vector<Entity *> result;
    result.reserve(it->second.size());

    for (EntityID entity_id : it->second) {
      auto entity_it = m_entities.find(entity_id);
      if (entity_it != m_entities.end()) {
        result.push_back(entity_it->second.get());
      }
    }

    return result;
  }

  auto get_units_owned_by(int owner_id) const -> std::vector<Entity *>;
  auto get_units_not_owned_by(int owner_id) const -> std::vector<Entity *>;
  auto get_allied_units(int owner_id) const -> std::vector<Entity *>;
  auto get_enemy_units(int owner_id) const -> std::vector<Entity *>;
  static auto count_troops_for_player(int owner_id) -> int;

  auto get_entities() const
      -> const std::unordered_map<EntityID, std::unique_ptr<Entity>> & {
    return m_entities;
  }

  auto get_next_entity_id() const -> EntityID;
  void set_next_entity_id(EntityID next_id);

  auto get_entity_mutex() -> std::recursive_mutex & { return m_entity_mutex; }

private:
  void on_component_changed(EntityID entity_id, std::type_index component_type,
                            bool added);

  void setup_entity_callback(Entity *entity);

  EntityID m_next_entity_id = 1;
  std::unordered_map<EntityID, std::unique_ptr<Entity>> m_entities;
  std::vector<std::unique_ptr<System>> m_systems;
  mutable std::recursive_mutex m_entity_mutex;

  std::unordered_map<std::type_index, std::unordered_set<EntityID>>
      m_component_index;
};

} 
