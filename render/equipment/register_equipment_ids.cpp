#include "register_equipment_internal.h"

namespace Render::GL::EquipmentRegistration {

void register_equipment_ids(EquipmentRegistry& registry) {
  registry.register_equipment_id(EquipmentCategory::Weapon, "bow");
  registry.register_equipment_id(EquipmentCategory::Weapon, "bow_carthage");
  registry.register_equipment_id(EquipmentCategory::Weapon, "bow_roman");
  registry.register_equipment_id(EquipmentCategory::Weapon, "bow_marcellus");
  registry.register_equipment_id(EquipmentCategory::Weapon, "bow_hasdrubal");
  registry.register_equipment_id(EquipmentCategory::Weapon, "quiver");
  registry.register_equipment_id(EquipmentCategory::Weapon, "quiver_roman");
  registry.register_equipment_id(EquipmentCategory::Weapon, "quiver_carthage");
  registry.register_equipment_id(EquipmentCategory::Weapon, "quiver_marcellus");
  registry.register_equipment_id(EquipmentCategory::Weapon, "quiver_hasdrubal");
  registry.register_equipment_id(EquipmentCategory::Weapon, "roman_scutum");
  registry.register_equipment_id(EquipmentCategory::Weapon, "sword");
  registry.register_equipment_id(EquipmentCategory::Weapon, "sword_carthage");
  registry.register_equipment_id(EquipmentCategory::Weapon, "sword_sepulcher");
  registry.register_equipment_id(EquipmentCategory::Weapon, "sword_roman");
  registry.register_equipment_id(EquipmentCategory::Weapon, "sword_scipio");
  registry.register_equipment_id(EquipmentCategory::Weapon, "sword_hannibal");
  registry.register_equipment_id(EquipmentCategory::Weapon, "spear");
  registry.register_equipment_id(EquipmentCategory::Weapon, "spear_fabius");
  registry.register_equipment_id(EquipmentCategory::Weapon, "spear_hanno");
  registry.register_equipment_id(EquipmentCategory::Weapon, "shield");
  registry.register_equipment_id(EquipmentCategory::Weapon, "shield_carthage");
  registry.register_equipment_id(EquipmentCategory::Weapon, "shield_carthage_cavalry");

  registry.register_equipment_id(EquipmentCategory::Helmet, "carthage_heavy");
  registry.register_equipment_id(EquipmentCategory::Helmet, "carthage_light");
  registry.register_equipment_id(EquipmentCategory::Helmet, "roman_heavy");
  registry.register_equipment_id(EquipmentCategory::Helmet, "roman_light");
  registry.register_equipment_id(EquipmentCategory::Helmet, "roman_montefortino");
  registry.register_equipment_id(EquipmentCategory::Helmet, "roman_boeotian_cavalry");
  registry.register_equipment_id(EquipmentCategory::Helmet, "carthage_punic_conical");
  registry.register_equipment_id(EquipmentCategory::Helmet,
                                 "carthage_thracian_crested");
  registry.register_equipment_id(EquipmentCategory::Helmet, "headwrap");

  registry.register_equipment_id(EquipmentCategory::Armor, "roman_heavy_armor");
  registry.register_equipment_id(EquipmentCategory::Armor, "roman_light_armor");
  registry.register_equipment_id(EquipmentCategory::Armor, "armor_light_carthage");
  registry.register_equipment_id(EquipmentCategory::Armor, "armor_heavy_carthage");
  registry.register_equipment_id(EquipmentCategory::Armor, "roman_hamata_mail");
  registry.register_equipment_id(EquipmentCategory::Armor, "roman_anatomical_cuirass");
  registry.register_equipment_id(EquipmentCategory::Armor, "carthage_linothorax");
  registry.register_equipment_id(EquipmentCategory::Armor, "carthage_gilded_scale");
  registry.register_equipment_id(EquipmentCategory::Armor, "roman_shoulder_cover");
  registry.register_equipment_id(EquipmentCategory::Armor,
                                 "roman_shoulder_cover_cavalry");
  registry.register_equipment_id(EquipmentCategory::Armor, "roman_greaves");
  registry.register_equipment_id(EquipmentCategory::Armor, "carthage_shoulder_cover");
  registry.register_equipment_id(EquipmentCategory::Armor,
                                 "carthage_shoulder_cover_cavalry");
  registry.register_equipment_id(EquipmentCategory::Armor, "cloak_carthage");
  registry.register_equipment_id(EquipmentCategory::Armor, "cloak_carthage_mounted");
  registry.register_equipment_id(EquipmentCategory::Armor, "cloak_sepulcher");
  registry.register_equipment_id(EquipmentCategory::Armor, "cloak_roman");
  registry.register_equipment_id(EquipmentCategory::Armor, "cloak_roman_mounted");
  registry.register_equipment_id(EquipmentCategory::Armor, "work_apron_roman");
  registry.register_equipment_id(EquipmentCategory::Armor, "work_apron_carthage");
  registry.register_equipment_id(EquipmentCategory::Armor, "tool_belt_roman");
  registry.register_equipment_id(EquipmentCategory::Armor, "tool_belt_carthage");
  registry.register_equipment_id(EquipmentCategory::Armor, "arm_guards");
  registry.register_equipment_id(EquipmentCategory::Armor, "arm_guards_roman");
  registry.register_equipment_id(EquipmentCategory::Armor, "arm_guards_carthage");

  registry.register_placeholder_equipment(EquipmentCategory::Armor,
                                          "builder_tunic_roman");
  registry.register_placeholder_equipment(EquipmentCategory::Armor,
                                          "roman_civilian_mantle");
  registry.register_placeholder_equipment(EquipmentCategory::Helmet,
                                          "headwrap_carthage_civilian");
  registry.register_placeholder_equipment(EquipmentCategory::Armor, "carthage_robes");
  registry.register_placeholder_equipment(EquipmentCategory::Armor,
                                          "carthage_civilian_sash");

  registry.register_equipment_id(EquipmentCategory::HorseTack, "roman_horse_saddle");
  registry.register_equipment_id(EquipmentCategory::HorseTack, "carthage_horse_saddle");
  registry.register_equipment_id(EquipmentCategory::HorseTack, "light_cavalry_saddle");
  registry.register_equipment_id(EquipmentCategory::HorseTack, "horse_bridle");
  registry.register_equipment_id(EquipmentCategory::HorseTack, "horse_reins");
  registry.register_equipment_id(EquipmentCategory::HorseTack, "horse_blanket");
  registry.register_equipment_id(EquipmentCategory::HorseArmor,
                                 "horse_leather_barding");
  registry.register_equipment_id(EquipmentCategory::HorseArmor, "horse_scale_barding");
  registry.register_equipment_id(EquipmentCategory::HorseArmor,
                                 "horse_champion_barding");
  registry.register_equipment_id(EquipmentCategory::HorseArmor, "horse_crupper");
  registry.register_equipment_id(EquipmentCategory::HorseDecoration,
                                 "horse_saddle_bag");
}

} // namespace Render::GL::EquipmentRegistration
