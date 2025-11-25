#pragma once

#include <QString>
#include <QVector3D>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Game::Map {

enum class TerrainType { Flat, Hill, Mountain, River };

enum class GroundType {
  ForestMud,    // Default: Deep green + mud ground (current style)
  GrassDry,     // Dry Mediterranean Grass
  SoilRocky,    // Light-Brown Rocky Soil
  AlpineMix,    // Alpine Rock + Snow Mix
  SoilFertile   // Dark Fertile Farmland Soil
};

inline auto groundTypeToQString(GroundType type) -> QString {
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

inline auto groundTypeToString(GroundType type) -> std::string {
  return groundTypeToQString(type).toStdString();
}

inline auto tryParseGroundType(const QString &value, GroundType &out) -> bool {
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
groundTypeFromString(const std::string &str) -> std::optional<GroundType> {
  GroundType result;
  if (tryParseGroundType(QString::fromStdString(str), result)) {
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
  }
  return QStringLiteral("flat");
}

inline auto terrainTypeToString(TerrainType type) -> std::string {
  return terrainTypeToQString(type).toStdString();
}

inline auto tryParseTerrainType(const QString &value,
                                TerrainType &out) -> bool {
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
  return false;
}

inline auto
terrainTypeFromString(const std::string &str) -> std::optional<TerrainType> {
  TerrainType result;
  if (tryParseTerrainType(QString::fromStdString(str), result)) {
    return result;
  }
  return std::nullopt;
}

struct BiomeSettings {
  GroundType groundType = GroundType::ForestMud;
  QVector3D grassPrimary{0.30F, 0.60F, 0.28F};
  QVector3D grassSecondary{0.44F, 0.70F, 0.32F};
  QVector3D grassDry{0.72F, 0.66F, 0.48F};
  QVector3D soilColor{0.28F, 0.24F, 0.18F};
  QVector3D rockLow{0.48F, 0.46F, 0.44F};
  QVector3D rockHigh{0.68F, 0.69F, 0.73F};
  float patchDensity = 4.5F;
  float patchJitter = 0.95F;
  float backgroundBladeDensity = 0.65F;
  float bladeHeightMin = 0.55F;
  float bladeHeightMax = 1.35F;
  float bladeWidthMin = 0.025F;
  float bladeWidthMax = 0.055F;
  float sway_strength = 0.25F;
  float sway_speed = 1.4F;
  float heightNoiseAmplitude = 0.16F;
  float heightNoiseFrequency = 0.05F;
  float terrainMacroNoiseScale = 0.035F;
  float terrainDetailNoiseScale = 0.14F;
  float terrainSoilHeight = 0.6F;
  float terrainSoilSharpness = 3.8F;
  float terrainRockThreshold = 0.42F;
  float terrainRockSharpness = 3.1F;
  float terrainAmbientBoost = 1.08F;
  float terrainRockDetailStrength = 0.35F;
  float backgroundSwayVariance = 0.2F;
  float backgroundScatterRadius = 0.35F;
  float plant_density = 0.5F;
  float spawnEdgePadding = 0.08F;
  std::uint32_t seed = 1337U;
  bool groundIrregularityEnabled = true;
  float irregularityScale = 0.15F;
  float irregularityAmplitude = 0.08F;
};

inline void applyGroundTypeDefaults(BiomeSettings &settings,
                                    GroundType groundType) {
  settings.groundType = groundType;
  switch (groundType) {
  case GroundType::ForestMud:
    // Default: Deep green + mud ground (current style)
    settings.grassPrimary = QVector3D(0.30F, 0.60F, 0.28F);
    settings.grassSecondary = QVector3D(0.44F, 0.70F, 0.32F);
    settings.grassDry = QVector3D(0.72F, 0.66F, 0.48F);
    settings.soilColor = QVector3D(0.28F, 0.24F, 0.18F);
    settings.rockLow = QVector3D(0.48F, 0.46F, 0.44F);
    settings.rockHigh = QVector3D(0.68F, 0.69F, 0.73F);
    settings.terrainAmbientBoost = 1.08F;
    settings.terrainRockDetailStrength = 0.35F;
    break;
  case GroundType::GrassDry:
    // Dry Mediterranean Grass
    settings.grassPrimary = QVector3D(0.55F, 0.52F, 0.30F);
    settings.grassSecondary = QVector3D(0.62F, 0.58F, 0.35F);
    settings.grassDry = QVector3D(0.75F, 0.68F, 0.42F);
    settings.soilColor = QVector3D(0.45F, 0.38F, 0.28F);
    settings.rockLow = QVector3D(0.58F, 0.55F, 0.50F);
    settings.rockHigh = QVector3D(0.72F, 0.70F, 0.65F);
    settings.terrainAmbientBoost = 1.15F;
    settings.terrainRockDetailStrength = 0.30F;
    break;
  case GroundType::SoilRocky:
    // Light-Brown Rocky Soil
    settings.grassPrimary = QVector3D(0.42F, 0.48F, 0.30F);
    settings.grassSecondary = QVector3D(0.50F, 0.54F, 0.35F);
    settings.grassDry = QVector3D(0.60F, 0.55F, 0.40F);
    settings.soilColor = QVector3D(0.50F, 0.42F, 0.32F);
    settings.rockLow = QVector3D(0.55F, 0.52F, 0.48F);
    settings.rockHigh = QVector3D(0.70F, 0.68F, 0.65F);
    settings.terrainAmbientBoost = 1.05F;
    settings.terrainRockDetailStrength = 0.55F;
    break;
  case GroundType::AlpineMix:
    // Alpine Rock + Snow Mix
    settings.grassPrimary = QVector3D(0.35F, 0.42F, 0.32F);
    settings.grassSecondary = QVector3D(0.40F, 0.48F, 0.38F);
    settings.grassDry = QVector3D(0.55F, 0.52F, 0.45F);
    settings.soilColor = QVector3D(0.38F, 0.35F, 0.32F);
    settings.rockLow = QVector3D(0.60F, 0.62F, 0.65F);
    settings.rockHigh = QVector3D(0.85F, 0.88F, 0.92F);
    settings.terrainAmbientBoost = 1.20F;
    settings.terrainRockDetailStrength = 0.45F;
    break;
  case GroundType::SoilFertile:
    // Dark Fertile Farmland Soil
    settings.grassPrimary = QVector3D(0.28F, 0.52F, 0.25F);
    settings.grassSecondary = QVector3D(0.38F, 0.62F, 0.32F);
    settings.grassDry = QVector3D(0.55F, 0.50F, 0.35F);
    settings.soilColor = QVector3D(0.22F, 0.18F, 0.14F);
    settings.rockLow = QVector3D(0.42F, 0.40F, 0.38F);
    settings.rockHigh = QVector3D(0.55F, 0.54F, 0.52F);
    settings.terrainAmbientBoost = 1.02F;
    settings.terrainRockDetailStrength = 0.25F;
    break;
  }
}

struct TerrainFeature {
  TerrainType type;
  float center_x{};
  float center_z{};
  float radius{};
  float width{};
  float depth{};
  float height{};

  std::vector<QVector3D> entrances;

  float rotationDeg = 0.0F;
};

struct RiverSegment {
  QVector3D start;
  QVector3D end;
  float width = 2.0F;
};

struct Bridge {
  QVector3D start;
  QVector3D end;
  float width = 3.0F;
  float height = 0.5F;
};

class TerrainHeightMap {
public:
  TerrainHeightMap(int width, int height, float tile_size);

  void buildFromFeatures(const std::vector<TerrainFeature> &features);

  void addRiverSegments(const std::vector<RiverSegment> &riverSegments);

  [[nodiscard]] auto getHeightAt(float world_x, float world_z) const -> float;

  [[nodiscard]] auto getHeightAtGrid(int grid_x, int grid_z) const -> float;

  [[nodiscard]] auto isWalkable(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto isHillEntrance(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto getTerrainType(int grid_x,
                                    int grid_z) const -> TerrainType;

  [[nodiscard]] auto isRiverOrNearby(int grid_x, int grid_z,
                                     int margin = 1) const -> bool;

  [[nodiscard]] auto getWidth() const -> int { return m_width; }
  [[nodiscard]] auto getHeight() const -> int { return m_height; }
  [[nodiscard]] auto getTileSize() const -> float { return m_tile_size; }

  [[nodiscard]] auto getHeightData() const -> const std::vector<float> & {
    return m_heights;
  }
  [[nodiscard]] auto
  getTerrainTypes() const -> const std::vector<TerrainType> & {
    return m_terrain_types;
  }
  [[nodiscard]] auto
  getRiverSegments() const -> const std::vector<RiverSegment> & {
    return m_riverSegments;
  }

  void addBridges(const std::vector<Bridge> &bridges);
  [[nodiscard]] auto getBridges() const -> const std::vector<Bridge> & {
    return m_bridges;
  }

  void applyBiomeVariation(const BiomeSettings &settings);

  void restoreFromData(const std::vector<float> &heights,
                       const std::vector<TerrainType> &terrain_types,
                       const std::vector<RiverSegment> &rivers,
                       const std::vector<Bridge> &bridges);

private:
  int m_width;
  int m_height;
  float m_tile_size;

  std::vector<float> m_heights;
  std::vector<TerrainType> m_terrain_types;
  std::vector<bool> m_hillEntrances;
  std::vector<bool> m_hillWalkable;
  std::vector<RiverSegment> m_riverSegments;
  std::vector<Bridge> m_bridges;

  [[nodiscard]] auto indexAt(int x, int z) const -> int;
  [[nodiscard]] auto inBounds(int x, int z) const -> bool;

  [[nodiscard]] static auto
  calculateFeatureHeight(const TerrainFeature &feature, float world_x,
                         float world_z) -> float;
};

} // namespace Game::Map
