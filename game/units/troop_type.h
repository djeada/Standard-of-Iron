#pragma once

#include <QString>

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <string>

namespace Game::Units {

enum class TroopType {
  Archer,
  Swordsman,
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
  Builder
};

inline auto troop_typeToQString(TroopType type) -> QString {
  switch (type) {
  case TroopType::Archer:
    return QStringLiteral("archer");
  case TroopType::Swordsman:
    return QStringLiteral("swordsman");
  case TroopType::Spearman:
    return QStringLiteral("spearman");
  case TroopType::MountedKnight:
    return QStringLiteral("horse_swordsman");
  case TroopType::HorseArcher:
    return QStringLiteral("horse_archer");
  case TroopType::HorseSpearman:
    return QStringLiteral("horse_spearman");
  case TroopType::Healer:
    return QStringLiteral("healer");
  case TroopType::Catapult:
    return QStringLiteral("catapult");
  case TroopType::Ballista:
    return QStringLiteral("ballista");
  case TroopType::Elephant:
    return QStringLiteral("elephant");
  case TroopType::RomanLegionOrganizer:
    return QStringLiteral("roman_legion_organizer");
  case TroopType::RomanVeteranConsul:
    return QStringLiteral("roman_veteran_consul");
  case TroopType::RomanFieldCommander:
    return QStringLiteral("roman_field_commander");
  case TroopType::CarthageMercenaryBroker:
    return QStringLiteral("carthage_mercenary_broker");
  case TroopType::CarthageCavalryPatron:
    return QStringLiteral("carthage_cavalry_patron");
  case TroopType::CarthageElephantMaster:
    return QStringLiteral("carthage_elephant_master");
  case TroopType::Civilian:
    return QStringLiteral("civilian");
  case TroopType::Builder:
    return QStringLiteral("builder");
  }
  return QStringLiteral("archer");
}

inline auto troop_typeToString(TroopType type) -> std::string {
  return troop_typeToQString(type).toStdString();
}

inline auto try_parse_troop_type(const QString& value, TroopType& out) -> bool {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("archer")) {
    out = TroopType::Archer;
    return true;
  }
  if (lowered == QStringLiteral("swordsman") ||
      lowered == QStringLiteral("swordsman")) {
    out = TroopType::Swordsman;
    return true;
  }
  if (lowered == QStringLiteral("spearman")) {
    out = TroopType::Spearman;
    return true;
  }
  if (lowered == QStringLiteral("horse_swordsman") ||
      lowered == QStringLiteral("horseswordsman")) {
    out = TroopType::MountedKnight;
    return true;
  }
  if (lowered == QStringLiteral("horse_archer") ||
      lowered == QStringLiteral("horsearcher")) {
    out = TroopType::HorseArcher;
    return true;
  }
  if (lowered == QStringLiteral("horse_spearman") ||
      lowered == QStringLiteral("horsespearman")) {
    out = TroopType::HorseSpearman;
    return true;
  }
  if (lowered == QStringLiteral("healer")) {
    out = TroopType::Healer;
    return true;
  }
  if (lowered == QStringLiteral("catapult")) {
    out = TroopType::Catapult;
    return true;
  }
  if (lowered == QStringLiteral("ballista")) {
    out = TroopType::Ballista;
    return true;
  }
  if (lowered == QStringLiteral("elephant")) {
    out = TroopType::Elephant;
    return true;
  }
  if (lowered == QStringLiteral("roman_legion_organizer")) {
    out = TroopType::RomanLegionOrganizer;
    return true;
  }
  if (lowered == QStringLiteral("roman_veteran_consul")) {
    out = TroopType::RomanVeteranConsul;
    return true;
  }
  if (lowered == QStringLiteral("roman_field_commander")) {
    out = TroopType::RomanFieldCommander;
    return true;
  }
  if (lowered == QStringLiteral("carthage_mercenary_broker")) {
    out = TroopType::CarthageMercenaryBroker;
    return true;
  }
  if (lowered == QStringLiteral("carthage_cavalry_patron")) {
    out = TroopType::CarthageCavalryPatron;
    return true;
  }
  if (lowered == QStringLiteral("carthage_elephant_master")) {
    out = TroopType::CarthageElephantMaster;
    return true;
  }
  if (lowered == QStringLiteral("civilian")) {
    out = TroopType::Civilian;
    return true;
  }
  if (lowered == QStringLiteral("builder")) {
    out = TroopType::Builder;
    return true;
  }
  return false;
}

inline auto troop_typeFromString(const std::string& str) -> TroopType {
  TroopType result;
  if (try_parse_troop_type(QString::fromStdString(str), result)) {
    return result;
  }
  return TroopType::Archer;
}

inline auto try_parse_troop_type(const std::string& str) -> std::optional<TroopType> {
  TroopType result;
  if (try_parse_troop_type(QString::fromStdString(str), result)) {
    return result;
  }
  return std::nullopt;
}

[[nodiscard]] inline auto is_commander_troop(TroopType type) noexcept -> bool {
  switch (type) {
  case TroopType::RomanLegionOrganizer:
  case TroopType::RomanVeteranConsul:
  case TroopType::RomanFieldCommander:
  case TroopType::CarthageMercenaryBroker:
  case TroopType::CarthageCavalryPatron:
  case TroopType::CarthageElephantMaster:
    return true;
  default:
    return false;
  }
}

} // namespace Game::Units

namespace std {
template <>
struct hash<Game::Units::TroopType> {
  auto operator()(Game::Units::TroopType type) const -> size_t {
    return hash<int>()(static_cast<int>(type));
  }
};
} // namespace std
