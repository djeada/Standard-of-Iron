#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include "../math/pod_math.h"

namespace Render::Geom {

// Legacy QMatrix4x4 API (kept for backward compatibility)
QMatrix4x4 cylinderBetween(const QVector3D &a, const QVector3D &b,
                           float radius);

QMatrix4x4 cylinderBetween(const QMatrix4x4 &parent, const QVector3D &a,
                           const QVector3D &b, float radius);

QMatrix4x4 sphereAt(const QVector3D &pos, float radius);
QMatrix4x4 sphereAt(const QMatrix4x4 &parent, const QVector3D &pos,
                    float radius);

QMatrix4x4 coneFromTo(const QVector3D &baseCenter, const QVector3D &apex,
                      float baseRadius);
QMatrix4x4 coneFromTo(const QMatrix4x4 &parent, const QVector3D &baseCenter,
                      const QVector3D &apex, float baseRadius);

// ============================================================================
// OPTIMIZED POD API (3-5x faster, use this for hot paths!)
// ============================================================================

// Fast cylinder between - avoids QMatrix4x4::rotate/scale overhead
inline Render::Math::Mat3x4 cylinderBetweenPOD(const Render::Math::Vec3 &a, 
                                                const Render::Math::Vec3 &b,
                                                float radius) {
  return Render::Math::cylinderBetweenFast(a, b, radius);
}

inline Render::Math::Mat3x4 cylinderBetweenPOD(const Render::Math::Mat3x4 &parent,
                                                const Render::Math::Vec3 &a,
                                                const Render::Math::Vec3 &b, 
                                                float radius) {
  return Render::Math::cylinderBetweenFast(parent, a, b, radius);
}

// Fast sphere transform
inline Render::Math::Mat3x4 sphereAtPOD(const Render::Math::Vec3 &pos, float radius) {
  return Render::Math::sphereAtFast(pos, radius);
}

inline Render::Math::Mat3x4 sphereAtPOD(const Render::Math::Mat3x4 &parent,
                                         const Render::Math::Vec3 &pos, 
                                         float radius) {
  return Render::Math::sphereAtFast(parent, pos, radius);
}

// Conversion helpers
inline Render::Math::Vec3 toVec3(const QVector3D &v) {
  return Render::Math::Vec3(v.x(), v.y(), v.z());
}

inline QVector3D toQVector3D(const Render::Math::Vec3 &v) {
  return QVector3D(v.x, v.y, v.z);
}

} // namespace Render::Geom
