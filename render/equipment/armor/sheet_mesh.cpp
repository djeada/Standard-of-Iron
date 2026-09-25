#include "sheet_mesh.h"

namespace Render::GL {

namespace {

auto safe_unit(const QVector3D& v, const QVector3D& fallback) -> QVector3D {
  return v.lengthSquared() > 1e-12F ? v.normalized() : fallback;
}

void push_vertex(std::vector<Vertex>& out,
                 const QVector3D& p,
                 const QVector3D& n,
                 const QVector2D& uv) {
  out.push_back({{p.x(), p.y(), p.z()}, {n.x(), n.y(), n.z()}, {uv.x(), uv.y()}});
}

} // namespace

auto sheet_grid_normals(const SheetGrid& grid, bool flip) -> std::vector<QVector3D> {
  std::vector<QVector3D> normals(grid.positions.size(), QVector3D());
  for (int r = 0; r < grid.rows; ++r) {
    for (int c = 0; c < grid.columns; ++c) {
      int const c0 = c > 0 ? c - 1 : c;
      int const c1 = c + 1 < grid.columns ? c + 1 : c;
      int const r0 = r > 0 ? r - 1 : r;
      int const r1 = r + 1 < grid.rows ? r + 1 : r;
      QVector3D const du =
          grid.positions[grid.index(c1, r)] - grid.positions[grid.index(c0, r)];
      QVector3D const dv =
          grid.positions[grid.index(c, r1)] - grid.positions[grid.index(c, r0)];
      QVector3D n = safe_unit(QVector3D::crossProduct(du, dv), QVector3D(0, 0, 1));
      normals[grid.index(c, r)] = flip ? -n : n;
    }
  }
  return normals;
}

auto make_thick_sheet_mesh(const SheetGrid& grid,
                           bool flip,
                           float thickness) -> std::unique_ptr<Mesh> {
  if (grid.columns < 2 || grid.rows < 2 ||
      grid.positions.size() != static_cast<std::size_t>(grid.columns * grid.rows) ||
      grid.uvs.size() != grid.positions.size()) {
    return nullptr;
  }

  std::vector<QVector3D> const normals = sheet_grid_normals(grid, flip);
  std::size_t const count = grid.positions.size();

  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(count * 2U +
                   static_cast<std::size_t>(2 * (grid.columns + grid.rows)) * 2U);

  for (std::size_t i = 0; i < count; ++i) {
    push_vertex(vertices, grid.positions[i], normals[i], grid.uvs[i]);
  }
  for (std::size_t i = 0; i < count; ++i) {
    push_vertex(
        vertices, grid.positions[i] - normals[i] * thickness, -normals[i], grid.uvs[i]);
  }

  auto const inner = static_cast<unsigned int>(count);
  auto quad = [&](unsigned int a,
                  unsigned int b,
                  unsigned int c,
                  unsigned int d,
                  bool outward) {
    // a-b along u, a-c along v; d opposite a.
    if (outward) {
      indices.insert(indices.end(), {a, b, c, b, d, c});
    } else {
      indices.insert(indices.end(), {a, c, b, b, c, d});
    }
  };

  for (int r = 0; r + 1 < grid.rows; ++r) {
    for (int c = 0; c + 1 < grid.columns; ++c) {
      auto const a = static_cast<unsigned int>(grid.index(c, r));
      auto const b = static_cast<unsigned int>(grid.index(c + 1, r));
      auto const cc = static_cast<unsigned int>(grid.index(c, r + 1));
      auto const d = static_cast<unsigned int>(grid.index(c + 1, r + 1));
      quad(a, b, cc, d, !flip);
      quad(inner + a, inner + b, inner + cc, inner + d, flip);
    }
  }

  // Rim strips: each edge of the grid gets its own vertices with a normal
  // pointing out of the sheet's edge, so the rim is lit as a bevel instead of
  // borrowing the face normal.
  auto add_rim = [&](const std::vector<std::size_t>& edge,
                     const QVector3D& fallback_out) {
    if (edge.size() < 2U) {
      return;
    }
    auto const base = static_cast<unsigned int>(vertices.size());
    for (std::size_t k = 0; k < edge.size(); ++k) {
      std::size_t const i = edge[k];
      std::size_t const prev = edge[k > 0 ? k - 1 : k];
      std::size_t const next = edge[k + 1 < edge.size() ? k + 1 : k];
      QVector3D const tangent = grid.positions[next] - grid.positions[prev];
      QVector3D out = QVector3D::crossProduct(tangent, normals[i]);
      out = safe_unit(out, fallback_out);
      push_vertex(vertices, grid.positions[i], out, grid.uvs[i]);
      push_vertex(
          vertices, grid.positions[i] - normals[i] * thickness, out, grid.uvs[i]);
    }
    for (std::size_t k = 0; k + 1 < edge.size(); ++k) {
      auto const a = base + static_cast<unsigned int>(2U * k);
      unsigned int const b = a + 2U;
      unsigned int const c = a + 1U;
      unsigned int const d = a + 3U;
      indices.insert(indices.end(), {a, c, b, b, c, d});
    }
  };

  std::vector<std::size_t> edge;
  edge.clear();
  for (int c = 0; c < grid.columns; ++c) {
    edge.push_back(grid.index(c, 0));
  }
  add_rim(edge, QVector3D(0, 1, 0));
  edge.clear();
  for (int c = grid.columns - 1; c >= 0; --c) {
    edge.push_back(grid.index(c, grid.rows - 1));
  }
  add_rim(edge, QVector3D(0, -1, 0));
  edge.clear();
  for (int r = grid.rows - 1; r >= 0; --r) {
    edge.push_back(grid.index(0, r));
  }
  add_rim(edge, QVector3D(-1, 0, 0));
  edge.clear();
  for (int r = 0; r < grid.rows; ++r) {
    edge.push_back(grid.index(grid.columns - 1, r));
  }
  add_rim(edge, QVector3D(1, 0, 0));

  return std::make_unique<Mesh>(vertices, indices);
}

} // namespace Render::GL
