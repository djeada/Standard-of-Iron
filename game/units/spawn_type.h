#pragma once

#include "troop_type.h"
#include <QString>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace Game::Units {

enum class SpawnType : std::uint8_t {
  Archer,
  Knight,
  Spearman,
  MountedKnight,
  HorseArcher,
  HorseSpearman,
  Healer,
  Catapult,
  Ballista,
  Elephant,
  RomanLegionOrganizer,
  RomanVeteranConsul,
  RomanFieldCommander,
  CarthageMercenaryBroker,
  CarthageCavalryPatron,
  CarthageElephantMaster,
  Civilian,
  Builder,
  Barracks,
  DefenseTower,
  Home
};

inline auto spawn_typeToQString(SpawnType type) -> QString {
  switch (type) {
  case SpawnType::Archer:
    return QStringLiteral("archer");
  case SpawnType::Knight:
    return QStringLiteral("swordsman");
  case SpawnType::Spearman:
    return QStringLiteral("spearman");
  case SpawnType::MountedKnight:
    return QStringLiteral("horse_swordsman");
  case SpawnType::HorseArcher:
    return QStringLiteral("horse_archer");
  case SpawnType::HorseSpearman:
    return QStringLiteral("horse_spearman");
  case SpawnType::Healer:
    return QStringLiteral("healer");
  case SpawnType::Catapult:
    return QStringLiteral("catapult");
  case SpawnType::Ballista:
    return QStringLiteral("ballista");
  case SpawnType::Elephant:
    return QStringLiteral("elephant");
  case SpawnType::RomanLegionOrganizer:
    return QStringLiteral("roman_legion_organizer");
  case SpawnType::RomanVeteranConsul:
    return QStringLiteral("roman_veteran_consul");
  case SpawnType::RomanFieldCommander:
    return QStringLiteral("roman_field_commander");
  case SpawnType::CarthageMercenaryBroker:
    return QStringLiteral("carthage_mercenary_broker");
  case SpawnType::CarthageCavalryPatron:
    return QStringLiteral("carthage_cavalry_patron");
  case SpawnType::CarthageElephantMaster:
    return QStringLiteral("carthage_elephant_master");
  case SpawnType::Civilian:
    return QStringLiteral("civilian");
  case SpawnType::Builder:
    return QStringLiteral("builder");
  case SpawnType::Barracks:
    return QStringLiteral("barracks");
  case SpawnType::DefenseTower:
    return QStringLiteral("defense_tower");
  case SpawnType::Home:
    return QStringLiteral("home");
  }
  return QStringLiteral("archer");
}

inline auto spawn_typeToString(SpawnType type) -> std::string {
  return spawn_typeToQString(type).toStdString();
}

inline auto try_parse_spawn_type(const QString &value, SpawnType &out) -> bool {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("archer")) {
    out = SpawnType::Archer;
    return true;
  }
  if (lowered == QStringLiteral("swordsman") ||
      lowered == QStringLiteral("swordsman")) {
    out = SpawnType::Knight;
    return true;
  }
  if (lowered == QStringLiteral("spearman")) {
    out = SpawnType::Spearman;
    return true;
  }
  if (lowered == QStringLiteral("horse_swordsman")) {
    out = SpawnType::MountedKnight;
    return true;
  }
  if (lowered == QStringLiteral("horse_archer")) {
    out = SpawnType::HorseArcher;
    return true;
  }
  if (lowered == QStringLiteral("horse_spearman")) {
    out = SpawnType::HorseSpearman;
    return true;
  }
  if (lowered == QStringLiteral("healer")) {
    out = SpawnType::Healer;
    return true;
  }
  if (lowered == QStringLiteral("catapult")) {
    out = SpawnType::Catapult;
    return true;
  }
  if (lowered == QStringLiteral("ballista")) {
    out = SpawnType::Ballista;
    return true;
  }
  if (lowered == QStringLiteral("elephant")) {
    out = SpawnType::Elephant;
    return true;
  }
  if (lowered == QStringLiteral("roman_legion_organizer")) {
    out = SpawnType::RomanLegionOrganizer;
    return true;
  }
  if (lowered == QStringLiteral("roman_veteran_consul")) {
    out = SpawnType::RomanVeteranConsul;
    return true;
  }
  if (lowered == QStringLiteral("roman_field_commander")) {
    out = SpawnType::RomanFieldCommander;
    return true;
  }
  if (lowered == QStringLiteral("carthage_mercenary_broker")) {
    out = SpawnType::CarthageMercenaryBroker;
    return true;
  }
  if (lowered == QStringLiteral("carthage_cavalry_patron")) {
    out = SpawnType::CarthageCavalryPatron;
    return true;
  }
  if (lowered == QStringLiteral("carthage_elephant_master")) {
    out = SpawnType::CarthageElephantMaster;
    return true;
  }
  if (lowered == QStringLiteral("civilian")) {
    out = SpawnType::Civilian;
    return true;
  }
  if (lowered == QStringLiteral("builder")) {
    out = SpawnType::Builder;
    return true;
  }
  if (lowered == QStringLiteral("barracks")) {
    out = SpawnType::Barracks;
    return true;
  }
  if (lowered == QStringLiteral("village")) {
    out = SpawnType::Barracks;
    return true;
  }
  if (lowered == QStringLiteral("defense_tower")) {
    out = SpawnType::DefenseTower;
    return true;
  }
  if (lowered == QStringLiteral("home")) {
    out = SpawnType::Home;
    return true;
  }
  return false;
}

