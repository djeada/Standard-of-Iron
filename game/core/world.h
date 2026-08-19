#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <functional>
#include <limits>
#include <memory>
#include <mutex>
#include <span>
#include <typeindex>
#include <utility>
#include <vector>

#include "entity.h"
#include "system.h"

namespace Engine::Core {

class World;

void publish_creature_presentation(Entity* entity, World* world);

class World {
public:
  using ObserverHandle = std::uint64_t;
  using ComponentObserverCallback =
      std::function<void(EntityID, std::type_index, bool)>;
  using EntityDestroyedCallback = std::function<void(EntityID)>;
  using WorldClearedCallback = std::function<void()>;

  World();
  ~World();

  World(const World&) = delete;
  World(World&&) = delete;
  auto operator=(const World&) -> World& = delete;
  auto operator=(World&&) -> World& = delete;

  auto create_entity() -> Entity*;
  auto create_entity_with_id(EntityID entity_id) -> Entity*;
  void destroy_entity(EntityID entity_id);
  auto get_entity(EntityID entity_id) -> Entity*;
  void clear();

  [[nodiscard]] auto is_alive(EntityID entity_id) const -> bool;

  void add_system(std::unique_ptr<System> system);
  void update(float delta_time);

  void set_presentation_enabled(bool enabled) noexcept {
    m_presentation_enabled = enabled;
  }
  [[nodiscard]] auto presentation_enabled() const noexcept -> bool {
    return m_presentation_enabled;
  }
  void request_render_snapshots(bool enabled = true) noexcept {
    m_render_snapshots_requested.store(enabled, std::memory_order_release);
  }

  void ensure_render_snapshot();

  [[nodiscard]] auto acquire_render_snapshot() const -> std::shared_ptr<World>;
  [[nodiscard]] auto is_render_snapshot() const noexcept -> bool {
    return m_is_render_snapshot;
  }
  [[nodiscard]] auto render_unit_ids() const -> std::span<const EntityID> {
    return m_render_unit_ids;
  }
  [[nodiscard]] auto render_building_ids() const -> std::span<const EntityID> {
    return m_render_building_ids;
  }
  [[nodiscard]] auto render_other_ids() const -> std::span<const EntityID> {
    return m_render_other_ids;
  }

  auto systems() -> std::vector<std::unique_ptr<System>>& { return m_systems; }

  template <typename T>
  auto get_system() -> T* {
    for (auto& system : m_systems) {
      if (auto* ptr = dynamic_cast<T*>(system.get())) {
        return ptr;
      }
    }
    return nullptr;
  }

  template <typename T>
  auto get_entities_with() -> std::vector<Entity*> {
    return collect_entities_with(component_type_id<T>());
  }

  template <typename T>
  [[nodiscard]] auto entities_with() const -> std::span<const EntityID> {
    return entities_with(component_type_id<T>());
  }

  [[nodiscard]] auto
  entities_with(ComponentTypeId type_id) const -> std::span<const EntityID>;

  void resolve_entities_into(std::span<const EntityID> ids,
                             std::vector<Entity*>& output) const;

  template <typename... Components, typename Fn>
  void each(Fn&& fn) {
    static_assert(sizeof...(Components) > 0);
    const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
    const std::array<ComponentTypeId, sizeof...(Components)> type_ids{
        component_type_id<Components>()...};

    ComponentTypeId smallest_type = 0;
    std::size_t smallest_size = std::numeric_limits<std::size_t>::max();
    for (const ComponentTypeId type_id : type_ids) {
      if (type_id >= m_component_sets.size()) {
        return;
      }
      const ComponentSet& candidate = m_component_sets[type_id];
      if (candidate.dense.size() < smallest_size) {
        smallest_type = type_id;
        smallest_size = candidate.dense.size();
      }
    }

    std::span<const EntityID> const ids = m_component_sets[smallest_type].dense;
    for (const EntityID id : ids) {
      Entity* entity = resolve(id);
      if (entity != nullptr &&
          ((entity->get_component<Components>() != nullptr) && ...)) {
        std::invoke(std::forward<Fn>(fn), id, *entity->get_component<Components>()...);
      }
    }
  }

