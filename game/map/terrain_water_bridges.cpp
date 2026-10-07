#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <vector>

#include "bridge_geometry.h"
#include "terrain.h"
#include "terrain_features.h"

namespace Game::Map {

namespace {

constexpr float k_river_bed_depth = 0.10F;
constexpr float k_minimum_bank_blend_cells = 4.5F;
constexpr float k_connected_endpoint_epsilon_sq = 0.0001F;

void level_connected_rivers(std::vector<RiverSegment>& rivers) {
  const auto endpoints_connect = [](const RiverSegment& lhs, const RiverSegment& rhs) {
    const auto points_match = [](const QVector3D& first, const QVector3D& second) {
      const float dx = first.x() - second.x();
      const float dz = first.z() - second.z();
      return dx * dx + dz * dz <= k_connected_endpoint_epsilon_sq;
    };
    return points_match(lhs.start, rhs.start) || points_match(lhs.start, rhs.end) ||
           points_match(lhs.end, rhs.start) || points_match(lhs.end, rhs.end);
  };

  std::vector<bool> flattened(rivers.size(), false);
  for (std::size_t root = 0; root < rivers.size(); ++root) {
    if (flattened[root] || rivers[root].elevation_mode != WaterElevationMode::Terrain) {
      continue;
    }

    std::vector<std::size_t> component{root};
    flattened[root] = true;
    float water_height = std::min(rivers[root].start.y(), rivers[root].end.y());

    for (std::size_t cursor = 0; cursor < component.size(); ++cursor) {
      const RiverSegment& connected = rivers[component[cursor]];
      for (std::size_t candidate = 0; candidate < rivers.size(); ++candidate) {
        if (flattened[candidate] ||
            rivers[candidate].elevation_mode != WaterElevationMode::Terrain ||
            !endpoints_connect(connected, rivers[candidate])) {
          continue;
        }
        flattened[candidate] = true;
        component.push_back(candidate);
        water_height =
            std::min(water_height,
                     std::min(rivers[candidate].start.y(), rivers[candidate].end.y()));
      }
    }

    for (const std::size_t index : component) {
      rivers[index].start.setY(water_height);
      rivers[index].end.setY(water_height);
    }
  }
}

} // namespace

void TerrainHeightMap::add_river_segments(
    const std::vector<RiverSegment>& river_segments) {
  m_river_segments.clear();
  m_river_segments.reserve(river_segments.size());
  auto water_surface_height = [this](const QVector3D& point) {
    const float shoreline_blend = m_tile_size * 1.25F;
    for (const auto& lake : m_lakes) {
      if (point_in_lake(lake, point.x(), point.z(), shoreline_blend)) {
        return lake.center.y();
      }
    }
    return get_height_at(point.x(), point.z());
  };

  for (const auto& authored_river : river_segments) {
    RiverSegment river = authored_river;
    if (river.elevation_mode == WaterElevationMode::Terrain) {
      river.start.setY(water_surface_height(river.start));
      river.end.setY(water_surface_height(river.end));
    }
    m_river_segments.push_back(river);
  }
  level_connected_rivers(m_river_segments);
  carve_river_channels();
  precompute_water_blocked();
}

void TerrainHeightMap::carve_river_channels() {
  const float campaign_bank_blend_cells = std::clamp(
      std::max(m_width, m_height) * 0.025F, k_minimum_bank_blend_cells, 16.0F);
  const std::vector<float> land_before_rivers = m_heights;

  for (const auto& river : m_river_segments) {
    carve_river_channel(river, land_before_rivers, campaign_bank_blend_cells);
  }
}

void TerrainHeightMap::carve_river_channel(const RiverSegment& river,
                                           const std::vector<float>& land_before_rivers,
                                           float campaign_bank_blend_cells) {
  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;
  const float delta_x = river.end.x() - river.start.x();
  const float delta_z = river.end.z() - river.start.z();
  const float horizontal_length = std::hypot(delta_x, delta_z);
  if (horizontal_length < 0.01F) {
    return;
  }

  const QVector3D horizontal_direction(
      delta_x / horizontal_length, 0.0F, delta_z / horizontal_length);

  int const steps = static_cast<int>(std::ceil(horizontal_length / m_tile_size)) + 1;

  for (int i = 0; i < steps; ++i) {
    float const t =
        static_cast<float>(i) / std::max(1.0F, static_cast<float>(steps - 1));
    QVector3D const center_pos = river.start + (river.end - river.start) * t;

    float const grid_center_x = (center_pos.x() / m_tile_size) + grid_half_width;
    float const grid_center_z = (center_pos.z() / m_tile_size) + grid_half_height;

    float const half_width = river.width * 0.5F / m_tile_size;
    float const bank_blend_width =
        std::max(campaign_bank_blend_cells, half_width * 0.90F);

    float const channel_extent =
        half_width + std::max(bank_blend_width, k_river_bank_max_blend_cells);

    int const min_x = std::max(
        0, static_cast<int>(std::floor(grid_center_x - channel_extent - 1.0F)));
    int const max_x =
        std::min(m_width - 1,
                 static_cast<int>(std::ceil(grid_center_x + channel_extent + 1.0F)));
    int const min_z = std::max(
        0, static_cast<int>(std::floor(grid_center_z - channel_extent - 1.0F)));
    int const max_z =
        std::min(m_height - 1,
                 static_cast<int>(std::ceil(grid_center_z + channel_extent + 1.0F)));

    for (int z = min_z; z <= max_z; ++z) {
      for (int x = min_x; x <= max_x; ++x) {

        float const from_start_x =
            static_cast<float>(x) - ((river.start.x() / m_tile_size) + grid_half_width);
        float const from_start_z = static_cast<float>(z) -
                                   ((river.start.z() / m_tile_size) + grid_half_height);
        float const length_cells = horizontal_length / m_tile_size;
        float const along = std::clamp((from_start_x * horizontal_direction.x()) +
                                           (from_start_z * horizontal_direction.z()),
                                       0.0F,
                                       length_cells);
        float const dx = from_start_x - (horizontal_direction.x() * along);
        float const dz = from_start_z - (horizontal_direction.z() * along);
        float const dist_along_perp = std::hypot(dx, dz);

        if (dist_along_perp > channel_extent) {
          continue;
        }

        int const idx = indexAt(x, z);
        float const along_t = length_cells > 0.0F ? along / length_cells : 0.0F;
        const float bed_height = river.start.y() +
                                 ((river.end.y() - river.start.y()) * along_t) -
                                 k_river_bed_depth;
        if (dist_along_perp <= half_width) {
          m_terrain_types[idx] = TerrainType::River;
          m_hill_entrances[idx] = false;
          m_hill_walkable[idx] = false;
          m_heights[idx] = bed_height;
          continue;
        }

        float const land_height = land_before_rivers[idx];
        float const drop = std::abs(land_height - bed_height);
        float const run_for_grade =
            (drop / k_river_bank_max_grade) / std::max(m_tile_size, 0.01F);
        float const effective_blend = std::min(
            std::max(bank_blend_width, run_for_grade), k_river_bank_max_blend_cells);
        float const bank_t =
            std::clamp((dist_along_perp - half_width) / effective_blend, 0.0F, 1.0F);
        const float smooth_bank_t = bank_t * bank_t * (3.0F - 2.0F * bank_t);
        float const banked = bed_height + (land_height - bed_height) * smooth_bank_t;

        m_heights[idx] = land_height > bed_height ? std::min(m_heights[idx], banked)
                                                  : std::max(m_heights[idx], banked);
      }
    }
  }
}

void TerrainHeightMap::add_lakes(const std::vector<Lake>& lakes) {
  m_lakes.clear();
  m_lakes.reserve(lakes.size());
  const float half_grid_width = m_width * 0.5F - 0.5F;
  const float half_grid_height = m_height * 0.5F - 0.5F;

  for (const auto& authored_lake : lakes) {
    Lake lake = authored_lake;
    if (lake.elevation_mode == WaterElevationMode::Terrain) {
      lake.center.setY(get_height_at(lake.center.x(), lake.center.z()));
    }
    m_lakes.push_back(lake);
    const float extent = std::max(lake.width, lake.depth) * 0.55F;
    const float grid_center_x = lake.center.x() / m_tile_size + half_grid_width;
    const float grid_center_z = lake.center.z() / m_tile_size + half_grid_height;
    const float grid_extent = extent / m_tile_size + 1.0F;
    const int min_x =
        std::max(0, static_cast<int>(std::floor(grid_center_x - grid_extent)));
    const int max_x =
        std::min(m_width - 1, static_cast<int>(std::ceil(grid_center_x + grid_extent)));
    const int min_z =
        std::max(0, static_cast<int>(std::floor(grid_center_z - grid_extent)));
    const int max_z = std::min(
        m_height - 1, static_cast<int>(std::ceil(grid_center_z + grid_extent)));

    for (int z = min_z; z <= max_z; ++z) {
      for (int x = min_x; x <= max_x; ++x) {
        const float world_x = (static_cast<float>(x) - half_grid_width) * m_tile_size;
        const float world_z = (static_cast<float>(z) - half_grid_height) * m_tile_size;
        const bool gameplay_water = point_in_lake(lake, world_x, world_z);

        const bool submerged_margin =
            point_in_lake(lake, world_x, world_z, m_tile_size * 0.85F);
        if (!submerged_margin) {
          continue;
        }
        const int index = indexAt(x, z);
        if (m_terrain_types[index] == TerrainType::Mountain) {
          continue;
        }
        const float bed_height = lake.center.y() - 0.10F;
        if (gameplay_water) {
          m_terrain_types[index] = TerrainType::Lake;
          m_heights[index] = bed_height;
        } else {
          m_heights[index] = std::min(m_heights[index], bed_height);
        }
      }
    }
  }
}

void TerrainHeightMap::add_bridges(const std::vector<Bridge>& bridges) {
  m_bridges.clear();
  m_bridges.reserve(bridges.size());

  auto spanned_water_level = [this](const Bridge& bridge) -> std::optional<float> {
    std::optional<float> level;
    const auto raise = [&level](float candidate) {
      level = level.has_value() ? std::max(*level, candidate) : candidate;
    };
    for (const auto& river : m_river_segments) {
      if (bridge_required_half_length_for_river(bridge, river).has_value()) {
        raise(std::max(river.start.y(), river.end.y()));
      }
    }
    const QVector3D midpoint = (bridge.start + bridge.end) * 0.5F;
    for (const auto& lake : m_lakes) {
      if (point_in_lake(lake, midpoint.x(), midpoint.z())) {
        raise(lake.center.y());
      }
    }
    const float landing_run =
        bridge_visual_landing_run(std::max(bridge.width, k_min_bridge_width));
    const float span = std::hypot(bridge.end.x() - bridge.start.x(),
                                  bridge.end.z() - bridge.start.z());
    if (auto const drawn =
            drawn_water_level_under_bridge(bridge,
                                           m_river_segments,
                                           m_lakes,
                                           drawn_lake_padding(m_tile_size),
                                           -landing_run,
                                           span + landing_run);
        drawn.has_value()) {
      raise(*drawn);
    }
    return level;
  };

  for (const auto& bridge : bridges) {
    Bridge adjusted = bridge;
    adjusted.width = std::max(adjusted.width, k_min_bridge_width);

    fit_bridge_span_to_riverbanks(adjusted, m_river_segments);
    adjusted.height = bridge_effective_height(adjusted);

    float abutment_floor = std::numeric_limits<float>::lowest();
    if (auto const water = spanned_water_level(adjusted); water.has_value()) {
      abutment_floor = bridge_abutment_floor_over_water(*water);
    }
    adjusted.start.setY(std::max(get_height_at(adjusted.start.x(), adjusted.start.z()),
                                 abutment_floor));
    adjusted.end.setY(
        std::max(get_height_at(adjusted.end.x(), adjusted.end.z()), abutment_floor));

    if ((adjusted.end - adjusted.start).length() < 0.01F) {
      continue;
    }

    m_bridges.push_back(adjusted);
  }

  precompute_bridge_data();
}

void TerrainHeightMap::precompute_water_blocked() {

  const auto grid_size = static_cast<size_t>(m_width * m_height);
  m_water_blocked.assign(grid_size, false);

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;
  const float tile = std::max(m_tile_size, 0.0001F);

  for (const auto& river : m_river_segments) {
    const float delta_x = river.end.x() - river.start.x();
    const float delta_z = river.end.z() - river.start.z();
    const float length_sq = (delta_x * delta_x) + (delta_z * delta_z);
    if (length_sq < 1.0e-4F) {
      continue;
    }

    const float blocked_half = river_bank_standing_half_width(river.width);
    const float blocked_half_cells = blocked_half / tile;

    const float start_x = (river.start.x() / tile) + grid_half_width;
    const float start_z = (river.start.z() / tile) + grid_half_height;
    const float end_x = (river.end.x() / tile) + grid_half_width;
    const float end_z = (river.end.z() / tile) + grid_half_height;

    int const min_x = std::max(
        0, static_cast<int>(std::floor(std::min(start_x, end_x) - blocked_half_cells)));
    int const max_x = std::min(
        m_width - 1,
        static_cast<int>(std::ceil(std::max(start_x, end_x) + blocked_half_cells)));
    int const min_z = std::max(
        0, static_cast<int>(std::floor(std::min(start_z, end_z) - blocked_half_cells)));
    int const max_z = std::min(
        m_height - 1,
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
        const float reach = (std::max(section.half_width, river.width * 0.5F) +
                             k_water_bank_clearance) /
                            tile;
        if ((lateral * lateral) + (beyond_end * beyond_end) > reach * reach) {
          continue;
        }
        m_water_blocked[static_cast<size_t>(indexAt(x, z))] = true;
      }
    }
  }
}