inline auto
spawn_typeFromString(const std::string &str) -> std::optional<SpawnType> {
  if (str == "archer") {
    return SpawnType::Archer;
  }
  if (str == "swordsman" || str == "swordsman") {
    return SpawnType::Knight;
  }
  if (str == "spearman") {
    return SpawnType::Spearman;
  }
  if (str == "horse_swordsman") {
    return SpawnType::MountedKnight;
  }
  if (str == "horse_archer") {
    return SpawnType::HorseArcher;
  }
  if (str == "horse_spearman") {
    return SpawnType::HorseSpearman;
  }
  if (str == "healer") {
    return SpawnType::Healer;
  }
  if (str == "catapult") {
    return SpawnType::Catapult;
  }
  if (str == "ballista") {
    return SpawnType::Ballista;
  }
  if (str == "elephant") {
    return SpawnType::Elephant;
  }
  if (str == "roman_legion_organizer") {
    return SpawnType::RomanLegionOrganizer;
  }
  if (str == "roman_veteran_consul") {
    return SpawnType::RomanVeteranConsul;
  }
  if (str == "roman_field_commander") {
    return SpawnType::RomanFieldCommander;
  }
  if (str == "carthage_mercenary_broker") {
    return SpawnType::CarthageMercenaryBroker;
  }
  if (str == "carthage_cavalry_patron") {
    return SpawnType::CarthageCavalryPatron;
  }
  if (str == "carthage_elephant_master") {
    return SpawnType::CarthageElephantMaster;
  }
  if (str == "civilian") {
    return SpawnType::Civilian;
  }
  if (str == "builder") {
    return SpawnType::Builder;
  }
  if (str == "barracks") {
    return SpawnType::Barracks;
  }
  if (str == "village") {
    return SpawnType::Barracks;
  }
  if (str == "defense_tower") {
    return SpawnType::DefenseTower;
  }
  if (str == "home") {
    return SpawnType::Home;
  }
  return std::nullopt;
}

inline auto is_troop_spawn(SpawnType type) -> bool {
  return type != SpawnType::Barracks && type != SpawnType::DefenseTower &&
         type != SpawnType::Home;
}

inline auto is_building_spawn(SpawnType type) -> bool {
  return type == SpawnType::Barracks || type == SpawnType::DefenseTower ||
         type == SpawnType::Home;
}

inline auto can_use_attack_mode(SpawnType type) -> bool {
  return type != SpawnType::Healer && type != SpawnType::Builder &&
         type != SpawnType::Barracks && type != SpawnType::DefenseTower &&
         type != SpawnType::Home;
}

inline auto can_use_guard_mode(SpawnType type) -> bool {
  return type != SpawnType::Barracks && type != SpawnType::DefenseTower &&
         type != SpawnType::Home;
}

inline auto can_use_hold_mode(SpawnType type) -> bool {
  return type == SpawnType::Archer || type == SpawnType::Spearman;
}

inline auto can_use_patrol_mode(SpawnType type) -> bool {
  return type != SpawnType::Barracks && type != SpawnType::DefenseTower &&
         type != SpawnType::Home;
}

