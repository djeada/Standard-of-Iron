#include "equipment_loadout_catalog.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <qjsonvalue.h>
#include <qstringliteral.h>

#include <unordered_map>
#include <utility>

namespace Render::GL::Nation {
namespace {

using LoadoutMap = std::unordered_map<std::string, EquipmentLoadoutIds>;

auto default_loadouts() -> LoadoutMap {
  LoadoutMap map;
  EquipmentLoadoutIds roman_horse_archer{};
  roman_horse_archer.bow = "bow_roman";
  roman_horse_archer.quiver = "quiver";
  roman_horse_archer.armor = "roman_light_armor";
  roman_horse_archer.cloak = "cloak_carthage";
  roman_horse_archer.horse_bridle = "horse_bridle";
  roman_horse_archer.horse_reins = "horse_reins";
  roman_horse_archer.horse_blanket = "horse_blanket";
  roman_horse_archer.horse_decoration = "horse_saddle_bag";
  map.emplace("troops/roman/horse_archer", std::move(roman_horse_archer));

  EquipmentLoadoutIds carthage_horse_archer{};
  carthage_horse_archer.bow = "bow_carthage";
  carthage_horse_archer.quiver = "quiver";
  carthage_horse_archer.helmet = "carthage_light";
  carthage_horse_archer.armor = "armor_light_carthage";
  carthage_horse_archer.cloak = "cloak_carthage";
  carthage_horse_archer.horse_bridle = "horse_bridle";
  carthage_horse_archer.horse_reins = "horse_reins";
  carthage_horse_archer.horse_blanket = "horse_blanket";
  carthage_horse_archer.horse_decoration = "horse_saddle_bag";
  map.emplace("troops/carthage/horse_archer", std::move(carthage_horse_archer));

  EquipmentLoadoutIds roman_horse_spearman{};
  roman_horse_spearman.spear = "spear";
  roman_horse_spearman.armor = "roman_heavy_armor";
  roman_horse_spearman.shoulder = "roman_shoulder_cover_cavalry";
  roman_horse_spearman.horse_bridle = "horse_bridle";
  roman_horse_spearman.horse_reins = "horse_reins";
  roman_horse_spearman.horse_blanket = "horse_blanket";
  roman_horse_spearman.horse_barding = "horse_leather_barding";
  map.emplace("troops/roman/horse_spearman", std::move(roman_horse_spearman));

  EquipmentLoadoutIds carthage_horse_spearman{};
  carthage_horse_spearman.spear = "spear";
  carthage_horse_spearman.helmet = "carthage_heavy";
  carthage_horse_spearman.armor = "armor_heavy_carthage";
  carthage_horse_spearman.shoulder = "carthage_shoulder_cover_cavalry";
  carthage_horse_spearman.horse_bridle = "horse_bridle";
  carthage_horse_spearman.horse_reins = "horse_reins";
  carthage_horse_spearman.horse_blanket = "horse_blanket";
  carthage_horse_spearman.horse_barding = "horse_leather_barding";
  map.emplace("troops/carthage/horse_spearman",
              std::move(carthage_horse_spearman));

  EquipmentLoadoutIds roman_horse_swordsman{};
  roman_horse_swordsman.sword = "sword_roman";
  roman_horse_swordsman.shield = "roman_scutum";
  roman_horse_swordsman.armor = "roman_heavy_armor";
  roman_horse_swordsman.shoulder = "roman_shoulder_cover_cavalry";
  roman_horse_swordsman.horse_bridle = "horse_bridle";
  roman_horse_swordsman.horse_reins = "horse_reins";
  roman_horse_swordsman.horse_blanket = "horse_blanket";
  roman_horse_swordsman.horse_barding = "horse_scale_barding";
  roman_horse_swordsman.horse_crupper = "horse_crupper";
  map.emplace("troops/roman/horse_swordsman", std::move(roman_horse_swordsman));

  EquipmentLoadoutIds carthage_horse_swordsman{};
  carthage_horse_swordsman.sword = "sword_carthage";
  carthage_horse_swordsman.shield = "shield_carthage";
  carthage_horse_swordsman.helmet = "carthage_heavy";
  carthage_horse_swordsman.armor = "armor_heavy_carthage";
  carthage_horse_swordsman.shoulder = "carthage_shoulder_cover_cavalry";
  carthage_horse_swordsman.horse_bridle = "horse_bridle";
  carthage_horse_swordsman.horse_reins = "horse_reins";
  carthage_horse_swordsman.horse_blanket = "horse_blanket";
  carthage_horse_swordsman.horse_barding = "horse_champion_barding";
  carthage_horse_swordsman.horse_crupper = "horse_crupper";
  map.emplace("troops/carthage/horse_swordsman",
              std::move(carthage_horse_swordsman));
  return map;
}

void parse_loadout_object(const QJsonObject &obj, EquipmentLoadoutIds &out) {
  out.bow = obj.value("bow").toString().toStdString();
  out.quiver = obj.value("quiver").toString().toStdString();
  out.sword = obj.value("sword").toString().toStdString();
  out.spear = obj.value("spear").toString().toStdString();
  out.shield = obj.value("shield").toString().toStdString();
  out.helmet = obj.value("helmet").toString().toStdString();
  out.armor = obj.value("armor").toString().toStdString();
  out.shoulder = obj.value("shoulder").toString().toStdString();
  out.cloak = obj.value("cloak").toString().toStdString();
  out.horse_bridle = obj.value("horse_bridle").toString().toStdString();
  out.horse_reins = obj.value("horse_reins").toString().toStdString();
  out.horse_blanket = obj.value("horse_blanket").toString().toStdString();
  out.horse_barding = obj.value("horse_barding").toString().toStdString();
  out.horse_crupper = obj.value("horse_crupper").toString().toStdString();
  out.horse_decoration = obj.value("horse_decoration").toString().toStdString();
}

void merge_json_loadouts(LoadoutMap &map) {
  QFile file(QStringLiteral(":/assets/visuals/unit_equipment_loadouts.json"));
  if (!file.open(QIODevice::ReadOnly)) {
    return;
  }

  const QByteArray bytes = file.readAll();
  file.close();
  const QJsonDocument doc = QJsonDocument::fromJson(bytes);
  if (doc.isNull() || !doc.isObject()) {
    return;
  }

  const auto root = doc.object();
  const auto loadouts = root.value("loadouts").toObject();
  for (auto it = loadouts.begin(); it != loadouts.end(); ++it) {
    if (!it.value().isObject()) {
      continue;
    }
    EquipmentLoadoutIds ids{};
    parse_loadout_object(it.value().toObject(), ids);
    map[it.key().toStdString()] = std::move(ids);
  }
}

auto loadouts() -> const LoadoutMap & {
  static const LoadoutMap k_loadouts = []() {
    auto map = default_loadouts();
    merge_json_loadouts(map);
    return map;
  }();
  return k_loadouts;
}

auto resolve_slot_handle(EquipmentCategory category,
                         const std::string &id) -> EquipmentHandle {
  if (id.empty()) {
    return kInvalidEquipmentHandle;
  }
  return EquipmentRegistry::instance().resolve_handle(category, id);
}

} // namespace

auto resolve_equipment_loadout(std::string_view renderer_key)
    -> ResolvedEquipmentLoadout {
  ResolvedEquipmentLoadout out{};
  const auto &map = loadouts();
  const auto it = map.find(std::string(renderer_key));
  if (it == map.end()) {
    return out;
  }

  out.found = true;
  out.ids = it->second;
  out.bow_handle = resolve_slot_handle(EquipmentCategory::Weapon, out.ids.bow);
  out.quiver_handle =
      resolve_slot_handle(EquipmentCategory::Weapon, out.ids.quiver);
  out.sword_handle =
      resolve_slot_handle(EquipmentCategory::Weapon, out.ids.sword);
  out.spear_handle =
      resolve_slot_handle(EquipmentCategory::Weapon, out.ids.spear);
  out.shield_handle =
      resolve_slot_handle(EquipmentCategory::Weapon, out.ids.shield);
  out.helmet_handle =
      resolve_slot_handle(EquipmentCategory::Helmet, out.ids.helmet);
  out.armor_handle =
      resolve_slot_handle(EquipmentCategory::Armor, out.ids.armor);
  out.shoulder_handle =
      resolve_slot_handle(EquipmentCategory::Armor, out.ids.shoulder);
  out.cloak_handle =
      resolve_slot_handle(EquipmentCategory::Armor, out.ids.cloak);
  out.horse_bridle_handle =
      resolve_slot_handle(EquipmentCategory::HorseTack, out.ids.horse_bridle);
  out.horse_reins_handle =
      resolve_slot_handle(EquipmentCategory::HorseTack, out.ids.horse_reins);
  out.horse_blanket_handle =
      resolve_slot_handle(EquipmentCategory::HorseTack, out.ids.horse_blanket);
  out.horse_barding_handle =
      resolve_slot_handle(EquipmentCategory::HorseArmor, out.ids.horse_barding);
  out.horse_crupper_handle =
      resolve_slot_handle(EquipmentCategory::HorseArmor, out.ids.horse_crupper);
  out.horse_decoration_handle = resolve_slot_handle(
      EquipmentCategory::HorseDecoration, out.ids.horse_decoration);
  return out;
}

} // namespace Render::GL::Nation
