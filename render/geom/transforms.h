#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include "../math/pod_math.h"

namespace Render::Geom {

auto cylinder_between(const QVector3D& a,
                      const QVector3D& b,
                      float radius) -> QMatrix4x4;

auto cylinder_between(const QMatrix4x4& parent,
                      const QVector3D& a,
                      const QVector3D& b,
                      float radius) -> QMatrix4x4;

auto sphere_at(const QVector3D& pos, float radius) -> QMatrix4x4;
auto sphere_at(const QMatrix4x4& parent,
               const QVector3D& pos,
               float radius) -> QMatrix4x4;

auto cone_from_to(const QVector3D& base_center,
                  const QVector3D& apex,
                  float base_radius) -> QMatrix4x4;
auto cone_from_to(const QMatrix4x4& parent,
                  const QVector3D& base_center,
                  const QVector3D& apex,
                  float base_radius) -> QMatrix4x4;

auto capsule_between(const QVector3D& a,
                     const QVector3D& b,
                     float radius) -> QMatrix4x4;
auto capsule_between(const QMatrix4x4& parent,
                     const QVector3D& a,
                     const QVector3D& b,
                     float radius) -> QMatrix4x4;

inline auto to_vec3(const QVector3D& v) -> Render::Math::Vec3 {
  return {v.x(), v.y(), v.z()};
}

inline auto to_qvector3d(const Render::Math::Vec3& v) -> QVector3D {
  return {v.x, v.y, v.z};
}

} // namespace Render::Geom
