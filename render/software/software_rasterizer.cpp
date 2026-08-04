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
  float z0{0.0F};
  float z1{0.0F};
  float z2{0.0F};
  float centroid_z{0.0F};
  QColor fill;
};

[[nodiscard]] auto
signed_area(const QPointF& a, const QPointF& b, const QPointF& c) -> float {
  return static_cast<float>((b.x() - a.x()) * (c.y() - a.y()) -
                            (b.y() - a.y()) * (c.x() - a.x()));
}

struct ProjectedVertex {
  QPointF screen;
  float ndc_z{0.0F};
  bool behind_camera{false};
  bool outside_ndc{false};
};

[[nodiscard]] auto project(const QVector3D& world,
                           const QMatrix4x4& view_proj,
                           int width,
                           int height) -> ProjectedVertex {
  QVector4D const clip = view_proj * QVector4D(world.x(), world.y(), world.z(), 1.0F);
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
  out.screen = QPointF((ndc_x * 0.5F + 0.5F) * static_cast<float>(width),
                       (1.0F - (ndc_y * 0.5F + 0.5F)) * static_cast<float>(height));
  out.ndc_z = ndc_z;
  return out;
}

[[nodiscard]] auto compute_normal(const QVector3D& a,
                                  const QVector3D& b,
                                  const QVector3D& c) -> QVector3D {
  QVector3D const n = QVector3D::crossProduct(b - a, c - a);
  float const l = n.length();
  return (l > 1e-6F) ? (n / l) : QVector3D(0.0F, 1.0F, 0.0F);
}

[[nodiscard]] auto shade(const QVector3D& base_color,
                         const QVector3D& normal,
                         const QVector3D& light_dir) -> QVector3D {
  QVector3D const n = normal;
  QVector3D const l = light_dir.normalized();
  float const lambert = std::clamp(QVector3D::dotProduct(n, -l), 0.0F, 1.0F);

  float const factor = 0.3F + 0.7F * lambert;
  return QVector3D(
      base_color.x() * factor, base_color.y() * factor, base_color.z() * factor);
}

[[nodiscard]] auto to_qcolor(const QVector3D& rgb, float alpha) -> QColor {
  auto chan = [](float c) {
    return static_cast<int>(std::clamp(c, 0.0F, 1.0F) * 255.0F + 0.5F);
  };
  return QColor(
      chan(rgb.x()), chan(rgb.y()), chan(rgb.z()), chan(std::clamp(alpha, 0.0F, 1.0F)));
}

