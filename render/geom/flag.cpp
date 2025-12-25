#include "flag.h"
#include <qmatrix4x4.h>
#include <qvectornd.h>

namespace Render::Geom {

namespace {
const QMatrix4x4 k_identity_matrix;
}

auto Flag::create(float world_x, float world_z, const QVector3D &flagColor,
                  const QVector3D &poleColor,
                  float scale) -> Flag::FlagMatrices {
  FlagMatrices result;
  result.pennantColor = flagColor;
  result.poleColor = poleColor;

  result.pole = k_identity_matrix;
  result.pole.translate(world_x, (0.15F + 0.35F) * scale, world_z);
  result.pole.scale(0.05F * scale, 0.70F * scale, 0.05F * scale);

  result.pennant = k_identity_matrix;
  result.pennant.translate(world_x + 0.20F * scale, (0.60F + 0.15F) * scale,
                           world_z);
  result.pennant.scale(0.38F * scale, 0.28F * scale, 0.03F * scale);

  result.finial = k_identity_matrix;
  result.finial.translate(world_x, (0.90F + 0.15F) * scale, world_z);
  result.finial.scale(0.10F * scale, 0.10F * scale, 0.10F * scale);

  return result;
}

} 
