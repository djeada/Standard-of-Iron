#include "healer_style.h"

#include <QVector3D>

#include "../../healer_renderer_common.h"
#include "../sepulcher/palette.h"

namespace {

constexpr QVector3D k_carthage_tunic{0.11F, 0.10F, 0.15F};

constexpr QVector3D k_carthage_skin{0.30F, 0.24F, 0.20F};
constexpr QVector3D k_sepulcher_skin = Render::GL::Sepulcher::k_bone;

constexpr QVector3D k_sepulcher_tunic{0.085F, 0.080F, 0.105F};
constexpr QVector3D k_sepulcher_leather = Render::GL::Sepulcher::k_grave_leather;
constexpr QVector3D k_sepulcher_leather_dark =
    Render::GL::Sepulcher::k_grave_leather_dark;
constexpr QVector3D k_sepulcher_metal = Render::GL::Sepulcher::k_grave_iron;
constexpr QVector3D k_sepulcher_wood = Render::GL::Sepulcher::k_grave_wood;
constexpr QVector3D k_sepulcher_cape{0.060F, 0.055F, 0.075F};

constexpr QVector3D k_carthage_leather{0.26F, 0.19F, 0.14F};
constexpr QVector3D k_carthage_leather_dark{0.15F, 0.11F, 0.09F};

constexpr QVector3D k_carthage_bronze{0.52F, 0.40F, 0.20F};

constexpr QVector3D k_carthage_wood{0.28F, 0.22F, 0.16F};

constexpr QVector3D k_carthage_purple{0.04F, 0.04F, 0.045F};
} // namespace

namespace Render::GL::Carthage {

void register_carthage_healer_style() {
  HealerStyleConfig style;
  style.cloth_color = k_carthage_tunic;
  style.skin_color = k_carthage_skin;
  style.leather_color = k_carthage_leather;
  style.leather_dark_color = k_carthage_leather_dark;
  style.metal_color = k_carthage_bronze;
  style.wood_color = k_carthage_wood;
  style.cape_color = k_carthage_purple;

  style.show_helmet = false;
  style.show_armor = false;
  style.show_cape = true;

  style.force_beard = true;

  Render::GL::register_healer_style("default", style);
  Render::GL::register_healer_style("carthage", style);

  HealerStyleConfig sepulcher = style;
  sepulcher.cloth_color = k_sepulcher_tunic;
  sepulcher.skin_color = k_sepulcher_skin;
  sepulcher.leather_color = k_sepulcher_leather;
  sepulcher.leather_dark_color = k_sepulcher_leather_dark;
  sepulcher.metal_color = k_sepulcher_metal;
  sepulcher.wood_color = k_sepulcher_wood;
  sepulcher.cape_color = k_sepulcher_cape;
  sepulcher.force_beard = false;
  Render::GL::register_healer_style("iron_sepulcher", sepulcher);
}

} // namespace Render::GL::Carthage
