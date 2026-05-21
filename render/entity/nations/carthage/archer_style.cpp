#include "archer_style.h"

#include <QVector3D>

#include <string>
#include <string_view>

#include "archer_renderer.h"

namespace {
constexpr QVector3D k_carthage_cloth{0.12F, 0.36F, 0.52F};
constexpr QVector3D k_sepulcher_bone{0.81F, 0.83F, 0.78F};
constexpr QVector3D k_sepulcher_cloth{0.22F, 0.22F, 0.25F};
constexpr QVector3D k_sepulcher_leather{0.25F, 0.21F, 0.18F};
constexpr QVector3D k_sepulcher_leather_dark{0.14F, 0.13F, 0.13F};
constexpr QVector3D k_sepulcher_metal{0.56F, 0.60F, 0.66F};
constexpr QVector3D k_sepulcher_wood{0.34F, 0.30F, 0.26F};
constexpr QVector3D k_sepulcher_fletch{0.62F, 0.66F, 0.70F};
constexpr QVector3D k_sepulcher_string{0.35F, 0.35F, 0.36F};
constexpr QVector3D k_sepulcher_cape{0.10F, 0.10F, 0.12F};
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
  style.armor_id = "armor_light_carthage";

  register_archer_style("carthage", style);

  ArcherStyleConfig sepulcher = style;
  sepulcher.skin_color = k_sepulcher_bone;
  sepulcher.cloth_color = k_sepulcher_cloth;
  sepulcher.leather_color = k_sepulcher_leather;
  sepulcher.leather_dark_color = k_sepulcher_leather_dark;
  sepulcher.metal_color = k_sepulcher_metal;
  sepulcher.wood_color = k_sepulcher_wood;
  sepulcher.fletching_color = k_sepulcher_fletch;
  sepulcher.bow_string_color = k_sepulcher_string;
  sepulcher.cape_color = k_sepulcher_cape;
  sepulcher.show_cape = false;
  sepulcher.show_shoulder_decor = false;
  sepulcher.force_beard = false;
  register_archer_style("iron_sepulcher", sepulcher);
}

} // namespace Render::GL::Carthage
