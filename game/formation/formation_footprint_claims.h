#pragma once

#include <QVector3D>

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "formation_frame.h"

namespace Game::Formation::planning {

class FootprintClaims {
public:
  FootprintClaims(float cell, const FrameAxes& axes, float gap);

  [[nodiscard]] auto
  is_free(const QVector3D& point, float half_width, float half_depth) const -> bool;

  void claim(const QVector3D& point, float half_width, float half_depth);

  void reserve(std::size_t count);

private:
  struct Claim {
    QVector3D point;
    float half_width{0.0F};
    float half_depth{0.0F};
  };

  [[nodiscard]] auto to_cell(float value) const -> std::int32_t;
  [[nodiscard]] static auto key(std::int32_t cell_x,
                                std::int32_t cell_z) -> std::uint64_t;

  std::unordered_map<std::uint64_t, std::vector<Claim>> m_cells;
  float m_cell{1.0F};
  FrameAxes m_axes;
  float m_gap{0.0F};
  float m_largest_extent{0.0F};
};

} // namespace Game::Formation::planning
