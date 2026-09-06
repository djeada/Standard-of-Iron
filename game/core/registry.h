#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <typeindex>
#include <utility>
#include <vector>

#include "component_registry.h"
#include "component_storage.h"
#include "entity_id.h"
#include "system_access_recorder.h"

namespace Engine::Core {

[[nodiscard]] auto this_thread_lock_token() noexcept -> std::uint64_t;

namespace Detail {

class ScopedRegistryLock {
public:
  ScopedRegistryLock(std::recursive_mutex& mutex, std::atomic<std::uint64_t>& owner)
      : m_mutex(&mutex)
      , m_owner(&owner) {
    const std::uint64_t token = this_thread_lock_token();
    if (owner.load(std::memory_order_relaxed) == token) {
      return;
    }
    mutex.lock();
    owner.store(token, std::memory_order_relaxed);
    m_acquired = true;
  }

  ScopedRegistryLock(const ScopedRegistryLock&) = delete;
  ScopedRegistryLock(ScopedRegistryLock&&) = delete;
  auto operator=(const ScopedRegistryLock&) -> ScopedRegistryLock& = delete;
  auto operator=(ScopedRegistryLock&&) -> ScopedRegistryLock& = delete;

  ~ScopedRegistryLock() {
    if (!m_acquired) {
      return;
    }
    m_owner->store(0, std::memory_order_relaxed);
    m_mutex->unlock();
  }

private:
  std::recursive_mutex* m_mutex;
  std::atomic<std::uint64_t>* m_owner;
  bool m_acquired{false};
};

} // namespace Detail

class Registry {
public:
  using ComponentChangeCallback =
      std::function<void(EntityID, ComponentTypeId, std::type_index, bool)>;
  using EntityCreatedCallback = std::function<void(EntityID)>;

  class Lock;

  Registry();
  ~Registry();

  Registry(const Registry&) = delete;
  Registry(Registry&&) = delete;
  auto operator=(const Registry&) -> Registry& = delete;
  auto operator=(Registry&&) -> Registry& = delete;

  [[nodiscard]] auto instance_id() const noexcept -> std::uint64_t {
    return m_instance_id;
  }

  auto create_entity() -> EntityID;
  auto create_entity_with_id(EntityID entity_id) -> EntityID;
  auto destroy_entity(EntityID entity_id) -> bool;
  void clear();

  [[nodiscard]] auto is_alive(EntityID entity_id) const noexcept -> bool {
    const std::uint32_t index = Handle::index_of(entity_id);
    if (index == 0 || index >= m_slots.size()) {
      return false;
    }
    const Slot& slot = m_slots[index];
    return slot.alive && slot.generation == Handle::generation_of(entity_id);
  }

  [[nodiscard]] auto entity_count() const noexcept -> std::size_t {
    return m_live_count;
  }
  [[nodiscard]] auto slot_count() const noexcept -> std::size_t {
    return m_slots.size();
  }
  [[nodiscard]] auto entity_at_index(std::uint32_t index) const noexcept -> EntityID {
    if (index == 0 || index >= m_slots.size() || !m_slots[index].alive) {
      return NULL_ENTITY;
    }
    return Handle::make(index, m_slots[index].generation);
  }
  void reserve_indices_below(std::uint32_t index);

  template <typename T, typename... Args>
  auto emplace(EntityID entity_id, Args&&... args) -> T* {
    if (!is_alive(entity_id)) {
      return nullptr;
    }
    const Detail::ScopedRegistryLock lock(m_mutex, m_lock_owner);
    auto& store = storage<T>();
    const bool existed = store.contains(entity_id);
    T& component = store.emplace(entity_id, std::forward<Args>(args)...);
    if (!existed) {
      notify(entity_id, store, true);
    }
    return &component;
  }

  template <typename T>
  [[nodiscard]] auto try_get(EntityID entity_id) noexcept -> T* {
    auto* store = find_storage<T>();
    return store == nullptr ? nullptr : store->try_get(entity_id);
  }

  template <typename T>
  [[nodiscard]] auto try_get(EntityID entity_id) const noexcept -> const T* {
    const auto* store = find_storage<T>();
    return store == nullptr ? nullptr : store->try_get(entity_id);
  }

