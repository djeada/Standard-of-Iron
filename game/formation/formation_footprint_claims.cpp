#include "formation_footprint_claims.h"

namespace Game::Formation::planning {

FootprintClaims::FootprintClaims(float cell, const FrameAxes& axes, float gap)
    : m_cell(std::max(cell, 0.5F))
    , m_axes(axes)
    , m_gap(gap) {
}

auto FootprintClaims::is_free(const QVector3D& point,
                              float half_width,
                              float half_depth) const -> bool {
  float const reach_metres =
      std::max(half_width, half_depth) + m_largest_extent + m_gap;
  int const reach = static_cast<int>(std::ceil(reach_metres * 1.415F / m_cell)) + 1;
  auto const cell_x = to_cell(point.x());
  auto const cell_z = to_cell(point.z());
  for (int dx = -reach; dx <= reach; ++dx) {
    for (int dz = -reach; dz <= reach; ++dz) {
      auto const it = m_cells.find(key(cell_x + dx, cell_z + dz));
      if (it == m_cells.end()) {
        continue;
      }
      for (const auto& claimed : it->second) {
        QVector3D const offset = point - claimed.point;
        if (local_overlap(QVector3D::dotProduct(offset, m_axes.lateral),
                          QVector3D::dotProduct(offset, m_axes.depth),
                          half_width,
                          half_depth,
                          claimed.half_width,
                          claimed.half_depth,
                          m_gap)) {
          return false;
        }
      }
    }
  }
  return true;
}

void FootprintClaims::claim(const QVector3D& point,
                            float half_width,
                            float half_depth) {
  m_cells[key(to_cell(point.x()), to_cell(point.z()))].push_back(
      {point, half_width, half_depth});
  m_largest_extent = std::max(m_largest_extent, std::max(half_width, half_depth));
}

void FootprintClaims::reserve(std::size_t count) {
  m_cells.reserve(count * 2U);
}

auto FootprintClaims::to_cell(float value) const -> std::int32_t {
  return static_cast<std::int32_t>(std::floor(value / m_cell));
}

auto FootprintClaims::key(std::int32_t cell_x, std::int32_t cell_z) -> std::uint64_t {
  return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_x)) << 32U) |
         static_cast<std::uint64_t>(static_cast<std::uint32_t>(cell_z));
}

} // namespace Game::Formation::planning
