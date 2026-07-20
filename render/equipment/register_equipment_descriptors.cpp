#include "armor/arm_guards_renderer.h"
#include "armor/armor_heavy_carthage.h"
#include "armor/armor_light_carthage.h"
#include "armor/carthage_shoulder_cover.h"
#include "armor/cloak_renderer.h"
#include "armor/roman_armor.h"
#include "armor/roman_greaves.h"
#include "armor/roman_shoulder_cover.h"
#include "armor/tool_belt_renderer.h"
#include "armor/work_apron_renderer.h"
#include "helmets/carthage_heavy_helmet.h"
#include "helmets/carthage_light_helmet.h"
#include "helmets/headwrap.h"
#include "helmets/historical_helmets.h"
#include "helmets/roman_heavy_helmet.h"
#include "helmets/roman_light_helmet.h"
#include "horse/armor/champion_renderer.h"
#include "horse/armor/crupper_renderer.h"
#include "horse/armor/leather_barding_renderer.h"
#include "horse/armor/scale_barding_renderer.h"
#include "horse/decorations/saddle_bag_renderer.h"
#include "horse/saddles/carthage_saddle_renderer.h"
#include "horse/saddles/light_cavalry_saddle_renderer.h"
#include "horse/saddles/roman_saddle_renderer.h"
#include "horse/tack/blanket_renderer.h"
#include "horse/tack/bridle_renderer.h"
#include "horse/tack/reins_renderer.h"
#include "register_equipment_internal.h"
#include "weapons/bow_renderer.h"
#include "weapons/quiver_renderer.h"
#include "weapons/roman_scutum.h"
#include "weapons/shield_carthage.h"
#include "weapons/spear_renderer.h"
#include "weapons/sword_carthage.h"
#include "weapons/sword_renderer.h"

