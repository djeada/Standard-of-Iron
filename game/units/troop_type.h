#pragma once

#include <QString>
#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <string>

namespace Game::Units {

enum class TroopType { Archer, Knight, Spearman, MountedKnight };

inline QString troopTypeToQString(TroopType type) {
  switch (type) {
  case TroopType::Archer:
    return QStringLiteral("archer");
  case TroopType::Knight:
    return QStringLiteral("knight");
  case TroopType::Spearman:
    return QStringLiteral("spearman");
  case TroopType::MountedKnight:
    return QStringLiteral("mounted_knight");
  }
  return QStringLiteral("archer");
}

inline std::string troopTypeToString(TroopType type) {
  return troopTypeToQString(type).toStdString();
}

inline bool tryParseTroopType(const QString &value, TroopType &out) {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("archer")) {
    out = TroopType::Archer;
    return true;
  }
  if (lowered == QStringLiteral("knight")) {
    out = TroopType::Knight;
    return true;
  }
  if (lowered == QStringLiteral("spearman")) {
    out = TroopType::Spearman;
    return true;
  }
  if (lowered == QStringLiteral("mounted_knight") ||
      lowered == QStringLiteral("mountedknight")) {
    out = TroopType::MountedKnight;
    return true;
  }
  return false;
}

inline TroopType troopTypeFromString(const std::string &str) {
  TroopType result;
  if (tryParseTroopType(QString::fromStdString(str), result)) {
    return result;
  }
  return TroopType::Archer;
}

inline std::optional<TroopType> tryParseTroopType(const std::string &str) {
  TroopType result;
  if (tryParseTroopType(QString::fromStdString(str), result)) {
    return result;
  }
  return std::nullopt;
}

} // namespace Game::Units

namespace std {
template <> struct hash<Game::Units::TroopType> {
  size_t operator()(Game::Units::TroopType type) const {
    return hash<int>()(static_cast<int>(type));
  }
};
} // namespace std
