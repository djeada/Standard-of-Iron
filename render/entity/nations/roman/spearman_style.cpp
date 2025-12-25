#include "spearman_style.h"
#include "spearman_renderer.h"

#include <QVector3D>

namespace {
constexpr QVector3D k_legionary_spear_shaft{0.46F, 0.28F, 0.14F};
constexpr QVector3D k_legionary_spearhead{0.80F, 0.74F, 0.58F};
constexpr QVector3D k_legionary_cloth{0.72F, 0.16F, 0.18F};
constexpr QVector3D k_legionary_leather{0.34F, 0.21F, 0.11F};
constexpr QVector3D k_legionary_leather_dark{0.24F, 0.14F, 0.08F};
constexpr QVector3D k_legionary_metal{0.78F, 0.72F, 0.58F};
} 

namespace Render::GL::Roman {

void register_roman_spearman_style() {
  SpearmanStyleConfig style;
  style.cloth_color = k_legionary_cloth;
  style.leather_color = k_legionary_leather;
  style.leather_dark_color = k_legionary_leather_dark;
  style.metal_color = k_legionary_metal;
  style.spear_shaft_color = k_legionary_spear_shaft;
  style.spearhead_color = k_legionary_spearhead;
  style.spear_length_scale = 1.05F;
  style.shader_id = "spearman_roman_republic";

  register_spearman_style("roman_republic", style);
}

} 
