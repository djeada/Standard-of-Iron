#pragma once

#include <QVector3D>

#include "render/entity/swordsman_renderer_common.h"
#include "render/equipment/material_palette.h"

namespace Render::GL::Gauls {

inline auto troop_style() -> SwordsmanStyleConfig {
  SwordsmanStyleConfig style;
  Nation::apply_materials(style, Nation::k_gallic_materials);
  style.cloth_team_weight = 0.18F;
  style.cloth_style_weight = 0.86F;
  style.has_scabbard = true;
  return style;
}

} // namespace Render::GL::Gauls
