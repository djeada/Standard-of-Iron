#pragma once

#include <QMatrix4x4>
#include <QVector3D>

#include <cmath>

namespace Render::GL::BackendPipelines {

inline constexpr float k_spark_along_stretch = 1.90F;
inline constexpr float k_spark_across_squash = 0.62F;
inline constexpr float k_min_spark_direction_length_squared = 1.0e-6F;

[[nodiscard]] inline auto spark_model_matrix(const QVector3D& position,
                                             float radius,
                                             const QVector3D& direction) -> QMatrix4x4 {
  QMatrix4x4 model;
  model.setToIdentity();
  model.translate(position);

  if (direction.lengthSquared() <= k_min_spark_direction_length_squared) {
    model.scale(radius);
    return model;
  }

  QVector3D const forward = direction.normalized();
  QVector3D reference(0.0F, 1.0F, 0.0F);
  if (std::abs(QVector3D::dotProduct(forward, reference)) > 0.95F) {
    reference = QVector3D(1.0F, 0.0F, 0.0F);
  }
  QVector3D const side = QVector3D::crossProduct(reference, forward).normalized();
  QVector3D const up = QVector3D::crossProduct(forward, side);

  model *= QMatrix4x4(forward.x(),
                      up.x(),
                      side.x(),
                      0.0F,
                      forward.y(),
                      up.y(),
                      side.y(),
                      0.0F,
                      forward.z(),
                      up.z(),
                      side.z(),
                      0.0F,
                      0.0F,
                      0.0F,
                      0.0F,
                      1.0F);
  model.scale(radius * k_spark_along_stretch,
              radius * k_spark_across_squash,
              radius * k_spark_across_squash);
  return model;
}

} // namespace Render::GL::BackendPipelines
