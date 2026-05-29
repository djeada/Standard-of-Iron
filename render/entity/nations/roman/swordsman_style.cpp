#include "swordsman_style.h"

namespace Render::GL::Roman {

void register_roman_swordsman_style() {
  KnightStyleConfig style;
  style.cloth_color = QVector3D(0.72F, 0.16F, 0.18F);
  style.leather_color = QVector3D(0.34F, 0.21F, 0.11F);
  style.leather_dark_color = QVector3D(0.24F, 0.14F, 0.08F);
  style.metal_color = QVector3D(0.78F, 0.72F, 0.58F);

  ::Render::GL::register_swordsman_style("roman_republic", style);
}

} // namespace Render::GL::Roman
