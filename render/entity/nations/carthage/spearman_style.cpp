#include "spearman_style.h"

#include <array>

namespace Render::GL::Carthage {
namespace {

const std::array<SpearmanStyleRegistration, 1> k_styles{{
    {"carthage",
     SpearmanStyleConfig{
         .cloth_color = QVector3D(0.74F, 0.58F, 0.32F),
         .leather_color = QVector3D(0.42F, 0.25F, 0.12F),
         .leather_dark_color = QVector3D(0.20F, 0.12F, 0.07F),
         .metal_color = QVector3D(0.78F, 0.64F, 0.32F),
         .spear_shaft_color = QVector3D(0.42F, 0.24F, 0.13F),
         .spearhead_color = QVector3D(0.72F, 0.68F, 0.58F),
         .spear_length_scale = 1.08F,
         .spear_shaft_radius_scale = 1.04F,
         .force_beard = false,
         .armor_id = "carthage_linen_cuirass",
     }},
}};

} // namespace

void register_carthage_spearman_styles() {
  register_spearman_styles(k_styles);
}

} // namespace Render::GL::Carthage
