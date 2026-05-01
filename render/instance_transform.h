#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <QVector4D>

namespace Render::GL {

inline auto make_basis_attachment_transform_scaled(
    const QMatrix4x4 &parent, const QVector3D &origin, const QVector3D &right,
    const QVector3D &up, const QVector3D &forward,
    const QVector3D &local_offset, const QVector3D &axis_scale) -> QMatrix4x4 {
  QMatrix4x4 m = parent;
  QVector3D const world_pos = origin +
                              right * (local_offset.x() * axis_scale.x()) +
                              up * (local_offset.y() * axis_scale.y()) +
                              forward * (local_offset.z() * axis_scale.z());
  m.translate(world_pos);

  QMatrix4x4 basis;
  basis.setColumn(0, QVector4D(right * axis_scale.x(), 0.0F));
  basis.setColumn(1, QVector4D(up * axis_scale.y(), 0.0F));
  basis.setColumn(2, QVector4D(forward * axis_scale.z(), 0.0F));
  basis.setColumn(3, QVector4D(0.0F, 0.0F, 0.0F, 1.0F));
  return m * basis;
}

inline auto make_basis_attachment_transform(
    const QMatrix4x4 &parent, const QVector3D &origin, const QVector3D &right,
    const QVector3D &up, const QVector3D &forward,
    const QVector3D &local_offset, float uniform_scale) -> QMatrix4x4 {
  return make_basis_attachment_transform_scaled(
      parent, origin, right, up, forward, local_offset,
      QVector3D(uniform_scale, uniform_scale, uniform_scale));
}

} // namespace Render::GL
