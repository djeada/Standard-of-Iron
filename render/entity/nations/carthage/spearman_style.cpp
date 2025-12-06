#include "spearman_style.h"
#include "spearman_renderer.h"

#include <QVector3D>

namespace {
constexpr QVector3D k_carthage_cloth{0.12F, 0.36F, 0.52F};
constexpr QVector3D k_carthage_leather{0.36F, 0.24F, 0.12F};
constexpr QVector3D k_carthage_leather_dark{0.22F, 0.16F, 0.10F};
constexpr QVector3D k_carthage_metal{0.75F, 0.66F, 0.42F};
constexpr QVector3D k_carthage_spear_shaft{0.38F, 0.28F, 0.18F};
constexpr QVector3D k_carthage_spearhead{0.90F, 0.88F, 0.70F};
} // namespace

namespace Render::GL::Carthage {

void register_carthage_spearman_style() {
  SpearmanStyleConfig style;
  style.cloth_color = k_carthage_cloth;
  style.leather_color = k_carthage_leather;
  style.leather_dark_color = k_carthage_leather_dark;
  style.metal_color = k_carthage_metal;
  style.spear_shaft_color = k_carthage_spear_shaft;
  style.spearhead_color = k_carthage_spearhead;
  style.spear_length_scale = 1.08F;
  style.shader_id = "spearman_carthage";
  style.armor_id = "armor_light_carthage";
  style.force_beard = true;

  register_spearman_style("carthage", style);
}

} // namespace Render::GL::Carthage
