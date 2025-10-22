#include "flag.h"

namespace Render {
namespace Geom {

namespace {
const QMatrix4x4 kIdentityMatrix;
}

Flag::FlagMatrices Flag::create(float worldX, float worldZ,
                                const QVector3D &flagColor,
                                const QVector3D &poleColor, float scale) {
  FlagMatrices result;
  result.pennantColor = flagColor;
  result.poleColor = poleColor;

  result.pole = kIdentityMatrix;
  result.pole.translate(worldX, (0.15f + 0.15f) * scale, worldZ);
  result.pole.scale(0.03f * scale, 0.30f * scale, 0.03f * scale);

  result.pennant = kIdentityMatrix;
  result.pennant.translate(worldX + 0.10f * scale, (0.25f + 0.15f) * scale,
                           worldZ);
  result.pennant.scale(0.18f * scale, 0.12f * scale, 0.02f * scale);

  result.finial = kIdentityMatrix;
  result.finial.translate(worldX, (0.32f + 0.15f) * scale, worldZ);
  result.finial.scale(0.05f * scale, 0.05f * scale, 0.05f * scale);

  return result;
}

} 
} 
