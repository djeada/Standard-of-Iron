#pragma once

#include <QString>

#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <string>

#include "../systems/nation_id.h"

namespace Game::Units {

enum class TroopType {
  Archer,
  Swordsman,
  Spearman,
  SkeletonSwordsman,
  SkeletonArcher,
  GravePriest,
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
  CarthageSpearCommander,
  CarthageBowCommander,
  CarthageSwordCommander,
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
  case TroopType::SkeletonSwordsman:
    return QStringLiteral("skeleton_swordsman");
  case TroopType::SkeletonArcher:
    return QStringLiteral("skeleton_archer");
  case TroopType::GravePriest:
    return QStringLiteral("grave_priest");
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
  case TroopType::CarthageSpearCommander:
    return QStringLiteral("carthage_spear_commander");
  case TroopType::CarthageBowCommander:
    return QStringLiteral("carthage_bow_commander");
  case TroopType::CarthageSwordCommander:
    return QStringLiteral("carthage_sword_commander");
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
  if (lowered == QStringLiteral("skeleton_swordsman")) {
    out = TroopType::SkeletonSwordsman;
    return true;
  }
  if (lowered == QStringLiteral("skeleton_archer")) {
    out = TroopType::SkeletonArcher;
    return true;
  }
  if (lowered == QStringLiteral("grave_priest")) {
    out = TroopType::GravePriest;
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
  if (lowered == QStringLiteral("carthage_spear_commander")) {
    out = TroopType::CarthageSpearCommander;
    return true;
  }
  if (lowered == QStringLiteral("carthage_bow_commander")) {
    out = TroopType::CarthageBowCommander;
    return true;
  }
  if (lowered == QStringLiteral("carthage_sword_commander")) {
    out = TroopType::CarthageSwordCommander;
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
  case TroopType::CarthageSpearCommander:
  case TroopType::CarthageBowCommander:
  case TroopType::CarthageSwordCommander:
    return true;
  default:
    return false;
  }
}

[[nodiscard]] inline auto commander_troop_nation(TroopType type) noexcept
    -> std::optional<Game::Systems::NationID> {
  switch (type) {
  case TroopType::RomanLegionOrganizer:
  case TroopType::RomanVeteranConsul:
  case TroopType::RomanFieldCommander:
    return Game::Systems::NationID::RomanRepublic;
  case TroopType::CarthageSpearCommander:
  case TroopType::CarthageBowCommander:
  case TroopType::CarthageSwordCommander:
    return Game::Systems::NationID::Carthage;
  default:
    return std::nullopt;
  }
}

[[nodiscard]] inline auto default_commander_troop_for_nation(
    Game::Systems::NationID nation) noexcept -> TroopType {
  if (nation == Game::Systems::NationID::Carthage) {
    return TroopType::CarthageSwordCommander;
  }
  return TroopType::RomanVeteranConsul;
}

[[nodiscard]] inline auto
default_commander_troop_for_nation(const QString& nation) -> QString {
  Game::Systems::NationID nation_id = Game::Systems::NationID::RomanRepublic;
  Game::Systems::try_parse_nation_id(nation, nation_id);
  return troop_typeToQString(default_commander_troop_for_nation(nation_id));
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
