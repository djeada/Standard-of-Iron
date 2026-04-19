#pragma once

#include <QMatrix4x4>
#include <QVector3D>
#include <cmath>

namespace Render::Math {

struct BoneFrame {
  QVector3D origin{0.0F, 0.0F, 0.0F};
  QVector3D right{1.0F, 0.0F, 0.0F};
  QVector3D up{0.0F, 1.0F, 0.0F};
  QVector3D forward{0.0F, 0.0F, 1.0F};

  [[nodiscard]] auto valid() const noexcept -> bool {
    return right.lengthSquared() > 1e-8F && up.lengthSquared() > 1e-8F &&
           forward.lengthSquared() > 1e-8F;
  }
};

[[nodiscard]] inline auto
make_bone_frame(const QVector3D &origin, const QVector3D &up_axis,
                const QVector3D &right_reference) noexcept -> BoneFrame {
  BoneFrame f;
  f.origin = origin;

  QVector3D up = up_axis;
  if (up.lengthSquared() < 1e-8F) {
    up = QVector3D{0.0F, 1.0F, 0.0F};
  } else {
    up.normalize();
  }

  QVector3D right =
      right_reference - up * QVector3D::dotProduct(right_reference, up);
  if (right.lengthSquared() < 1e-8F) {
    const QVector3D fallback = std::abs(up.y()) < 0.9F
                                   ? QVector3D{1.0F, 0.0F, 0.0F}
                                   : QVector3D{0.0F, 0.0F, 1.0F};
    right = fallback - up * QVector3D::dotProduct(fallback, up);
  }
  right.normalize();

  QVector3D forward = QVector3D::crossProduct(right, up);
  forward.normalize();

  f.up = up;
  f.right = right;
  f.forward = forward;
  return f;
}

} // namespace Render::Math
