#include "render/entity/nations/carthage/swordsman_style.h"

#include <array>

#include "render/entity/nations/material_palette.h"
#include "render/entity/nations/sepulcher/palette.h"

namespace Render::GL::Carthage {

void register_carthage_swordsman_style() {
  SwordsmanStyleConfig carthage;
  Nation::apply_materials(carthage, Nation::k_carthage_materials);
  carthage.shield_color = QVector3D(0.20F, 0.46F, 0.62F);
  carthage.shield_trim_color = Nation::k_carthage_materials.accent;
  carthage.shield_radius_scale = 0.9F;
  carthage.shield_aspect_ratio = 0.85F;
  carthage.has_scabbard = false;
  carthage.shield_cross_decal = false;

  SwordsmanStyleConfig sepulcher = carthage;
  sepulcher.skin_color = Sepulcher::k_bone;
  Nation::apply_materials(sepulcher, Nation::k_sepulcher_materials);
  sepulcher.shield_color = QVector3D(0.165F, 0.175F, 0.195F);
  sepulcher.shield_trim_color = QVector3D(0.44F, 0.42F, 0.36F);
  sepulcher.shield_cross_decal = false;
  sepulcher.has_scabbard = false;

  const std::array<SwordsmanStyleRegistration, 2> styles{{
      {.key = "carthage", .style = carthage},
      {.key = "iron_sepulcher", .style = sepulcher},
  }};
  ::Render::GL::register_swordsman_styles(styles);
}

} // namespace Render::GL::Carthage
