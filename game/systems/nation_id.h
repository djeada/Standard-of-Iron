#pragma once

#include <QString>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

namespace Game::Systems {

enum class NationID : std::uint8_t {
  RomanRepublic,
  Carthage,
  IronSepulcher,
  Gauls,
  Iberians
};

inline auto nation_id_to_qstring(NationID id) -> QString {
  switch (id) {
  case NationID::RomanRepublic:
    return QStringLiteral("roman_republic");
  case NationID::Carthage:
    return QStringLiteral("carthage");
  case NationID::IronSepulcher:
    return QStringLiteral("iron_sepulcher");
  case NationID::Gauls:
    return QStringLiteral("gauls");
  case NationID::Iberians:
    return QStringLiteral("iberians");
  }

  return QStringLiteral("roman_republic");
}

inline auto nation_id_to_string(NationID id) -> std::string {
  return nation_id_to_qstring(id).toStdString();
}

inline auto try_parse_nation_id(const QString& value, NationID& out) -> bool {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("roman_republic")) {
    out = NationID::RomanRepublic;
    return true;
  }
  if (lowered == QStringLiteral("carthage")) {
    out = NationID::Carthage;
    return true;
  }
  if (lowered == QStringLiteral("iron_sepulcher")) {
    out = NationID::IronSepulcher;
    return true;
  }
  if (lowered == QStringLiteral("gauls")) {
    out = NationID::Gauls;
    return true;
  }
  if (lowered == QStringLiteral("iberians")) {
    out = NationID::Iberians;
    return true;
  }
  return false;
}

inline auto authored_nation_or(const QString& authored, NationID fallback) -> NationID {
  if (authored.trimmed().isEmpty()) {
    return fallback;
  }
  NationID parsed{};
  return try_parse_nation_id(authored, parsed) ? parsed : fallback;
}

inline auto nation_id_from_string(const std::string& str) -> std::optional<NationID> {
  NationID result;
  if (try_parse_nation_id(QString::fromStdString(str), result)) {
    return result;
  }
  return std::nullopt;
}

} // namespace Game::Systems

namespace std {
template <>
struct hash<Game::Systems::NationID> {
  auto operator()(Game::Systems::NationID id) const noexcept -> size_t {
    return hash<std::uint8_t>()(static_cast<std::uint8_t>(id));
  }
};
} // namespace std
