#include "armor/arm_guards_renderer.h"
#include "armor/armor_heavy_carthage.h"
#include "armor/armor_light_carthage.h"
#include "armor/carthage_shoulder_cover.h"
#include "armor/chainmail_armor.h"
#include "armor/cloak_renderer.h"
#include "armor/roman_armor.h"
#include "armor/roman_greaves.h"
#include "armor/roman_shoulder_cover.h"
#include "armor/tool_belt_renderer.h"
#include "armor/work_apron_renderer.h"
#include "equipment_registry.h"
#include "helmets/carthage_heavy_helmet.h"
#include "helmets/carthage_light_helmet.h"
#include "helmets/headwrap.h"
#include "helmets/roman_heavy_helmet.h"
#include "helmets/roman_light_helmet.h"
#include "weapons/bow_renderer.h"
#include "weapons/quiver_renderer.h"
#include "weapons/roman_scutum.h"
#include "weapons/shield_carthage.h"
#include "weapons/shield_renderer.h"
#include "weapons/shield_roman.h"
#include "weapons/spear_renderer.h"
#include "weapons/sword_carthage.h"
#include "weapons/sword_renderer.h"
#include "weapons/sword_roman.h"
#include <memory>

namespace Render::GL {

void register_built_in_equipment() {
  auto &registry = EquipmentRegistry::instance();

  BowRenderConfig carthage_config;
  carthage_config.bow_depth = 0.28F;
  carthage_config.bow_curve_factor = 1.2F;
  carthage_config.bow_height_scale = 0.95F;
  auto carthage_bow = std::make_shared<BowRenderer>(carthage_config);
  registry.register_equipment(EquipmentCategory::Weapon, "bow_carthage",
                              carthage_bow);

  BowRenderConfig roman_config;
  roman_config.bow_depth = 0.22F;
  roman_config.bow_curve_factor = 1.0F;
  roman_config.bow_height_scale = 1.0F;
  auto roman_bow = std::make_shared<BowRenderer>(roman_config);
  registry.register_equipment(EquipmentCategory::Weapon, "bow_roman",
                              roman_bow);

  auto bow = std::make_shared<BowRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "bow", bow);

