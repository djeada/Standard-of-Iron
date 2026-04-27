#include "software_rasterizer.h"

#include <QPainter>
#include <QPolygonF>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>

namespace Render::Software {

namespace {

struct ProjectedTriangle {
  QPointF p0;
  QPointF p1;
  QPointF p2;
  float centroid_z{0.0F};
  QColor fill;
};

struct ProjectedVertex {
  QPointF screen;
  float ndc_z{0.0F};
  bool behind_camera{false};
  bool outside_ndc{false};
};

[[nodiscard]] auto project(const QVector3D &world, const QMatrix4x4 &view_proj,
                           int width, int height) -> ProjectedVertex {
  QVector4D const clip =
      view_proj * QVector4D(world.x(), world.y(), world.z(), 1.0F);
  ProjectedVertex out;
  if (clip.w() <= 0.0F) {
    out.behind_camera = true;
    return out;
  }
  float const ndc_x = clip.x() / clip.w();
  float const ndc_y = clip.y() / clip.w();
  float const ndc_z = clip.z() / clip.w();
  if (ndc_x < -1.2F || ndc_x > 1.2F || ndc_y < -1.2F || ndc_y > 1.2F) {
    out.outside_ndc = true;
  }
  out.screen =
      QPointF((ndc_x * 0.5F + 0.5F) * static_cast<float>(width),
              (1.0F - (ndc_y * 0.5F + 0.5F)) * static_cast<float>(height));
  out.ndc_z = ndc_z;
  return out;
}

[[nodiscard]] auto compute_normal(const QVector3D &a, const QVector3D &b,
                                  const QVector3D &c) -> QVector3D {
  QVector3D const n = QVector3D::crossProduct(b - a, c - a);
  float const l = n.length();
  return (l > 1e-6F) ? (n / l) : QVector3D(0.0F, 1.0F, 0.0F);
}

[[nodiscard]] auto shade(const QVector3D &base_color, const QVector3D &normal,
                         const QVector3D &light_dir) -> QVector3D {
  QVector3D const n = normal;
  QVector3D const l = light_dir.normalized();
  float const lambert = std::clamp(QVector3D::dotProduct(n, -l), 0.0F, 1.0F);

  float const factor = 0.3F + 0.7F * lambert;
  return QVector3D(base_color.x() * factor, base_color.y() * factor,
                   base_color.z() * factor);
}

[[nodiscard]] auto to_qcolor(const QVector3D &rgb, float alpha) -> QColor {
  auto chan = [](float c) {
    return static_cast<int>(std::clamp(c, 0.0F, 1.0F) * 255.0F + 0.5F);
  };
  return QColor(chan(rgb.x()), chan(rgb.y()), chan(rgb.z()),
                chan(std::clamp(alpha, 0.0F, 1.0F)));
}

struct CubeTri {
  int a;
  int b;
  int c;
};
constexpr std::array<QVector3D, 8> kCubeVerts = {
    QVector3D{-1, -1, -1}, QVector3D{1, -1, -1}, QVector3D{1, 1, -1},
    QVector3D{-1, 1, -1},  QVector3D{-1, -1, 1}, QVector3D{1, -1, 1},
    QVector3D{1, 1, 1},    QVector3D{-1, 1, 1},
};
constexpr std::array<CubeTri, 12> kCubeTris = {{
    {0, 2, 1},
    {0, 3, 2},
    {4, 5, 6},
    {4, 6, 7},
    {0, 1, 5},
    {0, 5, 4},
    {3, 6, 2},
    {3, 7, 6},
    {0, 4, 7},
    {0, 7, 3},
    {1, 2, 6},
    {1, 6, 5},
}};

} // namespace

void SoftwareRasterizer::submit_cube(const QMatrix4x4 &model,
                                     const QVector3D &color, float alpha) {
  std::array<QVector3D, 8> world;
  for (std::size_t i = 0; i < kCubeVerts.size(); ++i) {
    world[i] = model.map(kCubeVerts[i]);
  }
  for (auto const &t : kCubeTris) {
    m_triangles.push_back({world[t.a], world[t.b], world[t.c], color, alpha});
  }
}

auto SoftwareRasterizer::render() -> QImage {
  QImage image(m_settings.width, m_settings.height, QImage::Format_ARGB32);
  image.fill(m_settings.clear_color);

  std::vector<ProjectedTriangle> projected;
  projected.reserve(m_triangles.size());

  for (auto const &tri : m_triangles) {
    ProjectedVertex const a =
        project(tri.v0, m_view_proj, m_settings.width, m_settings.height);
    ProjectedVertex const b =
        project(tri.v1, m_view_proj, m_settings.width, m_settings.height);
    ProjectedVertex const c =
        project(tri.v2, m_view_proj, m_settings.width, m_settings.height);

    if (a.behind_camera || b.behind_camera || c.behind_camera) {
      continue;
    }
    if (a.outside_ndc && b.outside_ndc && c.outside_ndc) {
      continue;
    }

    ProjectedTriangle p;
    p.p0 = a.screen;
    p.p1 = b.screen;
    p.p2 = c.screen;
    p.centroid_z = (a.ndc_z + b.ndc_z + c.ndc_z) / 3.0F;

    QVector3D const shaded =
        shade(tri.color, compute_normal(tri.v0, tri.v1, tri.v2),
              m_settings.light_dir);
    p.fill = to_qcolor(shaded, tri.alpha);
    projected.push_back(p);
  }

  if (m_settings.depth_sort) {
    std::sort(projected.begin(), projected.end(),
              [](const ProjectedTriangle &x, const ProjectedTriangle &y) {
                return x.centroid_z > y.centroid_z;
              });
  }

  QPainter painter(&image);
  painter.setRenderHint(QPainter::Antialiasing, false);
  painter.setPen(Qt::NoPen);
  for (auto const &p : projected) {
    painter.setBrush(p.fill);
    QPolygonF poly;
    poly << p.p0 << p.p1 << p.p2;
    painter.drawPolygon(poly);
  }
  painter.end();

  return image;
}

} // namespace Render::Software
