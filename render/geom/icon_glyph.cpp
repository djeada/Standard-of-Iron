#include "icon_glyph.h"

#include <QPainterPath>
#include <QPointF>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace Render::Geom {

namespace {

constexpr float k_design_radius = 0.5F;

[[nodiscard]] auto perpendicular(QVector2D dir) -> QVector2D {
  return {-dir.y(), dir.x()};
}

} // namespace

void GlyphBuilder::set_normal_style(GlyphNormal style, float tilt_radians) {
  m_normal_style = style;
  m_normal_tilt = tilt_radians;
}

void GlyphBuilder::set_transform(QVector2D translation,
                                 float rotation_radians,
                                 float scale) {
  m_translation = translation;
  m_rotation = rotation_radians;
  m_scale = scale;
}

void GlyphBuilder::reset_transform() {
  m_translation = {0.0F, 0.0F};
  m_rotation = 0.0F;
  m_scale = 1.0F;
}

auto GlyphBuilder::apply(QVector2D point) const -> QVector2D {
  QVector2D const scaled = point * m_scale;
  float const cos_r = std::cos(m_rotation);
  float const sin_r = std::sin(m_rotation);
  return {scaled.x() * cos_r - scaled.y() * sin_r + m_translation.x(),
          scaled.x() * sin_r + scaled.y() * cos_r + m_translation.y()};
}

auto GlyphBuilder::normal_for(QVector2D point) const -> QVector3D {
  if (m_normal_style == GlyphNormal::Front || m_normal_tilt <= 0.0F) {
    return {0.0F, 0.0F, 1.0F};
  }
  QVector2D const radial =
      point.lengthSquared() > 1.0e-8F ? point.normalized() : QVector2D(0.0F, 1.0F);
  float const sin_t = std::sin(m_normal_tilt);
  float const cos_t = std::cos(m_normal_tilt);
  return QVector3D(radial.x() * sin_t, radial.y() * sin_t, cos_t).normalized();
}

void GlyphBuilder::emit_positioned(const QVector3D& a,
                                   const QVector3D& b,
                                   const QVector3D& c,
                                   const QVector3D& na,
                                   const QVector3D& nb,
                                   const QVector3D& nc) {
  float const area =
      (b.x() - a.x()) * (c.y() - a.y()) - (c.x() - a.x()) * (b.y() - a.y());
  if (std::abs(area) < 1.0e-9F && std::abs(a.z() - b.z()) < 1.0e-6F &&
      std::abs(a.z() - c.z()) < 1.0e-6F) {
    return;
  }

  float const layer_id = static_cast<float>(static_cast<std::size_t>(m_layer));
  const QVector3D positions[3] = {a, b, c};
  const QVector3D normals[3] = {na, nb, nc};
  bool const flip = area < 0.0F;
  const int order[3] = {0, flip ? 2 : 1, flip ? 1 : 2};

  auto const base = static_cast<unsigned int>(m_vertices.size());
  for (int const index : order) {
    const QVector3D& point = positions[index];
    const QVector3D& normal = normals[index];
    float const radial = QVector2D(point.x(), point.y()).length() / k_design_radius;
    m_vertices.push_back(Render::GL::Vertex{
        .position = {point.x(), point.y(), point.z()},
        .normal = {normal.x(), normal.y(), normal.z()},
        .tex_coord = {layer_id, radial},
    });
  }
  m_indices.push_back(base);
  m_indices.push_back(base + 1U);
  m_indices.push_back(base + 2U);
}

void GlyphBuilder::emit_triangle(QVector2D a, QVector2D b, QVector2D c) {
  emit_positioned(QVector3D(a.x(), a.y(), m_depth),
                  QVector3D(b.x(), b.y(), m_depth),
                  QVector3D(c.x(), c.y(), m_depth),
                  normal_for(a),
                  normal_for(b),
                  normal_for(c));
}

void GlyphBuilder::begin_glyph() {
  m_recording = true;
  m_recorded.clear();
}

