#include "healer_style.h"

#include <QVector3D>

#include "render/entity/healer_renderer_common.h"

namespace {

constexpr QVector3D k_roman_tunic{0.93F, 0.91F, 0.85F};

constexpr QVector3D k_roman_leather{0.52F, 0.39F, 0.27F};
constexpr QVector3D k_roman_leather_dark{0.33F, 0.25F, 0.18F};

constexpr QVector3D k_roman_gold{0.82F, 0.66F, 0.32F};

constexpr QVector3D k_roman_wood{0.52F, 0.42F, 0.28F};

constexpr QVector3D k_roman_clavus{0.42F, 0.12F, 0.34F};
} // namespace

namespace Render::GL::Roman {

void register_roman_healer_style() {
  HealerStyleConfig style;
  style.cloth_color = k_roman_tunic;
  style.leather_color = k_roman_leather;
  style.leather_dark_color = k_roman_leather_dark;
  style.metal_color = k_roman_gold;
  style.wood_color = k_roman_wood;
  style.cape_color = k_roman_clavus;

  style.show_helmet = false;
  style.show_armor = false;
  style.show_cape = false;

  Render::GL::register_healer_style("default", style);
  Render::GL::register_healer_style("roman_republic", style);
}

} // namespace Render::GL::Roman
