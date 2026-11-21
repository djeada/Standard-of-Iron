#include "render/entity/nations/roman/builder_style.h"

#include <QVector3D>

#include "render/entity/builder_renderer_common.h"
#include "render/equipment/material_palette.h"

namespace {

constexpr QVector3D k_roman_tunic{0.72F, 0.58F, 0.42F};

constexpr QVector3D k_roman_bronze{0.72F, 0.55F, 0.35F};

constexpr QVector3D k_roman_apron{0.45F, 0.38F, 0.28F};
} // namespace

namespace Render::GL::Roman {

void register_roman_builder_style() {
  BuilderStyleConfig style;
  style.cloth_color = k_roman_tunic;
  style.leather_color = Nation::k_roman_materials.leather;
  style.leather_dark_color = Nation::k_roman_materials.dark_leather;
  style.metal_color = k_roman_bronze;
  style.wood_color = Nation::k_roman_materials.timber;
  style.apron_color = k_roman_apron;

  style.show_helmet = true;
  style.show_armor = false;
  style.show_tool_belt = true;

  register_builder_style(std::string("default"), style);
  register_builder_style(std::string("roman_republic"), style);
}

} // namespace Render::GL::Roman
