#include "swordsman_style.h"

#include <QVector3D>

#include "swordsman_renderer.h"

namespace {
constexpr QVector3D k_carthage_cloth{0.15F, 0.36F, 0.55F};
constexpr QVector3D k_sepulcher_bone{0.82F, 0.84F, 0.79F};
constexpr QVector3D k_sepulcher_cloth{0.18F, 0.18F, 0.20F};
constexpr QVector3D k_sepulcher_leather{0.24F, 0.21F, 0.18F};
constexpr QVector3D k_sepulcher_leather_dark{0.12F, 0.12F, 0.13F};
constexpr QVector3D k_sepulcher_metal{0.58F, 0.61F, 0.67F};
constexpr QVector3D k_sepulcher_shield{0.26F, 0.28F, 0.31F};
constexpr QVector3D k_sepulcher_trim{0.71F, 0.72F, 0.76F};
constexpr QVector3D k_carthage_leather{0.32F, 0.22F, 0.12F};
constexpr QVector3D k_carthage_leather_dark{0.20F, 0.14F, 0.09F};
constexpr QVector3D k_carthage_metal{0.70F, 0.68F, 0.52F};
constexpr QVector3D k_carthage_shield{0.20F, 0.46F, 0.62F};
constexpr QVector3D k_carthage_trim{0.76F, 0.68F, 0.42F};
} // namespace

namespace Render::GL::Carthage {

void register_carthage_swordsman_style() {
  KnightStyleConfig style;
  style.cloth_color = k_carthage_cloth;
  style.leather_color = k_carthage_leather;
  style.leather_dark_color = k_carthage_leather_dark;
  style.metal_color = k_carthage_metal;
  style.shield_color = k_carthage_shield;
  style.shield_trim_color = k_carthage_trim;
  style.shield_radius_scale = 0.9F;
  style.shield_aspect_ratio = 0.85F;
  style.has_scabbard = false;
  style.shield_cross_decal = false;

  register_swordsman_style("carthage", style);

  KnightStyleConfig sepulcher = style;
  sepulcher.skin_color = k_sepulcher_bone;
  sepulcher.cloth_color = k_sepulcher_cloth;
  sepulcher.leather_color = k_sepulcher_leather;
  sepulcher.leather_dark_color = k_sepulcher_leather_dark;
  sepulcher.metal_color = k_sepulcher_metal;
  sepulcher.shield_color = k_sepulcher_shield;
  sepulcher.shield_trim_color = k_sepulcher_trim;
  sepulcher.shield_cross_decal = false;
  sepulcher.has_scabbard = false;
  register_swordsman_style("iron_sepulcher", sepulcher);
}

} // namespace Render::GL::Carthage
