#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

namespace Render::GL {

inline auto oriented_segment_transform(
    const QMatrix4x4 &parent, const QVector3D &origin, const QVector3D &axis_y,
    QVector3D right_hint = QVector3D(0.0F, 0.0F, 1.0F)) -> QMatrix4x4 {
  QVector3D y_axis = axis_y;
  float const length = y_axis.length();
  if (length <= 1e-5F) {
    return parent;
  }
  y_axis /= length;

  QVector3D x_axis = QVector3D::crossProduct(right_hint, y_axis);
  if (x_axis.lengthSquared() < 1e-5F) {
    x_axis = QVector3D::crossProduct(QVector3D(1.0F, 0.0F, 0.0F), y_axis);
  }
  x_axis.normalize();
  QVector3D z_axis = QVector3D::crossProduct(x_axis, y_axis).normalized();

  QMatrix4x4 local;
  local.setColumn(0, QVector4D(x_axis, 0.0F));
  local.setColumn(1, QVector4D(y_axis * length, 0.0F));
  local.setColumn(2, QVector4D(z_axis, 0.0F));
  local.setColumn(3, QVector4D(origin, 1.0F));
  return parent * local;
}

} // namespace Render::GL
