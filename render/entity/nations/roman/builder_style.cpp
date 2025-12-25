#include "builder_style.h"
#include "builder_renderer.h"

#include <QVector3D>

namespace {

constexpr QVector3D k_roman_tunic{0.72F, 0.58F, 0.42F};

constexpr QVector3D k_roman_leather{0.55F, 0.42F, 0.30F};
constexpr QVector3D k_roman_leather_dark{0.35F, 0.28F, 0.20F};

constexpr QVector3D k_roman_bronze{0.72F, 0.55F, 0.35F};

constexpr QVector3D k_roman_wood{0.52F, 0.42F, 0.28F};

constexpr QVector3D k_roman_apron{0.45F, 0.38F, 0.28F};
} 

namespace Render::GL::Roman {

void register_roman_builder_style() {
  BuilderStyleConfig style;
  style.cloth_color = k_roman_tunic;
  style.leather_color = k_roman_leather;
  style.leather_dark_color = k_roman_leather_dark;
  style.metal_color = k_roman_bronze;
  style.wood_color = k_roman_wood;
  style.apron_color = k_roman_apron;
  style.shader_id = "builder_roman_republic";

  style.show_helmet = true;
  style.show_armor = false;
  style.show_tool_belt = true;

  register_builder_style("default", style);
  register_builder_style("roman_republic", style);
}

} 
