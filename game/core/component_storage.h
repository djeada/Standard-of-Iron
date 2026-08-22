#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <span>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

#include "component_registry.h"
#include "entity_id.h"

namespace Engine::Core {

class IComponentStorage {
public:
  static constexpr std::uint32_t k_absent = 0xFFFFFFFFU;

  explicit IComponentStorage(ComponentTypeId type_id, std::type_index type)
      : m_type_id(type_id)
      , m_type(type) {}

  IComponentStorage(const IComponentStorage&) = delete;
  IComponentStorage(IComponentStorage&&) = delete;
  auto operator=(const IComponentStorage&) -> IComponentStorage& = delete;
  auto operator=(IComponentStorage&&) -> IComponentStorage& = delete;
  virtual ~IComponentStorage() = default;

  [[nodiscard]] auto type_id() const noexcept -> ComponentTypeId { return m_type_id; }
  [[nodiscard]] auto type() const noexcept -> std::type_index { return m_type; }

  [[nodiscard]] auto entities() const noexcept -> std::span<const EntityID> {
    return m_dense;
  }
  [[nodiscard]] auto size() const noexcept -> std::size_t { return m_dense.size(); }
  [[nodiscard]] auto empty() const noexcept -> bool { return m_dense.empty(); }

  [[nodiscard]] auto
  dense_index_of(EntityID entity_id) const noexcept -> std::uint32_t {
    const std::uint32_t index = Handle::index_of(entity_id);
    if (index >= m_sparse.size()) {
      return k_absent;
    }
    const std::uint32_t position = m_sparse[index];
    if (position == k_absent || m_dense[position] != entity_id) {
      return k_absent;
    }
    return position;
  }

  [[nodiscard]] auto contains(EntityID entity_id) const noexcept -> bool {
    return dense_index_of(entity_id) != k_absent;
  }

  virtual auto erase(EntityID entity_id) -> bool = 0;
  virtual void clear() = 0;

protected:
  auto track(EntityID entity_id) -> std::uint32_t {
    const std::uint32_t index = Handle::index_of(entity_id);
    if (m_sparse.size() <= index) {
      m_sparse.resize(static_cast<std::size_t>(index) + 1U, k_absent);
    }
    const auto position = static_cast<std::uint32_t>(m_dense.size());
    m_sparse[index] = position;
    m_dense.push_back(entity_id);
    return position;
  }

  void untrack(std::uint32_t position) {
    const EntityID removed = m_dense[position];
    const EntityID moved = m_dense.back();
    m_dense[position] = moved;
    m_sparse[Handle::index_of(moved)] = position;
    m_dense.pop_back();
    m_sparse[Handle::index_of(removed)] = k_absent;
  }

  void reset_tracking() {
    m_dense.clear();
    m_sparse.clear();
  }

private:
  ComponentTypeId m_type_id;
  std::type_index m_type;
  std::vector<EntityID> m_dense;
  std::vector<std::uint32_t> m_sparse;
};

template <typename T>
class ComponentStorage final : public IComponentStorage {
public:
  static constexpr std::size_t k_page_size = 128;

  ComponentStorage()
      : IComponentStorage(component_type_id<T>(), std::type_index(typeid(T))) {}

  ~ComponentStorage() override { destroy_all(); }

  template <typename... Args>
  auto emplace(EntityID entity_id, Args&&... args) -> T& {
    const std::uint32_t existing = dense_index_of(entity_id);
    if (existing != k_absent) {
      T* slot = element(m_slot_by_dense[existing]);
      if constexpr (std::is_move_assignable_v<T>) {
        T replacement(std::forward<Args>(args)...);
        *slot = std::move(replacement);
        return *slot;
      } else {
        slot->~T();
        return *(new (static_cast<void*>(slot)) T(std::forward<Args>(args)...));
      }
    }

    const std::uint32_t slot_index = acquire_slot();
    T* component = nullptr;
    try {
      component =
          new (static_cast<void*>(element(slot_index))) T(std::forward<Args>(args)...);
    } catch (...) {
      m_free_slots.push_back(slot_index);
      throw;
    }
    track(entity_id);
    m_slot_by_dense.push_back(slot_index);
    return *component;
  }

  [[nodiscard]] auto try_get(EntityID entity_id) noexcept -> T* {
    const std::uint32_t position = dense_index_of(entity_id);
    return position == k_absent ? nullptr : element(m_slot_by_dense[position]);
  }

  [[nodiscard]] auto try_get(EntityID entity_id) const noexcept -> const T* {
    const std::uint32_t position = dense_index_of(entity_id);
    return position == k_absent ? nullptr : element(m_slot_by_dense[position]);
  }

  [[nodiscard]] auto at(std::size_t position) noexcept -> T& {
    return *element(m_slot_by_dense[position]);
  }

  [[nodiscard]] auto at(std::size_t position) const noexcept -> const T& {
    return *element(m_slot_by_dense[position]);
  }

  auto erase(EntityID entity_id) -> bool override {
    const std::uint32_t position = dense_index_of(entity_id);
    if (position == k_absent) {
      return false;
    }

    const std::uint32_t slot_index = m_slot_by_dense[position];
    element(slot_index)->~T();
    m_free_slots.push_back(slot_index);

    m_slot_by_dense[position] = m_slot_by_dense.back();
    m_slot_by_dense.pop_back();
    untrack(position);
    return true;
  }

  void clear() override {
    destroy_all();
    m_slot_by_dense.clear();
    m_free_slots.clear();
    m_used_slots = 0;
    reset_tracking();
  }

private:
  struct Page {
    alignas(T) std::array<std::byte, sizeof(T) * k_page_size> storage;
  };

  [[nodiscard]] auto element(std::uint32_t slot_index) noexcept -> T* {
    Page& page = *m_pages[slot_index / k_page_size];
    return std::launder(reinterpret_cast<T*>(page.storage.data()) +
                        (slot_index % k_page_size));
  }

  [[nodiscard]] auto element(std::uint32_t slot_index) const noexcept -> const T* {
    const Page& page = *m_pages[slot_index / k_page_size];
    return std::launder(reinterpret_cast<const T*>(page.storage.data()) +
                        (slot_index % k_page_size));
  }

  auto acquire_slot() -> std::uint32_t {
    if (!m_free_slots.empty()) {
      const std::uint32_t slot_index = m_free_slots.back();
      m_free_slots.pop_back();
      return slot_index;
    }
    const auto slot_index = static_cast<std::uint32_t>(m_used_slots++);
    const std::size_t page_index = slot_index / k_page_size;
    while (m_pages.size() <= page_index) {
      m_pages.push_back(std::make_unique<Page>());
    }
    return slot_index;
  }

  void destroy_all() {
    for (const std::uint32_t slot_index : m_slot_by_dense) {
      element(slot_index)->~T();
    }
  }

  std::vector<std::unique_ptr<Page>> m_pages;
  std::vector<std::uint32_t> m_slot_by_dense;
  std::vector<std::uint32_t> m_free_slots;
  std::size_t m_used_slots = 0;
};

} // namespace Engine::Core
