#include "render/entity/nations/roman/swordsman_style.h"

#include "render/equipment/material_palette.h"

namespace Render::GL::Roman {

void register_roman_swordsman_style() {
  SwordsmanStyleConfig style;
  Nation::apply_materials(style, Nation::k_roman_materials);

  ::Render::GL::register_swordsman_style("roman_republic", style);
}

} // namespace Render::GL::Roman
