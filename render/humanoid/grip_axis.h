#pragma once

#include <QVector3D>

#include "humanoid_spec.h"

namespace Render::Humanoid {

[[nodiscard]] inline auto
hand_axis_for_weapon_direction(const QVector3D& wanted_direction,
                               const QVector3D& baked_direction,
                               bool right_hand = true) -> QVector3D {
  auto const& bind_frames = humanoid_bind_body_frames();
  auto const& bind_hand = right_hand ? bind_frames.hand_r : bind_frames.hand_l;

  QVector3D axis = bind_hand.up;
  if (axis.lengthSquared() < 1.0e-8F) {
    axis = QVector3D(0.0F, 1.0F, 0.0F);
  }
  axis.normalize();

  QVector3D right = bind_hand.right;
  if (right.lengthSquared() < 1.0e-8F) {
    return axis;
  }
  right.normalize();

  auto project = [&right](const QVector3D& direction) -> QVector3D {
    QVector3D planar = direction - right * QVector3D::dotProduct(direction, right);
    if (planar.lengthSquared() < 1.0e-8F) {
      return {};
    }
    planar.normalize();
    return planar;
  };

  QVector3D const from = project(baked_direction);
  QVector3D const to = project(wanted_direction);
  if (from.isNull() || to.isNull()) {
    return axis;
  }

  float const cos_turn = QVector3D::dotProduct(from, to);
  float const sin_turn =
      QVector3D::dotProduct(QVector3D::crossProduct(right, from), to);

  QVector3D const parallel = right * QVector3D::dotProduct(axis, right);
  QVector3D const perpendicular = axis - parallel;
  QVector3D rotated = parallel + perpendicular * cos_turn +
                      QVector3D::crossProduct(right, perpendicular) * sin_turn;
  if (rotated.lengthSquared() < 1.0e-8F) {
    return axis;
  }
  rotated.normalize();
  return rotated;
}

} // namespace Render::Humanoid
