#include "builder_style.h"
#include "builder_renderer.h"

#include <QVector3D>

namespace {

constexpr QVector3D k_carthage_tunic{0.68F, 0.54F, 0.38F};

constexpr QVector3D k_carthage_skin{0.08F, 0.07F, 0.065F};

constexpr QVector3D k_carthage_leather{0.48F, 0.35F, 0.22F};
constexpr QVector3D k_carthage_leather_dark{0.32F, 0.24F, 0.16F};

constexpr QVector3D k_carthage_bronze{0.70F, 0.52F, 0.32F};

constexpr QVector3D k_carthage_wood{0.45F, 0.35F, 0.22F};

constexpr QVector3D k_carthage_apron{0.42F, 0.35F, 0.25F};
} // namespace

namespace Render::GL::Carthage {

void register_carthage_builder_style() {
  BuilderStyleConfig style;
  style.cloth_color = k_carthage_tunic;
  style.skin_color = k_carthage_skin;
  style.leather_color = k_carthage_leather;
  style.leather_dark_color = k_carthage_leather_dark;
  style.metal_color = k_carthage_bronze;
  style.wood_color = k_carthage_wood;
  style.apron_color = k_carthage_apron;
  style.shader_id = "builder_carthage";

  style.show_helmet = false;
  style.show_armor = false;
  style.show_tool_belt = true;

  style.force_beard = true;

  register_builder_style("default", style);
  register_builder_style("carthage", style);
}

} // namespace Render::GL::Carthage
