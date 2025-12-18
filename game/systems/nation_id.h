#pragma once

#include <QString>
#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace Game::Systems {

enum class NationID : std::uint8_t { RomanRepublic, Carthage };

inline auto nation_id_to_qstring(NationID id) -> QString {
  switch (id) {
  case NationID::RomanRepublic:
    return QStringLiteral("roman_republic");
  case NationID::Carthage:
    return QStringLiteral("carthage");
  }

  return QStringLiteral("roman_republic");
}

inline auto nation_id_to_string(NationID id) -> std::string {
  return nation_id_to_qstring(id).toStdString();
}

inline auto try_parse_nation_id(const QString &value, NationID &out) -> bool {
  const QString lowered = value.trimmed().toLower();
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
nation_id_from_string(const std::string &str) -> std::optional<NationID> {
  NationID result;
  if (try_parse_nation_id(QString::fromStdString(str), result)) {
    return result;
  }
  return std::nullopt;
}

} 

namespace std {
template <> struct hash<Game::Systems::NationID> {
  auto operator()(Game::Systems::NationID id) const noexcept -> size_t {
    return hash<std::uint8_t>()(static_cast<std::uint8_t>(id));
  }
};
} 
