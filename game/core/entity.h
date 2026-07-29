#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <vector>

#include "component_pool.h"
#include "component_registry.h"

namespace Engine::Core {

using EntityID = std::uint64_t;
constexpr EntityID NULL_ENTITY = 0;

namespace Handle {

constexpr unsigned k_index_bits = 32U;
constexpr EntityID k_index_mask = (EntityID{1} << k_index_bits) - 1U;

constexpr auto make(std::uint32_t index, std::uint32_t generation) -> EntityID {
  return (static_cast<EntityID>(generation) << k_index_bits) |
         static_cast<EntityID>(index);
}

constexpr auto index_of(EntityID id) -> std::uint32_t {
  return static_cast<std::uint32_t>(id & k_index_mask);
}

constexpr auto generation_of(EntityID id) -> std::uint32_t {
  return static_cast<std::uint32_t>(id >> k_index_bits);
}

} // namespace Handle

class Component {
public:
  virtual ~Component() = default;
};

using ComponentChangeCallback =
    std::function<void(EntityID, ComponentTypeId, std::type_index, bool)>;

class Entity {
public:
  Entity(EntityID id);

  auto get_id() const -> EntityID;

  void set_component_change_callback(ComponentChangeCallback callback);

  template <typename T, typename... Args>
  auto add_component(Args&&... args) -> T* {
    static_assert(std::is_base_of_v<Component, T>, "T must inherit from Component");

    T* ptr =
        Detail::ComponentPool<T>::instance().construct(std::forward<Args>(args)...);
    const ComponentTypeId slot = component_type_id<T>();
    if (m_components_by_type.size() <= slot) {
      m_components_by_type.resize(slot + 1);
    }
    m_components_by_type[slot] =
        ComponentPtr(ptr, Detail::PooledComponentDeleter{&Detail::release_to_pool<T>});

    if (m_component_change_callback) {
      m_component_change_callback(m_id, slot, std::type_index(typeid(T)), true);
    }

    return ptr;
  }

  template <typename T>
  auto get_component() -> T* {
    const ComponentTypeId slot = component_type_id<T>();
    if (slot < m_components_by_type.size()) {
      return static_cast<T*>(m_components_by_type[slot].get());
    }
    return nullptr;
  }

  template <typename T>
  auto get_component() const -> const T* {
    const ComponentTypeId slot = component_type_id<T>();
    if (slot < m_components_by_type.size()) {
      return static_cast<const T*>(m_components_by_type[slot].get());
    }
    return nullptr;
  }

  template <typename T>
  void remove_component() {
    const ComponentTypeId slot = component_type_id<T>();
    if (slot < m_components_by_type.size() && m_components_by_type[slot] != nullptr) {
      m_components_by_type[slot].reset();

      if (m_component_change_callback) {
        m_component_change_callback(m_id, slot, std::type_index(typeid(T)), false);
      }
    }
  }

  template <typename T>
  auto has_component() const -> bool {
    const ComponentTypeId slot = component_type_id<T>();
    return slot < m_components_by_type.size() && m_components_by_type[slot] != nullptr;
  }

private:
  EntityID m_id;
  std::vector<ComponentPtr> m_components_by_type;
  ComponentChangeCallback m_component_change_callback;
};

template <typename T, typename... Args>
auto get_or_add_component(Entity& entity, Args&&... args) -> T* {
  if (T* existing = entity.get_component<T>()) {
    return existing;
  }
  return entity.add_component<T>(std::forward<Args>(args)...);
}

template <typename T, typename... Args>
auto get_or_add_component(Entity* entity, Args&&... args) -> T* {
  return entity == nullptr
             ? nullptr
             : get_or_add_component<T>(*entity, std::forward<Args>(args)...);
}

} // namespace Engine::Core
