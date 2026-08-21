#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <mutex>
#include <source_location>
#include <span>
#include <thread>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

#include "deferred_mutations.h"
#include "entity.h"
#include "system.h"
#include "system_profiler.h"
#include "world_spatial_index.h"

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

  class EntityLock;

  template <bool WithEntity, typename... Components>
  class BasicView;

  template <typename... Components>
  using View = BasicView<false, Components...>;

  template <typename... Components>
  using EntityView = BasicView<true, Components...>;

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

  void add_system(std::unique_ptr<System> system, SystemPhase phase);

  void update(float delta_time);

  [[nodiscard]] auto deferred() -> DeferredMutations& { return m_deferred; }

  [[nodiscard]] auto system_phases() const -> std::span<const SystemPhase> {
    return m_system_phases;
  }

  [[nodiscard]] auto
  plan_phase_schedule(SystemPhase phase) const -> std::vector<std::vector<std::size_t>>;

  [[nodiscard]] auto tick_id() const noexcept -> std::uint64_t { return m_tick_id; }

  [[nodiscard]] auto spatial_index() -> WorldSpatialIndex& { return m_spatial_index; }
  [[nodiscard]] auto spatial_index() const -> const WorldSpatialIndex& {
    return m_spatial_index;
  }

  [[nodiscard]] auto system_profiler() -> SystemProfiler& { return m_system_profiler; }
  [[nodiscard]] auto system_profiler() const -> const SystemProfiler& {
    return m_system_profiler;
  }

  [[nodiscard]] auto query_counters() const -> const SystemProfiler::QueryCounters& {
    return m_query_counters;
  }

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
  [[nodiscard]] auto
  collect_entities_with(std::source_location where = std::source_location::current())
      -> std::vector<Entity*> {
    return collect_entities_with_type(component_type_id<T>(), where);
  }

  template <typename T>
  [[nodiscard]] auto entities_with() const -> std::span<const EntityID> {
    return entities_with(component_type_id<T>());
  }

  template <typename... Components>
  [[nodiscard]] auto view() -> View<Components...> {
    return View<Components...>(*this);
  }

  template <typename... Components>
  [[nodiscard]] auto entity_view() -> EntityView<Components...> {
    return EntityView<Components...>(*this);
  }

  [[nodiscard]] auto
  entities_with(ComponentTypeId type_id) const -> std::span<const EntityID>;

  void resolve_entities_into(std::span<const EntityID> ids,
                             std::vector<Entity*>& output) const;

  template <typename... Components, typename Fn>
  void each(Fn&& fn) {
    static_assert(sizeof...(Components) > 0);
    for (auto entry : view<Components...>()) {
      std::apply(
          [&fn](EntityID id, Components&... components) {
            std::invoke(fn, id, components...);
          },
          entry);
    }
  }

  auto get_units_owned_by(int owner_id) const -> std::vector<Entity*>;
  auto get_units_not_owned_by(int owner_id) const -> std::vector<Entity*>;

  [[nodiscard]] auto entity_count() const -> std::size_t;

  template <typename Fn>
  void for_each_entity(Fn&& fn) const;

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

  auto
  collect_entities_with_type(ComponentTypeId type_id,
                             const std::source_location& where) -> std::vector<Entity*>;

  static auto system_display_name(const System& system) -> const char*;
  [[nodiscard]] auto current_query_counters() const -> SystemProfiler::QueryCounters;

  void note_view_opened(std::size_t candidates) {
    ++m_query_counters.views;
    m_query_counters.view_candidates += candidates;
  }

  [[nodiscard]] auto resolve(EntityID entity_id) const -> Entity*;
  void detach_from_all_component_sets(EntityID entity_id);
  void publish_render_snapshot();

  std::vector<EntitySlot> m_slots;
  std::vector<std::uint32_t> m_free_slots;
  std::size_t m_live_count = 0;

  std::vector<std::unique_ptr<System>> m_systems;
  std::vector<SystemPhase> m_system_phases;
  DeferredMutations m_deferred;
  mutable std::recursive_mutex m_entity_mutex;

  mutable std::atomic<std::uint64_t> m_entity_lock_owner{0};

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

  WorldSpatialIndex m_spatial_index;
  SystemProfiler m_system_profiler;
  SystemProfiler::QueryCounters m_query_counters;
  std::uint64_t m_tick_id{0};
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

[[nodiscard]] inline auto this_thread_lock_token() noexcept -> std::uint64_t {
  static std::atomic<std::uint64_t> next_token{1};
  static const thread_local std::uint64_t token =
      next_token.fetch_add(1, std::memory_order_relaxed);
  return token;
}

