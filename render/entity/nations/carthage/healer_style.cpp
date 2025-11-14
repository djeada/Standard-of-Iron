#include "healer_style.h"
#include "healer_renderer.h"

#include <QVector3D>

namespace {
constexpr QVector3D k_carthage_cloth{0.88F, 0.85F, 0.90F};
constexpr QVector3D k_carthage_leather{0.35F, 0.26F, 0.18F};
constexpr QVector3D k_carthage_leather_dark{0.20F, 0.17F, 0.14F};
constexpr QVector3D k_carthage_metal{0.68F, 0.65F, 0.60F};
constexpr QVector3D k_carthage_wood{0.38F, 0.28F, 0.16F};
constexpr QVector3D k_carthage_cape{0.32F, 0.22F, 0.78F};
} // namespace

namespace Render::GL::Carthage {

void register_carthage_healer_style() {
  HealerStyleConfig style;
  style.cloth_color = k_carthage_cloth;
  style.leather_color = k_carthage_leather;
  style.leather_dark_color = k_carthage_leather_dark;
  style.metal_color = k_carthage_metal;
  style.wood_color = k_carthage_wood;
  style.cape_color = k_carthage_cape;
  style.shader_id = "healer_carthage";

  register_healer_style("default", style);
  register_healer_style("carthage", style);
}

} // namespace Render::GL::Carthage
