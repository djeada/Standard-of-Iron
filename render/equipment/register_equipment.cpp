#include "equipment_registry.h"
#include "helmets/headwrap.h"
#include "helmets/montefortino_helmet.h"
#include "weapons/bow_renderer.h"
#include "weapons/quiver_renderer.h"
#include <memory>

namespace Render::GL {

void registerBuiltInEquipment() {
  auto &registry = EquipmentRegistry::instance();

  // Register Carthaginian bow (composite recurve bow - Phoenician style)
  // Carthaginians used composite bows influenced by Eastern Mediterranean designs
  BowRenderConfig carthage_config;
  carthage_config.bow_depth = 0.28F;
  carthage_config.bow_curve_factor = 1.2F;
  carthage_config.bow_height_scale = 0.95F;
  auto carthage_bow = std::make_shared<BowRenderer>(carthage_config);
  registry.registerEquipment(EquipmentCategory::Weapon, "bow_carthage", carthage_bow);

  // Register Roman bow (simple self bow with moderate curve)
  // Romans primarily used simple wooden bows, less curved than composite bows
  BowRenderConfig roman_config;
  roman_config.bow_depth = 0.22F;
  roman_config.bow_curve_factor = 1.0F;
  roman_config.bow_height_scale = 1.0F;
  auto roman_bow = std::make_shared<BowRenderer>(roman_config);
  registry.registerEquipment(EquipmentCategory::Weapon, "bow_roman", roman_bow);

  // Register Kingdom/European bow (longbow style - taller and straighter)
  // European medieval longbows were characteristically tall and relatively straight
  BowRenderConfig kingdom_config;
  kingdom_config.bow_depth = 0.20F;
  kingdom_config.bow_curve_factor = 0.8F;
  kingdom_config.bow_height_scale = 1.15F;
  auto kingdom_bow = std::make_shared<BowRenderer>(kingdom_config);
  registry.registerEquipment(EquipmentCategory::Weapon, "bow_kingdom", kingdom_bow);

  // Register generic bow for backward compatibility
  auto bow = std::make_shared<BowRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "bow", bow);

  // Register quiver renderer
  auto quiver = std::make_shared<QuiverRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "quiver", quiver);

  // Register helmet renderers
  // Montefortino helmet - Carthaginian/Punic style helmet with bronze bowl
  // and distinctive top knob, used by heavy infantry
  auto montefortino_helmet = std::make_shared<MontefortinoHelmetRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "montefortino",
                            montefortino_helmet);

  // Headwrap - cloth head covering used by archers and light infantry
  auto headwrap = std::make_shared<HeadwrapRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "headwrap", headwrap);
}

} // namespace Render::GL
