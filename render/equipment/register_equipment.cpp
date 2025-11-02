#include "equipment_registry.h"
#include "helmets/headwrap.h"
#include "helmets/kingdom_heavy_helmet.h"
#include "helmets/kingdom_light_helmet.h"
#include "helmets/montefortino_helmet.h"
#include "helmets/roman_heavy_helmet.h"
#include "helmets/roman_light_helmet.h"
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
  // CARTHAGE HELMETS
  // Montefortino helmet - Carthaginian/Punic style helmet with bronze bowl
  // and distinctive top knob, used by heavy infantry (spearmen, swordsmen)
  auto montefortino_helmet = std::make_shared<MontefortinoHelmetRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "montefortino",
                            montefortino_helmet);
  registry.registerEquipment(EquipmentCategory::Helmet, "carthage_heavy",
                            montefortino_helmet);

  // Headwrap - cloth head covering used by Carthaginian archers and light infantry
  auto headwrap = std::make_shared<HeadwrapRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "headwrap", headwrap);
  registry.registerEquipment(EquipmentCategory::Helmet, "carthage_light",
                            headwrap);

  // ROMAN HELMETS
  // Roman heavy helmet - Imperial Gallic style for legionaries with
  // visor cross, breathing holes, and brass decorations
  auto roman_heavy = std::make_shared<RomanHeavyHelmetRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "roman_heavy",
                            roman_heavy);

  // Roman light helmet - Simple cap helmet for archers and auxiliaries
  auto roman_light = std::make_shared<RomanLightHelmetRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "roman_light",
                            roman_light);

  // KINGDOM HELMETS
  // Kingdom heavy helmet - Great helm style for knights and heavy infantry
  // with enclosed design and heraldic cross
  auto kingdom_heavy = std::make_shared<KingdomHeavyHelmetRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "kingdom_heavy",
                            kingdom_heavy);

  // Kingdom light helmet - Kettle hat with wide brim for archers
  auto kingdom_light = std::make_shared<KingdomLightHelmetRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "kingdom_light",
                            kingdom_light);
}

} // namespace Render::GL
