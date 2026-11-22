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

[[nodiscard]] inline auto weapon_arc_model_matrix(const QVector3D& position,
                                                  float radius,
                                                  const QVector3D& direction,
                                                  float tilt_radians) -> QMatrix4x4 {
  QMatrix4x4 model;
  model.setToIdentity();
  model.translate(position);

  QVector3D forward(direction.x(), 0.0F, direction.z());
  if (forward.lengthSquared() <= k_min_spark_direction_length_squared) {
    forward = QVector3D(0.0F, 0.0F, 1.0F);
  }
  forward.normalize();
  QVector3D const up(0.0F, 1.0F, 0.0F);
  QVector3D const side = QVector3D::crossProduct(up, forward).normalized();
  float const cos_t = std::cos(tilt_radians);
  float const sin_t = std::sin(tilt_radians);
  QVector3D const across = side * cos_t + up * sin_t;
  QVector3D const normal = up * cos_t - side * sin_t;

  model *= QMatrix4x4(across.x(),
                      normal.x(),
                      forward.x(),
                      0.0F,
                      across.y(),
                      normal.y(),
                      forward.y(),
                      0.0F,
                      across.z(),
                      normal.z(),
                      forward.z(),
                      0.0F,
                      0.0F,
                      0.0F,
                      0.0F,
                      1.0F);
  model.scale(radius);
  return model;
}

} // namespace Render::GL::BackendPipelines
