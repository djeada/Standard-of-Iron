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
  case TroopType::Builder:
    return QStringLiteral("builder");
  }
  return QStringLiteral("archer");
}

inline auto troop_typeToString(TroopType type) -> std::string {
  return troop_typeToQString(type).toStdString();
}

inline auto tryParseTroopType(const QString &value, TroopType &out) -> bool {
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
  if (lowered == QStringLiteral("builder")) {
    out = TroopType::Builder;
    return true;
  }
  return false;
}

inline auto troop_typeFromString(const std::string &str) -> TroopType {
  TroopType result;
  if (tryParseTroopType(QString::fromStdString(str), result)) {
    return result;
  }
  return TroopType::Archer;
}

inline auto
tryParseTroopType(const std::string &str) -> std::optional<TroopType> {
  TroopType result;
  if (tryParseTroopType(QString::fromStdString(str), result)) {
    return result;
  }
  return std::nullopt;
}

} 

namespace std {
template <> struct hash<Game::Units::TroopType> {
  auto operator()(Game::Units::TroopType type) const -> size_t {
    return hash<int>()(static_cast<int>(type));
  }
};
} 
