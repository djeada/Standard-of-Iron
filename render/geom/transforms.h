#pragma once

#include <QMatrix4x4>
#include <QVector3D>

namespace Render::Geom {

QMatrix4x4 cylinderBetween(const QVector3D &a, const QVector3D &b,
                           float radius);

QMatrix4x4 sphereAt(const QVector3D &pos, float radius);

QMatrix4x4 coneFromTo(const QVector3D &baseCenter, const QVector3D &apex,
                      float baseRadius);

} // namespace Render::Geom