struct CubeTri {
  int a;
  int b;
  int c;
};
constexpr std::array<QVector3D, 8> k_cube_verts = {
    QVector3D{-1, -1, -1},
    QVector3D{1, -1, -1},
    QVector3D{1, 1, -1},
    QVector3D{-1, 1, -1},
    QVector3D{-1, -1, 1},
    QVector3D{1, -1, 1},
    QVector3D{1, 1, 1},
    QVector3D{-1, 1, 1},
};
constexpr std::array<CubeTri, 12> k_cube_tris = {{
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

void SoftwareRasterizer::submit_cube(const QMatrix4x4& model,
                                     const QVector3D& color,
                                     float alpha) {
  std::array<QVector3D, 8> world;
  for (std::size_t i = 0; i < k_cube_verts.size(); ++i) {
    world[i] = model.map(k_cube_verts[i]);
  }
  for (auto const& t : k_cube_tris) {
    m_triangles.push_back({world[t.a], world[t.b], world[t.c], color, alpha});
  }
}

auto SoftwareRasterizer::render() -> QImage {
  QImage image(m_settings.width, m_settings.height, QImage::Format_ARGB32);
  image.fill(m_settings.clear_color);

  std::vector<ProjectedTriangle> projected;
  projected.reserve(m_triangles.size());

  for (auto const& tri : m_triangles) {
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
    p.z0 = a.ndc_z;
    p.z1 = b.ndc_z;
    p.z2 = c.ndc_z;
    p.centroid_z = (a.ndc_z + b.ndc_z + c.ndc_z) / 3.0F;

    if (m_settings.backface_cull && tri.alpha >= 1.0F &&
        signed_area(p.p0, p.p1, p.p2) >= 0.0F) {
      continue;
    }

    QVector3D const shaded =
        shade(tri.color, compute_normal(tri.v0, tri.v1, tri.v2), m_settings.light_dir);
    p.fill = to_qcolor(shaded, tri.alpha);
    projected.push_back(p);
  }

  if (!m_settings.depth_test) {
    if (m_settings.depth_sort) {
      std::sort(projected.begin(),
                projected.end(),
                [](const ProjectedTriangle& x, const ProjectedTriangle& y) {
                  return x.centroid_z > y.centroid_z;
                });
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, false);
    painter.setPen(Qt::NoPen);
    for (auto const& p : projected) {
      painter.setBrush(p.fill);
      QPolygonF poly;
      poly << p.p0 << p.p1 << p.p2;
      painter.drawPolygon(poly);
    }
    painter.end();
    return image;
  }

  std::sort(projected.begin(),
            projected.end(),
            [](const ProjectedTriangle& x, const ProjectedTriangle& y) {
              return x.centroid_z > y.centroid_z;
            });

  std::vector<float> depth(static_cast<std::size_t>(m_settings.width) *
                               static_cast<std::size_t>(m_settings.height),
                           1e30F);

  for (auto const& p : projected) {
    float const area = signed_area(p.p0, p.p1, p.p2);
    if (std::fabs(area) < 1e-9F) {
      continue;
    }

    int const min_x = std::max(
        0, static_cast<int>(std::floor(std::min({p.p0.x(), p.p1.x(), p.p2.x()}))));
    int const max_x =
        std::min(m_settings.width - 1,
                 static_cast<int>(std::ceil(std::max({p.p0.x(), p.p1.x(), p.p2.x()}))));
    int const min_y = std::max(
        0, static_cast<int>(std::floor(std::min({p.p0.y(), p.p1.y(), p.p2.y()}))));
    int const max_y =
        std::min(m_settings.height - 1,
                 static_cast<int>(std::ceil(std::max({p.p0.y(), p.p1.y(), p.p2.y()}))));

    bool const opaque = p.fill.alpha() >= 255;
    float const inv_area = 1.0F / area;

    for (int y = min_y; y <= max_y; ++y) {
      auto* scanline = reinterpret_cast<QRgb*>(image.scanLine(y));
      for (int x = min_x; x <= max_x; ++x) {
        QPointF const sample(static_cast<float>(x) + 0.5F,
                             static_cast<float>(y) + 0.5F);
        float const w0 = signed_area(p.p1, p.p2, sample) * inv_area;
        float const w1 = signed_area(p.p2, p.p0, sample) * inv_area;
        float const w2 = signed_area(p.p0, p.p1, sample) * inv_area;
        if (w0 < 0.0F || w1 < 0.0F || w2 < 0.0F) {
          continue;
        }

        float const z = w0 * p.z0 + w1 * p.z1 + w2 * p.z2;
        std::size_t const index =
            static_cast<std::size_t>(y) * static_cast<std::size_t>(m_settings.width) +
            static_cast<std::size_t>(x);
        if (z >= depth[index]) {
          continue;
        }

        if (opaque) {
          depth[index] = z;
          scanline[x] = p.fill.rgb();
        } else {
          float const a = static_cast<float>(p.fill.alpha()) / 255.0F;
          QColor const dst = QColor::fromRgb(scanline[x]);
          scanline[x] =
              qRgb(static_cast<int>(p.fill.red() * a + dst.red() * (1.0F - a)),
                   static_cast<int>(p.fill.green() * a + dst.green() * (1.0F - a)),
                   static_cast<int>(p.fill.blue() * a + dst.blue() * (1.0F - a)));
        }
      }
    }
  }

  return image;
}

} // namespace Render::Software
