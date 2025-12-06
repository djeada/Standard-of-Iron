#include "healer_style.h"
#include "healer_renderer.h"

#include <QVector3D>

namespace {
// Carthaginian/Phoenician Healer - Eastern Mediterranean style
// Natural undyed linen with slight cream tone
constexpr QVector3D k_carthage_tunic{0.92F, 0.88F, 0.82F};
// Rich tan leather (North African style)
constexpr QVector3D k_carthage_leather{0.48F, 0.35F, 0.22F};
constexpr QVector3D k_carthage_leather_dark{0.32F, 0.24F, 0.16F};
// Bronze/copper for medical implements
constexpr QVector3D k_carthage_bronze{0.70F, 0.52F, 0.32F};
// Cedar/olive wood
constexpr QVector3D k_carthage_wood{0.45F, 0.35F, 0.22F};
// Tyrian purple trim - prestigious Phoenician dye
constexpr QVector3D k_carthage_purple{0.45F, 0.18F, 0.55F};
} // namespace

namespace Render::GL::Carthage {

void register_carthage_healer_style() {
  HealerStyleConfig style;
  style.cloth_color = k_carthage_tunic;
  style.leather_color = k_carthage_leather;
  style.leather_dark_color = k_carthage_leather_dark;
  style.metal_color = k_carthage_bronze;
  style.wood_color = k_carthage_wood;
  style.cape_color = k_carthage_purple;
  style.shader_id = "healer_carthage";

  // Carthage healers - simple robes, no helmet or heavy armor
  style.show_helmet = false;
  style.show_armor = false;
  style.show_cape = true; // Purple trim/sash

  // Carthaginian healers often depicted with beards (Eastern tradition)
  style.force_beard = true;

  register_healer_style("default", style);
  register_healer_style("carthage", style);
}

} // namespace Render::GL::Carthage
