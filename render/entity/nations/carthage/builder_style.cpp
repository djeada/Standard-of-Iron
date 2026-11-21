#include "render/entity/nations/carthage/builder_style.h"

#include <QVector3D>

#include "render/entity/builder_renderer_common.h"
#include "render/entity/nations/material_palette.h"

namespace {

constexpr QVector3D k_carthage_tunic{0.68F, 0.54F, 0.38F};

constexpr QVector3D k_carthage_skin{0.08F, 0.07F, 0.065F};

constexpr QVector3D k_carthage_bronze{0.70F, 0.52F, 0.32F};

constexpr QVector3D k_carthage_apron{0.42F, 0.35F, 0.25F};
} // namespace

namespace Render::GL::Carthage {

void register_carthage_builder_style() {
  BuilderStyleConfig style;
  style.cloth_color = k_carthage_tunic;
  style.skin_color = k_carthage_skin;
  style.leather_color = Nation::k_carthage_materials.leather;
  style.leather_dark_color = Nation::k_carthage_materials.dark_leather;
  style.metal_color = k_carthage_bronze;
  style.wood_color = Nation::k_carthage_materials.timber;
  style.apron_color = k_carthage_apron;

  style.show_helmet = false;
  style.show_armor = false;
  style.show_tool_belt = true;

  style.force_beard = true;

  register_builder_style(std::string("default"), style);
  register_builder_style(std::string("carthage"), style);
}

} // namespace Render::GL::Carthage
