#include "healer_style.h"
#include "healer_renderer.h"

#include <QVector3D>

namespace {
// Roman Medicus - Clean, practical appearance
// Off-white linen tunica (bleached wool or linen)
constexpr QVector3D k_roman_tunic{0.95F, 0.92F, 0.88F};
// Simple leather for sandals/belt (light tan)
constexpr QVector3D k_roman_leather{0.55F, 0.42F, 0.30F};
constexpr QVector3D k_roman_leather_dark{0.35F, 0.28F, 0.20F};
// Minimal metal (bronze medical tools only)
constexpr QVector3D k_roman_bronze{0.72F, 0.55F, 0.35F};
// Olive wood for medical implements
constexpr QVector3D k_roman_wood{0.52F, 0.42F, 0.28F};
// Deep red trim/sash (madder dye - indicates medical role)
constexpr QVector3D k_roman_sash{0.72F, 0.18F, 0.15F};
} // namespace

namespace Render::GL::Roman {

void register_roman_healer_style() {
  HealerStyleConfig style;
  style.cloth_color = k_roman_tunic;
  style.leather_color = k_roman_leather;
  style.leather_dark_color = k_roman_leather_dark;
  style.metal_color = k_roman_bronze;
  style.wood_color = k_roman_wood;
  style.cape_color = k_roman_sash;
  style.shader_id = "healer_roman_republic";

  // Healers wear simple tunics - no helmet or heavy armor
  style.show_helmet = false;
  style.show_armor = false;
  style.show_cape = true; // Red sash/trim indicating medical role

  register_healer_style("default", style);
  register_healer_style("roman_republic", style);
}

} // namespace Render::GL::Roman
