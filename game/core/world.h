#pragma once

#include <algorithm>
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
#include "registry.h"
#include "system.h"
#include "system_profiler.h"
#include "world_spatial_index.h"

namespace Engine::Core {

class World;
class CreaturePresentationComponent;

auto publish_creature_presentation(Entity* entity,
                                   World* world) -> CreaturePresentationComponent*;

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

  template <typename T, typename... Args>
  auto emplace(EntityID entity_id, Args&&... args) -> T* {
    return m_registry.emplace<T>(entity_id, std::forward<Args>(args)...);
  }

  template <typename T>
  [[nodiscard]] auto try_get(EntityID entity_id) -> T* {
    return m_registry.try_get<T>(entity_id);
  }

  template <typename T>
  [[nodiscard]] auto try_get(EntityID entity_id) const -> const T* {
    return m_registry.try_get<T>(entity_id);
  }

  template <typename T>
  [[nodiscard]] auto has(EntityID entity_id) const -> bool {
    return m_registry.has<T>(entity_id);
  }

  template <typename T>
  auto remove(EntityID entity_id) -> bool {
    return m_registry.remove<T>(entity_id);
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

  auto get_entity_mutex() -> std::recursive_mutex& { return m_registry.mutex(); }

  [[nodiscard]] auto registry() noexcept -> Registry& { return m_registry; }
  [[nodiscard]] auto registry() const noexcept -> const Registry& { return m_registry; }

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

  class HandleTable {
  public:
    static constexpr std::size_t k_page_size = 512;

    auto bind(EntityID entity_id, Registry* registry) -> Entity*;
    [[nodiscard]] auto find(std::uint32_t index) const -> Entity*;

  private:
    using Page = std::array<Entity, k_page_size>;

    std::vector<std::unique_ptr<Page>> m_pages;
  };

  void on_component_changed(EntityID entity_id,
                            ComponentTypeId type_id,
                            std::type_index component_type,
                            bool added);

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
  [[nodiscard]] auto collect_units_matching(int owner_id,
                                            bool owned) const -> std::vector<Entity*>;
  void publish_render_snapshot();

  Registry m_registry;
  mutable HandleTable m_handles;

  std::vector<std::unique_ptr<System>> m_systems;
  std::vector<SystemPhase> m_system_phases;
  DeferredMutations m_deferred;

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

class World::EntityLock {
public:
  explicit EntityLock(const World& world)
      : m_lock(world.m_registry) {}

  EntityLock(const EntityLock&) = delete;
  EntityLock(EntityLock&&) = delete;
  auto operator=(const EntityLock&) -> EntityLock& = delete;
  auto operator=(EntityLock&&) -> EntityLock& = delete;
  ~EntityLock() = default;

private:
  Registry::Lock m_lock;
};

template <typename Fn>
void World::for_each_entity(Fn&& fn) const {
  const EntityLock lock(*this);
  const std::size_t count = m_registry.slot_count();
  for (std::size_t index = 1; index < count; ++index) {
    if (Entity* entity =
            resolve(m_registry.entity_at_index(static_cast<std::uint32_t>(index)))) {
      fn(*entity);
    }
  }
}

template <bool WithEntity, typename... Components>
class World::BasicView {
public:
  static_assert(sizeof...(Components) > 0, "a view needs at least one component");

  using Storages = std::tuple<ComponentStorage<Components>*...>;
  using Head = std::conditional_t<WithEntity, Entity&, EntityID>;
  using Reference = std::tuple<Head, Components&...>;

  class Iterator {
  public:
    using iterator_category = std::input_iterator_tag;
    using value_type = Reference;
    using difference_type = std::ptrdiff_t;
    using pointer = void;
    using reference = Reference;

    Iterator(const BasicView* owner, std::size_t index)
        : m_owner(owner)
        , m_index(index) {
      seek();
    }

    auto operator*() const -> Reference {
      return std::apply(
          [this](auto*... components) {
            if constexpr (WithEntity) {
              return Reference(*m_entity, *components...);
            } else {
              return Reference(m_id, *components...);
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
      while (m_index < m_owner->live_count()) {
        m_id = m_owner->m_source->entities()[m_index];
        m_components = std::apply(
            [id = m_id](auto*... storages) {
              return std::tuple<Components*...>(storages->try_get(id)...);
            },
            m_owner->m_storages);
        const bool complete = std::apply(
            [](auto*... components) { return ((components != nullptr) && ...); },
            m_components);
        if (complete) {
          if constexpr (WithEntity) {
            m_entity = m_owner->m_world->resolve(m_id);
            if (m_entity == nullptr) {
              ++m_index;
              continue;
            }
          }
          return;
        }
        ++m_index;
      }
      m_index = m_owner->m_limit;
      m_entity = nullptr;
    }

    const BasicView* m_owner{nullptr};
    std::size_t m_index{0};
    EntityID m_id{NULL_ENTITY};
    Entity* m_entity{nullptr};
    std::tuple<Components*...> m_components{};
  };

  explicit BasicView(World& world)
      : m_world(&world)
      , m_lock(world) {
    m_storages = Storages(world.m_registry.find_storage<Components>()...);
    const bool complete = std::apply(
        [](auto*... storages) { return ((storages != nullptr) && ...); }, m_storages);
    if (!complete) {
      world.note_view_opened(0);
      return;
    }

    std::apply(
        [this](auto*... storages) {
          (
              [&] {
                if (m_source == nullptr || storages->size() < m_limit) {
                  m_source = storages;
                  m_limit = storages->size();
                }
              }(),
              ...);
        },
        m_storages);
    world.note_view_opened(m_limit);
  }

  [[nodiscard]] auto begin() const -> Iterator { return {this, 0}; }
  [[nodiscard]] auto end() const -> Iterator { return {this, m_limit}; }

  [[nodiscard]] auto candidate_count() const -> std::size_t { return m_limit; }

  [[nodiscard]] auto empty() const -> bool { return begin() == end(); }

private:
  [[nodiscard]] auto live_count() const -> std::size_t {
    return m_source == nullptr ? std::size_t{0} : std::min(m_limit, m_source->size());
  }

  World* m_world{nullptr};
  Storages m_storages{};
  const IComponentStorage* m_source{nullptr};
  std::size_t m_limit{0};
  EntityLock m_lock;
};

void copy_authoritative_snapshot_components(const Entity& source, Entity& destination);
void copy_presentation_snapshot_components(const Entity& source, Entity& destination);
void copy_render_components(const Entity& source, Entity& destination);

} // namespace Engine::Core
