#include "world_spatial_index.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "component_economy.h"
#include "world.h"

namespace Engine::Core {

namespace {

constexpr int k_max_cells_per_axis = 512;
constexpr float k_grid_margin = 4.0F;

} // namespace

WorldSpatialIndex::WorldSpatialIndex(float cell_size)
    : m_cell_size(cell_size > 0.0F ? cell_size : 1.0F)
    , m_inv_cell_size(1.0F / (cell_size > 0.0F ? cell_size : 1.0F)) {
}

auto WorldSpatialIndex::cell_of(float value, float origin) const -> int {
  return static_cast<int>(std::floor((value - origin) * m_inv_cell_size));
}

auto WorldSpatialIndex::clamp_cell(int cell, int count) -> int {
  return std::clamp(cell, 0, count - 1);
}

void WorldSpatialIndex::clear() {
  m_entries.clear();
  m_scratch.clear();
  m_cell_start.clear();
  m_cell_cursor.clear();
  m_cell_counts.clear();
  m_entry_by_slot.clear();
  m_cells_x = 0;
  m_cells_z = 0;
  m_ever_built = false;
  m_built_for_tick = 0;
}

void WorldSpatialIndex::refresh(const World& world) {
  const std::uint64_t tick = world.tick_id();
  if (tick != 0 && m_ever_built && m_built_for_tick == tick) {
    return;
  }
  rebuild(world);
}

void WorldSpatialIndex::rebuild(const World& world) {
  m_built_for_tick = world.tick_id();
  m_ever_built = true;
  ++m_stats.rebuilds;

  m_scratch.clear();

  float min_x = std::numeric_limits<float>::max();
  float min_z = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float max_z = std::numeric_limits<float>::lowest();

  for (auto [entity, transform, unit] :
       world.entity_view<TransformComponent, UnitComponent>()) {
    Entry entry;
    entry.id = entity.get_id();
    entry.x = transform.position.x;
    entry.z = transform.position.z;
    entry.owner_id = unit.owner_id;
    entry.health = unit.health;

    std::uint16_t flags = k_none;
    if (unit.health > 0) {
      flags |= k_alive;
    }
    if (entity.has_component<BuildingComponent>()) {
      flags |= k_building;
    }
    if (entity.has_component<WildlifeComponent>()) {
      flags |= k_wildlife;
    }
    if (entity.has_component<UndeadComponent>()) {
      flags |= k_undead;
    }
    if (entity.has_component<PendingRemovalComponent>()) {
      flags |= k_pending_removal;
    }
    entry.flags = flags;

    min_x = std::min(min_x, entry.x);
    max_x = std::max(max_x, entry.x);
    min_z = std::min(min_z, entry.z);
    max_z = std::max(max_z, entry.z);

    m_scratch.push_back(entry);
  }

  m_stats.entries_indexed += m_scratch.size();

  if (m_scratch.empty()) {
    m_entries.clear();
    m_cell_start.assign(1, 0);
    m_entry_by_slot.clear();
    m_cells_x = 0;
    m_cells_z = 0;
    return;
  }

  m_origin_x = min_x - k_grid_margin;
  m_origin_z = min_z - k_grid_margin;
  const float span_x = (max_x + k_grid_margin) - m_origin_x;
  const float span_z = (max_z + k_grid_margin) - m_origin_z;
  m_cells_x = std::clamp(
      static_cast<int>(span_x * m_inv_cell_size) + 1, 1, k_max_cells_per_axis);
  m_cells_z = std::clamp(
      static_cast<int>(span_z * m_inv_cell_size) + 1, 1, k_max_cells_per_axis);

  const std::size_t cell_count =
      static_cast<std::size_t>(m_cells_x) * static_cast<std::size_t>(m_cells_z);

  m_cell_counts.assign(cell_count, 0U);
  for (const Entry& entry : m_scratch) {
    const int cx = clamp_cell(cell_of(entry.x, m_origin_x), m_cells_x);
    const int cz = clamp_cell(cell_of(entry.z, m_origin_z), m_cells_z);
    ++m_cell_counts[static_cast<std::size_t>(cz) * static_cast<std::size_t>(m_cells_x) +
                    static_cast<std::size_t>(cx)];
  }

  m_cell_start.assign(cell_count + 1U, 0U);
  std::size_t running = 0;
  for (std::size_t cell = 0; cell < cell_count; ++cell) {
    m_cell_start[cell] = running;
    running += m_cell_counts[cell];
  }
  m_cell_start[cell_count] = running;

  m_entries.resize(m_scratch.size());
  m_cell_cursor.assign(m_cell_start.begin(), m_cell_start.end() - 1);
  for (const Entry& entry : m_scratch) {
    const int cx = clamp_cell(cell_of(entry.x, m_origin_x), m_cells_x);
    const int cz = clamp_cell(cell_of(entry.z, m_origin_z), m_cells_z);
    const std::size_t cell =
        static_cast<std::size_t>(cz) * static_cast<std::size_t>(m_cells_x) +
        static_cast<std::size_t>(cx);
    m_entries[m_cell_cursor[cell]++] = entry;
  }

  std::uint32_t highest_slot = 0;
  for (const Entry& entry : m_entries) {
    highest_slot = std::max(highest_slot, Handle::index_of(entry.id));
  }
  m_entry_by_slot.assign(static_cast<std::size_t>(highest_slot) + 1U, k_no_entry);
  for (std::size_t i = 0; i < m_entries.size(); ++i) {
    m_entry_by_slot[Handle::index_of(m_entries[i].id)] = static_cast<std::uint32_t>(i);
  }
}

void WorldSpatialIndex::query_radius(float x,
                                     float z,
                                     float radius,
                                     std::vector<const Entry*>& out) const {
  out.clear();
  for_each_in_radius(
      x, z, radius, [&out](const Entry& entry) { out.push_back(&entry); });
}

auto WorldSpatialIndex::find(EntityID id) const -> const Entry* {
  const std::uint32_t slot = Handle::index_of(id);
  if (slot >= m_entry_by_slot.size()) {
    return nullptr;
  }
  const std::uint32_t position = m_entry_by_slot[slot];
  if (position == k_no_entry || position >= m_entries.size()) {
    return nullptr;
  }
  const Entry& entry = m_entries[position];
  return entry.id == id ? &entry : nullptr;
}

} // namespace Engine::Core
