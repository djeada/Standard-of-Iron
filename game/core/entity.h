#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

#include "component_registry.h"
#include "entity_id.h"
#include "registry.h"

namespace Engine::Core {

using ComponentChangeCallback = Registry::ComponentChangeCallback;

class Entity {
public:
  Entity() = default;

  Entity(EntityID id, Registry* registry)
      : m_id(id)
      , m_registry(registry) {}

  [[nodiscard]] auto get_id() const -> EntityID { return m_id; }

  [[nodiscard]] auto registry() const -> Registry* { return m_registry; }

  template <typename T, typename... Args>
  auto add_component(Args&&... args) -> T* {
    return m_registry == nullptr
               ? nullptr
               : m_registry->emplace<T>(m_id, std::forward<Args>(args)...);
  }

  template <typename T>
  [[nodiscard]] auto get_component() -> T* {
    return m_registry == nullptr ? nullptr : m_registry->try_get<T>(m_id);
  }

  template <typename T>
  [[nodiscard]] auto get_component() const -> const T* {
    return m_registry == nullptr ? nullptr : m_registry->try_get<T>(m_id);
  }

  template <typename T>
  void remove_component() {
    if (m_registry != nullptr) {
      m_registry->remove<T>(m_id);
    }
  }

  template <typename T>
  [[nodiscard]] auto has_component() const -> bool {
    return m_registry != nullptr && m_registry->has<T>(m_id);
  }

private:
  EntityID m_id{NULL_ENTITY};
  Registry* m_registry{nullptr};
};

class StandaloneEntity {
public:
  explicit StandaloneEntity(EntityID entity_id = 1)
      : m_entity(m_registry.create_entity_with_id(entity_id), &m_registry) {}

  StandaloneEntity(const StandaloneEntity&) = delete;
  StandaloneEntity(StandaloneEntity&&) = delete;
  auto operator=(const StandaloneEntity&) -> StandaloneEntity& = delete;
  auto operator=(StandaloneEntity&&) -> StandaloneEntity& = delete;
  ~StandaloneEntity() = default;

  [[nodiscard]] auto entity() noexcept -> Entity& { return m_entity; }
  [[nodiscard]] auto entity() const noexcept -> const Entity& { return m_entity; }

  auto operator*() noexcept -> Entity& { return m_entity; }
  auto operator->() noexcept -> Entity* { return &m_entity; }

private:
  Registry m_registry;
  Entity m_entity;
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
