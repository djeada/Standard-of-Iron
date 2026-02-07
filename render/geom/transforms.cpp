#include "transforms.h"
#include "affine_matrix.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>

namespace Render::Geom {

namespace {
const float k_epsilon = 1e-6F;
const float k_epsilon_sq = k_epsilon * k_epsilon;
const QVector3D k_ref_x(1.0F, 0.0F, 0.0F);
const QVector3D k_ref_z(0.0F, 0.0F, 1.0F);

inline void set_affine_columns(QMatrix4x4 &m, const QVector3D &x_axis,
                               const QVector3D &y_axis,
                               const QVector3D &z_axis,
                               const QVector3D &translation) {
  float *d = m.data();
  d[0] = x_axis.x();
  d[1] = x_axis.y();
  d[2] = x_axis.z();
  d[3] = 0.0F;

  d[4] = y_axis.x();
  d[5] = y_axis.y();
  d[6] = y_axis.z();
  d[7] = 0.0F;

  d[8] = z_axis.x();
  d[9] = z_axis.y();
  d[10] = z_axis.z();
  d[11] = 0.0F;

  d[12] = translation.x();
  d[13] = translation.y();
  d[14] = translation.z();
  d[15] = 1.0F;
}

auto make_cylinder_local_matrix(const QVector3D &a, const QVector3D &b,
                                float radius) -> QMatrix4x4 {
  const QVector3D diff = b - a;
  const float len_sq = diff.lengthSquared();
  const QVector3D center = (a + b) * 0.5F;

  QMatrix4x4 m;
  if (len_sq <= k_epsilon_sq) {
    set_affine_columns(m, QVector3D(radius, 0.0F, 0.0F),
                       QVector3D(0.0F, 1.0F, 0.0F),
                       QVector3D(0.0F, 0.0F, radius), center);
    return m;
  }

  const float len = std::sqrt(len_sq);
  const QVector3D axis = diff / len;

  QVector3D tangent = QVector3D::crossProduct(k_ref_x, axis);
  if (tangent.lengthSquared() <= k_epsilon_sq) {
    tangent = QVector3D::crossProduct(k_ref_z, axis);
  }
  tangent.normalize();
  QVector3D bitangent = QVector3D::crossProduct(axis, tangent);
  bitangent.normalize();

  set_affine_columns(m, tangent * radius, axis * len, bitangent * radius,
                     center);
  return m;
}
} // namespace

auto cylinder_between(const QVector3D &a, const QVector3D &b,
                      float radius) -> QMatrix4x4 {
  return make_cylinder_local_matrix(a, b, radius);
}

auto sphere_at(const QVector3D &pos, float radius) -> QMatrix4x4 {
  QMatrix4x4 m;
  m.translate(pos);
  m.scale(radius, radius, radius);
  return m;
}

auto sphere_at(const QMatrix4x4 &parent, const QVector3D &pos,
               float radius) -> QMatrix4x4 {
  QMatrix4x4 m = parent;
  m.translate(pos);
  m.scale(radius, radius, radius);
  return m;
}

auto cylinder_between(const QMatrix4x4 &parent, const QVector3D &a,
                      const QVector3D &b, float radius) -> QMatrix4x4 {
  const QMatrix4x4 local = make_cylinder_local_matrix(a, b, radius);
  return multiply_affine(parent, local);
}

auto cone_from_to(const QVector3D &base_center, const QVector3D &apex,
                  float base_radius) -> QMatrix4x4 {
  return cylinder_between(base_center, apex, base_radius);
}

auto cone_from_to(const QMatrix4x4 &parent, const QVector3D &base_center,
                  const QVector3D &apex, float base_radius) -> QMatrix4x4 {
  return cylinder_between(parent, base_center, apex, base_radius);
}

auto capsule_between(const QVector3D &a, const QVector3D &b,
                     float radius) -> QMatrix4x4 {
  return cylinder_between(a, b, radius);
}

auto capsule_between(const QMatrix4x4 &parent, const QVector3D &a,
                     const QVector3D &b, float radius) -> QMatrix4x4 {
  return cylinder_between(parent, a, b, radius);
}

} // namespace Render::Geom
