#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <vector>

namespace Engine::Core {

using EntityID = std::uint32_t;
constexpr EntityID NULL_ENTITY = 0;

class Component {
public:
  virtual ~Component() = default;
};

using ComponentChangeCallback =
    std::function<void(EntityID, std::type_index, bool)>;

class Entity {
public:
  Entity(EntityID id);

  auto get_id() const -> EntityID;

  void set_component_change_callback(ComponentChangeCallback callback);

  template <typename T, typename... Args>
  auto add_component(Args &&...args) -> T * {
    static_assert(std::is_base_of_v<Component, T>,
                  "T must inherit from Component");
    auto component = std::make_unique<T>(std::forward<Args>(args)...);
    auto ptr = component.get();
    std::size_t const slot = component_type_id<T>();
    if (m_components_by_type.size() <= slot) {
      m_components_by_type.resize(slot + 1);
    }
    m_components_by_type[slot] = std::move(component);
    std::type_index const type_idx = std::type_index(typeid(T));

    if (m_component_change_callback) {
      m_component_change_callback(m_id, type_idx, true);
    }

    return ptr;
  }

  template <typename T> auto get_component() -> T * {
    std::size_t const slot = component_type_id<T>();
    if (slot < m_components_by_type.size()) {
      return static_cast<T *>(m_components_by_type[slot].get());
    }
    return nullptr;
  }

  template <typename T> auto get_component() const -> const T * {
    std::size_t const slot = component_type_id<T>();
    if (slot < m_components_by_type.size()) {
      return static_cast<const T *>(m_components_by_type[slot].get());
    }
    return nullptr;
  }

  template <typename T> void remove_component() {
    std::size_t const slot = component_type_id<T>();
    std::type_index const type_idx = std::type_index(typeid(T));
    if (slot < m_components_by_type.size() &&
        m_components_by_type[slot] != nullptr) {
      m_components_by_type[slot].reset();

      if (m_component_change_callback) {
        m_component_change_callback(m_id, type_idx, false);
      }
    }
  }

  template <typename T> auto has_component() const -> bool {
    std::size_t const slot = component_type_id<T>();
    return slot < m_components_by_type.size() &&
           m_components_by_type[slot] != nullptr;
  }

private:
  static auto resolve_component_type_id(std::type_index type) -> std::size_t;

  template <typename T> static auto component_type_id() -> std::size_t {
    static const std::size_t id =
        resolve_component_type_id(std::type_index(typeid(T)));
    return id;
  }

  EntityID m_id;
  std::vector<std::unique_ptr<Component>> m_components_by_type;
  ComponentChangeCallback m_component_change_callback;
};

} // namespace Engine::Core