class World::EntityLock {
public:
  explicit EntityLock(const World& world)
      : m_world(&world) {
    const std::uint64_t token = this_thread_lock_token();
    if (world.m_entity_lock_owner.load(std::memory_order_relaxed) == token) {
      return;
    }
    world.m_entity_mutex.lock();
    world.m_entity_lock_owner.store(token, std::memory_order_relaxed);
    m_acquired = true;
  }

  EntityLock(const EntityLock&) = delete;
  EntityLock(EntityLock&&) = delete;
  auto operator=(const EntityLock&) -> EntityLock& = delete;
  auto operator=(EntityLock&&) -> EntityLock& = delete;

  ~EntityLock() {
    if (!m_acquired) {
      return;
    }

    m_world->m_entity_lock_owner.store(0, std::memory_order_relaxed);
    m_world->m_entity_mutex.unlock();
  }

private:
  const World* m_world;
  bool m_acquired{false};
};

template <typename Fn>
void World::for_each_entity(Fn&& fn) const {
  const EntityLock lock(*this);
  for (const auto& slot : m_slots) {
    if (slot.entity != nullptr) {
      fn(*slot.entity);
    }
  }
}

template <bool WithEntity, typename... Components>
class World::BasicView {
public:
  static_assert(sizeof...(Components) > 0, "a view needs at least one component");

  using ComponentPointers = std::tuple<Components*...>;
  using Head = std::conditional_t<WithEntity, Entity&, EntityID>;
  using Reference = std::tuple<Head, Components&...>;

  class Iterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = Reference;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = Reference;

    Iterator(World* world, std::span<const EntityID> ids, std::size_t index)
        : m_world(world)
        , m_ids(ids)
        , m_index(index) {
      seek();
    }

    auto operator*() const -> Reference {
      return std::apply(
          [this](auto*... components) {
            if constexpr (WithEntity) {
              return Reference(*m_entity, *components...);
            } else {
              return Reference(m_ids[m_index], *components...);
            }
          },
          m_components);
    }

    auto operator++() -> Iterator& {
      ++m_index;
      seek();
      return *this;
    }

    auto operator==(const Iterator& other) const -> bool {
      return m_index == other.m_index;
    }

    auto operator!=(const Iterator& other) const -> bool {
      return m_index != other.m_index;
    }

  private:
    void seek() {
      while (m_index < m_ids.size()) {
        if (Entity* entity = m_world->resolve(m_ids[m_index])) {
          m_components = ComponentPointers(entity->get_component<Components>()...);
          const bool complete = std::apply(
              [](auto*... components) { return ((components != nullptr) && ...); },
              m_components);
          if (complete) {
            m_entity = entity;
            return;
          }
        }
        ++m_index;
      }
      m_entity = nullptr;
    }

    World* m_world{nullptr};
    std::span<const EntityID> m_ids;
    std::size_t m_index{0};
    Entity* m_entity{nullptr};
    ComponentPointers m_components{};
  };

  explicit BasicView(World& world)
      : m_world(&world)
      , m_lock(world) {
    const std::array<ComponentTypeId, sizeof...(Components)> type_ids{
        component_type_id<Components>()...};
    std::size_t smallest = std::numeric_limits<std::size_t>::max();
    for (const ComponentTypeId type_id : type_ids) {
      if (type_id >= world.m_component_sets.size()) {
        m_ids = {};
        world.note_view_opened(0);
        return;
      }
      const std::vector<EntityID>& dense = world.m_component_sets[type_id].dense;
      if (dense.size() < smallest) {
        smallest = dense.size();
        m_ids = dense;
      }
    }
    world.note_view_opened(m_ids.size());
  }

  [[nodiscard]] auto begin() const -> Iterator { return {m_world, m_ids, 0}; }
  [[nodiscard]] auto end() const -> Iterator { return {m_world, m_ids, m_ids.size()}; }

  [[nodiscard]] auto candidate_count() const -> std::size_t { return m_ids.size(); }

  [[nodiscard]] auto empty() const -> bool { return begin() == end(); }

private:
  World* m_world{nullptr};
  std::span<const EntityID> m_ids;
  EntityLock m_lock;
};

void copy_authoritative_snapshot_components(const Entity& source, Entity& destination);
void copy_presentation_snapshot_components(const Entity& source, Entity& destination);
void copy_render_components(const Entity& source, Entity& destination);

} // namespace Engine::Core
