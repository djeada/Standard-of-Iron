#include "render/humanoid/asset/humanoid_face_mesh.h"

#include <QVector2D>

#include <algorithm>
#include <array>
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
constexpr float k_eye_x = k_head_radius * 0.48F;
constexpr float k_eye_y = -k_head_radius * 0.68F;
constexpr QVector2D k_white_extent{0.0270F, 0.0190F};
constexpr QVector2D k_iris_extent{0.0140F, 0.0160F};
constexpr QVector2D k_pupil_extent{0.0072F, 0.0086F};
constexpr QVector2D k_catchlight_offset{0.0056F, 0.0066F};
constexpr float k_catchlight_radius = 0.0036F;
constexpr float k_eye_rim_lift = 0.0010F;
constexpr float k_eye_bulge = 0.0016F;
constexpr float k_catchlight_lift = 0.0008F;

constexpr float k_lid_rise = 0.0172F;
constexpr QVector2D k_lid_extent{0.0300F, 0.0046F};
constexpr float k_lid_roll = 0.07F;
constexpr float k_lid_rim_lift = 0.0018F;
constexpr float k_lid_bulge = 0.0010F;

constexpr float k_brow_rise = k_head_radius * 0.35F;
constexpr QVector2D k_brow_extent{0.0270F, 0.0056F};
constexpr float k_brow_roll = 0.035F;
constexpr float k_brow_rim_lift = 0.0012F;
constexpr float k_brow_bulge = 0.0022F;

constexpr float k_mouth_y = -k_head_radius * 1.23F;
constexpr QVector2D k_mouth_extent{0.0185F, 0.0040F};
constexpr float k_mouth_rim_lift = 0.0010F;
constexpr float k_mouth_bulge = 0.0006F;

struct Ellipsoid {
  QVector3D centre;
  QVector3D radii;
};

struct Feature {
  QVector2D centre;
  float roll = 0.0F;
  QVector2D extent;
  float rim_lift = 0.0F;
  float bulge = 0.0F;
};

class FaceBuilder {
public:
  FaceBuilder(const Ellipsoid& cranium, const Ellipsoid& jaw)
      : m_shells{cranium, jaw} {}

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
    float best_z = -1.0e9F;
    for (auto const& shell : m_shells) {
      float const nx = xy.x() / shell.radii.x();
      float const ny = (xy.y() - shell.centre.y()) / shell.radii.y();
      float const inside = 1.0F - (nx * nx) - (ny * ny);
      if (inside < 0.0F) {
        continue;
      }
      float const nz = std::sqrt(inside);
      float const z = shell.centre.z() + (shell.radii.z() * nz);
      if (z > best_z) {
        best_z = z;
        normal =
            QVector3D(nx / shell.radii.x(), ny / shell.radii.y(), nz / shell.radii.z())
                .normalized();
      }
    }
    if (best_z < -1.0e8F) {
      normal = QVector3D(0.0F, 0.0F, 1.0F);
      best_z = m_shells.front().centre.z();
    }
    return {xy.x(), xy.y(), best_z};
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

  std::array<Ellipsoid, 2> m_shells;
  std::vector<Vertex> m_vertices;
  std::vector<unsigned int> m_indices;
};

} // namespace

auto build_humanoid_face_mesh(const QVector3D& cranium_centre,
                              const QVector3D& cranium_radii,
                              const QVector3D& jaw_centre,
                              const QVector3D& jaw_radii)
    -> std::unique_ptr<Render::GL::Mesh> {
  FaceBuilder face({cranium_centre, cranium_radii}, {jaw_centre, jaw_radii});
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

    Feature const lid{.centre = {side * k_eye_x, k_eye_y + k_lid_rise},
                      .roll = -side * k_lid_roll,
                      .extent = k_lid_extent,
                      .rim_lift = k_lid_rim_lift,
                      .bulge = k_lid_bulge};
    face.disc(lid, k_skin_role, FaceFeatureTone::Lid);

    Feature const brow{.centre = {side * k_eye_x, k_eye_y + k_brow_rise},
                       .roll = side * k_brow_roll,
                       .extent = k_brow_extent,
                       .rim_lift = k_brow_rim_lift,
                       .bulge = k_brow_bulge};
    face.disc(brow, k_humanoid_hair_role, FaceFeatureTone::EyeWhite);
  }
  Feature const mouth{.centre = {0.0F, k_mouth_y},
                      .extent = k_mouth_extent,
                      .rim_lift = k_mouth_rim_lift,
                      .bulge = k_mouth_bulge};
  face.disc(mouth, k_skin_role, FaceFeatureTone::Mouth);
  return face.take();
}

} // namespace Render::Humanoid