void TerrainHeightMap::precompute_bridge_data() {
  const auto grid_size = static_cast<size_t>(m_width * m_height);
  m_on_bridge.clear();
  m_on_bridge.resize(grid_size, false);
  m_bridge_walkable.clear();
  m_bridge_walkable.resize(grid_size, false);
  m_bridge_centerline.clear();
  m_bridge_centerline.resize(grid_size, false);
  m_bridge_centers.clear();
  m_bridge_centers.resize(grid_size, QVector3D(0.0F, 0.0F, 0.0F));
  for (const auto& bridge : m_bridges) {
    stamp_bridge_cells(bridge);
  }
}

void TerrainHeightMap::stamp_bridge_cells(const Bridge& bridge) {
  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;
  QVector3D dir = bridge.end - bridge.start;
  float const length = dir.length();
  if (length < 0.01F) {
    return;
  }

  dir.normalize();
  QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
  float const bridge_half_width = bridge.width * 0.5F / std::max(m_tile_size, 0.0001F);
  float const walkable_half_width =
      bridge_walkable_half_width(bridge.width) / std::max(m_tile_size, 0.0001F);

  float const entry_margin = m_tile_size * k_bridge_entry_margin_tiles;
  float const extended_length = length + (entry_margin * 2.0F);
  int const steps = static_cast<int>(std::ceil(extended_length / m_tile_size)) + 1;

  for (int i = 0; i < steps; ++i) {
    float const t =
        static_cast<float>(i) / std::max(1.0F, static_cast<float>(steps - 1));
    float const along = -entry_margin + extended_length * t;
    QVector3D const center_pos = bridge.start + dir * along;

    int const center_x =
        static_cast<int>(std::round((center_pos.x() / m_tile_size) + grid_half_width));
    int const center_z =
        static_cast<int>(std::round((center_pos.z() / m_tile_size) + grid_half_height));
    if (in_bounds(center_x, center_z)) {
      m_bridge_centerline[indexAt(center_x, center_z)] = true;
    }

    float const grid_center_x = (center_pos.x() / m_tile_size) + grid_half_width;
    float const grid_center_z = (center_pos.z() / m_tile_size) + grid_half_height;

    int const min_x =
        std::max(0, static_cast<int>(std::floor(grid_center_x - bridge_half_width)));
    int const max_x = std::min(
        m_width - 1, static_cast<int>(std::ceil(grid_center_x + bridge_half_width)));
    int const min_z =
        std::max(0, static_cast<int>(std::floor(grid_center_z - bridge_half_width)));
    int const max_z = std::min(
        m_height - 1, static_cast<int>(std::ceil(grid_center_z + bridge_half_width)));

    for (int z = min_z; z <= max_z; ++z) {
      for (int x = min_x; x <= max_x; ++x) {
        float const dx = static_cast<float>(x) - grid_center_x;
        float const dz = static_cast<float>(z) - grid_center_z;

        float const dist_along_perp =
            std::abs(dx * perpendicular.x() + dz * perpendicular.z());

        if (dist_along_perp > bridge_half_width) {
          continue;
        }

        float const cell_world_x =
            (static_cast<float>(x) - grid_half_width) * m_tile_size;
        float const cell_world_z =
            (static_cast<float>(z) - grid_half_height) * m_tile_size;
        QVector3D const cell_point(cell_world_x, 0.0F, cell_world_z);
        QVector3D const to_cell = cell_point - bridge.start;
        float const center_along = QVector3D::dotProduct(to_cell, dir);

        if (center_along < -entry_margin || center_along > length + entry_margin) {
          continue;
        }

        int const idx = indexAt(x, z);
        m_on_bridge[idx] = true;
        if (dist_along_perp <= walkable_half_width) {
          m_bridge_walkable[idx] = true;
        }

        float const clamped_along = std::clamp(center_along, 0.0F, length);
        m_bridge_centers[idx] = bridge.start + dir * clamped_along;
      }
    }
  }
}

