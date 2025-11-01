#pragma once

#include <QString>
#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <string>

namespace Game::Units {

enum class TroopType { Archer, Swordsman, Spearman, MountedKnight };

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

} // namespace Game::Units

namespace std {
template <> struct hash<Game::Units::TroopType> {
  auto operator()(Game::Units::TroopType type) const -> size_t {
    return hash<int>()(static_cast<int>(type));
  }
};
} // namespace std
