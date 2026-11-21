#include "render/entity/nations/roman/archer_style.h"

#include "render/equipment/material_palette.h"

namespace Render::GL::Roman {

void register_roman_archer_style() {
  ArcherStyleConfig style;
  Nation::apply_materials(style, Nation::k_roman_materials);
  style.wood_color = Nation::k_roman_materials.timber;
  style.cape_color = Nation::k_roman_materials.cloth;
  style.fletching_color = QVector3D(0.93F, 0.83F, 0.33F);
  style.bow_string_color = QVector3D(0.28F, 0.28F, 0.32F);
  style.show_helmet = true;

  ::Render::GL::register_archer_style("roman_republic", style);
}

} // namespace Render::GL::Roman
