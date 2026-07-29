#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
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

  auto get_units_owned_by(int owner_id) const -> std::vector<Entity*>;
  auto get_units_not_owned_by(int owner_id) const -> std::vector<Entity*>;
  auto get_allied_units(int owner_id) const -> std::vector<Entity*>;
  auto get_enemy_units(int owner_id) const -> std::vector<Entity*>;
  static auto count_troops_for_player(int owner_id) -> int;

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

  ObserverHandle add_component_observer(ComponentObserverCallback callback);
  ObserverHandle add_entity_destroyed_observer(EntityDestroyedCallback callback);
  ObserverHandle add_world_cleared_observer(WorldClearedCallback callback);
  void remove_component_observer(ObserverHandle handle);
  void remove_entity_destroyed_observer(ObserverHandle handle);
  void remove_world_cleared_observer(ObserverHandle handle);

private:
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
};

} // namespace Engine::Core
