#pragma once

#include <QString>

#include <optional>
#include <string>

namespace Game::Units {

enum class BuildingType : std::uint8_t {
  Barracks,
  DefenseTower,
  Home,
  Marketplace
};

inline auto building_type_to_q_string(BuildingType type) -> QString {
  switch (type) {
  case BuildingType::Barracks:
    return QStringLiteral("barracks");
  case BuildingType::DefenseTower:
    return QStringLiteral("defense_tower");
  case BuildingType::Home:
    return QStringLiteral("home");
  case BuildingType::Marketplace:
    return QStringLiteral("marketplace");
  }

  return QStringLiteral("barracks");
}

inline auto building_type_to_string(BuildingType type) -> std::string {
  return building_type_to_q_string(type).toStdString();
}

inline auto try_parse_building_type(const QString& value, BuildingType& out) -> bool {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("barracks")) {
    out = BuildingType::Barracks;
    return true;
  }
  if (lowered == QStringLiteral("defense_tower")) {
    out = BuildingType::DefenseTower;
    return true;
  }
  if (lowered == QStringLiteral("home")) {
    out = BuildingType::Home;
    return true;
  }
  if (lowered == QStringLiteral("marketplace")) {
    out = BuildingType::Marketplace;
    return true;
  }
  return false;
}

inline auto
building_type_from_string(const std::string& str) -> std::optional<BuildingType> {
  BuildingType result;
  if (try_parse_building_type(QString::fromStdString(str), result)) {
    return result;
  }
  return std::nullopt;
}

} // namespace Game::Units