  auto get_units_owned_by(int owner_id) const -> std::vector<Entity*>;
  auto get_units_not_owned_by(int owner_id) const -> std::vector<Entity*>;

  [[nodiscard]] auto entity_count() const -> std::size_t;

  template <typename Fn>
  void for_each_entity(Fn&& fn) const {
    const std::lock_guard<std::recursive_mutex> lock(m_entity_mutex);
    for (const auto& slot : m_slots) {
      if (slot.entity != nullptr) {
        fn(*slot.entity);
      }
    }
  }

  auto get_next_entity_id() const -> EntityID;
  void set_next_entity_id(EntityID next_id);

  auto get_entity_mutex() -> std::recursive_mutex& { return m_entity_mutex; }

  using EntityDestroyedHook = void (*)(EntityID);
  static void set_entity_destroyed_hook(EntityDestroyedHook hook);

  ObserverHandle add_component_observer(ComponentObserverCallback callback);
  ObserverHandle add_entity_destroyed_observer(EntityDestroyedCallback callback);
  ObserverHandle add_world_cleared_observer(WorldClearedCallback callback);
  void remove_component_observer(ObserverHandle handle);
  void remove_entity_destroyed_observer(ObserverHandle handle);
  void remove_world_cleared_observer(ObserverHandle handle);

private:
  World(bool presentation_enabled, bool render_snapshot);

  struct ComponentSet {
    static constexpr std::uint32_t k_absent = 0xFFFFFFFFU;

    std::vector<EntityID> dense;
    std::vector<std::uint32_t> sparse;

    void insert(EntityID id);
    void erase(EntityID id);
    [[nodiscard]] auto contains(EntityID id) const -> bool;
    void clear();
  };

  struct EntitySlot {
    std::unique_ptr<Entity> entity;

    std::uint32_t generation = 0;
  };

  void on_component_changed(EntityID entity_id,
                            ComponentTypeId type_id,
                            std::type_index component_type,
                            bool added);

  void setup_entity_callback(Entity* entity);

  auto collect_entities_with(ComponentTypeId type_id) -> std::vector<Entity*>;

  [[nodiscard]] auto resolve(EntityID entity_id) const -> Entity*;
  void detach_from_all_component_sets(EntityID entity_id);
  void publish_render_snapshot();

  std::vector<EntitySlot> m_slots;
  std::vector<std::uint32_t> m_free_slots;
  std::size_t m_live_count = 0;

  std::vector<std::unique_ptr<System>> m_systems;
  mutable std::recursive_mutex m_entity_mutex;

  std::vector<ComponentSet> m_component_sets;

  template <typename Callback>
  struct ObserverEntry {
    ObserverHandle handle{0};
    Callback callback;
  };

  ObserverHandle m_next_observer_handle{1};
  std::vector<ObserverEntry<ComponentObserverCallback>> m_component_observers;
  std::vector<ObserverEntry<EntityDestroyedCallback>> m_entity_destroyed_observers;
  std::vector<ObserverEntry<WorldClearedCallback>> m_world_cleared_observers;

  bool m_presentation_enabled{true};
  bool m_is_render_snapshot{false};
  std::atomic<bool> m_render_snapshots_requested{false};
  std::shared_ptr<World> m_render_snapshot;
  std::array<std::shared_ptr<World>, 2> m_render_snapshot_buffers;
  std::size_t m_next_render_snapshot_buffer{0};
  std::vector<EntityID> m_render_unit_ids;
  std::vector<EntityID> m_render_building_ids;
  std::vector<EntityID> m_render_other_ids;
  std::vector<std::uint64_t> m_render_entity_signatures;
  std::uint64_t m_render_publish_revision{0};
};

void copy_authoritative_snapshot_components(const Entity& source, Entity& destination);
void copy_presentation_snapshot_components(const Entity& source, Entity& destination);
void copy_render_components(const Entity& source, Entity& destination);

} // namespace Engine::Core
