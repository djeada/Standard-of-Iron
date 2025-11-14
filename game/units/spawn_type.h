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
  Barracks
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
  case SpawnType::Barracks:
    return QStringLiteral("barracks");
  }
  return QStringLiteral("archer");
}

inline auto spawn_typeToString(SpawnType type) -> std::string {
  return spawn_typeToQString(type).toStdString();
}

inline auto tryParseSpawnType(const QString &value, SpawnType &out) -> bool {
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
  if (lowered == QStringLiteral("barracks")) {
    out = SpawnType::Barracks;
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
  if (str == "barracks") {
    return SpawnType::Barracks;
  }
  return std::nullopt;
}

inline auto isTroopSpawn(SpawnType type) -> bool {
  return type != SpawnType::Barracks;
}

inline auto isBuildingSpawn(SpawnType type) -> bool {
  return type == SpawnType::Barracks;
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
  case SpawnType::Barracks:
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
