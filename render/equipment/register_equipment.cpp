#include "armor/carthage_armor.h"
#include "armor/chainmail_armor.h"
#include "armor/kingdom_armor.h"
#include "armor/roman_armor.h"
#include "equipment_registry.h"
#include "helmets/carthage_light_helmet.h"
#include "helmets/headwrap.h"
#include "helmets/kingdom_heavy_helmet.h"
#include "helmets/kingdom_light_helmet.h"
#include "helmets/montefortino_helmet.h"
#include "helmets/roman_heavy_helmet.h"
#include "helmets/roman_light_helmet.h"
#include "weapons/bow_renderer.h"
#include "weapons/quiver_renderer.h"
#include "weapons/roman_scutum.h"
#include "weapons/shield_renderer.h"
#include "weapons/spear_renderer.h"
#include "weapons/sword_renderer.h"
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

  auto roman_scutum = std::make_shared<RomanScutumRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "roman_scutum",
                             roman_scutum);

  // CARTHAGE HELMETS: Heavy=bronze Montefortino, Light=leather cap
  auto carthage_heavy_helmet = std::make_shared<MontefortinoHelmetRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "montefortino",
                             carthage_heavy_helmet);
  registry.registerEquipment(EquipmentCategory::Helmet, "carthage_heavy",
                             carthage_heavy_helmet);

  auto carthage_light_helmet = std::make_shared<CarthageLightHelmetRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "carthage_light",
                             carthage_light_helmet);

  auto headwrap = std::make_shared<HeadwrapRenderer>();
  registry.registerEquipment(EquipmentCategory::Helmet, "headwrap", headwrap);

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

  auto kingdom_heavy_armor = std::make_shared<KingdomHeavyArmorRenderer>();
  registry.registerEquipment(EquipmentCategory::Armor, "kingdom_heavy_armor",
                             kingdom_heavy_armor);

  auto kingdom_light_armor = std::make_shared<KingdomLightArmorRenderer>();
  registry.registerEquipment(EquipmentCategory::Armor, "kingdom_light_armor",
                             kingdom_light_armor);

  auto roman_heavy_armor = std::make_shared<RomanHeavyArmorRenderer>();
  registry.registerEquipment(EquipmentCategory::Armor, "roman_heavy_armor",
                             roman_heavy_armor);

  auto roman_light_armor = std::make_shared<RomanLightArmorRenderer>();
  registry.registerEquipment(EquipmentCategory::Armor, "roman_light_armor",
                             roman_light_armor);

  // CARTHAGE ARMOR: Heavy=steel chainmail, Light=linen linothorax
  auto carthage_heavy_armor = std::make_shared<CarthageHeavyArmorRenderer>();
  registry.registerEquipment(EquipmentCategory::Armor, "carthage_heavy_armor",
                             carthage_heavy_armor);

  auto carthage_light_armor = std::make_shared<CarthageLightArmorRenderer>();
  registry.registerEquipment(EquipmentCategory::Armor, "carthage_light_armor",
                             carthage_light_armor);

  auto sword = std::make_shared<SwordRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "sword", sword);

  auto spear = std::make_shared<SpearRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "spear", spear);

  auto shield = std::make_shared<ShieldRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "shield", shield);
}

} // namespace Render::GL