void GlyphBuilder::end_glyph(const GlyphExtrusion& extrusion) {
  m_recording = false;

  GlyphLayer const saved_layer = m_layer;
  auto const edges = collect_boundary_edges();

  auto shadow_edges = edges;
  for (BoundaryEdge& edge : shadow_edges) {
    edge.from += extrusion.shadow_offset;
    edge.to += extrusion.shadow_offset;
  }
  emit_skirt(shadow_edges,
             GlyphLayer::Shadow,
             extrusion.shadow_depth,
             0.0F,
             extrusion.shadow_width,
             0.0F,
             1.0F);

  emit_skirt(edges,
             GlyphLayer::Outline,
             extrusion.outline_depth,
             0.0F,
             extrusion.outline_width,
             0.0F,
             0.0F);

  set_normal_style(GlyphNormal::Front);
  emit_glyph_walls(edges, extrusion.back_depth, extrusion.face_depth);

  m_layer = GlyphLayer::Glyph;
  m_depth = extrusion.face_depth;
  for (PendingTri const& tri : m_recorded) {
    emit_triangle(tri.a, tri.b, tri.c);
  }

  m_layer = saved_layer;
  m_recorded.clear();
}

auto GlyphBuilder::collect_boundary_edges() const -> std::vector<BoundaryEdge> {
  QPainterPath silhouette;
  silhouette.setFillRule(Qt::WindingFill);
  for (PendingTri const& tri : m_recorded) {
    QPainterPath triangle;
    triangle.moveTo(tri.a.x(), tri.a.y());
    triangle.lineTo(tri.b.x(), tri.b.y());
    triangle.lineTo(tri.c.x(), tri.c.y());
    triangle.closeSubpath();
    if (silhouette.isEmpty()) {
      silhouette = triangle;
    } else {
      silhouette = silhouette.united(triangle);
    }
  }

  constexpr float k_probe_distance = 0.001F;
  std::vector<BoundaryEdge> edges;
  for (QPolygonF const& contour : silhouette.toSubpathPolygons()) {
    if (contour.size() < 3) {
      continue;
    }
    qsizetype count = contour.size();
    if (contour.front() == contour.back()) {
      --count;
    }
    for (qsizetype i = 0; i < count; ++i) {
      QPointF const& from_point = contour[i];
      QPointF const& to_point = contour[(i + 1) % count];
      QVector2D const from(static_cast<float>(from_point.x()),
                           static_cast<float>(from_point.y()));
      QVector2D const to(static_cast<float>(to_point.x()),
                         static_cast<float>(to_point.y()));
      QVector2D const along = to - from;
      if (along.lengthSquared() <= 1.0e-10F) {
        continue;
      }

      QVector2D outward(along.y(), -along.x());
      outward.normalize();
      QVector2D const midpoint = (from + to) * 0.5F;
      QVector2D const probe = midpoint + outward * k_probe_distance;
      if (silhouette.contains(QPointF(probe.x(), probe.y()))) {
        outward = -outward;
      }
      edges.push_back({from, to, outward});
    }
  }
  return edges;
}

void GlyphBuilder::emit_glyph_walls(const std::vector<BoundaryEdge>& edges,
                                    float back_depth,
                                    float face_depth) {
  m_layer = GlyphLayer::GlyphSide;
  for (BoundaryEdge const& edge : edges) {
    QVector3D const normal =
        QVector3D(edge.outward.x(), edge.outward.y(), 0.34F).normalized();
    QVector3D const p0(edge.from.x(), edge.from.y(), back_depth);
    QVector3D const p1(edge.to.x(), edge.to.y(), back_depth);
    QVector3D const p2(edge.to.x(), edge.to.y(), face_depth);
    QVector3D const p3(edge.from.x(), edge.from.y(), face_depth);
    emit_positioned(p0, p1, p2, normal, normal, normal);
    emit_positioned(p0, p2, p3, normal, normal, normal);
  }
}

void GlyphBuilder::push_skirt_vertex(GlyphLayer layer,
                                     QVector2D point,
                                     float depth,
                                     float falloff) {
  m_vertices.push_back(Render::GL::Vertex{
      .position = {point.x(), point.y(), depth},
      .normal = {0.0F, 0.0F, 1.0F},
      .tex_coord = {static_cast<float>(static_cast<std::size_t>(layer)), falloff},
  });
  m_indices.push_back(static_cast<unsigned int>(m_vertices.size() - 1U));
}

