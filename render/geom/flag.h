#pragma once

#include <QMatrix4x4>
#include <QVector3D>

namespace Render::Geom {

class Flag {
public:
  struct FlagMatrices {
    QMatrix4x4 pole;
    QMatrix4x4 pennant;
    QMatrix4x4 finial;
    QVector3D pennant_color;
    QVector3D pole_color;
  };

  static auto create(float world_x, float world_z,
                     const QVector3D &flag_color = QVector3D(1.0F, 0.9F, 0.2F),
                     const QVector3D &pole_color = QVector3D(0.3F, 0.2F, 0.1F),
                     float scale = 1.0F) -> FlagMatrices;
};

} // namespace Render::Geom
