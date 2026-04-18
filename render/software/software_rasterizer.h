// Stage 13 — software rasterizer for the ShaderQuality::None path.
//
// Purpose: allow the game to render a passable picture on systems with
// no working OpenGL 3.3 context (headless CI, remote desktop without
// GPU acceleration, museum kiosks). The rasterizer consumes a
// view-projection matrix plus a list of ColoredTriangle primitives,
// projects vertices to screen space, depth-sorts the triangles back
// to front, and fills each one with a flat Lambertian-shaded colour
// into a QImage.
//
// The data types below are deliberately decoupled from Render::GL —
// callers are expected to convert from their native mesh/draw-cmd
// types into ColoredTriangle before handing work to the rasterizer.
// This keeps the rasterizer free of Mesh VBO/shader dependencies and
// makes it unit-testable without an OpenGL context.
//
// Scope limits (intentional):
//   * Flat per-triangle colour only. No per-vertex gradients, no
//     textures, no transparency beyond back-to-front painter's
//     algorithm.
//   * Triangles rejected if all vertices fall outside NDC — full
//     clipping is out of scope (units are small so this is rare).
//   * Depth resolved per-triangle (centroid z), not per-pixel — good
//     enough for the low-detail silhouettes this tier serves.

#pragma once

#include <QColor>
#include <QImage>
#include <QMatrix4x4>
#include <QVector3D>

#include <cstdint>
#include <vector>

namespace Render::Software {

struct ColoredTriangle {
  QVector3D v0;
  QVector3D v1;
  QVector3D v2;
  QVector3D color{1.0F, 1.0F, 1.0F};
  float alpha{1.0F};
};

struct RasterSettings {
  int width{800};
  int height{600};
  QColor clear_color{40, 48, 32, 255};
  QVector3D light_dir{-0.4F, -0.8F, -0.3F};
  bool depth_sort{true};
};

class SoftwareRasterizer {
public:
  explicit SoftwareRasterizer(RasterSettings settings = {})
      : m_settings(std::move(settings)) {}

  void set_view_projection(const QMatrix4x4 &view_proj) {
    m_view_proj = view_proj;
  }

  void submit(const ColoredTriangle &tri) { m_triangles.push_back(tri); }

  void submit_cube(const QMatrix4x4 &model, const QVector3D &color,
                   float alpha = 1.0F);

  [[nodiscard]] auto render() -> QImage;

  void clear() { m_triangles.clear(); }

  [[nodiscard]] auto triangle_count() const noexcept -> std::size_t {
    return m_triangles.size();
  }

  [[nodiscard]] auto settings() const noexcept -> const RasterSettings & {
    return m_settings;
  }

private:
  RasterSettings m_settings;
  QMatrix4x4 m_view_proj;
  std::vector<ColoredTriangle> m_triangles;
};

} // namespace Render::Software
