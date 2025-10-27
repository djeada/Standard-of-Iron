#pragma once

#include <QString>
#include <algorithm>
#include <cctype>
#include <functional>
#include <optional>
#include <string>

namespace Game::Units {

enum class TroopType { Archer, Knight, Spearman, MountedKnight };

inline auto troop_typeToQString(TroopType type) -> QString {
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

inline auto troop_typeToString(TroopType type) -> std::string {
  return troop_typeToQString(type).toStdString();
}

inline auto tryParseTroopType(const QString &value, TroopType &out) -> bool {
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
