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

  // Ground-type-specific shader parameters
  float snowCoverage = 0.0F;      // 0-1: snow accumulation (alpine_mix)
  float moistureLevel = 0.5F;     // 0-1: wetness/dryness (affects soil/grass)
  float crackIntensity = 0.0F;    // 0-1: ground cracking (grass_dry)
  float rockExposure = 0.3F;      // 0-1: how much rock shows through (soil_rocky)
  float grassSaturation = 1.0F;   // 0-1.5: grass color intensity
  float soilRoughness = 0.5F;     // 0-1: soil texture roughness
  QVector3D snowColor{0.92F, 0.94F, 0.98F}; // Snow tint color (alpine_mix)
};

inline void applyGroundTypeDefaults(BiomeSettings &settings,
                                    GroundType groundType) {
  settings.groundType = groundType;
  switch (groundType) {
  case GroundType::ForestMud:
    // Default: Deep green + mud ground (current style)
    // Lush vegetation with wet, muddy soil in shaded forest areas
    settings.grassPrimary = QVector3D(0.30F, 0.60F, 0.28F);
    settings.grassSecondary = QVector3D(0.44F, 0.70F, 0.32F);
    settings.grassDry = QVector3D(0.72F, 0.66F, 0.48F);
    settings.soilColor = QVector3D(0.28F, 0.24F, 0.18F);
    settings.rockLow = QVector3D(0.48F, 0.46F, 0.44F);
    settings.rockHigh = QVector3D(0.68F, 0.69F, 0.73F);
    settings.terrainAmbientBoost = 1.08F;
    settings.terrainRockDetailStrength = 0.35F;
    // Grass/vegetation properties - tall lush grass
    settings.patchDensity = 4.5F;
    settings.patchJitter = 0.95F;
    settings.backgroundBladeDensity = 0.70F;
    settings.bladeHeightMin = 0.60F;
    settings.bladeHeightMax = 1.40F;
    settings.bladeWidthMin = 0.028F;
    settings.bladeWidthMax = 0.058F;
    settings.sway_strength = 0.28F;
    settings.sway_speed = 1.3F;
    // Terrain noise - moderate variation with mud patches
    settings.terrainMacroNoiseScale = 0.035F;
    settings.terrainDetailNoiseScale = 0.14F;
    settings.terrainSoilHeight = 0.65F;
    settings.terrainSoilSharpness = 3.5F;
    settings.terrainRockThreshold = 0.48F;
    settings.terrainRockSharpness = 3.2F;
    // Ground irregularity - moderate for forest floor
    settings.groundIrregularityEnabled = true;
    settings.irregularityScale = 0.15F;
    settings.irregularityAmplitude = 0.09F;
    settings.plant_density = 0.60F;
    // Ground-type-specific shader parameters - forest_mud
    settings.snowCoverage = 0.0F;
    settings.moistureLevel = 0.70F;
    settings.crackIntensity = 0.0F;
    settings.rockExposure = 0.25F;
    settings.grassSaturation = 1.05F;
    settings.soilRoughness = 0.55F;
    settings.snowColor = QVector3D(0.92F, 0.94F, 0.98F);
    break;

  case GroundType::GrassDry:
    // Dry Mediterranean Grass
    // Sparse, yellowed grass with dusty exposed soil and cracked earth
    settings.grassPrimary = QVector3D(0.58F, 0.54F, 0.32F);
    settings.grassSecondary = QVector3D(0.65F, 0.60F, 0.38F);
    settings.grassDry = QVector3D(0.78F, 0.72F, 0.45F);
    settings.soilColor = QVector3D(0.52F, 0.44F, 0.32F);
    settings.rockLow = QVector3D(0.62F, 0.58F, 0.52F);
    settings.rockHigh = QVector3D(0.78F, 0.75F, 0.70F);
    settings.terrainAmbientBoost = 1.18F;
    settings.terrainRockDetailStrength = 0.28F;
    // Grass/vegetation properties - short sparse dry grass
    settings.patchDensity = 2.8F;
    settings.patchJitter = 0.75F;
    settings.backgroundBladeDensity = 0.35F;
    settings.bladeHeightMin = 0.35F;
    settings.bladeHeightMax = 0.80F;
    settings.bladeWidthMin = 0.018F;
    settings.bladeWidthMax = 0.038F;
    settings.sway_strength = 0.15F;
    settings.sway_speed = 1.8F;
    // Terrain noise - less variation, more cracked appearance
    settings.terrainMacroNoiseScale = 0.028F;
    settings.terrainDetailNoiseScale = 0.22F;
    settings.terrainSoilHeight = 0.50F;
    settings.terrainSoilSharpness = 4.5F;
    settings.terrainRockThreshold = 0.38F;
    settings.terrainRockSharpness = 2.8F;
    // Ground irregularity - low for dry packed earth
    settings.groundIrregularityEnabled = true;
    settings.irregularityScale = 0.10F;
    settings.irregularityAmplitude = 0.04F;
    settings.plant_density = 0.25F;
    // Ground-type-specific shader parameters - grass_dry
    settings.snowCoverage = 0.0F;
    settings.moistureLevel = 0.15F;
    settings.crackIntensity = 0.65F;
    settings.rockExposure = 0.35F;
    settings.grassSaturation = 0.75F;
    settings.soilRoughness = 0.72F;
    settings.snowColor = QVector3D(0.92F, 0.94F, 0.98F);
    break;

  case GroundType::SoilRocky:
    // Light-Brown Rocky Soil
    // Exposed rocks with thin soil cover, mountain foothills aesthetic
    settings.grassPrimary = QVector3D(0.40F, 0.45F, 0.28F);
    settings.grassSecondary = QVector3D(0.48F, 0.52F, 0.32F);
    settings.grassDry = QVector3D(0.58F, 0.52F, 0.38F);
    settings.soilColor = QVector3D(0.55F, 0.48F, 0.38F);
    settings.rockLow = QVector3D(0.52F, 0.50F, 0.46F);
    settings.rockHigh = QVector3D(0.72F, 0.70F, 0.66F);
    settings.terrainAmbientBoost = 1.05F;
    settings.terrainRockDetailStrength = 0.65F;
    // Grass/vegetation properties - sparse clumps between rocks
    settings.patchDensity = 2.2F;
    settings.patchJitter = 0.60F;
    settings.backgroundBladeDensity = 0.28F;
    settings.bladeHeightMin = 0.30F;
    settings.bladeHeightMax = 0.70F;
    settings.bladeWidthMin = 0.020F;
    settings.bladeWidthMax = 0.040F;
    settings.sway_strength = 0.18F;
    settings.sway_speed = 1.5F;
    // Terrain noise - high detail for rocky texture
    settings.terrainMacroNoiseScale = 0.055F;
    settings.terrainDetailNoiseScale = 0.28F;
    settings.terrainSoilHeight = 0.40F;
    settings.terrainSoilSharpness = 5.0F;
    settings.terrainRockThreshold = 0.28F;
    settings.terrainRockSharpness = 4.0F;
    // Ground irregularity - high for rocky terrain
    settings.groundIrregularityEnabled = true;
    settings.irregularityScale = 0.22F;
    settings.irregularityAmplitude = 0.14F;
    settings.plant_density = 0.18F;
    // Ground-type-specific shader parameters - soil_rocky
    settings.snowCoverage = 0.0F;
    settings.moistureLevel = 0.35F;
    settings.crackIntensity = 0.25F;
    settings.rockExposure = 0.75F;
    settings.grassSaturation = 0.85F;
    settings.soilRoughness = 0.85F;
    settings.snowColor = QVector3D(0.92F, 0.94F, 0.98F);
    break;

  case GroundType::AlpineMix:
    // Alpine Rock + Snow Mix
    // High altitude with snow patches, lichen-covered rocks, hardy grass
    settings.grassPrimary = QVector3D(0.32F, 0.40F, 0.30F);
    settings.grassSecondary = QVector3D(0.38F, 0.46F, 0.36F);
    settings.grassDry = QVector3D(0.50F, 0.48F, 0.42F);
    settings.soilColor = QVector3D(0.42F, 0.40F, 0.38F);
    settings.rockLow = QVector3D(0.58F, 0.60F, 0.64F);
    settings.rockHigh = QVector3D(0.88F, 0.90F, 0.94F);
    settings.terrainAmbientBoost = 1.25F;
    settings.terrainRockDetailStrength = 0.52F;
    // Grass/vegetation properties - short hardy alpine grass
    settings.patchDensity = 1.8F;
    settings.patchJitter = 0.50F;
    settings.backgroundBladeDensity = 0.22F;
    settings.bladeHeightMin = 0.20F;
    settings.bladeHeightMax = 0.50F;
    settings.bladeWidthMin = 0.015F;
    settings.bladeWidthMax = 0.032F;
    settings.sway_strength = 0.22F;
    settings.sway_speed = 2.0F;
    // Terrain noise - moderate with snow accumulation effect
    settings.terrainMacroNoiseScale = 0.042F;
    settings.terrainDetailNoiseScale = 0.18F;
    settings.terrainSoilHeight = 0.55F;
    settings.terrainSoilSharpness = 3.0F;
    settings.terrainRockThreshold = 0.32F;
    settings.terrainRockSharpness = 2.5F;
    // Ground irregularity - moderate for alpine terrain
    settings.groundIrregularityEnabled = true;
    settings.irregularityScale = 0.18F;
    settings.irregularityAmplitude = 0.12F;
    settings.plant_density = 0.12F;
    // Ground-type-specific shader parameters - alpine_mix
    settings.snowCoverage = 0.55F;
    settings.moistureLevel = 0.45F;
    settings.crackIntensity = 0.10F;
    settings.rockExposure = 0.60F;
    settings.grassSaturation = 0.80F;
    settings.soilRoughness = 0.62F;
    settings.snowColor = QVector3D(0.94F, 0.96F, 1.0F);
    break;

  case GroundType::SoilFertile:
    // Dark Fertile Farmland Soil
    // Rich dark earth with lush green grass, ideal for agriculture
    settings.grassPrimary = QVector3D(0.25F, 0.55F, 0.22F);
    settings.grassSecondary = QVector3D(0.35F, 0.65F, 0.30F);
    settings.grassDry = QVector3D(0.52F, 0.48F, 0.32F);
    settings.soilColor = QVector3D(0.20F, 0.16F, 0.12F);
    settings.rockLow = QVector3D(0.38F, 0.36F, 0.34F);
    settings.rockHigh = QVector3D(0.52F, 0.50F, 0.48F);
    settings.terrainAmbientBoost = 1.02F;
    settings.terrainRockDetailStrength = 0.22F;
    // Grass/vegetation properties - thick healthy grass
    settings.patchDensity = 5.2F;
    settings.patchJitter = 0.90F;
    settings.backgroundBladeDensity = 0.80F;
    settings.bladeHeightMin = 0.55F;
    settings.bladeHeightMax = 1.25F;
    settings.bladeWidthMin = 0.030F;
    settings.bladeWidthMax = 0.062F;
    settings.sway_strength = 0.32F;
    settings.sway_speed = 1.2F;
    // Terrain noise - smooth fertile soil
    settings.terrainMacroNoiseScale = 0.025F;
    settings.terrainDetailNoiseScale = 0.10F;
    settings.terrainSoilHeight = 0.75F;
    settings.terrainSoilSharpness = 2.8F;
    settings.terrainRockThreshold = 0.58F;
    settings.terrainRockSharpness = 2.2F;
    // Ground irregularity - low for flat farmland
    settings.groundIrregularityEnabled = true;
    settings.irregularityScale = 0.08F;
    settings.irregularityAmplitude = 0.05F;
    settings.plant_density = 0.45F;
    // Ground-type-specific shader parameters - soil_fertile
    settings.snowCoverage = 0.0F;
    settings.moistureLevel = 0.80F;
    settings.crackIntensity = 0.0F;
    settings.rockExposure = 0.12F;
    settings.grassSaturation = 1.15F;
    settings.soilRoughness = 0.42F;
    settings.snowColor = QVector3D(0.92F, 0.94F, 0.98F);
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
