#include "transforms.h"
#include "affine_matrix.h"
#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>

namespace Render::Geom {

namespace {
const float k_epsilon = 1e-6F;
const float k_epsilon_sq = k_epsilon * k_epsilon;

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

auto make_cylinder_local_matrix(const QVector3D &a, const QVector3D &b,
                                float radius) -> QMatrix4x4 {
  const float ax = a.x();
  const float ay = a.y();
  const float az = a.z();
  const float bx = b.x();
  const float by = b.y();
  const float bz = b.z();

  const float dx = bx - ax;
  const float dy = by - ay;
  const float dz = bz - az;
  const float len_sq = dx * dx + dy * dy + dz * dz;
  const float cx = (ax + bx) * 0.5F;
  const float cy = (ay + by) * 0.5F;
  const float cz = (az + bz) * 0.5F;

  QMatrix4x4 m;
  if (len_sq <= k_epsilon_sq) {
    set_affine_columns(m, radius, 0.0F, 0.0F, 0.0F, 1.0F, 0.0F, 0.0F, 0.0F,
                       radius, cx, cy, cz);
    return m;
  }

  const float len = std::sqrt(len_sq);
  const float inv_len = 1.0F / len;
  const float ux = dx * inv_len;
  const float uy = dy * inv_len;
  const float uz = dz * inv_len;

  float tx = 0.0F;
  float ty = 0.0F;
  float tz = 0.0F;
  if (std::abs(uy) < 0.999F) {
    tx = uz;
    ty = 0.0F;
    tz = -ux;
  } else {
    tx = 0.0F;
    ty = -uz;
    tz = uy;
  }
  const float t_len_sq = tx * tx + ty * ty + tz * tz;
  if (t_len_sq > k_epsilon_sq) {
    const float inv_t_len = 1.0F / std::sqrt(t_len_sq);
    tx *= inv_t_len;
    ty *= inv_t_len;
    tz *= inv_t_len;
  } else {
    tx = 1.0F;
    ty = 0.0F;
    tz = 0.0F;
  }

  const float bxv = (uy * tz) - (uz * ty);
  const float byv = (uz * tx) - (ux * tz);
  const float bzv = (ux * ty) - (uy * tx);

  set_affine_columns(m, tx * radius, ty * radius, tz * radius, dx, dy, dz,
                     bxv * radius, byv * radius, bzv * radius, cx, cy, cz);
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
