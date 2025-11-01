#include "archer_style.h"
#include "archer_renderer.h"

#include <QVector3D>

namespace {
constexpr QVector3D k_kingdom_cloth{0.58F, 0.56F, 0.62F};
constexpr QVector3D k_kingdom_leather{0.32F, 0.23F, 0.16F};
constexpr QVector3D k_kingdom_leather_dark{0.18F, 0.16F, 0.14F};
constexpr QVector3D k_kingdom_metal{0.70F, 0.70F, 0.72F};
constexpr QVector3D k_kingdom_wood{0.40F, 0.30F, 0.18F};
constexpr QVector3D k_kingdom_cape{0.20F, 0.24F, 0.32F};
constexpr QVector3D k_kingdom_fletch{0.84F, 0.82F, 0.78F};
constexpr QVector3D k_kingdom_string{0.22F, 0.22F, 0.24F};
} // namespace

namespace Render::GL::Kingdom {

void register_kingdom_archer_style() {
  ArcherStyleConfig style;
  style.cloth_color = k_kingdom_cloth;
  style.leather_color = k_kingdom_leather;
  style.leather_dark_color = k_kingdom_leather_dark;
  style.metal_color = k_kingdom_metal;
  style.wood_color = k_kingdom_wood;
  style.cape_color = k_kingdom_cape;
  style.fletching_color = k_kingdom_fletch;
  style.bow_string_color = k_kingdom_string;
  style.shader_id = "archer_kingdom_of_iron";

  register_archer_style("default", style);
  register_archer_style("kingdom_of_iron", style);
}

} // namespace Render::GL::Kingdom
