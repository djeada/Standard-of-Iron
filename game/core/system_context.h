#pragma once

#include <cstddef>
#include <span>
#include <utility>

#include "component_registry.h"
#include "deferred_mutations.h"
#include "entity_id.h"
#include "world.h"
#include "world_spatial_index.h"

namespace Engine::Core {

class SystemContext {
public:
  SystemContext(World& world, float delta_time)
      : m_world(&world)
      , m_delta_time(delta_time) {}

  [[nodiscard]] auto delta_time() const noexcept -> float { return m_delta_time; }

  template <typename... Components>
  [[nodiscard]] auto view() -> World::View<Components...> {
    return m_world->view<Components...>();
  }

  template <typename... Components>
  [[nodiscard]] auto entity_view() -> World::EntityView<Components...> {
    return m_world->entity_view<Components...>();
  }

  template <typename T>
  [[nodiscard]] auto try_get(EntityID entity_id) -> T* {
    return m_world->try_get<T>(entity_id);
  }

  template <typename T>
  [[nodiscard]] auto has(EntityID entity_id) const -> bool {
    return m_world->has<T>(entity_id);
  }

  template <typename T, typename... Args>
  auto emplace(EntityID entity_id, Args&&... args) -> T* {
    return m_world->emplace<T>(entity_id, std::forward<Args>(args)...);
  }

  template <typename T>
  auto remove(EntityID entity_id) -> bool {
    return m_world->remove<T>(entity_id);
  }

  template <typename T>
  [[nodiscard]] auto entities_with() const -> std::span<const EntityID> {
    return m_world->entities_with<T>();
  }

  [[nodiscard]] auto is_alive(EntityID entity_id) const -> bool {
    return m_world->is_alive(entity_id);
  }

  [[nodiscard]] auto spatial_index() -> WorldSpatialIndex& {
    return m_world->spatial_index();
  }

  [[nodiscard]] auto deferred() -> DeferredMutations& { return m_world->deferred(); }

  [[nodiscard]] auto world() noexcept -> World& { return *m_world; }

private:
  World* m_world;
  float m_delta_time;
};

} // namespace Engine::Core
