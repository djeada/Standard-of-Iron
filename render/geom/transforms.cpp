#include "transforms.h"
#include <algorithm>
#include <cmath>

namespace Render::Geom {

namespace {
const QVector3D kYAxis(0, 1, 0);
const float kRadToDeg = 57.2957795131f;
const float kEpsilon = 1e-6f;
const float kEpsilonSq = kEpsilon * kEpsilon;
} // namespace

QMatrix4x4 cylinderBetween(const QVector3D &a, const QVector3D &b,
                           float radius) {

  const float dx = b.x() - a.x();
  const float dy = b.y() - a.y();
  const float dz = b.z() - a.z();
  const float lenSq = dx * dx + dy * dy + dz * dz;

  QMatrix4x4 M;

  M.translate((a.x() + b.x()) * 0.5f, (a.y() + b.y()) * 0.5f,
              (a.z() + b.z()) * 0.5f);

  if (lenSq > kEpsilonSq) {
    const float len = std::sqrt(lenSq);

    const float invLen = 1.0f / len;
    const float ndx = dx * invLen;
    const float ndy = dy * invLen;
    const float ndz = dz * invLen;

    const float dot = std::clamp(ndy, -1.0f, 1.0f);
    const float angleDeg = std::acos(dot) * kRadToDeg;

    const float axisX = ndz;
    const float axisZ = -ndx;
    const float axisLenSq = axisX * axisX + axisZ * axisZ;

    if (axisLenSq < kEpsilonSq) {

      if (dot < 0.0f) {
        M.rotate(180.0f, 1.0f, 0.0f, 0.0f);
      }
    } else {

      const float axisInvLen = 1.0f / std::sqrt(axisLenSq);
      M.rotate(angleDeg, axisX * axisInvLen, 0.0f, axisZ * axisInvLen);
    }
    M.scale(radius, len, radius);
  } else {
    M.scale(radius, 1.0f, radius);
  }
  return M;
}

QMatrix4x4 sphereAt(const QVector3D &pos, float radius) {
  QMatrix4x4 M;
  M.translate(pos);
  M.scale(radius, radius, radius);
  return M;
}

QMatrix4x4 sphereAt(const QMatrix4x4 &parent, const QVector3D &pos,
                    float radius) {
  QMatrix4x4 M = parent;
  M.translate(pos);
  M.scale(radius, radius, radius);
  return M;
}

QMatrix4x4 cylinderBetween(const QMatrix4x4 &parent, const QVector3D &a,
                           const QVector3D &b, float radius) {

  const float dx = b.x() - a.x();
  const float dy = b.y() - a.y();
  const float dz = b.z() - a.z();
  const float lenSq = dx * dx + dy * dy + dz * dz;

  QMatrix4x4 M = parent;

  M.translate((a.x() + b.x()) * 0.5f, (a.y() + b.y()) * 0.5f,
              (a.z() + b.z()) * 0.5f);

  if (lenSq > kEpsilonSq) {
    const float len = std::sqrt(lenSq);

    const float invLen = 1.0f / len;
    const float ndx = dx * invLen;
    const float ndy = dy * invLen;
    const float ndz = dz * invLen;

    const float dot = std::clamp(ndy, -1.0f, 1.0f);
    const float angleDeg = std::acos(dot) * kRadToDeg;

    const float axisX = ndz;
    const float axisZ = -ndx;
    const float axisLenSq = axisX * axisX + axisZ * axisZ;

    if (axisLenSq < kEpsilonSq) {

      if (dot < 0.0f) {
        M.rotate(180.0f, 1.0f, 0.0f, 0.0f);
      }
    } else {

      const float axisInvLen = 1.0f / std::sqrt(axisLenSq);
      M.rotate(angleDeg, axisX * axisInvLen, 0.0f, axisZ * axisInvLen);
    }
    M.scale(radius, len, radius);
  } else {
    M.scale(radius, 1.0f, radius);
  }
  return M;
}

QMatrix4x4 coneFromTo(const QVector3D &baseCenter, const QVector3D &apex,
                      float baseRadius) {
  return cylinderBetween(baseCenter, apex, baseRadius);
}

QMatrix4x4 coneFromTo(const QMatrix4x4 &parent, const QVector3D &baseCenter,
                      const QVector3D &apex, float baseRadius) {
  return cylinderBetween(parent, baseCenter, apex, baseRadius);
}

QMatrix4x4 capsuleBetween(const QVector3D &a, const QVector3D &b,
                          float radius) {
  return cylinderBetween(a, b, radius);
}

QMatrix4x4 capsuleBetween(const QMatrix4x4 &parent, const QVector3D &a,
                          const QVector3D &b, float radius) {
  return cylinderBetween(parent, a, b, radius);
}

} // namespace Render::Geom
