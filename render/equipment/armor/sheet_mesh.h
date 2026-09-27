#pragma once

#include <QVector2D>
#include <QVector3D>

#include <cstddef>
#include <memory>
#include <vector>

#include "render/gl/mesh.h"

namespace Render::GL {

struct SheetGrid {
  int columns{0};
  int rows{0};
  std::vector<QVector3D> positions;
  std::vector<QVector2D> uvs;

  [[nodiscard]] auto index(int column, int row) const -> std::size_t {
    return static_cast<std::size_t>(row * columns + column);
  }
};

[[nodiscard]] auto sheet_grid_normals(const SheetGrid& grid,
                                      bool flip) -> std::vector<QVector3D>;

[[nodiscard]] auto make_thick_sheet_mesh(const SheetGrid& grid,
                                         bool flip,
                                         float thickness) -> std::unique_ptr<Mesh>;

} // namespace Render::GL
