#include "swordsman_style.h"
#include "swordsman_renderer.h"

#include <QVector3D>

namespace {
constexpr QVector3D k_carthage_cloth{0.15F, 0.36F, 0.55F};
constexpr QVector3D k_carthage_leather{0.32F, 0.22F, 0.12F};
constexpr QVector3D k_carthage_leather_dark{0.20F, 0.14F, 0.09F};
constexpr QVector3D k_carthage_metal{0.70F, 0.68F, 0.52F};
constexpr QVector3D k_carthage_shield{0.20F, 0.46F, 0.62F};
constexpr QVector3D k_carthage_trim{0.76F, 0.68F, 0.42F};
} 

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
  style.shader_id = "swordsman_carthage";

  register_swordsman_style("carthage", style);
}

} 
