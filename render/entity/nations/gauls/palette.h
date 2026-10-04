#pragma once

#include <QVector3D>

#include "render/entity/swordsman_renderer_common.h"

namespace Render::GL::Gauls {

inline auto troop_style() -> SwordsmanStyleConfig {
  SwordsmanStyleConfig style;
  style.cloth_color = QVector3D(0.50F, 0.33F, 0.15F);
  style.leather_color = QVector3D(0.38F, 0.25F, 0.13F);
  style.leather_dark_color = QVector3D(0.24F, 0.16F, 0.10F);
  style.metal_color = QVector3D(0.60F, 0.61F, 0.60F);
  style.cloth_team_weight = 0.30F;
  style.cloth_style_weight = 0.72F;
  style.has_scabbard = true;
  return style;
}

} // namespace Render::GL::Gauls
