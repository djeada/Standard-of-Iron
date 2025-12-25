#pragma once

#include "../../gl/draw_context.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render::GL {

inline QMatrix4x4 createArmorTransform(const DrawContext &ctx,
                                       const QVector3D &center,
                                       const QVector3D &up,
                                       const QVector3D &right,
                                       const QVector3D &forward, float width,
                                       float height, float depth) {
  QMatrix4x4 transform = ctx.model;
  transform.translate(center);

  QMatrix4x4 orientation;
  orientation(0, 0) = right.x();
  orientation(0, 1) = up.x();
  orientation(0, 2) = forward.x();
  orientation(1, 0) = right.y();
  orientation(1, 1) = up.y();
  orientation(1, 2) = forward.y();
  orientation(2, 0) = right.z();
  orientation(2, 1) = up.z();
  orientation(2, 2) = forward.z();

  transform = transform * orientation;
  transform.scale(width, height, depth);

  return transform;
}

} 
