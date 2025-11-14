#include "healer_style.h"
#include "healer_renderer.h"

#include <QVector3D>

namespace {
constexpr QVector3D k_roman_cloth{0.92F, 0.88F, 0.85F};
constexpr QVector3D k_roman_leather{0.38F, 0.28F, 0.20F};
constexpr QVector3D k_roman_leather_dark{0.22F, 0.18F, 0.15F};
constexpr QVector3D k_roman_metal{0.75F, 0.72F, 0.68F};
constexpr QVector3D k_roman_wood{0.42F, 0.32F, 0.20F};
constexpr QVector3D k_roman_cape{0.88F, 0.28F, 0.32F};
} // namespace

namespace Render::GL::Roman {

void register_roman_healer_style() {
  HealerStyleConfig style;
  style.cloth_color = k_roman_cloth;
  style.leather_color = k_roman_leather;
  style.leather_dark_color = k_roman_leather_dark;
  style.metal_color = k_roman_metal;
  style.wood_color = k_roman_wood;
  style.cape_color = k_roman_cape;
  style.shader_id = "healer_roman_republic";

  register_healer_style("default", style);
  register_healer_style("roman_republic", style);
}

} // namespace Render::GL::Roman
