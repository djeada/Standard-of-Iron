#pragma once

#include <QVector3D>

#include "render/entity/swordsman_renderer_common.h"

namespace Render::GL::Iberians {

inline auto troop_style() -> SwordsmanStyleConfig {
  SwordsmanStyleConfig style;
  style.cloth_color = QVector3D(0.93F, 0.91F, 0.86F);
  style.leather_color = QVector3D(0.34F, 0.22F, 0.12F);
  style.leather_dark_color = QVector3D(0.22F, 0.14F, 0.09F);
  style.metal_color = QVector3D(0.66F, 0.66F, 0.66F);
  style.cloth_team_weight = 0.14F;
  style.cloth_style_weight = 0.86F;
  style.has_scabbard = true;
  return style;
}

} // namespace Render::GL::Iberians
