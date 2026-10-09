#include "render/humanoid/asset/humanoid_face_mesh.h"

#include <QVector2D>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <vector>

#include "animation/rig/humanoid_proportions.h"
#include "render/gl/mesh.h"
#include "render/humanoid/asset/humanoid_spec.h"
#include "render/humanoid/schema/skeleton_schema.h"

namespace Render::Humanoid {

namespace {

using Render::GL::Vertex;

constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;
constexpr std::uint8_t k_head_bone = static_cast<std::uint8_t>(HumanoidBone::Head);
constexpr std::uint8_t k_skin_role = 2;
constexpr unsigned int k_eye_segments = 12;
constexpr unsigned int k_detail_segments = 6;
constexpr unsigned int k_stroke_segments = 8;

constexpr float k_head_radius = Render::GL::HumanProportions::HEAD_RADIUS;
constexpr float k_face_drop = k_head_radius * 0.80F;
constexpr float k_eye_x = k_head_radius * 0.48F;
constexpr float k_eye_y = (k_head_radius * 0.12F) - k_face_drop;
constexpr QVector2D k_white_extent{0.030F, 0.025F};
constexpr QVector2D k_iris_extent{0.0150F, 0.0160F};
constexpr QVector2D k_pupil_extent{0.0074F, 0.0088F};
constexpr QVector2D k_catchlight_offset{0.0070F, 0.0076F};
constexpr float k_catchlight_radius = 0.0034F;
constexpr float k_eye_rim_lift = 0.0010F;
constexpr float k_eye_bulge = 0.0018F;
constexpr float k_catchlight_lift = 0.0008F;

constexpr float k_brow_rise = k_head_radius * 0.35F;
constexpr QVector2D k_brow_extent{0.026F, 0.0048F};
constexpr float k_brow_roll = 0.05F;
constexpr float k_brow_rim_lift = 0.0012F;
constexpr float k_brow_bulge = 0.0022F;

constexpr float k_mouth_drop = k_head_radius * 0.55F;
constexpr QVector2D k_mouth_extent{0.020F, 0.0035F};
constexpr float k_mouth_rim_lift = 0.0008F;
constexpr float k_mouth_bulge = 0.0006F;

struct Feature {
  QVector2D centre;
  float roll = 0.0F;
  QVector2D extent;
  float rim_lift = 0.0F;
  float bulge = 0.0F;
};

class FaceBuilder {
public:
  FaceBuilder(const QVector3D& centre, const QVector3D& radii)
      : m_centre(centre)
      , m_radii(radii) {}

  void ring_band(const Feature& f,
                 QVector2D inner,
                 QVector2D outer,
                 QVector2D offset,
                 float extra_lift,
                 std::uint8_t role,
                 FaceFeatureTone tone,
                 unsigned int segments) {
    auto const first = static_cast<unsigned int>(m_vertices.size());
    bool const fan = inner.x() <= 0.0F;
    if (fan) {
      add(f, offset, extra_lift, role, tone);
    }
    for (unsigned int i = 0; i < segments; ++i) {
      float const angle =
          k_two_pi * static_cast<float>(i) / static_cast<float>(segments);
      QVector2D const dir(std::cos(angle), std::sin(angle));
      if (!fan) {
        add(f, offset + inner * dir, extra_lift, role, tone);
      }
      add(f, offset + outer * dir, extra_lift, role, tone);
    }
    for (unsigned int i = 0; i < segments; ++i) {
      unsigned int const j = (i + 1) % segments;
      if (fan) {
        triangle(first, first + 1 + i, first + 1 + j);
      } else {
        unsigned int const a = first + (2 * i);
        unsigned int const b = first + (2 * j);
        triangle(a, a + 1, b + 1);
        triangle(a, b + 1, b);
      }
    }
  }

  void disc(const Feature& f, std::uint8_t role, FaceFeatureTone tone) {
    ring_band(f, {}, f.extent, {}, 0.0F, role, tone, k_stroke_segments);
  }

  auto take() -> std::unique_ptr<Render::GL::Mesh> {
    return std::make_unique<Render::GL::Mesh>(m_vertices, m_indices);
  }

private:
  auto surface(QVector2D xy, QVector3D& normal) const -> QVector3D {
    float const nx = xy.x() / m_radii.x();
    float const ny = (xy.y() - m_centre.y()) / m_radii.y();
    float const nz = std::sqrt(std::max(1.0F - (nx * nx) - (ny * ny), 0.0F));
    QVector3D const p(xy.x(), xy.y(), m_centre.z() + (m_radii.z() * nz));
    normal =
        QVector3D(nx / m_radii.x(), ny / m_radii.y(), nz / m_radii.z()).normalized();
    return p;
  }