void GlyphBuilder::emit_skirt(const std::vector<BoundaryEdge>& edges,
                              GlyphLayer layer,
                              float depth,
                              float inner_offset,
                              float outer_offset,
                              float inner_falloff,
                              float outer_falloff) {
  if (outer_offset <= inner_offset) {
    return;
  }

  constexpr int k_corner_segments = 8;

  for (BoundaryEdge const& edge : edges) {
    QVector2D const inner_from = edge.from + edge.outward * inner_offset;
    QVector2D const inner_to = edge.to + edge.outward * inner_offset;
    QVector2D const outer_from = edge.from + edge.outward * outer_offset;
    QVector2D const outer_to = edge.to + edge.outward * outer_offset;

    push_skirt_vertex(layer, inner_from, depth, inner_falloff);
    push_skirt_vertex(layer, inner_to, depth, inner_falloff);
    push_skirt_vertex(layer, outer_to, depth, outer_falloff);

    push_skirt_vertex(layer, inner_from, depth, inner_falloff);
    push_skirt_vertex(layer, outer_to, depth, outer_falloff);
    push_skirt_vertex(layer, outer_from, depth, outer_falloff);

    for (int i = 0; i < k_corner_segments; ++i) {
      float const a0 = (static_cast<float>(i) / static_cast<float>(k_corner_segments)) *
                       k_glyph_two_pi;
      float const a1 =
          (static_cast<float>(i + 1) / static_cast<float>(k_corner_segments)) *
          k_glyph_two_pi;
      QVector2D const d0(std::cos(a0), std::sin(a0));
      QVector2D const d1(std::cos(a1), std::sin(a1));

      push_skirt_vertex(layer, edge.from + d0 * inner_offset, depth, inner_falloff);
      push_skirt_vertex(layer, edge.from + d0 * outer_offset, depth, outer_falloff);
      push_skirt_vertex(layer, edge.from + d1 * outer_offset, depth, outer_falloff);

      push_skirt_vertex(layer, edge.from + d0 * inner_offset, depth, inner_falloff);
      push_skirt_vertex(layer, edge.from + d1 * outer_offset, depth, outer_falloff);
      push_skirt_vertex(layer, edge.from + d1 * inner_offset, depth, inner_falloff);
    }
  }
}

void GlyphBuilder::tri(QVector2D a, QVector2D b, QVector2D c) {
  QVector2D const ta = apply(a);
  QVector2D const tb = apply(b);
  QVector2D const tc = apply(c);
  if (m_recording) {
    m_recorded.push_back({ta, tb, tc});
    return;
  }
  emit_triangle(ta, tb, tc);
}

void GlyphBuilder::quad(QVector2D a, QVector2D b, QVector2D c, QVector2D d) {
  tri(a, b, c);
  tri(a, c, d);
}

void GlyphBuilder::convex(std::span<const QVector2D> points) {
  if (points.size() < 3U) {
    return;
  }
  for (std::size_t i = 1U; i + 1U < points.size(); ++i) {
    tri(points[0], points[i], points[i + 1U]);
  }
}

void GlyphBuilder::rect(QVector2D center,
                        QVector2D half_extent,
                        float rotation_radians) {
  float const cos_r = std::cos(rotation_radians);
  float const sin_r = std::sin(rotation_radians);
  auto corner = [&](float sx, float sy) {
    QVector2D const local(half_extent.x() * sx, half_extent.y() * sy);
    return center + QVector2D(local.x() * cos_r - local.y() * sin_r,
                              local.x() * sin_r + local.y() * cos_r);
  };
  quad(corner(-1.0F, -1.0F),
       corner(1.0F, -1.0F),
       corner(1.0F, 1.0F),
       corner(-1.0F, 1.0F));
}

void GlyphBuilder::bar(QVector2D from, QVector2D to, float width) {
  QVector2D const delta = to - from;
  if (delta.lengthSquared() < 1.0e-10F) {
    return;
  }
  QVector2D const normal = perpendicular(delta.normalized()) * (width * 0.5F);
  quad(from - normal, to - normal, to + normal, from + normal);
}

void GlyphBuilder::polyline(std::span<const QVector2D> points,
                            float width,
                            bool closed) {
  if (points.size() < 2U) {
    return;
  }
  std::size_t const segments = closed ? points.size() : points.size() - 1U;
  for (std::size_t i = 0U; i < segments; ++i) {
    const QVector2D& a = points[i];
    const QVector2D& b = points[(i + 1U) % points.size()];
    bar(a, b, width);
    if (i + 1U < segments || closed) {
      disc(b, width * 0.5F, 14);
    }
  }
}

