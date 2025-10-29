#include "transforms.h"
#include <algorithm>
#include <cmath>
#include <qmatrix4x4.h>
#include <qvectornd.h>

namespace Render::Geom {

namespace {
const QVector3D k_yaxis(0, 1, 0);
const float k_rad_to_deg = 57.2957795131F;
const float k_epsilon = 1e-6F;
const float k_epsilonSq = k_epsilon * k_epsilon;
constexpr float k_flip_rotation_degrees = 180.0F;
} // namespace

auto cylinderBetween(const QVector3D &a, const QVector3D &b,
                     float radius) -> QMatrix4x4 {

  const float dx = b.x() - a.x();
  const float dy = b.y() - a.y();
  const float dz = b.z() - a.z();
  const float len_sq = dx * dx + dy * dy + dz * dz;

  QMatrix4x4 M;

  M.translate((a.x() + b.x()) * 0.5F, (a.y() + b.y()) * 0.5F,
              (a.z() + b.z()) * 0.5F);

  if (len_sq > k_epsilonSq) {
    const float len = std::sqrt(len_sq);

    const float inv_len = 1.0F / len;
    const float ndx = dx * inv_len;
    const float ndy = dy * inv_len;
    const float ndz = dz * inv_len;

    const float dot = std::clamp(ndy, -1.0F, 1.0F);
    const float angle_deg = std::acos(dot) * k_rad_to_deg;

    const float axis_x = ndz;
    const float axis_z = -ndx;
    const float axis_len_sq = axis_x * axis_x + axis_z * axis_z;

    if (axis_len_sq < k_epsilonSq) {

      if (dot < 0.0F) {
        M.rotate(k_flip_rotation_degrees, 1.0F, 0.0F, 0.0F);
      }
    } else {

      const float axis_inv_len = 1.0F / std::sqrt(axis_len_sq);
      M.rotate(angle_deg, axis_x * axis_inv_len, 0.0F, axis_z * axis_inv_len);
    }
    M.scale(radius, len, radius);
  } else {
    M.scale(radius, 1.0F, radius);
  }
  return M;
}

auto sphereAt(const QVector3D &pos, float radius) -> QMatrix4x4 {
  QMatrix4x4 M;
  M.translate(pos);
  M.scale(radius, radius, radius);
  return M;
}

auto sphereAt(const QMatrix4x4 &parent, const QVector3D &pos,
              float radius) -> QMatrix4x4 {
  QMatrix4x4 M = parent;
  M.translate(pos);
  M.scale(radius, radius, radius);
  return M;
}

auto cylinderBetween(const QMatrix4x4 &parent, const QVector3D &a,
                     const QVector3D &b, float radius) -> QMatrix4x4 {

  const float dx = b.x() - a.x();
  const float dy = b.y() - a.y();
  const float dz = b.z() - a.z();
  const float len_sq = dx * dx + dy * dy + dz * dz;

  QMatrix4x4 M = parent;

  M.translate((a.x() + b.x()) * 0.5F, (a.y() + b.y()) * 0.5F,
              (a.z() + b.z()) * 0.5F);

  if (len_sq > k_epsilonSq) {
    const float len = std::sqrt(len_sq);

    const float inv_len = 1.0F / len;
    const float ndx = dx * inv_len;
    const float ndy = dy * inv_len;
    const float ndz = dz * inv_len;

    const float dot = std::clamp(ndy, -1.0F, 1.0F);
    const float angle_deg = std::acos(dot) * k_rad_to_deg;

    const float axis_x = ndz;
    const float axis_z = -ndx;
    const float axis_len_sq = axis_x * axis_x + axis_z * axis_z;

    if (axis_len_sq < k_epsilonSq) {

      if (dot < 0.0F) {
        M.rotate(k_flip_rotation_degrees, 1.0F, 0.0F, 0.0F);
      }
    } else {

      const float axis_inv_len = 1.0F / std::sqrt(axis_len_sq);
      M.rotate(angle_deg, axis_x * axis_inv_len, 0.0F, axis_z * axis_inv_len);
    }
    M.scale(radius, len, radius);
  } else {
    M.scale(radius, 1.0F, radius);
  }
  return M;
}

auto coneFromTo(const QVector3D &base_center, const QVector3D &apex,
                float base_radius) -> QMatrix4x4 {
  return cylinderBetween(base_center, apex, base_radius);
}

auto coneFromTo(const QMatrix4x4 &parent, const QVector3D &base_center,
                const QVector3D &apex, float base_radius) -> QMatrix4x4 {
  return cylinderBetween(parent, base_center, apex, base_radius);
}

auto capsuleBetween(const QVector3D &a, const QVector3D &b,
                    float radius) -> QMatrix4x4 {
  return cylinderBetween(a, b, radius);
}

auto capsuleBetween(const QMatrix4x4 &parent, const QVector3D &a,
                    const QVector3D &b, float radius) -> QMatrix4x4 {
  return cylinderBetween(parent, a, b, radius);
}

} // namespace Render::Geom
