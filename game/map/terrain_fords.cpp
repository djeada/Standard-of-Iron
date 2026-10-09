#include <QDebug>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <vector>

#include "bridge_geometry.h"
#include "terrain.h"
#include "terrain_features.h"

namespace Game::Map {

namespace {

constexpr float k_min_tile = 0.0001F;

// Room beyond the drawn water that a ford zone still claims across the river,
// so the bank clearance strip on both sides opens with it.
constexpr float k_ford_lateral_slack = 1.5F;

// How far into the water the bed takes to fall from the bank to the full
// ford depth. Scaled by the river, clamped so a narrow stream still gets a
// shelf to wade in on and a wide river does not drop off a cliff.
constexpr float k_ford_shelf_fraction = 0.45F;
constexpr float k_ford_min_shelf = 0.8F;
constexpr float k_ford_max_shelf = 3.0F;

// A man counts as wading once the water is over his ankles.
constexpr float k_min_wading_depth = 0.05F;

struct ReachCell {
  int x;
  int z;
  // World-space distance from the drawn centre line, signed by side.
  float lateral;
  // Half-width of the drawn water at this point of the river, world units.
  float drawn_half_width;
  // 0..1 position along the segment.
  float t;
};

// Visits every grid cell inside the blocked reach of a river segment: the
// drawn water plus the bank clearance strip. Mirrors precompute_water_blocked.
template <typename Visit>
void for_each_river_reach_cell(
    const RiverSegment& river, int width, int height, float tile_size, Visit&& visit) {
  const float delta_x = river.end.x() - river.start.x();
  const float delta_z = river.end.z() - river.start.z();
  if ((delta_x * delta_x) + (delta_z * delta_z) < 1.0e-4F) {
    return;
  }
  const float tile = std::max(tile_size, k_min_tile);
  const float grid_half_width = static_cast<float>(width) * 0.5F - 0.5F;
  const float grid_half_height = static_cast<float>(height) * 0.5F - 0.5F;
  const float blocked_half_cells = river_bank_standing_half_width(river.width) / tile;

  const float start_x = (river.start.x() / tile) + grid_half_width;
  const float start_z = (river.start.z() / tile) + grid_half_height;
  const float end_x = (river.end.x() / tile) + grid_half_width;
  const float end_z = (river.end.z() / tile) + grid_half_height;

  const int min_x = std::max(
      0, static_cast<int>(std::floor(std::min(start_x, end_x) - blocked_half_cells)));
  const int max_x = std::min(
      width - 1,
      static_cast<int>(std::ceil(std::max(start_x, end_x) + blocked_half_cells)));
  const int min_z = std::max(
      0, static_cast<int>(std::floor(std::min(start_z, end_z) - blocked_half_cells)));
  const int max_z = std::min(
      height - 1,
      static_cast<int>(std::ceil(std::max(start_z, end_z) + blocked_half_cells)));

  const float span_x = end_x - start_x;
  const float span_z = end_z - start_z;
  const float span_length_sq = (span_x * span_x) + (span_z * span_z);
  const float span_length = std::sqrt(span_length_sq);
  const float perp_x = -span_z / span_length;
  const float perp_z = span_x / span_length;

  for (int z = min_z; z <= max_z; ++z) {
    for (int x = min_x; x <= max_x; ++x) {
      const float dx = static_cast<float>(x) - start_x;
      const float dz = static_cast<float>(z) - start_z;
      const float raw_t = ((dx * span_x) + (dz * span_z)) / span_length_sq;
      const float t = std::clamp(raw_t, 0.0F, 1.0F);

      const RibbonCrossSection section = river_drawn_cross_section(river, t);
      const float center_x = (section.center.x() / tile) + grid_half_width;
      const float center_z = (section.center.z() / tile) + grid_half_height;
      const float lateral = ((static_cast<float>(x) - center_x) * perp_x) +
                            ((static_cast<float>(z) - center_z) * perp_z);
      const float beyond_end = (raw_t - t) * span_length;
      const float drawn_half = std::max(section.half_width, river.width * 0.5F);
      const float reach = (drawn_half + k_water_bank_clearance) / tile;
      if ((lateral * lateral) + (beyond_end * beyond_end) > reach * reach) {
        continue;
      }
      visit(ReachCell{.x = x,
                      .z = z,
                      .lateral = lateral * tile,
                      .drawn_half_width = section.half_width,
                      .t = t});
    }
  }
}

struct RiverHit {
  std::size_t segment = 0;
  float t = 0.0F;
  float distance = std::numeric_limits<float>::max();
};

auto nearest_river(const std::vector<RiverSegment>& rivers,
                   const QVector3D& point) -> std::optional<RiverHit> {
  std::optional<RiverHit> best;
  for (std::size_t index = 0; index < rivers.size(); ++index) {
    const RiverSegment& river = rivers[index];
    const float delta_x = river.end.x() - river.start.x();
    const float delta_z = river.end.z() - river.start.z();
    const float length_sq = (delta_x * delta_x) + (delta_z * delta_z);
    if (length_sq < 1.0e-6F) {
      continue;
    }
    const float t = std::clamp((((point.x() - river.start.x()) * delta_x) +
                                ((point.z() - river.start.z()) * delta_z)) /
                                   length_sq,
                               0.0F,
                               1.0F);
    const RibbonCrossSection section = river_drawn_cross_section(river, t);
    const float distance =
        std::hypot(point.x() - section.center.x(), point.z() - section.center.z());
    if (distance >
        std::max(section.half_width, river.width * 0.5F) + k_water_bank_clearance) {
      continue;
    }
    if (!best.has_value() || distance < best->distance) {
      best = RiverHit{.segment = index, .t = t, .distance = distance};
    }
  }
  return best;
}

auto water_y_along(const RiverSegment& river, float t) -> float {
  return river.start.y() + ((river.end.y() - river.start.y()) * t);
}

} // namespace

void TerrainHeightMap::add_fords(const std::vector<FordCrossing>& fords) {
  m_fords.clear();
  m_fords.reserve(fords.size());
  for (const FordCrossing& authored : fords) {
    FordCrossing ford = authored;
    ford.profile = ford.profile.clamped();
    ford.length = std::clamp(ford.length, k_min_ford_length, k_max_ford_length);
    if (!nearest_river(m_river_segments, ford.position).has_value()) {
      qWarning() << "Ford" << ford.id << "at" << ford.position.x() << ford.position.z()
                 << "is not on a river - skipping";
      continue;
    }
    m_fords.push_back(ford);
  }
  precompute_ford_cells(true);
}

auto TerrainHeightMap::ford_table_index(const FordProfile& profile,
                                        float water_y) -> std::uint8_t {
  for (std::size_t index = 0; index < m_ford_table.size(); ++index) {
    const FordCell& entry = m_ford_table[index];
    if (entry.profile == profile && std::abs(entry.water_y - water_y) < 0.01F) {
      return static_cast<std::uint8_t>(index + 1U);
    }
  }
  if (m_ford_table.size() >= std::numeric_limits<std::uint8_t>::max()) {
    return 0U;
  }
  m_ford_table.push_back({profile, water_y});
  return static_cast<std::uint8_t>(m_ford_table.size());
}

void TerrainHeightMap::stamp_ford_cell(int x, int z, std::uint8_t entry) {
  if (entry == 0U || !in_bounds(x, z)) {
    return;
  }
  const auto index = static_cast<std::size_t>(indexAt(x, z));
  if (m_terrain_types[index] == TerrainType::Mountain) {
    return;
  }
  const bool wet = is_water_terrain(m_terrain_types[index]) ||
                   (index < m_water_blocked.size() && m_water_blocked[index]);
  if (!wet) {
    return;
  }
  m_ford_cells[index] = entry;
}

void TerrainHeightMap::precompute_ford_cells(bool lower_beds) {
  const auto count =
      static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
  m_ford_table.clear();
  m_ford_cells.assign(count, 0U);

  const float tile = std::max(m_tile_size, k_min_tile);

  const auto lower_bed = [this, lower_beds](const ReachCell& cell,
                                            const FordCell& ford) {
    if (!lower_beds) {
      return;
    }
    const float inside = cell.drawn_half_width - std::abs(cell.lateral);
    if (inside <= 0.0F) {
      return;
    }
    const float shelf = std::clamp(cell.drawn_half_width * k_ford_shelf_fraction,
                                   k_ford_min_shelf,
                                   k_ford_max_shelf);
    const float ramp = std::clamp(inside / shelf, 0.0F, 1.0F);
    const float smooth = ramp * ramp * (3.0F - (2.0F * ramp));
    const float bed = ford.water_y - (ford.profile.depth * smooth);
    const auto index = static_cast<std::size_t>(indexAt(cell.x, cell.z));
    m_heights[index] = std::min(m_heights[index], bed);
  };

  for (const RiverSegment& river : m_river_segments) {
    if (!river.ford.has_value()) {
      continue;
    }
    const FordProfile profile = river.ford->clamped();
    for_each_river_reach_cell(
        river, m_width, m_height, tile, [&](const ReachCell& cell) {
          const float water_y = water_y_along(river, cell.t);
          const std::uint8_t entry = ford_table_index(profile, water_y);
          stamp_ford_cell(cell.x, cell.z, entry);
          if (entry != 0U &&
              m_ford_cells[static_cast<std::size_t>(indexAt(cell.x, cell.z))] ==
                  entry) {
            lower_bed(cell, m_ford_table[entry - 1U]);
          }
        });
  }

  for (const FordCrossing& ford : m_fords) {
    const auto hit = nearest_river(m_river_segments, ford.position);
    if (!hit.has_value()) {
      continue;
    }
    const RiverSegment& host = m_river_segments[hit->segment];
    QVector3D along = host.end - host.start;
    along.setY(0.0F);
    along.normalize();
    const QVector3D across(-along.z(), 0.0F, along.x());
    const float water_y = water_y_along(host, hit->t);
    const float half_length = ford.length * 0.5F;
    const float half_span =
        river_bank_standing_half_width(host.width) + k_ford_lateral_slack;
    const std::uint8_t entry = ford_table_index(ford.profile, water_y);
    if (entry == 0U) {
      continue;
    }

    const float grid_half_width = static_cast<float>(m_width) * 0.5F - 0.5F;
    const float grid_half_height = static_cast<float>(m_height) * 0.5F - 0.5F;
    for (const RiverSegment& river : m_river_segments) {
      for_each_river_reach_cell(
          river, m_width, m_height, tile, [&](const ReachCell& cell) {
            const QVector3D world((static_cast<float>(cell.x) - grid_half_width) * tile,
                                  0.0F,
                                  (static_cast<float>(cell.z) - grid_half_height) *
                                      tile);
            const QVector3D offset(
                world.x() - ford.position.x(), 0.0F, world.z() - ford.position.z());
            if (std::abs(QVector3D::dotProduct(offset, along)) > half_length ||
                std::abs(QVector3D::dotProduct(offset, across)) > half_span) {
              return;
            }
            stamp_ford_cell(cell.x, cell.z, entry);
            if (m_ford_cells[static_cast<std::size_t>(indexAt(cell.x, cell.z))] ==
                entry) {
              lower_bed(cell, m_ford_table[entry - 1U]);
            }
          });
    }
  }
}

auto TerrainHeightMap::is_ford_cell(int grid_x, int grid_z) const -> bool {
  return ford_profile_at_grid(grid_x, grid_z) != nullptr;
}

auto TerrainHeightMap::ford_profile_at_grid(int grid_x,
                                            int grid_z) const -> const FordProfile* {
  if (!in_bounds(grid_x, grid_z) || m_ford_cells.empty()) {
    return nullptr;
  }
  const std::uint8_t entry =
      m_ford_cells[static_cast<std::size_t>(indexAt(grid_x, grid_z))];
  if (entry == 0U || entry > m_ford_table.size()) {
    return nullptr;
  }
  return &m_ford_table[entry - 1U].profile;
}

auto TerrainHeightMap::ford_entry_at(float world_x,
                                     float world_z) const -> const FordCell* {
  if (m_ford_cells.empty()) {
    return nullptr;
  }
  const float tile = std::max(m_tile_size, k_min_tile);
  const float grid_half_width = static_cast<float>(m_width) * 0.5F - 0.5F;
  const float grid_half_height = static_cast<float>(m_height) * 0.5F - 0.5F;
  const int grid_x = static_cast<int>(std::lround((world_x / tile) + grid_half_width));
  const int grid_z = static_cast<int>(std::lround((world_z / tile) + grid_half_height));
  if (!in_bounds(grid_x, grid_z)) {
    return nullptr;
  }
  const std::uint8_t entry =
      m_ford_cells[static_cast<std::size_t>(indexAt(grid_x, grid_z))];
  if (entry == 0U || entry > m_ford_table.size()) {
    return nullptr;
  }
  return &m_ford_table[entry - 1U];
}

auto TerrainHeightMap::ford_profile_at(float world_x,
                                       float world_z) const -> const FordProfile* {
  const FordCell* entry = ford_entry_at(world_x, world_z);
  return entry != nullptr ? &entry->profile : nullptr;
}

auto TerrainHeightMap::ford_water_level_at(float world_x, float world_z) const
    -> std::optional<float> {
  const FordCell* entry = ford_entry_at(world_x, world_z);
  if (entry == nullptr) {
    return std::nullopt;
  }
  return entry->water_y;
}

auto TerrainHeightMap::ford_water_depth_at(float world_x,
                                           float world_z) const -> float {
  const FordCell* entry = ford_entry_at(world_x, world_z);
  if (entry == nullptr) {
    return 0.0F;
  }
  const float depth = entry->water_y - get_base_height_at(world_x, world_z);
  return depth > k_min_wading_depth ? depth : 0.0F;
}

} // namespace Game::Map
