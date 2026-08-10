#pragma once

#include <QVector2D>
#include <QVector3D>

#include <cstdint>
#include <memory>
#include <numbers>
#include <span>
#include <vector>

#include "render/gl/mesh.h"

namespace Render::Geom {

inline constexpr float k_glyph_two_pi = 2.0F * std::numbers::pi_v<float>;

enum class GlyphLayer : std::uint8_t {
  Shadow = 0,
  Outline = 1,
  Glyph = 2,
  Accent = 3,
  GlyphSide = 4,
};

inline constexpr std::uint8_t k_glyph_layer_count = 5;

enum class GlyphNormal : std::uint8_t {
  Front,
  TiltOut,
};

class GlyphBuilder {
public:
  void set_layer(GlyphLayer layer) { m_layer = layer; }
  void set_depth(float depth) { m_depth = depth; }
  void set_normal_style(GlyphNormal style, float tilt_radians = 0.0F);

  void set_transform(QVector2D translation, float rotation_radians, float scale = 1.0F);
  void reset_transform();

  [[nodiscard]] auto vertex_mark() const -> std::size_t { return m_vertices.size(); }
  void fit_since(std::size_t mark, float target_radius);
  void center_since(std::size_t mark);

  void begin_glyph();
  struct GlyphExtrusion {
    float outline_depth = 0.0F;
    float outline_width = 0.0F;
    float back_depth = 0.0F;
    float face_depth = 0.0F;

    float shadow_depth = 0.0F;
    float shadow_grow = 0.0F;
    float halo_grow = 0.0F;
    QVector2D shadow_offset{};
  };

  void end_glyph(const GlyphExtrusion& extrusion);

  void tri(QVector2D a, QVector2D b, QVector2D c);
  void quad(QVector2D a, QVector2D b, QVector2D c, QVector2D d);
  void convex(std::span<const QVector2D> points);
  void rect(QVector2D center, QVector2D half_extent, float rotation_radians = 0.0F);
  void bar(QVector2D from, QVector2D to, float width);
  void polyline(std::span<const QVector2D> points, float width, bool closed = false);
  void disc(QVector2D center, float radius, int segments = 28);
  void ring(QVector2D center,
            float inner_radius,
            float outer_radius,
            int segments = 28,
            float start_angle = 0.0F,
            float sweep = k_glyph_two_pi);
  void arrow_head(QVector2D tip, QVector2D direction, float length, float half_width);

  [[nodiscard]] auto vertices() const -> const std::vector<Render::GL::Vertex>& {
    return m_vertices;
  }
  [[nodiscard]] auto indices() const -> const std::vector<unsigned int>& {
    return m_indices;
  }
  [[nodiscard]] auto triangle_count() const -> std::size_t {
    return m_indices.size() / 3U;
  }
  [[nodiscard]] auto empty() const -> bool { return m_indices.empty(); }

  void normalize_extent(float target_half_extent);

  [[nodiscard]] auto build() const -> std::unique_ptr<Render::GL::Mesh>;

private:
  struct PendingTri {
    QVector2D a;
    QVector2D b;
    QVector2D c;
  };

  [[nodiscard]] auto apply(QVector2D point) const -> QVector2D;
  [[nodiscard]] auto normal_for(QVector2D point) const -> QVector3D;

  void emit_triangle(QVector2D a, QVector2D b, QVector2D c);
  struct BoundaryEdge {
    QVector2D from;
    QVector2D to;
    QVector2D outward;
  };

  [[nodiscard]] auto collect_boundary_edges() const -> std::vector<BoundaryEdge>;
  void emit_silhouette(
      GlyphLayer layer, float depth, float grow, QVector2D offset, float shade);
  void emit_glyph_walls(const std::vector<BoundaryEdge>& edges,
                        float back_depth,
                        float face_depth);
  void emit_skirt(const std::vector<BoundaryEdge>& edges,
                  GlyphLayer layer,
                  float depth,
                  float inner_offset,
                  float outer_offset,
                  float inner_falloff,
                  float outer_falloff);
  void push_skirt_vertex(GlyphLayer layer, QVector2D point, float depth, float falloff);
  void emit_positioned(const QVector3D& a,
                       const QVector3D& b,
                       const QVector3D& c,
                       const QVector3D& na,
                       const QVector3D& nb,
                       const QVector3D& nc);

  std::vector<Render::GL::Vertex> m_vertices;
  std::vector<unsigned int> m_indices;
  std::vector<PendingTri> m_recorded;

  QVector2D m_translation{0.0F, 0.0F};
  float m_rotation{0.0F};
  float m_scale{1.0F};
  float m_depth{0.0F};
  GlyphLayer m_layer{GlyphLayer::Glyph};
  GlyphNormal m_normal_style{GlyphNormal::Front};
  float m_normal_tilt{0.0F};
  bool m_recording{false};
};

} // namespace Render::Geom
