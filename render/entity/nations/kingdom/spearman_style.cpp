#include "spearman_style.h"
#include "spearman_renderer.h"
#include <QVector3D>

namespace {
constexpr QVector3D k_kingdom_cloth{0.40F, 0.44F, 0.52F};
constexpr QVector3D k_kingdom_leather{0.29F, 0.20F, 0.12F};
constexpr QVector3D k_kingdom_leather_dark{0.18F, 0.16F, 0.14F};
constexpr QVector3D k_kingdom_metal{0.68F, 0.69F, 0.72F};
constexpr QVector3D k_kingdom_shaft{0.36F, 0.28F, 0.16F};
constexpr QVector3D k_kingdom_head{0.80F, 0.82F, 0.88F};
} // namespace

namespace Render::GL::Kingdom {

void register_kingdom_spearman_style() {
  SpearmanStyleConfig style;
  style.cloth_color = k_kingdom_cloth;
  style.leather_color = k_kingdom_leather;
  style.leather_dark_color = k_kingdom_leather_dark;
  style.metal_color = k_kingdom_metal;
  style.spear_shaft_color = k_kingdom_shaft;
  style.spearhead_color = k_kingdom_head;
  style.shader_id = "spearman_kingdom_of_iron";

  register_spearman_style("default", style);
  register_spearman_style("kingdom_of_iron", style);
}

} // namespace Render::GL::Kingdom
