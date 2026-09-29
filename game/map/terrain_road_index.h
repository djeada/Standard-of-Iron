#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "map_definition.h"

namespace Game::Map {

class RoadSpatialIndex {
public:
  void clear();

  void rebuild(const std::vector<RoadSegment>& roads,
               float tile_size,
               int grid_width,
               int grid_height);

  [[nodiscard]] auto
  is_near(float world_x, float world_z, float clearance) const -> bool;

private:
  struct QuerySegment {
    float start_x{0.0F};
    float start_z{0.0F};
    float delta_x{0.0F};
    float delta_z{0.0F};
    float inverse_length_sq{0.0F};
    float half_width{0.0F};
    float min_x{0.0F};
    float max_x{0.0F};
    float min_z{0.0F};
    float max_z{0.0F};
  };

  struct Bounds {
    float min_x{0.0F};
    float max_x{0.0F};
    float min_z{0.0F};
    float max_z{0.0F};
  };

  [[nodiscard]] auto cell_x(float world_x) const -> int;
  [[nodiscard]] auto cell_z(float world_z) const -> int;
  [[nodiscard]] auto cell_index(int x, int z) const -> std::size_t;
  [[nodiscard]] auto cell_span(const QuerySegment& segment) const -> std::array<int, 4>;

  auto build_query_segments(const std::vector<RoadSegment>& roads,
                            Bounds bounds) -> Bounds;
  void size_grid(const Bounds& bounds);
  void fill_cells();

  std::vector<QuerySegment> m_segments;
  float m_origin_x{0.0F};
  float m_origin_z{0.0F};
  float m_cell_size{1.0F};
  int m_columns{0};
  int m_rows{0};
  std::vector<std::uint32_t> m_offsets;
  std::vector<std::uint32_t> m_segment_ids;
};

} // namespace Game::Map
