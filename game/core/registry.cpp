#include "registry.h"

#include <algorithm>

namespace Engine::Core {

auto this_thread_lock_token() noexcept -> std::uint64_t {
  static std::atomic<std::uint64_t> next_token{1};
  static const thread_local std::uint64_t token =
      next_token.fetch_add(1, std::memory_order_relaxed);
  return token;
}

Registry::Registry() {
  m_slots.emplace_back();
}

Registry::~Registry() = default;

void Registry::detach_all_components(EntityID entity_id) {
  for (auto& store : m_storages) {
    if (store != nullptr) {
      store->erase(entity_id);
    }
  }
}

auto Registry::create_entity() -> EntityID {
  const Lock lock(*this);

  std::uint32_t index = 0;
  if (!m_free_slots.empty()) {
    index = m_free_slots.back();
    m_free_slots.pop_back();
  } else {
    index = static_cast<std::uint32_t>(m_slots.size());
    m_slots.emplace_back();
  }

  Slot& slot = m_slots[index];
  slot.alive = true;
  ++m_live_count;
  return Handle::make(index, slot.generation);
}

auto Registry::create_entity_with_id(EntityID entity_id) -> EntityID {
  const Lock lock(*this);
  if (entity_id == NULL_ENTITY) {
    return NULL_ENTITY;
  }

  const std::uint32_t index = Handle::index_of(entity_id);
  if (index == 0) {
    return NULL_ENTITY;
  }

  if (m_slots.size() <= index) {
    const auto previous_size = m_slots.size();
    m_slots.resize(static_cast<std::size_t>(index) + 1U);
    for (std::size_t i = previous_size; i < index; ++i) {
      m_free_slots.push_back(static_cast<std::uint32_t>(i));
    }
  } else {
    std::erase(m_free_slots, index);
  }

  Slot& slot = m_slots[index];
  if (slot.alive) {
    detach_all_components(Handle::make(index, slot.generation));
    slot.alive = false;
    --m_live_count;
  }

  slot.generation = Handle::generation_of(entity_id);
  slot.alive = true;
  ++m_live_count;
  return entity_id;
}

auto Registry::destroy_entity(EntityID entity_id) -> bool {
  const Lock lock(*this);

  const std::uint32_t index = Handle::index_of(entity_id);
  if (index == 0 || index >= m_slots.size()) {
    return false;
  }
  Slot& slot = m_slots[index];
  if (!slot.alive || slot.generation != Handle::generation_of(entity_id)) {
    return false;
  }

  detach_all_components(entity_id);
  slot.alive = false;
  ++slot.generation;
  m_free_slots.push_back(index);
  --m_live_count;
  return true;
}

void Registry::clear() {
  const Lock lock(*this);

  for (std::size_t i = 1; i < m_slots.size(); ++i) {
    if (m_slots[i].alive) {
      m_slots[i].alive = false;
      ++m_slots[i].generation;
    }
  }
  m_free_slots.clear();
  for (std::size_t i = m_slots.size(); i-- > 1;) {
    m_free_slots.push_back(static_cast<std::uint32_t>(i));
  }
  m_live_count = 0;

  for (auto& store : m_storages) {
    if (store != nullptr) {
      store->clear();
    }
  }
}

void Registry::reserve_indices_below(std::uint32_t index) {
  const Lock lock(*this);
  if (m_slots.size() >= index) {
    return;
  }
  const auto previous_size = m_slots.size();
  m_slots.resize(index);
  for (std::size_t i = previous_size; i < index; ++i) {
    m_free_slots.push_back(static_cast<std::uint32_t>(i));
  }
}

} // namespace Engine::Core
