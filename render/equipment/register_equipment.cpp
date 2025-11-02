#include "equipment_registry.h"
#include "armor/tunic_renderer.h"
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

  BowRenderConfig carthage_config;
  carthage_config.bow_depth = 0.28F;
  carthage_config.bow_curve_factor = 1.2F;
  carthage_config.bow_height_scale = 0.95F;
  auto carthage_bow = std::make_shared<BowRenderer>(carthage_config);
  registry.registerEquipment(EquipmentCategory::Weapon, "bow_carthage",
                             carthage_bow);

  BowRenderConfig roman_config;
  roman_config.bow_depth = 0.22F;
  roman_config.bow_curve_factor = 1.0F;
  roman_config.bow_height_scale = 1.0F;
  auto roman_bow = std::make_shared<BowRenderer>(roman_config);
  registry.registerEquipment(EquipmentCategory::Weapon, "bow_roman", roman_bow);

  BowRenderConfig kingdom_config;
  kingdom_config.bow_depth = 0.20F;
  kingdom_config.bow_curve_factor = 0.8F;
  kingdom_config.bow_height_scale = 1.15F;
  auto kingdom_bow = std::make_shared<BowRenderer>(kingdom_config);
  registry.registerEquipment(EquipmentCategory::Weapon, "bow_kingdom",
                             kingdom_bow);

  auto bow = std::make_shared<BowRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "bow", bow);

  auto quiver = std::make_shared<QuiverRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "quiver", quiver);

  auto montefortino_helmet = std::make_shared<MontefortinoHelmetRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "montefortino",
                             montefortino_helmet);
  registry.registerEquipment(EquipmentCategory::Helmet, "carthage_heavy",
                             montefortino_helmet);

  auto headwrap = std::make_shared<HeadwrapRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "headwrap", headwrap);
  registry.registerEquipment(EquipmentCategory::Helmet, "carthage_light",
                             headwrap);

  auto roman_heavy = std::make_shared<RomanHeavyHelmetRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "roman_heavy",
                             roman_heavy);

  auto roman_light = std::make_shared<RomanLightHelmetRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "roman_light",
                             roman_light);

  auto kingdom_heavy = std::make_shared<KingdomHeavyHelmetRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "kingdom_heavy",
                             kingdom_heavy);

  auto kingdom_light = std::make_shared<KingdomLightHelmetRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "kingdom_light",
                             kingdom_light);

  auto tunic = std::make_shared<TunicRenderer>();
  registry.registerEquipment(EquipmentCategory::Armor, "tunic", tunic);

  TunicConfig heavy_config;
  heavy_config.torso_scale = 1.08F;
  heavy_config.include_pauldrons = true;
  heavy_config.include_gorget = true;
  heavy_config.include_belt = true;
  auto heavy_tunic = std::make_shared<TunicRenderer>(heavy_config);
  registry.registerEquipment(EquipmentCategory::Armor, "heavy_tunic",
                             heavy_tunic);

  TunicConfig light_config;
  light_config.torso_scale = 1.04F;
  light_config.include_pauldrons = false;
  light_config.include_gorget = false;
  light_config.include_belt = true;
  auto light_tunic = std::make_shared<TunicRenderer>(light_config);
  registry.registerEquipment(EquipmentCategory::Armor, "light_tunic",
                             light_tunic);
}

} // namespace Render::GL
