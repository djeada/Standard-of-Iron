#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace Engine::Core {

using EntityID = std::uint32_t;
constexpr EntityID NULL_ENTITY = 0;

class Component {
public:
  virtual ~Component() = default;
};

// Callback type for component change notifications
using ComponentChangeCallback =
    std::function<void(EntityID, std::type_index, bool)>;

class Entity {
public:
  Entity(EntityID id);

  auto get_id() const -> EntityID;

  /**
   * @brief Set a callback to be notified when components are added/removed.
   * @param callback Function called with (entity_id, component_type, added)
   */
  void set_component_change_callback(ComponentChangeCallback callback);

  template <typename T, typename... Args>
  auto add_component(Args &&...args) -> T * {
    static_assert(std::is_base_of_v<Component, T>,
                  "T must inherit from Component");
    auto component = std::make_unique<T>(std::forward<Args>(args)...);
    auto ptr = component.get();
    std::type_index const type_idx = std::type_index(typeid(T));
    m_components[type_idx] = std::move(component);

    // Notify about component addition
    if (m_component_change_callback) {
      m_component_change_callback(m_id, type_idx, true);
    }

    return ptr;
  }

  template <typename T> auto get_component() -> T * {
    auto it = m_components.find(std::type_index(typeid(T)));
    if (it != m_components.end()) {
      return static_cast<T *>(it->second.get());
    }
    return nullptr;
  }

  template <typename T> auto get_component() const -> const T * {
    auto it = m_components.find(std::type_index(typeid(T)));
    if (it != m_components.end()) {
      return static_cast<const T *>(it->second.get());
    }
    return nullptr;
  }

  template <typename T> void remove_component() {
    std::type_index const type_idx = std::type_index(typeid(T));
    auto it = m_components.find(type_idx);
    if (it != m_components.end()) {
      m_components.erase(it);

      // Notify about component removal
      if (m_component_change_callback) {
        m_component_change_callback(m_id, type_idx, false);
      }
    }
  }

  template <typename T> auto has_component() const -> bool {
    return m_components.find(std::type_index(typeid(T))) != m_components.end();
  }

private:
  EntityID m_id;
  std::unordered_map<std::type_index, std::unique_ptr<Component>> m_components;
  ComponentChangeCallback m_component_change_callback;
};

} // namespace Engine::Core
