#include "transforms.h"
#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>
#include <algorithm>
#include <cmath>

namespace Render::Geom {

namespace {
const float k_epsilon = 1e-6F;
const float k_epsilon_sq = k_epsilon * k_epsilon;
constexpr float k_flip_rotation_degrees = 180.0F;
} 

auto cylinder_between(const QVector3D &a, const QVector3D &b,
                      float radius) -> QMatrix4x4 {

  const QVector3D diff = b - a;
  const float len_sq = diff.lengthSquared();

  QMatrix4x4 m;
  m.translate((a + b) * 0.5F);

  if (len_sq > k_epsilon_sq) {
    const float len = std::sqrt(len_sq);
    const QVector3D dir = diff / len;

    const QVector3D up(0.0F, 1.0F, 0.0F);
    if (QVector3D::dotProduct(up, dir) < -0.99999F) {
      m.rotate(180.0F, 1.0F, 0.0F, 0.0F);
    } else {
      QQuaternion rot = QQuaternion::rotationTo(up, dir);
      m.rotate(rot);
    }
    m.scale(radius, len, radius);
  } else {
    m.scale(radius, 1.0F, radius);
  }
  return m;
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

  const QVector3D diff = b - a;
  const float len_sq = diff.lengthSquared();

  QMatrix4x4 m = parent;
  m.translate((a + b) * 0.5F);

  if (len_sq > k_epsilon_sq) {
    const float len = std::sqrt(len_sq);
    const QVector3D dir = diff / len;

    QQuaternion rot = QQuaternion::rotationTo(QVector3D(0.0F, 1.0F, 0.0F), dir);
    m.rotate(rot);
    m.scale(radius, len, radius);
  } else {
    m.scale(radius, 1.0F, radius);
  }
  return m;
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

} 
