#include "flag.h"

#include <qmatrix4x4.h>
#include <qvectornd.h>

namespace Render::Geom {

namespace {
const QMatrix4x4 k_identity_matrix;
}

auto Flag::create(float world_x,
                  float world_z,
                  const QVector3D& flag_color,
                  const QVector3D& pole_color,
                  float scale) -> Flag::FlagMatrices {
  FlagMatrices result;
  result.pennant_color = flag_color;
  result.pole_color = pole_color;
  QVector3D const warm_trim(0.88F, 0.76F, 0.28F);
  result.pennant_trim_color = QVector3D(flag_color.x() * 0.42F + warm_trim.x() * 0.58F,
                                        flag_color.y() * 0.42F + warm_trim.y() * 0.58F,
                                        flag_color.z() * 0.42F + warm_trim.z() * 0.58F);

  float const pole_base_y = 0.08F * scale;
  float const pole_top_y = 1.18F * scale;
  result.pole_start = QVector3D(world_x, pole_base_y, world_z);
  result.pole_end = QVector3D(world_x, pole_top_y, world_z);
  result.pole_radius = 0.028F * scale;

  float const pennant_width = 0.52F * scale;
  float const pennant_height = 0.30F * scale;
  float const crossbeam_y = pole_top_y - 0.13F * scale;
  result.crossbeam_start = QVector3D(world_x + 0.02F * scale, crossbeam_y, world_z);
  result.crossbeam_end =
      QVector3D(world_x + pennant_width * 0.84F, crossbeam_y, world_z);
  result.crossbeam_radius = 0.013F * scale;

  QVector3D const pennant_center(world_x + pennant_width * 0.50F,
                                 crossbeam_y - pennant_height * 0.52F,
                                 world_z + 0.015F * scale);
  result.pennant = k_identity_matrix;
  result.pennant.translate(pennant_center);
  result.pennant.rotate(90.0F, 1.0F, 0.0F, 0.0F);
  result.pennant.scale(pennant_width, pennant_height, 1.0F);

  result.pennant_fallback = k_identity_matrix;
  result.pennant_fallback.translate(pennant_center);
  result.pennant_fallback.scale(pennant_width * 0.92F, pennant_height, 0.018F * scale);

  return result;
}

} // namespace Render::Geom
