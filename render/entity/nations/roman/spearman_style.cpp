#include "render/entity/nations/roman/spearman_style.h"

#include <array>

#include "render/entity/nations/material_palette.h"

namespace Render::GL::Roman {
namespace {

const std::array<SpearmanStyleRegistration, 1> k_styles{{
    {"roman_republic",
     SpearmanStyleConfig{
         .cloth_color = Nation::k_roman_materials.cloth,
         .leather_color = Nation::k_roman_materials.leather,
         .leather_dark_color = Nation::k_roman_materials.dark_leather,
         .metal_color = Nation::k_roman_materials.metal,
         .spear_shaft_color = Nation::k_roman_materials.timber,
         .spearhead_color = QVector3D(0.78F, 0.78F, 0.74F),
         .spear_length_scale = 1.04F,
         .spear_shaft_radius_scale = 1.0F,
         .armor_id = "roman_hamata",
     }},
}};

}

void register_roman_spearman_styles() {
  register_spearman_styles(k_styles);
}

} // namespace Render::GL::Roman
