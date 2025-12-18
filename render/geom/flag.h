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
    QVector3D pennantColor;
    QVector3D poleColor;
  };

  static auto create(float world_x, float world_z,
                     const QVector3D &flagColor = QVector3D(1.0F, 0.9F, 0.2F),
                     const QVector3D &poleColor = QVector3D(0.3F, 0.2F, 0.1F),
                     float scale = 1.0F) -> FlagMatrices;
};

} 
