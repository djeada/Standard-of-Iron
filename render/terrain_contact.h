#pragma once

#include <QMatrix4x4>
#include <QQuaternion>
#include <QVector3D>

#include <algorithm>
#include <cmath>

#include "game/map/terrain_service.h"
#include "game/map/terrain_surface.h"

namespace Render {

[[nodiscard]] inline auto ground_tilt_rotation(const QVector3D& ground_normal,
                                               float max_degrees,
                                               float weight = 1.0F) -> QQuaternion {
  QVector3D const up(0.0F, 1.0F, 0.0F);
  if (weight <= 0.0F || ground_normal.y() <= 0.0F) {
    return {};
  }
  QVector3D const normal = ground_normal.normalized();
  float const cosine = std::clamp(QVector3D::dotProduct(up, normal), -1.0F, 1.0F);
  float const angle = std::acos(cosine) * (180.0F / 3.14159265358979323846F);
  if (angle < 0.05F) {
    return {};
  }
  QVector3D const axis = QVector3D::crossProduct(up, normal).normalized();
  float const applied = std::min(angle, max_degrees) * std::clamp(weight, 0.0F, 1.0F);
  return QQuaternion::fromAxisAndAngle(axis, applied);
}

inline void tilt_model_to_ground(QMatrix4x4& model,
                                 const Game::Map::TerrainService& terrain,
                                 float max_degrees,
                                 float weight = 1.0F) {
  if (!terrain.is_initialized() || weight <= 0.0F) {
    return;
  }
  QVector3D const origin = model.column(3).toVector3D();
  QQuaternion const tilt = ground_tilt_rotation(
      terrain.sample_ground_normal(origin.x(), origin.z()), max_degrees, weight);
  if (tilt.isIdentity()) {
    return;
  }
  QMatrix4x4 world_tilt;
  world_tilt.translate(origin);
  world_tilt.rotate(tilt);
  world_tilt.translate(-origin);
  model = world_tilt * model;
}

inline void pitch_model_to_ground(QMatrix4x4& model,
                                  const Game::Map::TerrainService& terrain,
                                  float max_degrees) {
  if (!terrain.is_initialized()) {
    return;
  }
  QVector3D const origin = model.column(3).toVector3D();
  QVector3D forward = model.column(2).toVector3D();
  forward.setY(0.0F);
  if (forward.lengthSquared() < 1.0e-8F) {
    return;
  }
  forward.normalize();
  QVector3D const normal = terrain.sample_ground_normal(origin.x(), origin.z());
  if (normal.y() <= 1.0e-3F) {
    return;
  }
  float const rise =
      -(normal.x() * forward.x() + normal.z() * forward.z()) / normal.y();
  float const degrees = std::clamp(
      std::atan(rise) * (180.0F / 3.14159265358979323846F), -max_degrees, max_degrees);
  if (std::abs(degrees) < 0.05F) {
    return;
  }
  QVector3D const axis =
      QVector3D::crossProduct(forward, QVector3D(0.0F, 1.0F, 0.0F)).normalized();
  QMatrix4x4 world_pitch;
  world_pitch.translate(origin);
  world_pitch.rotate(QQuaternion::fromAxisAndAngle(axis, degrees));
  world_pitch.translate(-origin);
  model = world_pitch * model;
}

[[nodiscard]] inline auto bedded_prop_world_y(const Game::Map::TerrainService& terrain,
                                              float world_x,
                                              float world_z,
                                              float surface_y,
                                              float contact_radius,
                                              float max_depth) -> float {
  if (!terrain.is_initialized()) {
    return surface_y;
  }
  return surface_y -
         Game::Map::slope_bed_depth(
             terrain.sample_ground_normal(world_x, world_z), contact_radius, max_depth);
}

} // namespace Render
