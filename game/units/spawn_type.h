#pragma once

#include "troop_type.h"
#include <QString>
#include <optional>
#include <string>

namespace Game::Units {

enum class SpawnType { Archer, Knight, Spearman, MountedKnight, Barracks };

inline QString spawnTypeToQString(SpawnType type) {
  switch (type) {
  case SpawnType::Archer:
    return QStringLiteral("archer");
  case SpawnType::Knight:
    return QStringLiteral("knight");
  case SpawnType::Spearman:
    return QStringLiteral("spearman");
  case SpawnType::MountedKnight:
    return QStringLiteral("mounted_knight");
  case SpawnType::Barracks:
    return QStringLiteral("barracks");
  }
  return QStringLiteral("archer");
}

inline std::string spawnTypeToString(SpawnType type) {
  return spawnTypeToQString(type).toStdString();
}

inline bool tryParseSpawnType(const QString &value, SpawnType &out) {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("archer")) {
    out = SpawnType::Archer;
    return true;
  }
  if (lowered == QStringLiteral("knight")) {
    out = SpawnType::Knight;
    return true;
  }
  if (lowered == QStringLiteral("spearman")) {
    out = SpawnType::Spearman;
    return true;
  }
  if (lowered == QStringLiteral("mounted_knight")) {
    out = SpawnType::MountedKnight;
    return true;
  }
  if (lowered == QStringLiteral("barracks")) {
    out = SpawnType::Barracks;
    return true;
  }
  return false;
}

inline bool isTroopSpawn(SpawnType type) {
  return type != SpawnType::Barracks;
}

inline std::optional<TroopType> spawnTypeToTroopType(SpawnType type) {
  switch (type) {
  case SpawnType::Archer:
    return TroopType::Archer;
  case SpawnType::Knight:
    return TroopType::Knight;
  case SpawnType::Spearman:
    return TroopType::Spearman;
  case SpawnType::MountedKnight:
    return TroopType::MountedKnight;
  case SpawnType::Barracks:
    return std::nullopt;
  }
  return std::nullopt;
}

inline SpawnType spawnTypeFromTroopType(TroopType type) {
  switch (type) {
  case TroopType::Archer:
    return SpawnType::Archer;
  case TroopType::Knight:
    return SpawnType::Knight;
  case TroopType::Spearman:
    return SpawnType::Spearman;
  case TroopType::MountedKnight:
    return SpawnType::MountedKnight;
  }
  return SpawnType::Archer;
}

} // namespace Game::Units

namespace std {
template <> struct hash<Game::Units::SpawnType> {
  size_t operator()(Game::Units::SpawnType type) const noexcept {
    return hash<int>()(static_cast<int>(type));
  }
};
} // namespace std