[[nodiscard]] inline auto can_use_run_mode(SpawnType type) noexcept -> bool {
  switch (type) {
  case SpawnType::Archer:
  case SpawnType::Knight:
  case SpawnType::Spearman:
  case SpawnType::Healer:
  case SpawnType::Civilian:
  case SpawnType::Builder:
  case SpawnType::MountedKnight:
  case SpawnType::HorseArcher:
  case SpawnType::HorseSpearman:
  case SpawnType::RomanLegionOrganizer:
  case SpawnType::RomanVeteranConsul:
  case SpawnType::RomanFieldCommander:
  case SpawnType::CarthageMercenaryBroker:
  case SpawnType::CarthageCavalryPatron:
  case SpawnType::CarthageElephantMaster:
    return true;
  case SpawnType::Catapult:
  case SpawnType::Ballista:
  case SpawnType::Barracks:
  case SpawnType::DefenseTower:
  case SpawnType::Home:
    return false;
  }
  return false;
}

inline auto spawn_typeToTroopType(SpawnType type) -> std::optional<TroopType> {
  switch (type) {
  case SpawnType::Archer:
    return TroopType::Archer;
  case SpawnType::Knight:
    return TroopType::Swordsman;
  case SpawnType::Spearman:
    return TroopType::Spearman;
  case SpawnType::MountedKnight:
    return TroopType::MountedKnight;
  case SpawnType::HorseArcher:
    return TroopType::HorseArcher;
  case SpawnType::HorseSpearman:
    return TroopType::HorseSpearman;
  case SpawnType::Healer:
    return TroopType::Healer;
  case SpawnType::Catapult:
    return TroopType::Catapult;
  case SpawnType::Ballista:
    return TroopType::Ballista;
  case SpawnType::Elephant:
    return TroopType::Elephant;
  case SpawnType::RomanLegionOrganizer:
    return TroopType::RomanLegionOrganizer;
  case SpawnType::RomanVeteranConsul:
    return TroopType::RomanVeteranConsul;
  case SpawnType::RomanFieldCommander:
    return TroopType::RomanFieldCommander;
  case SpawnType::CarthageMercenaryBroker:
    return TroopType::CarthageMercenaryBroker;
  case SpawnType::CarthageCavalryPatron:
    return TroopType::CarthageCavalryPatron;
  case SpawnType::CarthageElephantMaster:
    return TroopType::CarthageElephantMaster;
  case SpawnType::Civilian:
    return TroopType::Civilian;
  case SpawnType::Builder:
    return TroopType::Builder;
  case SpawnType::Barracks:
    return std::nullopt;
  case SpawnType::DefenseTower:
    return std::nullopt;
  case SpawnType::Home:
    return std::nullopt;
  }
  return std::nullopt;
}

inline auto spawn_typeFromTroopType(TroopType type) -> SpawnType {
  switch (type) {
  case TroopType::Archer:
    return SpawnType::Archer;
  case TroopType::Swordsman:
    return SpawnType::Knight;
  case TroopType::Spearman:
    return SpawnType::Spearman;
  case TroopType::MountedKnight:
    return SpawnType::MountedKnight;
  case TroopType::HorseArcher:
    return SpawnType::HorseArcher;
  case TroopType::HorseSpearman:
    return SpawnType::HorseSpearman;
  case TroopType::Healer:
    return SpawnType::Healer;
  case TroopType::Catapult:
    return SpawnType::Catapult;
  case TroopType::Ballista:
    return SpawnType::Ballista;
  case TroopType::Elephant:
    return SpawnType::Elephant;
  case TroopType::RomanLegionOrganizer:
    return SpawnType::RomanLegionOrganizer;
  case TroopType::RomanVeteranConsul:
    return SpawnType::RomanVeteranConsul;
  case TroopType::RomanFieldCommander:
    return SpawnType::RomanFieldCommander;
  case TroopType::CarthageMercenaryBroker:
    return SpawnType::CarthageMercenaryBroker;
  case TroopType::CarthageCavalryPatron:
    return SpawnType::CarthageCavalryPatron;
  case TroopType::CarthageElephantMaster:
    return SpawnType::CarthageElephantMaster;
  case TroopType::Civilian:
    return SpawnType::Civilian;
  case TroopType::Builder:
    return SpawnType::Builder;
  }
  return SpawnType::Archer;
}

} // namespace Game::Units

namespace std {
template <> struct hash<Game::Units::SpawnType> {
  auto operator()(Game::Units::SpawnType type) const noexcept -> size_t {
    return hash<std::uint8_t>()(static_cast<std::uint8_t>(type));
  }
};
} // namespace std
