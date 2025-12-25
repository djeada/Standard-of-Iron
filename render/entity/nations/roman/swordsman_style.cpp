#include "swordsman_style.h"
#include "swordsman_renderer.h"

#include <QVector3D>

namespace {
constexpr QVector3D k_legionary_cloth{0.72F, 0.16F, 0.18F};
constexpr QVector3D k_legionary_leather{0.34F, 0.21F, 0.11F};
constexpr QVector3D k_legionary_leather_dark{0.24F, 0.14F, 0.08F};
constexpr QVector3D k_legionary_metal{0.78F, 0.72F, 0.58F};
constexpr QVector3D k_legionary_shield{0.75F, 0.18F, 0.12F};
constexpr QVector3D k_legionary_trim{0.88F, 0.66F, 0.32F};
} 

namespace Render::GL::Roman {

void register_roman_swordsman_style() {
  KnightStyleConfig style;
  style.cloth_color = k_legionary_cloth;
  style.leather_color = k_legionary_leather;
  style.leather_dark_color = k_legionary_leather_dark;
  style.metal_color = k_legionary_metal;
  style.shield_color = k_legionary_shield;
  style.shield_trim_color = k_legionary_trim;
  style.shield_radius_scale = 0.95F;
  style.shield_aspect_ratio = 0.65F;
  style.has_scabbard = true;
  style.shield_cross_decal = false;
  style.shader_id = "swordsman_roman_republic";

  register_swordsman_style("roman_republic", style);
}

} 
