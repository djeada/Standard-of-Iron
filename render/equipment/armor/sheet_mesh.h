#pragma once

#include <QVector2D>
#include <QVector3D>

#include <cstddef>
#include <memory>
#include <vector>

#include "render/gl/mesh.h"

namespace Render::GL {

// A rectangular grid of points describing the outer side of a thin sheet:
// cloth, a greave, a strap. Columns run along u, rows along v, row-major.
struct SheetGrid {
  int columns{0};
  int rows{0};
  std::vector<QVector3D> positions;
  std::vector<QVector2D> uvs;

  [[nodiscard]] auto index(int column, int row) const -> std::size_t {
    return static_cast<std::size_t>(row * columns + column);
  }
};

// Unit normals of the sheet's outer side. The outer side is the one
// cross(dP/du, dP/dv) points to, or the opposite one when `flip` is set.
[[nodiscard]] auto sheet_grid_normals(const SheetGrid& grid,
                                      bool flip) -> std::vector<QVector3D>;

// Builds a closed sheet of the given thickness: the outer face sits on the
// grid, the inner face `thickness` behind it along the normals, and the four
// rims are stitched, so the sheet reads as one solid layer from any side
// without relying on face culling or two-sided lighting.
[[nodiscard]] auto make_thick_sheet_mesh(const SheetGrid& grid,
                                         bool flip,
                                         float thickness) -> std::unique_ptr<Mesh>;

} // namespace Render::GL