void GlyphBuilder::disc(QVector2D center, float radius, int segments) {
  int const count = std::max(segments, 3);
  for (int i = 0; i < count; ++i) {
    float const a0 =
        (static_cast<float>(i) / static_cast<float>(count)) * k_glyph_two_pi;
    float const a1 =
        (static_cast<float>(i + 1) / static_cast<float>(count)) * k_glyph_two_pi;
    tri(center,
        center + QVector2D(std::cos(a0), std::sin(a0)) * radius,
        center + QVector2D(std::cos(a1), std::sin(a1)) * radius);
  }
}

void GlyphBuilder::ring(QVector2D center,
                        float inner_radius,
                        float outer_radius,
                        int segments,
                        float start_angle,
                        float sweep) {
  int const count = std::max(segments, 3);
  for (int i = 0; i < count; ++i) {
    float const t0 = static_cast<float>(i) / static_cast<float>(count);
    float const t1 = static_cast<float>(i + 1) / static_cast<float>(count);
    float const a0 = start_angle + sweep * t0;
    float const a1 = start_angle + sweep * t1;
    QVector2D const d0(std::cos(a0), std::sin(a0));
    QVector2D const d1(std::cos(a1), std::sin(a1));
    quad(center + d0 * inner_radius,
         center + d0 * outer_radius,
         center + d1 * outer_radius,
         center + d1 * inner_radius);
  }
}

void GlyphBuilder::arrow_head(QVector2D tip,
                              QVector2D direction,
                              float length,
                              float half_width) {
  QVector2D const dir = direction.normalized();
  QVector2D const back = tip - dir * length;
  QVector2D const side = perpendicular(dir) * half_width;
  tri(tip, back - side, back + side);
}

void GlyphBuilder::fit_since(std::size_t mark, float target_radius) {
  if (mark >= m_vertices.size() || target_radius <= 0.0F) {
    return;
  }

  float radius_sq = 0.0F;
  for (std::size_t i = mark; i < m_vertices.size(); ++i) {
    const Render::GL::Vertex& vertex = m_vertices[i];
    radius_sq = std::max(radius_sq,
                         vertex.position[0] * vertex.position[0] +
                             vertex.position[1] * vertex.position[1]);
  }
  if (radius_sq <= 1.0e-10F) {
    return;
  }

  float const factor = target_radius / std::sqrt(radius_sq);
  for (std::size_t i = mark; i < m_vertices.size(); ++i) {
    Render::GL::Vertex& vertex = m_vertices[i];
    vertex.position[0] *= factor;
    vertex.position[1] *= factor;
  }
}

void GlyphBuilder::center_since(std::size_t mark) {
  if (mark >= m_vertices.size()) {
    return;
  }

  float min_x = m_vertices[mark].position[0];
  float max_x = min_x;
  float min_y = m_vertices[mark].position[1];
  float max_y = min_y;
  for (std::size_t i = mark; i < m_vertices.size(); ++i) {
    const Render::GL::Vertex& vertex = m_vertices[i];
    min_x = std::min(min_x, vertex.position[0]);
    max_x = std::max(max_x, vertex.position[0]);
    min_y = std::min(min_y, vertex.position[1]);
    max_y = std::max(max_y, vertex.position[1]);
  }

  float const shift_x = (min_x + max_x) * 0.5F;
  float const shift_y = (min_y + max_y) * 0.5F;
  for (std::size_t i = mark; i < m_vertices.size(); ++i) {
    Render::GL::Vertex& vertex = m_vertices[i];
    vertex.position[0] -= shift_x;
    vertex.position[1] -= shift_y;
  }
}

void GlyphBuilder::normalize_extent(float target_half_extent) {
  float extent = 0.0F;
  for (const Render::GL::Vertex& vertex : m_vertices) {
    extent = std::max(extent, std::abs(vertex.position[0]));
    extent = std::max(extent, std::abs(vertex.position[1]));
  }
  if (extent <= 1.0e-5F) {
    return;
  }

  float const factor = target_half_extent / extent;
  for (Render::GL::Vertex& vertex : m_vertices) {
    vertex.position[0] *= factor;
    vertex.position[1] *= factor;
    vertex.position[2] *= factor;
  }
}

auto GlyphBuilder::build() const -> std::unique_ptr<Render::GL::Mesh> {
  if (m_indices.empty()) {
    return nullptr;
  }
  return std::make_unique<Render::GL::Mesh>(m_vertices, m_indices);
}

} // namespace Render::Geom
