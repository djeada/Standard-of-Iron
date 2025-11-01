#include "knight_style.h"
#include "knight_renderer.h"

#include <QVector3D>

namespace {
constexpr QVector3D k_kingdom_cloth{0.24F, 0.28F, 0.34F};
constexpr QVector3D k_kingdom_leather{0.30F, 0.20F, 0.12F};
constexpr QVector3D k_kingdom_leather_dark{0.18F, 0.14F, 0.10F};
constexpr QVector3D k_kingdom_metal{0.72F, 0.74F, 0.78F};
constexpr QVector3D k_kingdom_shield{0.20F, 0.22F, 0.30F};
constexpr QVector3D k_kingdom_shield_trim{0.78F, 0.76F, 0.62F};
} // namespace

namespace Render::GL::Kingdom {

void register_kingdom_knight_style() {
  KnightStyleConfig style;
  style.cloth_color = k_kingdom_cloth;
  style.leather_color = k_kingdom_leather;
  style.leather_dark_color = k_kingdom_leather_dark;
  style.metal_color = k_kingdom_metal;
  style.shield_color = k_kingdom_shield;
  style.shield_trim_color = k_kingdom_shield_trim;
  style.shield_radius_scale = 1.0F;
  style.shield_aspect_ratio = 1.0F;
  style.has_scabbard = true;
  style.shader_id = "knight_kingdom_of_iron";

  register_knight_style("kingdom_of_iron", style);
}

} // namespace Render::GL::Kingdom
