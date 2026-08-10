#include "swordsman_style.h"

#include <array>

#include "render/entity/nations/sepulcher/palette.h"

namespace Render::GL::Carthage {

void register_carthage_swordsman_style() {
  KnightStyleConfig carthage;
  carthage.cloth_color = QVector3D(0.15F, 0.36F, 0.55F);
  carthage.leather_color = QVector3D(0.32F, 0.22F, 0.12F);
  carthage.leather_dark_color = QVector3D(0.32F, 0.22F, 0.15F);
  carthage.metal_color = QVector3D(0.70F, 0.68F, 0.52F);
  carthage.shield_color = QVector3D(0.20F, 0.46F, 0.62F);
  carthage.shield_trim_color = QVector3D(0.76F, 0.68F, 0.42F);
  carthage.shield_radius_scale = 0.9F;
  carthage.shield_aspect_ratio = 0.85F;
  carthage.has_scabbard = false;
  carthage.shield_cross_decal = false;

  KnightStyleConfig sepulcher = carthage;
  sepulcher.skin_color = Sepulcher::k_bone;
  sepulcher.cloth_color = Sepulcher::k_grave_cloth;
  sepulcher.leather_color = Sepulcher::k_grave_leather;
  sepulcher.leather_dark_color = Sepulcher::k_grave_leather_dark;
  sepulcher.metal_color = Sepulcher::k_grave_iron;
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
