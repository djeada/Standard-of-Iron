#include "render/entity/nations/carthage/spearman_style.h"

#include <QVector3D>

#include <array>

#include "render/equipment/material_palette.h"

namespace Render::GL::Carthage {
namespace {

const std::array<SpearmanStyleRegistration, 1> k_styles{{
    {"carthage",
     SpearmanStyleConfig{
         .cloth_color = QVector3D(0.37F, 0.17F, 0.47F),
         .leather_color = Nation::k_carthage_materials.leather,
         .leather_dark_color = Nation::k_carthage_materials.dark_leather,
         .metal_color = Nation::k_carthage_materials.metal,
         .spear_shaft_color = Nation::k_carthage_materials.timber,
         .spearhead_color = QVector3D(0.72F, 0.68F, 0.58F),
         .spear_length_scale = 1.08F,
         .spear_shaft_radius_scale = 1.04F,
         .force_beard = false,
         .armor_id = "carthage_linen_cuirass",
     }},
}};

}

void register_carthage_spearman_styles() {
  register_spearman_styles(k_styles);
}

} // namespace Render::GL::Carthage