  void add(const Feature& f,
           QVector2D local,
           float extra_lift,
           std::uint8_t role,
           FaceFeatureTone tone) {
    float const c = std::cos(f.roll);
    float const s = std::sin(f.roll);
    QVector2D const xy = f.centre + QVector2D((c * local.x()) - (s * local.y()),
                                              (s * local.x()) + (c * local.y()));
    QVector2D const unit(local.x() / f.extent.x(), local.y() / f.extent.y());
    float const t = std::clamp(1.0F - unit.lengthSquared(), 0.0F, 1.0F);
    QVector3D normal;
    QVector3D const skin = surface(xy, normal);
    QVector3D const p = skin + (normal * (f.rim_lift + (f.bulge * t) + extra_lift));
    Vertex v{};
    v.position = {p.x(), p.y(), p.z()};
    v.normal = {normal.x(), normal.y(), normal.z()};
    v.tex_coord = role == k_skin_role ? std::array<float, 2>{k_face_feature_uv,
                                                             static_cast<float>(tone)}
                                      : std::array<float, 2>{0.0F, 0.0F};
    v.color_role = role;
    v.bone_indices = {k_head_bone, 0U, 0U, 0U};
    v.bone_weights = {1.0F, 0.0F, 0.0F, 0.0F};
    m_vertices.push_back(v);
  }

  void triangle(unsigned int a, unsigned int b, unsigned int c) {
    auto pos = [this](unsigned int i) {
      auto const& p = m_vertices[i].position;
      return QVector3D(p[0], p[1], p[2]);
    };
    auto const& n = m_vertices[a].normal;
    QVector3D const face = QVector3D::crossProduct(pos(b) - pos(a), pos(c) - pos(a));
    if (QVector3D::dotProduct(face, QVector3D(n[0], n[1], n[2])) < 0.0F) {
      std::swap(b, c);
    }
    m_indices.insert(m_indices.end(), {a, b, c});
  }

  QVector3D m_centre;
  QVector3D m_radii;
  std::vector<Vertex> m_vertices;
  std::vector<unsigned int> m_indices;
};

} // namespace

auto build_humanoid_face_mesh(const QVector3D& cranium_centre,
                              const QVector3D& cranium_radii)
    -> std::unique_ptr<Render::GL::Mesh> {
  FaceBuilder face(cranium_centre, cranium_radii);
  for (float const side : {-1.0F, 1.0F}) {
    Feature const eye{.centre = {side * k_eye_x, k_eye_y},
                      .extent = k_white_extent,
                      .rim_lift = k_eye_rim_lift,
                      .bulge = k_eye_bulge};
    face.ring_band(eye,
                   {},
                   k_pupil_extent,
                   {},
                   0.0F,
                   k_skin_role,
                   FaceFeatureTone::Pupil,
                   k_eye_segments);
    face.ring_band(eye,
                   k_pupil_extent,
                   k_iris_extent,
                   {},
                   0.0F,
                   k_skin_role,
                   FaceFeatureTone::Iris,
                   k_eye_segments);
    face.ring_band(eye,
                   k_iris_extent,
                   k_white_extent,
                   {},
                   0.0F,
                   k_skin_role,
                   FaceFeatureTone::EyeWhite,
                   k_eye_segments);
    QVector2D const catchlight(k_catchlight_radius, k_catchlight_radius);
    face.ring_band(eye,
                   {},
                   catchlight,
                   k_catchlight_offset,
                   k_catchlight_lift,
                   k_skin_role,
                   FaceFeatureTone::Catchlight,
                   k_detail_segments);

    Feature const brow{.centre = {side * k_eye_x, k_eye_y + k_brow_rise},
                       .roll = -side * k_brow_roll,
                       .extent = k_brow_extent,
                       .rim_lift = k_brow_rim_lift,
                       .bulge = k_brow_bulge};
    face.disc(brow, k_humanoid_hair_role, FaceFeatureTone::EyeWhite);
  }
  Feature const mouth{.centre = {0.0F, k_eye_y - k_mouth_drop},
                      .extent = k_mouth_extent,
                      .rim_lift = k_mouth_rim_lift,
                      .bulge = k_mouth_bulge};
  face.disc(mouth, k_skin_role, FaceFeatureTone::Mouth);
  return face.take();
}

} // namespace Render::Humanoid
