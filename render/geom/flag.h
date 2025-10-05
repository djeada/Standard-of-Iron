#pragma once

#include <QMatrix4x4>
#include <QVector3D>

namespace Render {
namespace Geom {

class Flag {
public:
  struct FlagMatrices {
    QMatrix4x4 pole;
    QMatrix4x4 pennant;
    QMatrix4x4 finial;
    QVector3D pennantColor;
    QVector3D poleColor;
  };

  static FlagMatrices
  create(float worldX, float worldZ,
         const QVector3D &flagColor = QVector3D(1.0f, 0.9f, 0.2f),
         const QVector3D &poleColor = QVector3D(0.3f, 0.2f, 0.1f),
         float scale = 1.0f);
};

} // namespace Geom
} // namespace Render
