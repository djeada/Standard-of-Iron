#include "terrain_road_index.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Game::Map {

namespace {

constexpr float k_min_tile_size = 0.0001F;
constexpr float k_road_index_tile_span = 8.0F;
constexpr float k_degenerate_length_sq = 0.0001F;

} // namespace

void RoadSpatialIndex::clear() {
  m_offsets.clear();
  m_segment_ids.clear();
  m_segments.clear();
  m_columns = 0;
  m_rows = 0;
}

auto RoadSpatialIndex::cell_x(float world_x) const -> int {
  return std::clamp(static_cast<int>(std::floor((world_x - m_origin_x) / m_cell_size)),
                    0,
                    m_columns - 1);
}

auto RoadSpatialIndex::cell_z(float world_z) const -> int {
  return std::clamp(static_cast<int>(std::floor((world_z - m_origin_z) / m_cell_size)),
                    0,
                    m_rows - 1);
}

auto RoadSpatialIndex::cell_index(int x, int z) const -> std::size_t {
  return static_cast<std::size_t>(z) * static_cast<std::size_t>(m_columns) +
         static_cast<std::size_t>(x);
}

auto RoadSpatialIndex::cell_span(const QuerySegment& segment) const
    -> std::array<int, 4> {
  return {cell_x(segment.min_x),
          cell_x(segment.max_x),
          cell_z(segment.min_z),
          cell_z(segment.max_z)};
}

auto RoadSpatialIndex::build_query_segments(const std::vector<RoadSegment>& roads,
                                            Bounds bounds) -> Bounds {
  m_segments.reserve(roads.size());
  for (const auto& segment : roads) {
    const float half_width = std::max(0.0F, segment.width * 0.5F);
    const float start_x = segment.start.x();
    const float start_z = segment.start.z();
    const float delta_x = segment.end.x() - start_x;
    const float delta_z = segment.end.z() - start_z;
    const float length_sq = delta_x * delta_x + delta_z * delta_z;
    QuerySegment const query{
        .start_x = start_x,
        .start_z = start_z,
        .delta_x = delta_x,
        .delta_z = delta_z,
        .inverse_length_sq =
            length_sq >= k_degenerate_length_sq ? 1.0F / length_sq : 0.0F,
        .half_width = half_width,
        .min_x = std::min(start_x, segment.end.x()) - half_width,
        .max_x = std::max(start_x, segment.end.x()) + half_width,
        .min_z = std::min(start_z, segment.end.z()) - half_width,
        .max_z = std::max(start_z, segment.end.z()) + half_width,
    };
    bounds.min_x = std::min(bounds.min_x, query.min_x);
    bounds.max_x = std::max(bounds.max_x, query.max_x);
    bounds.min_z = std::min(bounds.min_z, query.min_z);
    bounds.max_z = std::max(bounds.max_z, query.max_z);
    m_segments.push_back(query);
  }
  return bounds;
}

void RoadSpatialIndex::size_grid(const Bounds& bounds) {
  m_origin_x = bounds.min_x;
  m_origin_z = bounds.min_z;
  m_columns = std::max(
      1, static_cast<int>(std::floor((bounds.max_x - bounds.min_x) / m_cell_size)) + 1);
  m_rows = std::max(
      1, static_cast<int>(std::floor((bounds.max_z - bounds.min_z) / m_cell_size)) + 1);
}

void RoadSpatialIndex::fill_cells() {
  const std::size_t cell_count =
      static_cast<std::size_t>(m_columns) * static_cast<std::size_t>(m_rows);
  m_offsets.assign(cell_count + 1U, 0U);
  for (const auto& segment : m_segments) {
    const auto span = cell_span(segment);
    for (int z = span[2]; z <= span[3]; ++z) {
      for (int x = span[0]; x <= span[1]; ++x) {
        ++m_offsets[cell_index(x, z) + 1U];
      }
    }
  }
  for (std::size_t cell = 1; cell < m_offsets.size(); ++cell) {
    m_offsets[cell] += m_offsets[cell - 1U];
  }

  m_segment_ids.resize(m_offsets.back());
  auto cursors = m_offsets;
  for (std::uint32_t segment_id = 0;
       segment_id < static_cast<std::uint32_t>(m_segments.size());
       ++segment_id) {
    const auto span = cell_span(m_segments[segment_id]);
    for (int z = span[2]; z <= span[3]; ++z) {
      for (int x = span[0]; x <= span[1]; ++x) {
        m_segment_ids[cursors[cell_index(x, z)]++] = segment_id;
      }
    }
  }
}

void RoadSpatialIndex::rebuild(const std::vector<RoadSegment>& roads,
                               float tile_size,
                               int grid_width,
                               int grid_height) {
  clear();
  if (roads.empty()) {
    return;
  }

  const float safe_tile = std::max(tile_size, k_min_tile_size);
  m_cell_size = std::max(1.0F, safe_tile * k_road_index_tile_span);

  const float half_world_width = static_cast<float>(grid_width) * safe_tile * 0.5F;
  const float half_world_height = static_cast<float>(grid_height) * safe_tile * 0.5F;
  const Bounds world_bounds{
      .min_x = -half_world_width,
      .max_x = half_world_width,
      .min_z = -half_world_height,
      .max_z = half_world_height,
  };
  size_grid(build_query_segments(roads, world_bounds));
  fill_cells();
}

auto RoadSpatialIndex::is_near(float world_x,
                               float world_z,
                               float clearance) const -> bool {
  if (m_columns <= 0 || m_rows <= 0 || m_segments.empty()) {
    return false;
  }

  const float expanded = std::max(clearance, 0.0F);
  const float index_max_x = m_origin_x + m_cell_size * m_columns;
  const float index_max_z = m_origin_z + m_cell_size * m_rows;
  if (world_x + expanded < m_origin_x || world_z + expanded < m_origin_z ||
      world_x - expanded >= index_max_x || world_z - expanded >= index_max_z) {
    return false;
  }

  const int min_x = cell_x(world_x - expanded);
  const int max_x = cell_x(world_x + expanded);
  const int min_z = cell_z(world_z - expanded);
  const int max_z = cell_z(world_z + expanded);

  for (int cell_z_index = min_z; cell_z_index <= max_z; ++cell_z_index) {
    for (int cell_x_index = min_x; cell_x_index <= max_x; ++cell_x_index) {
      const std::size_t cell = cell_index(cell_x_index, cell_z_index);
      for (std::uint32_t cursor = m_offsets[cell]; cursor < m_offsets[cell + 1U];
           ++cursor) {
        const QuerySegment& segment = m_segments[m_segment_ids[cursor]];
        const float radius = segment.half_width + expanded;
        const float px = world_x - segment.start_x;
        const float pz = world_z - segment.start_z;
        float closest_x = segment.start_x;
        float closest_z = segment.start_z;
        if (segment.inverse_length_sq > 0.0F) {
          float t =
              (px * segment.delta_x + pz * segment.delta_z) * segment.inverse_length_sq;
          if (t <= 0.0F) {
            t = 0.0F;
          } else if (t >= 1.0F) {
            t = 1.0F;
          }
          closest_x += t * segment.delta_x;
          closest_z += t * segment.delta_z;
        }
        const float dx = world_x - closest_x;
        const float dz = world_z - closest_z;
        if (dx * dx + dz * dz <= radius * radius) {
          return true;
        }
      }
    }
  }
  return false;
}

} // namespace Game::Map
