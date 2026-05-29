#include "archer_style.h"

namespace Render::GL::Roman {

void register_roman_archer_style() {
  ArcherStyleConfig style;
  style.cloth_color = QVector3D(0.72F, 0.16F, 0.18F);
  style.leather_color = QVector3D(0.34F, 0.21F, 0.11F);
  style.leather_dark_color = QVector3D(0.24F, 0.14F, 0.08F);
  style.metal_color = QVector3D(0.78F, 0.72F, 0.58F);
  style.wood_color = QVector3D(0.46F, 0.32F, 0.18F);
  style.cape_color = QVector3D(0.70F, 0.15F, 0.18F);
  style.fletching_color = QVector3D(0.93F, 0.83F, 0.33F);
  style.bow_string_color = QVector3D(0.28F, 0.28F, 0.32F);
  style.show_helmet = true;

  ::Render::GL::register_archer_style("roman_republic", style);
}

} // namespace Render::GL::Roman
