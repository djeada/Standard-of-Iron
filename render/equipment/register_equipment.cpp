#include "armor/armor_heavy_carthage.h"
#include "armor/armor_light_carthage.h"
#include "armor/chainmail_armor.h"
#include "armor/kingdom_armor.h"
#include "armor/roman_armor.h"
#include "equipment_registry.h"
#include "helmets/carthage_heavy_helmet.h"
#include "helmets/carthage_light_helmet.h"
#include "helmets/headwrap.h"
#include "helmets/kingdom_heavy_helmet.h"
#include "helmets/kingdom_light_helmet.h"
#include "helmets/roman_heavy_helmet.h"
#include "helmets/roman_light_helmet.h"
#include "weapons/bow_renderer.h"
#include "weapons/quiver_renderer.h"
#include "weapons/roman_scutum.h"
#include "weapons/shield_carthage.h"
#include "weapons/shield_kingdom.h"
#include "weapons/shield_renderer.h"
#include "weapons/shield_roman.h"
#include "weapons/spear_renderer.h"
#include "weapons/sword_carthage.h"
#include "weapons/sword_kingdom.h"
#include "weapons/sword_renderer.h"
#include "weapons/sword_roman.h"
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

  auto carthage_heavy_helmet = std::make_shared<CarthageHeavyHelmetRenderer>();
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

  auto armor_light_carthage = std::make_shared<ArmorLightCarthageRenderer>();
  registry.registerEquipment(EquipmentCategory::Armor, "armor_light_carthage",
                             armor_light_carthage);

  auto armor_heavy_carthage = std::make_shared<ArmorHeavyCarthageRenderer>();
  registry.registerEquipment(EquipmentCategory::Armor, "armor_heavy_carthage",
                             armor_heavy_carthage);

  auto sword = std::make_shared<SwordRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "sword", sword);

  auto sword_carthage = std::make_shared<CarthageSwordRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "sword_carthage",
                             sword_carthage);

  auto sword_roman = std::make_shared<RomanSwordRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "sword_roman",
                             sword_roman);

  auto sword_kingdom = std::make_shared<KingdomSwordRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "sword_kingdom",
                             sword_kingdom);

  auto spear = std::make_shared<SpearRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "spear", spear);

  auto shield = std::make_shared<ShieldRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "shield", shield);

  auto shield_carthage = std::make_shared<CarthageShieldRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "shield_carthage",
                             shield_carthage);

  auto shield_carthage_cavalry =
      std::make_shared<CarthageShieldRenderer>(0.84F);
  registry.registerEquipment(EquipmentCategory::Weapon,
                             "shield_carthage_cavalry",
                             shield_carthage_cavalry);

  auto shield_roman = std::make_shared<RomanShieldRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "shield_roman",
                             shield_roman);

  auto shield_kingdom = std::make_shared<KingdomShieldRenderer>();
  registry.registerEquipment(EquipmentCategory::Weapon, "shield_kingdom",
                             shield_kingdom);
}

} // namespace Render::GL
