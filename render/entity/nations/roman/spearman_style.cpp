#include "spearman_style.h"

#include <array>

namespace Render::GL::Roman {
namespace {

const std::array<SpearmanStyleRegistration, 1> k_styles{{
    {"roman_republic",
     SpearmanStyleConfig{
         .cloth_color = QVector3D(0.55F, 0.08F, 0.06F),
         .leather_color = QVector3D(0.36F, 0.20F, 0.10F),
         .leather_dark_color = QVector3D(0.18F, 0.10F, 0.06F),
         .metal_color = QVector3D(0.72F, 0.70F, 0.66F),
         .spear_shaft_color = QVector3D(0.44F, 0.26F, 0.14F),
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