auto TerrainHeightMap::getBridgeDeckHeight(float world_x, float world_z) const
    -> std::optional<float> {

  float const along_margin =
      m_tile_size * (k_bridge_entry_margin_tiles + k_bridge_cell_half_span_tiles);
  float const perp_margin = m_tile_size * k_bridge_cell_half_span_tiles;

  std::optional<float> deck_y;
  float nearest_perp_dist = std::numeric_limits<float>::max();

  for (const auto& bridge : m_bridges) {
    QVector3D dir = bridge.end - bridge.start;
    float const length = dir.length();
    if (length < 0.01F) {
      continue;
    }

    dir.normalize();
    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
    float const bridge_half_width = bridge.width * 0.5F;

    QVector3D const query_point(world_x, 0.0F, world_z);
    QVector3D const to_query = query_point - bridge.start;

    float const along = QVector3D::dotProduct(to_query, dir);

    if (along < -along_margin || along > length + along_margin) {
      continue;
    }

    float const perp_dist = std::abs(QVector3D::dotProduct(to_query, perpendicular));

    if (perp_dist > bridge_half_width + perp_margin || perp_dist >= nearest_perp_dist) {
      continue;
    }

    float const t = std::clamp(along / length, 0.0F, 1.0F);

    nearest_perp_dist = perp_dist;
    deck_y = bridge_deck_world_y(bridge, t);
  }

  return deck_y;
}

