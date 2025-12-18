#include "archer_style.h"
#include "archer_renderer.h"

#include <QVector3D>

namespace {
constexpr QVector3D k_roman_cloth{0.72F, 0.16F, 0.18F};
constexpr QVector3D k_roman_leather{0.34F, 0.21F, 0.11F};
constexpr QVector3D k_roman_leather_dark{0.24F, 0.14F, 0.08F};
constexpr QVector3D k_roman_metal{0.78F, 0.72F, 0.58F};
constexpr QVector3D k_roman_wood{0.46F, 0.32F, 0.18F};
constexpr QVector3D k_roman_cape{0.70F, 0.15F, 0.18F};
constexpr QVector3D k_roman_fletch{0.93F, 0.83F, 0.33F};
constexpr QVector3D k_roman_string{0.28F, 0.28F, 0.32F};
} // namespace

namespace Render::GL::Roman {

void register_roman_archer_style() {
  ArcherStyleConfig style;
  style.cloth_color = k_roman_cloth;
  style.leather_color = k_roman_leather;
  style.leather_dark_color = k_roman_leather_dark;
  style.metal_color = k_roman_metal;
  style.wood_color = k_roman_wood;
  style.cape_color = k_roman_cape;
  style.fletching_color = k_roman_fletch;
  style.bow_string_color = k_roman_string;
  style.show_helmet = true;

  style.shader_id = "archer_roman_republic";
  register_archer_style("roman_republic", style);
}

} // namespace Render::GL::Roman
