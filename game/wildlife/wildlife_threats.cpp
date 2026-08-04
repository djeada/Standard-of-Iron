#include "wildlife_threats.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Game::Wildlife {

namespace {

constexpr float k_cell_size = 8.0F;
constexpr int k_max_grid_side = 512;

} // namespace

void ThreatField::clear() {
  m_sources.clear();
  m_order.clear();
  m_cells.clear();
  m_width = 0;
  m_height = 0;
  m_indexed = false;
}

void ThreatField::add(const ThreatSource& source) {
  m_sources.push_back(source);
  m_indexed = false;
}

auto ThreatField::cell_index(int cell_x, int cell_z) const noexcept -> std::size_t {
  return static_cast<std::size_t>(cell_z) * static_cast<std::size_t>(m_width) +
         static_cast<std::size_t>(cell_x);
}

void ThreatField::finalize() {
  m_order.clear();
  m_cells.clear();
  m_width = 0;
  m_height = 0;
  m_indexed = false;
  if (m_sources.empty()) {
    return;
  }

  float min_x = std::numeric_limits<float>::max();
  float min_z = std::numeric_limits<float>::max();
  float max_x = std::numeric_limits<float>::lowest();
  float max_z = std::numeric_limits<float>::lowest();
  for (const auto& source : m_sources) {
    min_x = std::min(min_x, source.x);
    min_z = std::min(min_z, source.z);
    max_x = std::max(max_x, source.x);
    max_z = std::max(max_z, source.z);
  }

  m_min_x = min_x;
  m_min_z = min_z;
  m_width = static_cast<int>(std::floor((max_x - min_x) / k_cell_size)) + 1;
  m_height = static_cast<int>(std::floor((max_z - min_z) / k_cell_size)) + 1;
  if (m_width <= 0 || m_height <= 0 || m_width > k_max_grid_side ||
      m_height > k_max_grid_side) {
    m_width = 0;
    m_height = 0;
    return;
  }

  m_cells.assign(static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height),
                 Cell{});
  std::vector<std::uint32_t> cell_of(m_sources.size(), 0U);
  for (std::size_t i = 0; i < m_sources.size(); ++i) {
    int const cell_x = std::clamp(
        static_cast<int>(std::floor((m_sources[i].x - m_min_x) / k_cell_size)),
        0,
        m_width - 1);
    int const cell_z = std::clamp(
        static_cast<int>(std::floor((m_sources[i].z - m_min_z) / k_cell_size)),
        0,
        m_height - 1);
    auto const index = cell_index(cell_x, cell_z);
    cell_of[i] = static_cast<std::uint32_t>(index);
    m_cells[index].count += 1U;
  }

  std::uint32_t running = 0U;
  for (auto& cell : m_cells) {
    cell.first = running;
    running += cell.count;
    cell.count = 0U;
  }

  m_order.resize(m_sources.size());
  for (std::size_t i = 0; i < m_sources.size(); ++i) {
    auto& cell = m_cells[cell_of[i]];
    m_order[cell.first + cell.count] = static_cast<std::uint32_t>(i);
    cell.count += 1U;
  }
  m_indexed = true;
}

template <typename Visitor>
void ThreatField::visit(float world_x,
                        float world_z,
                        float radius,
                        Visitor&& visitor) const {
  if (m_sources.empty()) {
    return;
  }
  if (!m_indexed) {
    for (const auto& source : m_sources) {
      visitor(source);
    }
    return;
  }

  int const min_cell_x = std::clamp(
      static_cast<int>(std::floor((world_x - radius - m_min_x) / k_cell_size)),
      0,
      m_width - 1);
  int const max_cell_x = std::clamp(
      static_cast<int>(std::floor((world_x + radius - m_min_x) / k_cell_size)),
      0,
      m_width - 1);
  int const min_cell_z = std::clamp(
      static_cast<int>(std::floor((world_z - radius - m_min_z) / k_cell_size)),
      0,
      m_height - 1);
  int const max_cell_z = std::clamp(
      static_cast<int>(std::floor((world_z + radius - m_min_z) / k_cell_size)),
      0,
      m_height - 1);

  for (int cell_z = min_cell_z; cell_z <= max_cell_z; ++cell_z) {
    for (int cell_x = min_cell_x; cell_x <= max_cell_x; ++cell_x) {
      const auto& cell = m_cells[cell_index(cell_x, cell_z)];
      for (std::uint32_t offset = 0U; offset < cell.count; ++offset) {
        visitor(m_sources[m_order[cell.first + offset]]);
      }
    }
  }
}

auto ThreatField::nearest(float world_x,
                          float world_z,
                          float radius,
                          bool civilians_only) const -> ThreatQuery {
  ThreatQuery result;
  float best_sq = radius * radius;
  visit(world_x, world_z, radius, [&](const ThreatSource& source) {
    if (civilians_only && !source.civilian) {
      return;
    }
    float const dx = source.x - world_x;
    float const dz = source.z - world_z;
    float const distance_sq = (dx * dx) + (dz * dz);
    if (distance_sq > best_sq) {
      return;
    }
    best_sq = distance_sq;
    result.found = true;
    result.x = source.x;
    result.z = source.z;
    result.distance = std::sqrt(distance_sq);
    result.strength = source.strength;
    result.civilian = source.civilian;
  });
  return result;
}

auto ThreatField::strength_within(float world_x,
                                  float world_z,
                                  float radius) const -> float {
  float total = 0.0F;
  float const radius_sq = radius * radius;
  visit(world_x, world_z, radius, [&](const ThreatSource& source) {
    float const dx = source.x - world_x;
    float const dz = source.z - world_z;
    if ((dx * dx) + (dz * dz) > radius_sq) {
      return;
    }
    total += source.strength;
  });
  return total;
}

} // namespace Game::Wildlife
