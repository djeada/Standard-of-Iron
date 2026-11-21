#include "render/entity/nations/roman/healer_style.h"

#include <QVector3D>

#include "render/entity/healer_renderer_common.h"
#include "render/entity/nations/material_palette.h"

namespace {

constexpr QVector3D k_roman_tunic{0.93F, 0.91F, 0.85F};

constexpr QVector3D k_roman_gold{0.82F, 0.66F, 0.32F};

constexpr QVector3D k_roman_clavus{0.56F, 0.055F, 0.065F};
} // namespace

namespace Render::GL::Roman {

void register_roman_healer_style() {
  HealerStyleConfig style;
  style.cloth_color = k_roman_tunic;
  style.leather_color = Nation::k_roman_materials.leather;
  style.leather_dark_color = Nation::k_roman_materials.dark_leather;
  style.metal_color = k_roman_gold;
  style.wood_color = Nation::k_roman_materials.timber;
  style.cape_color = k_roman_clavus;

  style.show_helmet = false;
  style.show_armor = false;
  style.show_cape = false;

  Render::GL::register_healer_style("default", style);
  Render::GL::register_healer_style("roman_republic", style);
}

} // namespace Render::GL::Roman
