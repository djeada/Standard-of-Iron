#include "archer_style.h"
#include "archer_renderer.h"

#include <QVector3D>
#include <string>
#include <string_view>

namespace {
constexpr QVector3D k_carthage_cloth{0.12F, 0.36F, 0.52F};
constexpr QVector3D k_carthage_leather{0.36F, 0.24F, 0.12F};
constexpr QVector3D k_carthage_leather_dark{0.22F, 0.16F, 0.10F};
constexpr QVector3D k_carthage_metal{0.75F, 0.66F, 0.42F};
constexpr QVector3D k_carthage_wood{0.38F, 0.28F, 0.18F};
constexpr QVector3D k_carthage_fletch{0.90F, 0.82F, 0.28F};
constexpr QVector3D k_carthage_string{0.32F, 0.30F, 0.26F};
constexpr QVector3D k_carthage_cape{0.14F, 0.38F, 0.54F};
} // namespace

namespace Render::GL::Carthage {

void register_carthage_archer_style() {
  ArcherStyleConfig style;
  style.cloth_color = k_carthage_cloth;
  style.leather_color = k_carthage_leather;
  style.leather_dark_color = k_carthage_leather_dark;
  style.metal_color = k_carthage_metal;
  style.wood_color = k_carthage_wood;
  style.fletching_color = k_carthage_fletch;
  style.bow_string_color = k_carthage_string;
  style.cape_color = k_carthage_cape;
  style.show_helmet = true;
  style.show_armor = true;
  style.show_shoulder_decor = false;
  style.show_cape = true;
  style.force_beard = true;
  style.attachment_profile.clear();
  style.shader_id = "archer_carthage";
  style.armor_id = "armor_light_carthage";

  register_archer_style("carthage", style);
}

} // namespace Render::GL::Carthage