namespace Render::GL::EquipmentRegistration {
namespace {

void register_humanoid_descriptor(EquipmentCategory category,
                                  const char* id,
                                  HumanoidEquipmentContribution contribution) {
  const auto handle = EquipmentRegistry::instance().resolve_handle(category, id);
  register_humanoid_equipment_contribution(handle, contribution);
}

void register_horse_descriptor(EquipmentCategory category,
                               const char* id,
                               HorseEquipmentContribution contribution) {
  const auto handle = EquipmentRegistry::instance().resolve_handle(category, id);
  register_horse_equipment_contribution(handle, contribution);
}

} // namespace

void register_equipment_descriptors() {
  register_horse_descriptor(
      EquipmentCategory::HorseTack,
      "roman_horse_saddle",
      {.build_attachments = &build_roman_horse_saddle_attachment,
       .append_role_colors = &roman_horse_saddle_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_roman_saddle_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseTack,
      "carthage_horse_saddle",
      {.build_attachments = &build_carthage_horse_saddle_attachment,
       .append_role_colors = &carthage_horse_saddle_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_carthage_saddle_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseTack,
      "light_cavalry_saddle",
      {.build_attachments = &build_light_cavalry_saddle_attachment,
       .append_role_colors = &light_cavalry_saddle_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_light_cavalry_saddle_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseTack,
      "horse_bridle",
      {.build_attachments = &build_horse_bridle_attachment,
       .append_role_colors = &horse_bridle_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_bridle_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseTack,
      "horse_reins",
      {.build_attachments = &build_horse_reins_attachment,
       .append_role_colors = &horse_reins_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_reins_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseTack,
      "horse_blanket",
      {.build_attachments = &build_horse_blanket_attachment,
       .append_role_colors = &horse_blanket_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_blanket_role_count)});
  register_horse_descriptor(EquipmentCategory::HorseArmor,
                            "horse_leather_barding",
                            {.build_attachments = &build_leather_barding_attachments,
                             .append_role_colors = &leather_barding_role_colors,
                             .role_count = static_cast<std::uint8_t>(
                                 Render::GL::k_leather_barding_role_count)});
  register_horse_descriptor(EquipmentCategory::HorseArmor,
                            "horse_scale_barding",
                            {.build_attachments = &build_scale_barding_attachments,
                             .append_role_colors = &scale_barding_role_colors,
                             .role_count = static_cast<std::uint8_t>(
                                 Render::GL::k_scale_barding_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseArmor,
      "horse_champion_barding",
      {.build_attachments = &build_champion_barding_attachment,
       .append_role_colors = &champion_barding_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_champion_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseArmor,
      "horse_crupper",
      {.build_attachments = &build_crupper_attachment,
       .append_role_colors = &horse_crupper_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_crupper_role_count)});
  register_horse_descriptor(
      EquipmentCategory::HorseDecoration,
      "horse_saddle_bag",
      {.build_attachments = &build_saddle_bag_attachment,
       .append_role_colors = &saddle_bag_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_saddle_bag_role_count)});

  register_humanoid_descriptor(
      EquipmentCategory::Helmet,
      "carthage_light",
      {.build_attachments = &build_carthage_light_helmet_attachments,
       .append_role_colors = &carthage_light_helmet_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_carthage_light_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet,
      "carthage_heavy",
      {.build_attachments = &build_carthage_heavy_helmet_attachments,
       .append_role_colors = &carthage_heavy_helmet_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_carthage_heavy_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet,
      "roman_heavy",
      {.build_attachments = &build_roman_heavy_helmet_attachment,
       .append_role_colors = &roman_heavy_helmet_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_roman_heavy_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet,
      "roman_light",
      {.build_attachments = &build_roman_light_helmet_attachment,
       .append_role_colors = &roman_light_helmet_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_roman_light_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet,
      "roman_montefortino",
      {.build_attachments = &build_roman_montefortino_attachment,
       .append_role_colors = &roman_montefortino_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_historical_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet,
      "roman_boeotian_cavalry",
      {.build_attachments = &build_roman_boeotian_cavalry_attachment,
       .append_role_colors = &roman_boeotian_cavalry_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_historical_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet,
      "carthage_punic_conical",
      {.build_attachments = &build_carthage_punic_conical_attachment,
       .append_role_colors = &carthage_punic_conical_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_historical_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet,
      "carthage_thracian_crested",
      {.build_attachments = &build_carthage_thracian_crested_attachment,
       .append_role_colors = &carthage_thracian_crested_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_historical_helmet_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Helmet,
      "headwrap",
      {.build_attachments = &build_headwrap_attachment,
       .append_role_colors = [](const void* variant_void,
                                QVector3D* out,
                                std::uint32_t base_count,
                                std::size_t max_count) -> std::uint32_t {
         return with_variant_palette(
             variant_void,
             [](const HumanoidVariant& variant,
                QVector3D* colors,
                std::uint32_t count,
                std::size_t max) {
               return count + Render::GL::headwrap_fill_role_colors(
                                  variant.palette, colors + count, max - count);
             },
             out,
             base_count,
             max_count);
       },
       .role_count = static_cast<std::uint8_t>(Render::GL::k_headwrap_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "roman_light_armor",
      {.build_attachments = &build_roman_light_armor_attachment,
       .append_role_colors = &roman_light_armor_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_roman_light_armor_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "roman_heavy_armor",
      {.build_attachments = &build_roman_heavy_armor_attachment,
       .append_role_colors = &roman_heavy_armor_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_roman_heavy_armor_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "armor_light_carthage",
      {.build_attachments = &build_carthage_light_armor_attachment,
       .append_role_colors = &carthage_light_armor_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_armor_light_carthage_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "armor_heavy_carthage",
      {.build_attachments = &build_carthage_heavy_armor_attachment,
       .append_role_colors = &carthage_heavy_armor_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_armor_heavy_carthage_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "roman_hamata_mail",
      {.build_attachments = &build_roman_light_armor_attachment,
       .append_role_colors = &roman_light_armor_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_roman_light_armor_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "roman_anatomical_cuirass",
      {.build_attachments = &build_roman_heavy_armor_attachment,
       .append_role_colors = &roman_heavy_armor_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_roman_heavy_armor_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "carthage_linothorax",
      {.build_attachments = &build_carthage_light_armor_attachment,
       .append_role_colors = &carthage_light_armor_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_armor_light_carthage_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "carthage_gilded_scale",
      {.build_attachments = &build_carthage_heavy_armor_attachment,
       .append_role_colors = &carthage_heavy_armor_role_colors,
       .role_count =
           static_cast<std::uint8_t>(Render::GL::k_armor_heavy_carthage_role_count)});
  register_humanoid_descriptor(EquipmentCategory::Armor,
                               "roman_greaves",
                               {.build_attachments = &build_roman_greaves_attachments,
                                .append_role_colors = &roman_greaves_role_colors,
                                .role_count = static_cast<std::uint8_t>(
                                    Render::GL::k_roman_greaves_role_count)});
  register_humanoid_descriptor(EquipmentCategory::Armor,
                               "roman_shoulder_cover",
                               {.build_attachments = &build_roman_shoulder_attachments,
                                .append_role_colors = &roman_shoulder_role_colors,
                                .role_count = static_cast<std::uint8_t>(
                                    Render::GL::k_roman_shoulder_cover_role_count)});
  register_humanoid_descriptor(EquipmentCategory::Armor,
                               "roman_shoulder_cover_cavalry",
                               {.build_attachments = &build_roman_shoulder_attachments,
                                .append_role_colors = &roman_shoulder_role_colors,
                                .role_count = static_cast<std::uint8_t>(
                                    Render::GL::k_roman_shoulder_cover_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "carthage_shoulder_cover",
      {.build_attachments = &build_carthage_shoulder_attachments,
       .append_role_colors = &carthage_shoulder_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_carthage_shoulder_cover_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "carthage_shoulder_cover_cavalry",
      {.build_attachments = &build_carthage_shoulder_attachments,
       .append_role_colors = &carthage_shoulder_role_colors,
       .role_count = static_cast<std::uint8_t>(
           Render::GL::k_carthage_shoulder_cover_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "spear",
      {.build_attachments = &build_spear_attachments,
       .append_role_colors = &spear_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_spear_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "spear_fabius",
      {.build_attachments = &build_fabius_spear_attachments,
       .append_role_colors = &fabius_spear_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_spear_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "spear_hanno",
      {.build_attachments = &build_hanno_spear_attachments,
       .append_role_colors = &hanno_spear_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_spear_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "roman_scutum",
      {.build_attachments = &build_roman_scutum_attachment,
       .append_role_colors = &roman_scutum_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_roman_scutum_role_count)});
  register_humanoid_descriptor(EquipmentCategory::Weapon,
                               "shield_carthage",
                               {.build_attachments = &build_carthage_shield_attachment,
                                .append_role_colors = &carthage_shield_role_colors,
                                .role_count = static_cast<std::uint8_t>(
                                    Render::GL::k_carthage_shield_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "sword_roman",
      {.build_attachments = &build_roman_sword_attachments,
       .append_role_colors = &roman_sword_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_sword_role_count +
                                               Render::GL::k_scabbard_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "sword_scipio",
      {.build_attachments = &build_scipio_sword_attachments,
       .append_role_colors = &scipio_sword_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_sword_role_count +
                                               Render::GL::k_scabbard_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "sword_carthage",
      {.build_attachments = &build_carthage_sword_attachments,
       .append_role_colors = &carthage_sword_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_sword_role_count +
                                               Render::GL::k_scabbard_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "sword_hannibal",
      {.build_attachments = &build_hannibal_sword_attachments,
       .append_role_colors = &hannibal_sword_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_sword_role_count +
                                               Render::GL::k_scabbard_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "sword_sepulcher",
      {.build_attachments = &build_sepulcher_sword_attachments,
       .append_role_colors = &sepulcher_sword_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_sword_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "bow_roman",
      {.build_attachments = &build_roman_bow_attachments,
       .append_role_colors = &bow_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_bow_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "bow_marcellus",
      {.build_attachments = &build_marcellus_bow_attachments,
       .append_role_colors = &marcellus_bow_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_bow_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "bow_carthage",
      {.build_attachments = &build_carthage_bow_attachments,
       .append_role_colors = &bow_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_bow_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "bow_hasdrubal",
      {.build_attachments = &build_hasdrubal_bow_attachments,
       .append_role_colors = &hasdrubal_bow_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_bow_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "quiver",
      {.build_attachments = &build_quiver_attachments,
       .append_role_colors = &quiver_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_quiver_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "quiver_roman",
      {.build_attachments = &build_roman_quiver_attachments,
       .append_role_colors = &roman_quiver_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_quiver_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "quiver_marcellus",
      {.build_attachments = &build_marcellus_quiver_attachments,
       .append_role_colors = &marcellus_quiver_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_quiver_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "quiver_carthage",
      {.build_attachments = &build_carthage_quiver_attachments,
       .append_role_colors = &carthage_quiver_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_quiver_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Weapon,
      "quiver_hasdrubal",
      {.build_attachments = &build_hasdrubal_quiver_attachments,
       .append_role_colors = &hasdrubal_quiver_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_quiver_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "cloak_carthage",
      {.build_attachments = &build_carthage_cloak_attachment,
       .append_role_colors = &carthage_cloak_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_cloak_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "cloak_carthage_mounted",
      {.build_attachments = &build_carthage_mounted_cloak_attachment,
       .append_role_colors = &carthage_cloak_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_cloak_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "cloak_sepulcher",
      {.build_attachments = &build_sepulcher_cloak_attachment,
       .append_role_colors = &sepulcher_cloak_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_cloak_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "cloak_roman",
      {.build_attachments = &build_roman_cloak_attachment,
       .append_role_colors = &roman_cloak_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_cloak_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "cloak_roman_mounted",
      {.build_attachments = &build_roman_mounted_cloak_attachment,
       .append_role_colors = &roman_cloak_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_cloak_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "tool_belt_roman",
      {.build_attachments = &build_tool_belt_attachments,
       .append_role_colors = &roman_tool_belt_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_tool_belt_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "tool_belt_carthage",
      {.build_attachments = &build_tool_belt_attachments,
       .append_role_colors = &carthage_tool_belt_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_tool_belt_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "work_apron_roman",
      {.build_attachments = &build_work_apron_attachments,
       .append_role_colors = &roman_work_apron_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_work_apron_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "work_apron_carthage",
      {.build_attachments = &build_work_apron_attachments,
       .append_role_colors = &carthage_work_apron_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_work_apron_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "arm_guards_roman",
      {.build_attachments = &build_arm_guards_attachments,
       .append_role_colors = &roman_arm_guards_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_arm_guards_role_count)});
  register_humanoid_descriptor(
      EquipmentCategory::Armor,
      "arm_guards_carthage",
      {.build_attachments = &build_arm_guards_attachments,
       .append_role_colors = &carthage_arm_guards_role_colors,
       .role_count = static_cast<std::uint8_t>(Render::GL::k_arm_guards_role_count)});
}

} // namespace Render::GL::EquipmentRegistration
