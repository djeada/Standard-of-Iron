#include "healer_style.h"
#include "healer_renderer.h"

#include <QVector3D>

namespace {
constexpr QVector3D k_kingdom_cloth{0.85F, 0.92F, 0.88F};
constexpr QVector3D k_kingdom_leather{0.32F, 0.23F, 0.16F};
constexpr QVector3D k_kingdom_leather_dark{0.18F, 0.16F, 0.14F};
constexpr QVector3D k_kingdom_metal{0.70F, 0.70F, 0.72F};
constexpr QVector3D k_kingdom_wood{0.40F, 0.30F, 0.18F};
constexpr QVector3D k_kingdom_cape{0.25F, 0.85F, 0.40F};
} // namespace

namespace Render::GL::Kingdom {

void register_kingdom_healer_style() {
  HealerStyleConfig style;
  style.cloth_color = k_kingdom_cloth;
  style.leather_color = k_kingdom_leather;
  style.leather_dark_color = k_kingdom_leather_dark;
  style.metal_color = k_kingdom_metal;
  style.wood_color = k_kingdom_wood;
  style.cape_color = k_kingdom_cape;
  style.shader_id = "healer_kingdom_of_iron";

  register_healer_style("default", style);
  register_healer_style("kingdom_of_iron", style);
}

} // namespace Render::GL::Kingdom
