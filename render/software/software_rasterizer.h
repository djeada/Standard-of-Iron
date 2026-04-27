

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
