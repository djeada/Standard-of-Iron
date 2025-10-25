#pragma once

#include <QVector3D>
#include <QString>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace Game::Map {

enum class TerrainType { Flat, Hill, Mountain, River };

// String conversion utilities for TerrainType
inline QString terrainTypeToQString(TerrainType type) {
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

inline std::string terrainTypeToString(TerrainType type) {
  return terrainTypeToQString(type).toStdString();
}

// Case-insensitive parsing with validation
inline bool tryParseTerrainType(const QString &value, TerrainType &out) {
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

// For std::string compatibility
inline std::optional<TerrainType> terrainTypeFromString(const std::string &str) {
  TerrainType result;
  if (tryParseTerrainType(QString::fromStdString(str), result)) {
    return result;
  }
  return std::nullopt;
}

struct BiomeSettings {
  QVector3D grassPrimary{0.30f, 0.60f, 0.28f};
  QVector3D grassSecondary{0.44f, 0.70f, 0.32f};
  QVector3D grassDry{0.72f, 0.66f, 0.48f};
  QVector3D soilColor{0.28f, 0.24f, 0.18f};
  QVector3D rockLow{0.48f, 0.46f, 0.44f};
  QVector3D rockHigh{0.68f, 0.69f, 0.73f};
  float patchDensity = 4.5f;
  float patchJitter = 0.95f;
  float backgroundBladeDensity = 0.65f;
  float bladeHeightMin = 0.55f;
  float bladeHeightMax = 1.35f;
  float bladeWidthMin = 0.025f;
  float bladeWidthMax = 0.055f;
  float swayStrength = 0.25f;
  float swaySpeed = 1.4f;
  float heightNoiseAmplitude = 0.16f;
  float heightNoiseFrequency = 0.05f;
  float terrainMacroNoiseScale = 0.035f;
  float terrainDetailNoiseScale = 0.14f;
  float terrainSoilHeight = 0.6f;
  float terrainSoilSharpness = 3.8f;
  float terrainRockThreshold = 0.42f;
  float terrainRockSharpness = 3.1f;
  float terrainAmbientBoost = 1.08f;
  float terrainRockDetailStrength = 0.35f;
  float backgroundSwayVariance = 0.2f;
  float backgroundScatterRadius = 0.35f;
  float plantDensity = 0.5f;
  float spawnEdgePadding = 0.08f;
  std::uint32_t seed = 1337u;
};

struct TerrainFeature {
  TerrainType type;
  float centerX;
  float centerZ;
  float radius;
  float width;
  float depth;
  float height;

  std::vector<QVector3D> entrances;

  float rotationDeg = 0.0f;
};

struct RiverSegment {
  QVector3D start;
  QVector3D end;
  float width = 2.0f;
};

struct Bridge {
  QVector3D start;
  QVector3D end;
  float width = 3.0f;
  float height = 0.5f;
};

class TerrainHeightMap {
public:
  TerrainHeightMap(int width, int height, float tileSize);

  void buildFromFeatures(const std::vector<TerrainFeature> &features);

  void addRiverSegments(const std::vector<RiverSegment> &riverSegments);

  float getHeightAt(float worldX, float worldZ) const;

  float getHeightAtGrid(int gridX, int gridZ) const;

  bool isWalkable(int gridX, int gridZ) const;

  bool isHillEntrance(int gridX, int gridZ) const;

  TerrainType getTerrainType(int gridX, int gridZ) const;

  bool isRiverOrNearby(int gridX, int gridZ, int margin = 1) const;

  int getWidth() const { return m_width; }
  int getHeight() const { return m_height; }
  float getTileSize() const { return m_tileSize; }

  const std::vector<float> &getHeightData() const { return m_heights; }
  const std::vector<TerrainType> &getTerrainTypes() const {
    return m_terrainTypes;
  }
  const std::vector<RiverSegment> &getRiverSegments() const {
    return m_riverSegments;
  }

  void addBridges(const std::vector<Bridge> &bridges);
  const std::vector<Bridge> &getBridges() const { return m_bridges; }

  void applyBiomeVariation(const BiomeSettings &settings);

  void restoreFromData(const std::vector<float> &heights,
                       const std::vector<TerrainType> &terrainTypes,
                       const std::vector<RiverSegment> &rivers,
                       const std::vector<Bridge> &bridges);

private:
  int m_width;
  int m_height;
  float m_tileSize;

  std::vector<float> m_heights;
  std::vector<TerrainType> m_terrainTypes;
  std::vector<bool> m_hillEntrances;
  std::vector<bool> m_hillWalkable;
  std::vector<RiverSegment> m_riverSegments;
  std::vector<Bridge> m_bridges;

  int indexAt(int x, int z) const;
  bool inBounds(int x, int z) const;

  float calculateFeatureHeight(const TerrainFeature &feature, float worldX,
                               float worldZ) const;
};

} // namespace Game::Map
