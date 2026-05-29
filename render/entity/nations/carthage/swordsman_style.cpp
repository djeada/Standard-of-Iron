#include "swordsman_style.h"

#include <array>

namespace Render::GL::Carthage {

void register_carthage_swordsman_style() {
  KnightStyleConfig carthage;
  carthage.cloth_color = QVector3D(0.15F, 0.36F, 0.55F);
  carthage.leather_color = QVector3D(0.32F, 0.22F, 0.12F);
  carthage.leather_dark_color = QVector3D(0.20F, 0.14F, 0.09F);
  carthage.metal_color = QVector3D(0.70F, 0.68F, 0.52F);
  carthage.shield_color = QVector3D(0.20F, 0.46F, 0.62F);
  carthage.shield_trim_color = QVector3D(0.76F, 0.68F, 0.42F);
  carthage.shield_radius_scale = 0.9F;
  carthage.shield_aspect_ratio = 0.85F;
  carthage.has_scabbard = false;
  carthage.shield_cross_decal = false;

  KnightStyleConfig sepulcher = carthage;
  sepulcher.skin_color = QVector3D(0.82F, 0.84F, 0.79F);
  sepulcher.cloth_color = QVector3D(0.18F, 0.18F, 0.20F);
  sepulcher.leather_color = QVector3D(0.24F, 0.21F, 0.18F);
  sepulcher.leather_dark_color = QVector3D(0.12F, 0.12F, 0.13F);
  sepulcher.metal_color = QVector3D(0.58F, 0.61F, 0.67F);
  sepulcher.shield_color = QVector3D(0.26F, 0.28F, 0.31F);
  sepulcher.shield_trim_color = QVector3D(0.71F, 0.72F, 0.76F);
  sepulcher.shield_cross_decal = false;
  sepulcher.has_scabbard = false;

  const std::array<SwordsmanStyleRegistration, 2> styles{{
      {.key = "carthage", .style = carthage},
      {.key = "iron_sepulcher", .style = sepulcher},
  }};
  ::Render::GL::register_swordsman_styles(styles);
}

} // namespace Render::GL::Carthage
