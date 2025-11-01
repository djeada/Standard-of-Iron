#pragma once

#include <QString>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace Game::Systems {

enum class NationID : std::uint8_t { KingdomOfIron, RomanRepublic, Carthage };

inline auto nationIDToQString(NationID id) -> QString {
  switch (id) {
  case NationID::KingdomOfIron:
    return QStringLiteral("kingdom_of_iron");
  case NationID::RomanRepublic:
    return QStringLiteral("roman_republic");
  case NationID::Carthage:
    return QStringLiteral("carthage");
  }

  return QStringLiteral("kingdom_of_iron");
}

inline auto nationIDToString(NationID id) -> std::string {
  return nationIDToQString(id).toStdString();
}

inline auto tryParseNationID(const QString &value, NationID &out) -> bool {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("kingdom_of_iron")) {
    out = NationID::KingdomOfIron;
    return true;
  }
  if (lowered == QStringLiteral("roman_republic")) {
    out = NationID::RomanRepublic;
    return true;
  }
  if (lowered == QStringLiteral("carthage")) {
    out = NationID::Carthage;
    return true;
  }
  return false;
}

inline auto
nationIDFromString(const std::string &str) -> std::optional<NationID> {
  NationID result;
  if (tryParseNationID(QString::fromStdString(str), result)) {
    return result;
  }
  return std::nullopt;
}

} // namespace Game::Systems

namespace std {
template <> struct hash<Game::Systems::NationID> {
  auto operator()(Game::Systems::NationID id) const noexcept -> size_t {
    return hash<std::uint8_t>()(static_cast<std::uint8_t>(id));
  }
};
} // namespace std
