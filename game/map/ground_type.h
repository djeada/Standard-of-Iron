#pragma once

#include <QString>

#include <cstdint>
#include <optional>
#include <string>

namespace Game::Map {

enum class TerrainType {
  Flat,
  Hill,
  Mountain,
  River,
  Forest,
  Lake
};

[[nodiscard]] constexpr auto is_water_terrain(TerrainType type) noexcept -> bool {
  return type == TerrainType::River || type == TerrainType::Lake;
}

enum class GroundType {
  ForestMud,
  GrassDry,
  SoilRocky,
  AlpineMix,
  SoilFertile
};

inline auto ground_type_to_qstring(GroundType type) -> QString {
  switch (type) {
  case GroundType::ForestMud:
    return QStringLiteral("forest_mud");
  case GroundType::GrassDry:
    return QStringLiteral("grass_dry");
  case GroundType::SoilRocky:
    return QStringLiteral("soil_rocky");
  case GroundType::AlpineMix:
    return QStringLiteral("alpine_mix");
  case GroundType::SoilFertile:
    return QStringLiteral("soil_fertile");
  }
  return QStringLiteral("forest_mud");
}

inline auto ground_type_to_string(GroundType type) -> std::string {
  return ground_type_to_qstring(type).toStdString();
}

inline auto try_parse_ground_type(const QString& value, GroundType& out) -> bool {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("forest_mud")) {
    out = GroundType::ForestMud;
    return true;
  }
  if (lowered == QStringLiteral("grass_dry")) {
    out = GroundType::GrassDry;
    return true;
  }
  if (lowered == QStringLiteral("soil_rocky")) {
    out = GroundType::SoilRocky;
    return true;
  }
  if (lowered == QStringLiteral("alpine_mix")) {
    out = GroundType::AlpineMix;
    return true;
  }
  if (lowered == QStringLiteral("soil_fertile")) {
    out = GroundType::SoilFertile;
    return true;
  }
  return false;
}

inline auto
ground_type_from_string(const std::string& str) -> std::optional<GroundType> {
  GroundType result;
  if (try_parse_ground_type(QString::fromStdString(str), result)) {
    return result;
  }
  return std::nullopt;
}

inline auto terrainTypeToQString(TerrainType type) -> QString {
  switch (type) {
  case TerrainType::Flat:
    return QStringLiteral("flat");
  case TerrainType::Hill:
    return QStringLiteral("hill");
  case TerrainType::Mountain:
    return QStringLiteral("mountain");
  case TerrainType::River:
    return QStringLiteral("river");
  case TerrainType::Forest:
    return QStringLiteral("forest");
  case TerrainType::Lake:
    return QStringLiteral("lake");
  }
  return QStringLiteral("flat");
}

inline auto terrain_type_to_string(TerrainType type) -> std::string {
  return terrainTypeToQString(type).toStdString();
}

inline auto try_parse_terrain_type(const QString& value, TerrainType& out) -> bool {
  const QString lowered = value.trimmed().toLower();
  if (lowered == QStringLiteral("flat")) {
    out = TerrainType::Flat;
    return true;
  }
  if (lowered == QStringLiteral("hill")) {
    out = TerrainType::Hill;
    return true;
  }
  if (lowered == QStringLiteral("mountain")) {
    out = TerrainType::Mountain;
    return true;
  }
  if (lowered == QStringLiteral("river")) {
    out = TerrainType::River;
    return true;
  }
  if (lowered == QStringLiteral("forest")) {
    out = TerrainType::Forest;
    return true;
  }
  if (lowered == QStringLiteral("lake")) {
    out = TerrainType::Lake;
    return true;
  }
  return false;
}

inline auto
terrainTypeFromString(const std::string& str) -> std::optional<TerrainType> {
  TerrainType result;
  if (try_parse_terrain_type(QString::fromStdString(str), result)) {
    return result;
  }
  return std::nullopt;
}

} // namespace Game::Map
