#include "swordsman_style.h"

#include <QVector3D>

#include "swordsman_renderer.h"

namespace {
constexpr QVector3D k_legionary_cloth{0.72F, 0.16F, 0.18F};
constexpr QVector3D k_legionary_leather{0.34F, 0.21F, 0.11F};
constexpr QVector3D k_legionary_leather_dark{0.24F, 0.14F, 0.08F};
constexpr QVector3D k_legionary_metal{0.78F, 0.72F, 0.58F};
} // namespace

namespace Render::GL::Roman {

void register_roman_swordsman_style() {
  KnightStyleConfig style;
  style.cloth_color = k_legionary_cloth;
  style.leather_color = k_legionary_leather;
  style.leather_dark_color = k_legionary_leather_dark;
  style.metal_color = k_legionary_metal;

  register_swordsman_style("roman_republic", style);
}

} // namespace Render::GL::Roman
