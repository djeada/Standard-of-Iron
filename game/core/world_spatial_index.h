#pragma once

#include <cstdint>
#include <vector>

#include "entity.h"

namespace Engine::Core {

class World;

class WorldSpatialIndex {
public:
  enum Flags : std::uint16_t {
    k_none = 0U,
    k_building = 1U << 0U,
    k_wildlife = 1U << 1U,
    k_undead = 1U << 2U,
    k_pending_removal = 1U << 3U,
    k_alive = 1U << 4U,
  };

  struct Entry {
    EntityID id{0};
    float x{0.0F};
    float z{0.0F};
    int owner_id{0};
    int health{0};
    std::uint16_t flags{k_none};

    [[nodiscard]] auto is(std::uint16_t flag) const noexcept -> bool {
      return (flags & flag) != 0U;
    }
  };

  explicit WorldSpatialIndex(float cell_size = 8.0F);

  void refresh(World& world);

  void rebuild(World& world);

  void clear();

  void
  query_radius(float x, float z, float radius, std::vector<const Entry*>& out) const;

  template <typename Fn>
  void for_each_in_radius(float x, float z, float radius, Fn&& fn) const {
    visit_cells(x, z, radius, [&](const Entry& entry) {
      const float dx = entry.x - x;
      const float dz = entry.z - z;
      if (dx * dx + dz * dz <= radius * radius) {
        fn(entry);
      }
    });
  }

  [[nodiscard]] auto find(EntityID id) const -> const Entry*;

  [[nodiscard]] auto entries() const -> const std::vector<Entry>& { return m_entries; }

  [[nodiscard]] auto entry_count() const -> std::size_t { return m_entries.size(); }

  struct Stats {
    std::uint64_t rebuilds{0};
    std::uint64_t queries{0};
    std::uint64_t candidates_examined{0};
    std::uint64_t entries_indexed{0};
  };

  [[nodiscard]] auto stats() const -> const Stats& { return m_stats; }
  void reset_stats() { m_stats = {}; }

private:
  template <typename Fn>
  void visit_cells(float x, float z, float radius, Fn&& fn) const {
    ++m_stats.queries;
    if (m_cells_x <= 0 || m_cells_z <= 0 || m_entries.empty()) {
      return;
    }
    const int min_cx = clamp_cell(cell_of(x - radius, m_origin_x), m_cells_x);
    const int max_cx = clamp_cell(cell_of(x + radius, m_origin_x), m_cells_x);
    const int min_cz = clamp_cell(cell_of(z - radius, m_origin_z), m_cells_z);
    const int max_cz = clamp_cell(cell_of(z + radius, m_origin_z), m_cells_z);

    for (int cz = min_cz; cz <= max_cz; ++cz) {
      const std::size_t row =
          static_cast<std::size_t>(cz) * static_cast<std::size_t>(m_cells_x);
      for (int cx = min_cx; cx <= max_cx; ++cx) {
        const std::size_t cell = row + static_cast<std::size_t>(cx);
        const std::size_t begin = m_cell_start[cell];
        const std::size_t end = m_cell_start[cell + 1];
        m_stats.candidates_examined += end - begin;
        for (std::size_t i = begin; i < end; ++i) {
          fn(m_entries[i]);
        }
      }
    }
  }

  [[nodiscard]] auto cell_of(float value, float origin) const -> int;
  [[nodiscard]] static auto clamp_cell(int cell, int count) -> int;

  float m_cell_size;
  float m_inv_cell_size;

  float m_origin_x{0.0F};
  float m_origin_z{0.0F};
  int m_cells_x{0};
  int m_cells_z{0};

  static constexpr std::uint32_t k_no_entry = 0xFFFFFFFFU;

  std::vector<Entry> m_entries;
  std::vector<Entry> m_scratch;
  std::vector<std::size_t> m_cell_start;
  std::vector<std::size_t> m_cell_cursor;
  std::vector<std::uint32_t> m_cell_counts;

  std::vector<std::uint32_t> m_entry_by_slot;

  std::uint64_t m_built_for_tick{0};
  bool m_ever_built{false};

  mutable Stats m_stats;
};

} // namespace Engine::Core
