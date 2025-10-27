#pragma once

#include "../math/pod_math.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::Geom {

auto cylinderBetween(const QVector3D &a, const QVector3D &b,
                     float radius) -> QMatrix4x4;

auto cylinderBetween(const QMatrix4x4 &parent, const QVector3D &a,
                     const QVector3D &b, float radius) -> QMatrix4x4;

auto sphereAt(const QVector3D &pos, float radius) -> QMatrix4x4;
auto sphereAt(const QMatrix4x4 &parent, const QVector3D &pos,
              float radius) -> QMatrix4x4;

auto coneFromTo(const QVector3D &base_center, const QVector3D &apex,
                float base_radius) -> QMatrix4x4;
auto coneFromTo(const QMatrix4x4 &parent, const QVector3D &base_center,
                const QVector3D &apex, float base_radius) -> QMatrix4x4;

auto capsuleBetween(const QVector3D &a, const QVector3D &b,
                    float radius) -> QMatrix4x4;
auto capsuleBetween(const QMatrix4x4 &parent, const QVector3D &a,
                    const QVector3D &b, float radius) -> QMatrix4x4;

inline auto cylinderBetweenPOD(const Render::Math::Vec3 &a,
                               const Render::Math::Vec3 &b,
                               float radius) -> Render::Math::Mat3x4 {
  return Render::Math::cylinderBetweenFast(a, b, radius);
}

inline auto cylinderBetweenPOD(const Render::Math::Mat3x4 &parent,
                               const Render::Math::Vec3 &a,
                               const Render::Math::Vec3 &b,
                               float radius) -> Render::Math::Mat3x4 {
  return Render::Math::cylinderBetweenFast(parent, a, b, radius);
}

inline auto sphereAtPOD(const Render::Math::Vec3 &pos,
                        float radius) -> Render::Math::Mat3x4 {
  return Render::Math::sphereAtFast(pos, radius);
}

inline auto sphereAtPOD(const Render::Math::Mat3x4 &parent,
                        const Render::Math::Vec3 &pos,
                        float radius) -> Render::Math::Mat3x4 {
  return Render::Math::sphereAtFast(parent, pos, radius);
}

inline auto toVec3(const QVector3D &v) -> Render::Math::Vec3 {
  return {v.x(), v.y(), v.z()};
}

inline auto toQVector3D(const Render::Math::Vec3 &v) -> QVector3D {
  return {v.x, v.y, v.z};
}

} // namespace Render::Geom
