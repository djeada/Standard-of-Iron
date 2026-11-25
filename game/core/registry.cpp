#include "registry.h"

#include <algorithm>
#include <atomic>

namespace Engine::Core {

auto this_thread_lock_token() noexcept -> std::uint64_t {
  static std::atomic<std::uint64_t> next_token{1};
  static const thread_local std::uint64_t token =
      next_token.fetch_add(1, std::memory_order_relaxed);
  return token;
}

namespace {
std::atomic<std::uint64_t> g_registry_instances{0};
}

Registry::Registry()
    : m_instance_id(g_registry_instances.fetch_add(1, std::memory_order_relaxed) + 1) {
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
  note_structure_change();

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
  note_structure_change();
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
  note_structure_change();

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
  note_structure_change();

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

auto Registry::capture_checkpoint() const -> RegistryCheckpoint {
  const Lock lock(*this);
  RegistryCheckpoint state;
  state.generations.reserve(m_slots.size());
  for (const auto& slot : m_slots) {
    state.generations.push_back(slot.generation);
  }
  state.free_slots = m_free_slots;
  for (const auto& store : m_storages) {
    if (store != nullptr && !store->empty()) {
      const auto ids = store->entities();
      state.component_orders.push_back(
          {store->type().name(), {ids.begin(), ids.end()}});
    }
  }
  std::sort(state.component_orders.begin(),
            state.component_orders.end(),
            [](const auto& left, const auto& right) { return left.type < right.type; });
  return state;
}

auto Registry::restore_checkpoint(const RegistryCheckpoint& state) -> bool {
  const Lock lock(*this);
  if (state.generations.size() != m_live_count + state.free_slots.size() + 1) {
    return false;
  }
  std::vector<bool> free(state.generations.size(), false);
  for (const auto index : state.free_slots) {
    if (index == 0 || index >= free.size() || free[index] ||
        entity_at_index(index) != NULL_ENTITY) {
      return false;
    }
    free[index] = true;
  }
  for (std::size_t index = 1; index < state.generations.size(); ++index) {
    if (!free[index] &&
        entity_at_index(static_cast<std::uint32_t>(index)) !=
            Handle::make(static_cast<std::uint32_t>(index), state.generations[index])) {
      return false;
    }
  }
  for (auto& store : m_storages) {
    if (store == nullptr || store->empty()) {
      continue;
    }
    const auto found = std::find_if(
        state.component_orders.begin(),
        state.component_orders.end(),
        [&](const auto& order) { return order.type == store->type().name(); });
    if (found == state.component_orders.end()) {
      continue;
    }
    std::uint32_t next = 0;
    for (const auto id : found->entities) {
      const auto index = store->dense_index_of(id);
      if (index == IComponentStorage::k_absent) {
        continue;
      }
      if (index < next) {
        return false;
      }
      store->swap_dense(next++, index);
    }
  }
  m_slots.resize(state.generations.size());
  for (std::size_t index = 0; index < m_slots.size(); ++index) {
    m_slots[index].generation = state.generations[index];
  }
  m_free_slots = state.free_slots;
  return true;
}

} // namespace Engine::Core
