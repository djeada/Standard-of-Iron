#include "parts.h"
#include "affine_matrix.h"
#include <cmath>

namespace Render::Geom {

namespace {
constexpr float k_eps = 1e-6F;
constexpr float k_eps_sq = k_eps * k_eps;

inline void set_affine_columns(QMatrix4x4 &m, float xx, float xy, float xz,
                               float yx, float yy, float yz, float zx, float zy,
                               float zz, float tx, float ty, float tz) {
  float *d = m.data();
  d[0] = xx;
  d[1] = xy;
  d[2] = xz;
  d[3] = 0.0F;
  d[4] = yx;
  d[5] = yy;
  d[6] = yz;
  d[7] = 0.0F;
  d[8] = zx;
  d[9] = zy;
  d[10] = zz;
  d[11] = 0.0F;
  d[12] = tx;
  d[13] = ty;
  d[14] = tz;
  d[15] = 1.0F;
}

auto build_oriented_cylinder_matrix(const QVector3D &a, const QVector3D &b,
                                    const QVector3D &right_reference,
                                    float radius_right,
                                    float radius_forward) -> QMatrix4x4 {
  const QVector3D delta = b - a;
  const float len_sq = delta.lengthSquared();
  const QVector3D centre = (a + b) * 0.5F;

  QMatrix4x4 m;

  if (len_sq <= k_eps_sq) {

    QVector3D up{0.0F, 1.0F, 0.0F};
    QVector3D right = right_reference;
    if (right.lengthSquared() < k_eps_sq) {
      right = QVector3D{1.0F, 0.0F, 0.0F};
    }
    right = right - up * QVector3D::dotProduct(right, up);
    if (right.lengthSquared() < k_eps_sq) {
      right = QVector3D{1.0F, 0.0F, 0.0F};
    }
    right.normalize();
    const QVector3D forward = QVector3D::crossProduct(right, up).normalized();
    set_affine_columns(
        m, right.x() * radius_right, right.y() * radius_right,
        right.z() * radius_right, up.x(), up.y(), up.z(),
        forward.x() * radius_forward, forward.y() * radius_forward,
        forward.z() * radius_forward, centre.x(), centre.y(), centre.z());
    return m;
  }

  const QVector3D up_axis = delta;
  const QVector3D up_unit = up_axis.normalized();

  QVector3D right = right_reference -
                    up_unit * QVector3D::dotProduct(right_reference, up_unit);
  if (right.lengthSquared() < k_eps_sq) {

    const QVector3D fallback = std::abs(up_unit.y()) < 0.9F
                                   ? QVector3D{1.0F, 0.0F, 0.0F}
                                   : QVector3D{0.0F, 0.0F, 1.0F};
    right = fallback - up_unit * QVector3D::dotProduct(fallback, up_unit);
    if (right.lengthSquared() < k_eps_sq) {
      right = QVector3D{1.0F, 0.0F, 0.0F};
    }
  }
  right.normalize();

  QVector3D forward = QVector3D::crossProduct(right, up_unit);
  if (forward.lengthSquared() < k_eps_sq) {
    forward = QVector3D{0.0F, 0.0F, 1.0F};
  } else {
    forward.normalize();
  }

  set_affine_columns(m, right.x() * radius_right, right.y() * radius_right,
                     right.z() * radius_right, up_axis.x(), up_axis.y(),
                     up_axis.z(), forward.x() * radius_forward,
                     forward.y() * radius_forward, forward.z() * radius_forward,
                     centre.x(), centre.y(), centre.z());
  return m;
}

auto build_oriented_basis_matrix(const Render::Math::BoneFrame &frame,
                                 const QVector3D &scale) -> QMatrix4x4 {
  QMatrix4x4 m;
  set_affine_columns(m, frame.right.x() * scale.x(),
                     frame.right.y() * scale.x(), frame.right.z() * scale.x(),
                     frame.up.x() * scale.y(), frame.up.y() * scale.y(),
                     frame.up.z() * scale.y(), frame.forward.x() * scale.z(),
                     frame.forward.y() * scale.z(),
                     frame.forward.z() * scale.z(), frame.origin.x(),
                     frame.origin.y(), frame.origin.z());
  return m;
}

} // namespace

auto oriented_cylinder(const QVector3D &a, const QVector3D &b,
                       const QVector3D &right_reference, float radius_right,
                       float radius_forward) -> QMatrix4x4 {
  return build_oriented_cylinder_matrix(a, b, right_reference, radius_right,
                                        radius_forward);
}

auto oriented_cylinder(const QMatrix4x4 &parent, const QVector3D &a,
                       const QVector3D &b, const QVector3D &right_reference,
                       float radius_right, float radius_forward) -> QMatrix4x4 {
  const QMatrix4x4 local = build_oriented_cylinder_matrix(
      a, b, right_reference, radius_right, radius_forward);
  return multiply_affine(parent, local);
}

auto oriented_box(const Render::Math::BoneFrame &frame,
                  const QVector3D &half_extents) -> QMatrix4x4 {
  return build_oriented_basis_matrix(frame, half_extents * 2.0F);
}

auto oriented_box(const QMatrix4x4 &parent,
                  const Render::Math::BoneFrame &frame,
                  const QVector3D &half_extents) -> QMatrix4x4 {
  return multiply_affine(parent, oriented_box(frame, half_extents));
}

auto oriented_sphere(const Render::Math::BoneFrame &frame,
                     const QVector3D &radii) -> QMatrix4x4 {
  return build_oriented_basis_matrix(frame, radii);
}

auto oriented_sphere(const QMatrix4x4 &parent,
                     const Render::Math::BoneFrame &frame,
                     const QVector3D &radii) -> QMatrix4x4 {
  return multiply_affine(parent, oriented_sphere(frame, radii));
}

} // namespace Render::Geom
