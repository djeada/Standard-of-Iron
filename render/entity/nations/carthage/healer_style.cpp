#include "healer_style.h"

#include <QVector3D>

#include "../../healer_renderer_common.h"

namespace {

constexpr QVector3D k_carthage_tunic{0.45F, 0.45F, 0.47F};

constexpr QVector3D k_carthage_skin{0.08F, 0.07F, 0.065F};
constexpr QVector3D k_sepulcher_skin{0.82F, 0.84F, 0.79F};
constexpr QVector3D k_sepulcher_tunic{0.16F, 0.16F, 0.18F};
constexpr QVector3D k_sepulcher_leather{0.24F, 0.20F, 0.18F};
constexpr QVector3D k_sepulcher_leather_dark{0.12F, 0.12F, 0.13F};
constexpr QVector3D k_sepulcher_metal{0.60F, 0.64F, 0.68F};
constexpr QVector3D k_sepulcher_wood{0.33F, 0.31F, 0.28F};
constexpr QVector3D k_sepulcher_cape{0.12F, 0.12F, 0.14F};

constexpr QVector3D k_carthage_leather{0.48F, 0.35F, 0.22F};
constexpr QVector3D k_carthage_leather_dark{0.32F, 0.24F, 0.16F};

constexpr QVector3D k_carthage_bronze{0.70F, 0.52F, 0.32F};

constexpr QVector3D k_carthage_wood{0.45F, 0.35F, 0.22F};

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