  auto quiver = std::make_shared<QuiverRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "quiver", quiver);

  auto roman_scutum = std::make_shared<RomanScutumRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "roman_scutum",
                              roman_scutum);

  auto carthage_heavy_helmet = std::make_shared<CarthageHeavyHelmetRenderer>();
  registry.register_equipment(EquipmentCategory::Helmet, "carthage_heavy",
                              carthage_heavy_helmet);

  auto carthage_light_helmet = std::make_shared<CarthageLightHelmetRenderer>();
  registry.register_equipment(EquipmentCategory::Helmet, "carthage_light",
                              carthage_light_helmet);

  auto headwrap = std::make_shared<HeadwrapRenderer>();
  registry.register_equipment(EquipmentCategory::Helmet, "headwrap", headwrap);

  auto roman_heavy = std::make_shared<RomanHeavyHelmetRenderer>();
  registry.register_equipment(EquipmentCategory::Helmet, "roman_heavy",
                              roman_heavy);

  auto roman_light = std::make_shared<RomanLightHelmetRenderer>();
  registry.register_equipment(EquipmentCategory::Helmet, "roman_light",
                              roman_light);

  auto roman_heavy_armor = std::make_shared<RomanHeavyArmorRenderer>();
  registry.register_equipment(EquipmentCategory::Armor, "roman_heavy_armor",
                              roman_heavy_armor);

  auto roman_light_armor = std::make_shared<RomanLightArmorRenderer>();
  registry.register_equipment(EquipmentCategory::Armor, "roman_light_armor",
                              roman_light_armor);

  auto armor_light_carthage = std::make_shared<ArmorLightCarthageRenderer>();
  registry.register_equipment(EquipmentCategory::Armor, "armor_light_carthage",
                              armor_light_carthage);

  auto armor_heavy_carthage = std::make_shared<ArmorHeavyCarthageRenderer>();
  registry.register_equipment(EquipmentCategory::Armor, "armor_heavy_carthage",
                              armor_heavy_carthage);

  auto roman_shoulder_cover = std::make_shared<RomanShoulderCoverRenderer>();
  registry.register_equipment(EquipmentCategory::Armor, "roman_shoulder_cover",
                              roman_shoulder_cover);
  auto roman_shoulder_cover_cavalry =
      std::make_shared<RomanShoulderCoverRenderer>(1.8F);
  registry.register_equipment(EquipmentCategory::Armor,
                              "roman_shoulder_cover_cavalry",
                              roman_shoulder_cover_cavalry);

  auto roman_greaves = std::make_shared<RomanGreavesRenderer>();
  registry.register_equipment(EquipmentCategory::Armor, "roman_greaves",
                              roman_greaves);

  auto carthage_shoulder_cover =
      std::make_shared<CarthageShoulderCoverRenderer>();
  registry.register_equipment(EquipmentCategory::Armor,
                              "carthage_shoulder_cover",
                              carthage_shoulder_cover);
  auto carthage_shoulder_cover_cavalry =
      std::make_shared<CarthageShoulderCoverRenderer>(1.8F);
  registry.register_equipment(EquipmentCategory::Armor,
                              "carthage_shoulder_cover_cavalry",
                              carthage_shoulder_cover_cavalry);
  CloakConfig carthage_cloak_config;
  carthage_cloak_config.primary_color = QVector3D(0.14F, 0.38F, 0.54F);
  carthage_cloak_config.trim_color = QVector3D(0.75F, 0.66F, 0.42F);
  auto cloak_carthage = std::make_shared<CloakRenderer>(carthage_cloak_config);
  registry.register_equipment(EquipmentCategory::Armor, "cloak_carthage",
                              cloak_carthage);

  auto sword = std::make_shared<SwordRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "sword", sword);

  auto sword_carthage = std::make_shared<CarthageSwordRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "sword_carthage",
                              sword_carthage);

  auto sword_roman = std::make_shared<RomanSwordRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "sword_roman",
                              sword_roman);

  auto spear = std::make_shared<SpearRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "spear", spear);

  auto shield = std::make_shared<ShieldRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "shield", shield);

  auto shield_carthage = std::make_shared<CarthageShieldRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "shield_carthage",
                              shield_carthage);

  auto shield_carthage_cavalry =
      std::make_shared<CarthageShieldRenderer>(0.84F);
  registry.register_equipment(EquipmentCategory::Weapon,
                              "shield_carthage_cavalry",
                              shield_carthage_cavalry);

  auto shield_roman = std::make_shared<RomanShieldRenderer>();
  registry.register_equipment(EquipmentCategory::Weapon, "shield_roman",
                              shield_roman);

  WorkApronConfig roman_apron_config;
  roman_apron_config.leather_color = QVector3D(0.48F, 0.35F, 0.22F);
  auto work_apron_roman =
      std::make_shared<WorkApronRenderer>(roman_apron_config);
  registry.register_equipment(EquipmentCategory::Armor, "work_apron_roman",
                              work_apron_roman);

  WorkApronConfig carthage_apron_config;
  carthage_apron_config.leather_color = QVector3D(0.44F, 0.30F, 0.18F);
  auto work_apron_carthage =
      std::make_shared<WorkApronRenderer>(carthage_apron_config);
  registry.register_equipment(EquipmentCategory::Armor, "work_apron_carthage",
                              work_apron_carthage);

  ToolBeltConfig roman_tool_belt_config;
  roman_tool_belt_config.leather_color = QVector3D(0.52F, 0.40F, 0.28F);
  auto tool_belt_roman =
      std::make_shared<ToolBeltRenderer>(roman_tool_belt_config);
  registry.register_equipment(EquipmentCategory::Armor, "tool_belt_roman",
                              tool_belt_roman);

  ToolBeltConfig carthage_tool_belt_config;
  carthage_tool_belt_config.leather_color = QVector3D(0.46F, 0.34F, 0.22F);
  auto tool_belt_carthage =
      std::make_shared<ToolBeltRenderer>(carthage_tool_belt_config);
  registry.register_equipment(EquipmentCategory::Armor, "tool_belt_carthage",
                              tool_belt_carthage);

  ArmGuardsConfig arm_guards_config;
  arm_guards_config.leather_color = QVector3D(0.50F, 0.38F, 0.26F);
  auto arm_guards = std::make_shared<ArmGuardsRenderer>(arm_guards_config);
  registry.register_equipment(EquipmentCategory::Armor, "arm_guards",
                              arm_guards);
}

} 
