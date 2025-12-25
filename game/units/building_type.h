#pragma once

#include <QString>
#include <optional>
#include <string>

namespace Game::Units {

enum class BuildingType : std::uint8_t { Barracks, DefenseTower, Home };

inline auto buildingTypeToQString(BuildingType type) -> QString {
  switch (type) {
  case BuildingType::Barracks:
    return QStringLiteral("barracks");
  case BuildingType::DefenseTower:
    return QStringLiteral("defense_tower");
  case BuildingType::Home:
    return QStringLiteral("home");
  }

  return QStringLiteral("barracks");
}

inline auto buildingTypeToString(BuildingType type) -> std::string {
  return buildingTypeToQString(type).toStdString();
}

inline auto tryParseBuildingType(const QString &value,
                                 BuildingType &out) -> bool {
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
  return false;
}

inline auto
buildingTypeFromString(const std::string &str) -> std::optional<BuildingType> {
  BuildingType result;
  if (tryParseBuildingType(QString::fromStdString(str), result)) {
    return result;
  }
  return std::nullopt;
}

} // namespace Game::Units
