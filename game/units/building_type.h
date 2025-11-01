#pragma once

#include <QString>
#include <optional>
#include <string>

namespace Game::Units {

enum class BuildingType : std::uint8_t { Barracks };

inline auto buildingTypeToQString(BuildingType type) -> QString {
  switch (type) {
  case BuildingType::Barracks:
    return QStringLiteral("barracks");
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