auto TerrainHeightMap::isOnBridge(float world_x, float world_z) const -> bool {

  if (m_on_bridge.empty()) {
    return false;
  }

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;
  const int grid_x =
      static_cast<int>(std::round((world_x / m_tile_size) + grid_half_width));
  const int grid_z =
      static_cast<int>(std::round((world_z / m_tile_size) + grid_half_height));

  if (!in_bounds(grid_x, grid_z)) {
    return false;
  }
  return m_on_bridge[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::isBridgeCell(int grid_x, int grid_z) const -> bool {
  if (!in_bounds(grid_x, grid_z) || m_on_bridge.empty()) {
    return false;
  }
  return m_on_bridge[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::isBridgeCenterline(int grid_x, int grid_z) const -> bool {
  if (!in_bounds(grid_x, grid_z) || m_bridge_centerline.empty()) {
    return false;
  }
  return m_bridge_centerline[indexAt(grid_x, grid_z)];
}

auto TerrainHeightMap::getBridgeCenterPosition(float world_x, float world_z) const
    -> std::optional<QVector3D> {

  if (m_on_bridge.empty()) {
    return std::nullopt;
  }

  const float grid_half_width = m_width * 0.5F - 0.5F;
  const float grid_half_height = m_height * 0.5F - 0.5F;
  const int grid_x =
      static_cast<int>(std::round((world_x / m_tile_size) + grid_half_width));
  const int grid_z =
      static_cast<int>(std::round((world_z / m_tile_size) + grid_half_height));

  if (!in_bounds(grid_x, grid_z)) {
    return std::nullopt;
  }

  const int idx = indexAt(grid_x, grid_z);
  if (!m_on_bridge[idx]) {
    return std::nullopt;
  }

  return m_bridge_centers[idx];
}

auto TerrainHeightMap::getBridgeTraversalPosition(float world_x, float world_z) const
    -> std::optional<QVector3D> {

  for (const auto& bridge : m_bridges) {
    QVector3D dir = bridge.end - bridge.start;
    float const length = dir.length();
    if (length < 0.01F) {
      continue;
    }

    dir.normalize();
    QVector3D const perpendicular(-dir.z(), 0.0F, dir.x());
    float const entry_margin = bridge_crossing_entry_margin(bridge.width, m_tile_size);
    float const alignment_half_width =
        bridge_crossing_alignment_half_width(bridge.width, m_tile_size);

    QVector3D const query_point(world_x, 0.0F, world_z);
    QVector3D const to_query = query_point - bridge.start;
    float const along = QVector3D::dotProduct(to_query, dir);
    if (along < -entry_margin || along > length + entry_margin) {
      continue;
    }

    float const perp_dist = std::abs(QVector3D::dotProduct(to_query, perpendicular));
    if (perp_dist > alignment_half_width) {
      continue;
    }

    return bridge.start + dir * along;
  }

  return std::nullopt;
}

} // namespace Game::Map