  template <typename T>
  [[nodiscard]] auto has(EntityID entity_id) const noexcept -> bool {
    const auto* store = find_storage<T>();
    return store != nullptr && store->contains(entity_id);
  }

  template <typename T>
  auto remove(EntityID entity_id) -> bool {
    auto* store = find_storage<T>();
    if (store == nullptr || !store->contains(entity_id)) {
      return false;
    }
    const Detail::ScopedRegistryLock lock(m_mutex, m_lock_owner);
    if (!store->erase(entity_id)) {
      return false;
    }
    notify(entity_id, *store, false);
    return true;
  }

  template <typename T>
  [[nodiscard]] auto storage() -> ComponentStorage<T>& {
    const ComponentTypeId type_id = component_type_id<T>();
    Detail::note_component_access(type_id, true);
    if (m_storages.size() <= type_id) {
      m_storages.resize(static_cast<std::size_t>(type_id) + 1U);
    }
    if (m_storages[type_id] == nullptr) {
      m_storages[type_id] = std::make_unique<ComponentStorage<T>>();
    }
    return static_cast<ComponentStorage<T>&>(*m_storages[type_id]);
  }

  template <typename T>
  [[nodiscard]] auto find_storage() noexcept -> ComponentStorage<T>* {
    const ComponentTypeId type_id = component_type_id<T>();
    Detail::note_component_access(type_id, true);
    if (type_id >= m_storages.size() || m_storages[type_id] == nullptr) {
      return nullptr;
    }
    return static_cast<ComponentStorage<T>*>(m_storages[type_id].get());
  }

  template <typename T>
  [[nodiscard]] auto find_storage() const noexcept -> const ComponentStorage<T>* {
    const ComponentTypeId type_id = component_type_id<T>();
    Detail::note_component_access(type_id, false);
    if (type_id >= m_storages.size() || m_storages[type_id] == nullptr) {
      return nullptr;
    }
    return static_cast<const ComponentStorage<T>*>(m_storages[type_id].get());
  }

  [[nodiscard]] auto
  entities_with(ComponentTypeId type_id) const noexcept -> std::span<const EntityID> {
    if (type_id >= m_storages.size() || m_storages[type_id] == nullptr) {
      return {};
    }
    return m_storages[type_id]->entities();
  }

  template <typename T>
  [[nodiscard]] auto entities_with() const noexcept -> std::span<const EntityID> {
    return entities_with(component_type_id<T>());
  }

  void set_component_change_callback(ComponentChangeCallback callback) {
    m_component_change_callback = std::move(callback);
  }

  [[nodiscard]] auto mutex() const noexcept -> std::recursive_mutex& { return m_mutex; }
  [[nodiscard]] auto lock_owner() const noexcept -> std::atomic<std::uint64_t>& {
    return m_lock_owner;
  }

private:
  const std::uint64_t m_instance_id;
  struct Slot {
    std::uint32_t generation = 0;
    bool alive = false;
  };

  void notify(EntityID entity_id, const IComponentStorage& store, bool added) {
    if (m_component_change_callback) {
      m_component_change_callback(entity_id, store.type_id(), store.type(), added);
    }
  }

  void detach_all_components(EntityID entity_id);

  std::vector<Slot> m_slots;
  std::vector<std::uint32_t> m_free_slots;
  std::size_t m_live_count = 0;
  std::vector<std::unique_ptr<IComponentStorage>> m_storages;
  ComponentChangeCallback m_component_change_callback;

  mutable std::recursive_mutex m_mutex;
  mutable std::atomic<std::uint64_t> m_lock_owner{0};
};

class Registry::Lock {
public:
  explicit Lock(const Registry& registry)
      : m_lock(registry.m_mutex, registry.m_lock_owner) {}

  Lock(const Lock&) = delete;
  Lock(Lock&&) = delete;
  auto operator=(const Lock&) -> Lock& = delete;
  auto operator=(Lock&&) -> Lock& = delete;
  ~Lock() = default;

private:
  Detail::ScopedRegistryLock m_lock;
};

} // namespace Engine::Core
