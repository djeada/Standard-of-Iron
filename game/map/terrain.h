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

inline auto try_parse_ground_type(const QString &value,
                                  GroundType &out) -> bool {
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
ground_type_from_string(const std::string &str) -> std::optional<GroundType> {
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
  GroundType ground_type = GroundType::ForestMud;
  QVector3D grass_primary{0.30F, 0.60F, 0.28F};
  QVector3D grass_secondary{0.44F, 0.70F, 0.32F};
  QVector3D grass_dry{0.72F, 0.66F, 0.48F};
  QVector3D soil_color{0.28F, 0.24F, 0.18F};
  QVector3D rock_low{0.48F, 0.46F, 0.44F};
  QVector3D rock_high{0.68F, 0.69F, 0.73F};
  float patch_density = 4.5F;
  float patch_jitter = 0.95F;
  float background_blade_density = 0.65F;
  float blade_height_min = 0.55F;
  float blade_height_max = 1.35F;
  float blade_width_min = 0.025F;
  float blade_width_max = 0.055F;
  float sway_strength = 0.25F;
  float sway_speed = 1.4F;
  float height_noise_amplitude = 0.16F;
  float height_noise_frequency = 0.05F;
  float terrain_macro_noise_scale = 0.035F;
  float terrain_detail_noise_scale = 0.14F;
  float terrain_soil_height = 0.6F;
  float terrain_soil_sharpness = 3.8F;
  float terrain_rock_threshold = 0.42F;
  float terrain_rock_sharpness = 3.1F;
  float terrain_ambient_boost = 1.08F;
  float terrain_rock_detail_strength = 0.35F;
  float background_sway_variance = 0.2F;
  float background_scatter_radius = 0.35F;
  float plant_density = 0.5F;
  float spawn_edge_padding = 0.08F;
  std::uint32_t seed = 1337U;
  bool ground_irregularity_enabled = true;
  float irregularity_scale = 0.15F;
  float irregularity_amplitude = 0.08F;

  float snow_coverage = 0.0F;
  float moisture_level = 0.5F;
  float crack_intensity = 0.0F;
  float rock_exposure = 0.3F;
  float grass_saturation = 1.0F;
  float soil_roughness = 0.5F;
  QVector3D snow_color{0.92F, 0.94F, 0.98F};
};

inline void apply_ground_type_defaults(BiomeSettings &settings,
                                       GroundType ground_type) {
  settings.ground_type = ground_type;
  switch (ground_type) {
  case GroundType::ForestMud:

    settings.grass_primary = QVector3D(0.30F, 0.60F, 0.28F);
    settings.grass_secondary = QVector3D(0.44F, 0.70F, 0.32F);
    settings.grass_dry = QVector3D(0.72F, 0.66F, 0.48F);
    settings.soil_color = QVector3D(0.28F, 0.24F, 0.18F);
    settings.rock_low = QVector3D(0.48F, 0.46F, 0.44F);
    settings.rock_high = QVector3D(0.68F, 0.69F, 0.73F);
    settings.terrain_ambient_boost = 1.08F;
    settings.terrain_rock_detail_strength = 0.35F;

    settings.patch_density = 4.5F;
    settings.patch_jitter = 0.95F;
    settings.background_blade_density = 0.70F;
    settings.blade_height_min = 0.60F;
    settings.blade_height_max = 1.40F;
    settings.blade_width_min = 0.028F;
    settings.blade_width_max = 0.058F;
    settings.sway_strength = 0.28F;
    settings.sway_speed = 1.3F;

    settings.terrain_macro_noise_scale = 0.035F;
    settings.terrain_detail_noise_scale = 0.14F;
    settings.terrain_soil_height = 0.65F;
    settings.terrain_soil_sharpness = 3.5F;
    settings.terrain_rock_threshold = 0.48F;
    settings.terrain_rock_sharpness = 3.2F;

    settings.ground_irregularity_enabled = true;
    settings.irregularity_scale = 0.15F;
    settings.irregularity_amplitude = 0.09F;
    settings.plant_density = 0.60F;

    settings.snow_coverage = 0.0F;
    settings.moisture_level = 0.70F;
    settings.crack_intensity = 0.0F;
    settings.rock_exposure = 0.25F;
    settings.grass_saturation = 1.05F;
    settings.soil_roughness = 0.55F;
    settings.snow_color = QVector3D(0.92F, 0.94F, 0.98F);
    break;

  case GroundType::GrassDry:

    settings.grass_primary = QVector3D(0.58F, 0.54F, 0.32F);
    settings.grass_secondary = QVector3D(0.65F, 0.60F, 0.38F);
    settings.grass_dry = QVector3D(0.78F, 0.72F, 0.45F);
    settings.soil_color = QVector3D(0.52F, 0.44F, 0.32F);
    settings.rock_low = QVector3D(0.62F, 0.58F, 0.52F);
    settings.rock_high = QVector3D(0.78F, 0.75F, 0.70F);
    settings.terrain_ambient_boost = 1.18F;
    settings.terrain_rock_detail_strength = 0.28F;

    settings.patch_density = 2.8F;
    settings.patch_jitter = 0.75F;
    settings.background_blade_density = 0.35F;
    settings.blade_height_min = 0.35F;
    settings.blade_height_max = 0.80F;
    settings.blade_width_min = 0.018F;
    settings.blade_width_max = 0.038F;
    settings.sway_strength = 0.15F;
    settings.sway_speed = 1.8F;

    settings.terrain_macro_noise_scale = 0.028F;
    settings.terrain_detail_noise_scale = 0.22F;
    settings.terrain_soil_height = 0.50F;
    settings.terrain_soil_sharpness = 4.5F;
    settings.terrain_rock_threshold = 0.38F;
    settings.terrain_rock_sharpness = 2.8F;

    settings.ground_irregularity_enabled = true;
    settings.irregularity_scale = 0.10F;
    settings.irregularity_amplitude = 0.04F;
    settings.plant_density = 0.25F;

    settings.snow_coverage = 0.0F;
    settings.moisture_level = 0.15F;
    settings.crack_intensity = 0.65F;
    settings.rock_exposure = 0.35F;
    settings.grass_saturation = 0.75F;
    settings.soil_roughness = 0.72F;
    settings.snow_color = QVector3D(0.92F, 0.94F, 0.98F);
    break;

  case GroundType::SoilRocky:

    settings.grass_primary = QVector3D(0.40F, 0.45F, 0.28F);
    settings.grass_secondary = QVector3D(0.48F, 0.52F, 0.32F);
    settings.grass_dry = QVector3D(0.58F, 0.52F, 0.38F);
    settings.soil_color = QVector3D(0.55F, 0.48F, 0.38F);
    settings.rock_low = QVector3D(0.52F, 0.50F, 0.46F);
    settings.rock_high = QVector3D(0.72F, 0.70F, 0.66F);
    settings.terrain_ambient_boost = 1.05F;
    settings.terrain_rock_detail_strength = 0.65F;

    settings.patch_density = 2.2F;
    settings.patch_jitter = 0.60F;
    settings.background_blade_density = 0.28F;
    settings.blade_height_min = 0.30F;
    settings.blade_height_max = 0.70F;
    settings.blade_width_min = 0.020F;
    settings.blade_width_max = 0.040F;
    settings.sway_strength = 0.18F;
    settings.sway_speed = 1.5F;

    settings.terrain_macro_noise_scale = 0.055F;
    settings.terrain_detail_noise_scale = 0.28F;
    settings.terrain_soil_height = 0.40F;
    settings.terrain_soil_sharpness = 5.0F;
    settings.terrain_rock_threshold = 0.28F;
    settings.terrain_rock_sharpness = 4.0F;

    settings.ground_irregularity_enabled = true;
    settings.irregularity_scale = 0.22F;
    settings.irregularity_amplitude = 0.14F;
    settings.plant_density = 0.18F;

    settings.snow_coverage = 0.0F;
    settings.moisture_level = 0.35F;
    settings.crack_intensity = 0.25F;
    settings.rock_exposure = 0.75F;
    settings.grass_saturation = 0.85F;
    settings.soil_roughness = 0.85F;
    settings.snow_color = QVector3D(0.92F, 0.94F, 0.98F);
    break;

  case GroundType::AlpineMix:

    settings.grass_primary = QVector3D(0.32F, 0.40F, 0.30F);
    settings.grass_secondary = QVector3D(0.38F, 0.46F, 0.36F);
    settings.grass_dry = QVector3D(0.50F, 0.48F, 0.42F);
    settings.soil_color = QVector3D(0.42F, 0.40F, 0.38F);
    settings.rock_low = QVector3D(0.58F, 0.60F, 0.64F);
    settings.rock_high = QVector3D(0.88F, 0.90F, 0.94F);
    settings.terrain_ambient_boost = 1.25F;
    settings.terrain_rock_detail_strength = 0.52F;

    settings.patch_density = 1.8F;
    settings.patch_jitter = 0.50F;
    settings.background_blade_density = 0.22F;
    settings.blade_height_min = 0.20F;
    settings.blade_height_max = 0.50F;
    settings.blade_width_min = 0.015F;
    settings.blade_width_max = 0.032F;
    settings.sway_strength = 0.22F;
    settings.sway_speed = 2.0F;

    settings.terrain_macro_noise_scale = 0.042F;
    settings.terrain_detail_noise_scale = 0.18F;
    settings.terrain_soil_height = 0.55F;
    settings.terrain_soil_sharpness = 3.0F;
    settings.terrain_rock_threshold = 0.32F;
    settings.terrain_rock_sharpness = 2.5F;

    settings.ground_irregularity_enabled = true;
    settings.irregularity_scale = 0.18F;
    settings.irregularity_amplitude = 0.12F;
    settings.plant_density = 0.12F;

    settings.snow_coverage = 0.55F;
    settings.moisture_level = 0.45F;
    settings.crack_intensity = 0.10F;
    settings.rock_exposure = 0.60F;
    settings.grass_saturation = 0.80F;
    settings.soil_roughness = 0.62F;
    settings.snow_color = QVector3D(0.94F, 0.96F, 1.0F);
    break;

  case GroundType::SoilFertile:

    settings.grass_primary = QVector3D(0.25F, 0.55F, 0.22F);
    settings.grass_secondary = QVector3D(0.35F, 0.65F, 0.30F);
    settings.grass_dry = QVector3D(0.52F, 0.48F, 0.32F);
    settings.soil_color = QVector3D(0.20F, 0.16F, 0.12F);
    settings.rock_low = QVector3D(0.38F, 0.36F, 0.34F);
    settings.rock_high = QVector3D(0.52F, 0.50F, 0.48F);
    settings.terrain_ambient_boost = 1.02F;
    settings.terrain_rock_detail_strength = 0.22F;

    settings.patch_density = 5.2F;
    settings.patch_jitter = 0.90F;
    settings.background_blade_density = 0.80F;
    settings.blade_height_min = 0.55F;
    settings.blade_height_max = 1.25F;
    settings.blade_width_min = 0.030F;
    settings.blade_width_max = 0.062F;
    settings.sway_strength = 0.32F;
    settings.sway_speed = 1.2F;

    settings.terrain_macro_noise_scale = 0.025F;
    settings.terrain_detail_noise_scale = 0.10F;
    settings.terrain_soil_height = 0.75F;
    settings.terrain_soil_sharpness = 2.8F;
    settings.terrain_rock_threshold = 0.58F;
    settings.terrain_rock_sharpness = 2.2F;

    settings.ground_irregularity_enabled = true;
    settings.irregularity_scale = 0.08F;
    settings.irregularity_amplitude = 0.05F;
    settings.plant_density = 0.45F;

    settings.snow_coverage = 0.0F;
    settings.moisture_level = 0.80F;
    settings.crack_intensity = 0.0F;
    settings.rock_exposure = 0.12F;
    settings.grass_saturation = 1.15F;
    settings.soil_roughness = 0.42F;
    settings.snow_color = QVector3D(0.92F, 0.94F, 0.98F);
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

struct RoadSegment {
  QVector3D start;
  QVector3D end;
  float width = 3.0F;
  QString style = QStringLiteral("default");
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

  [[nodiscard]] auto is_walkable(int grid_x, int grid_z) const -> bool;

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
  [[nodiscard]] auto getHillEntrances() const -> const std::vector<bool> & {
    return m_hillEntrances;
  }
  [[nodiscard]] auto
  getRiverSegments() const -> const std::vector<RiverSegment> & {
    return m_riverSegments;
  }

  void addBridges(const std::vector<Bridge> &bridges);
  [[nodiscard]] auto getBridges() const -> const std::vector<Bridge> & {
    return m_bridges;
  }

  [[nodiscard]] auto isOnBridge(float world_x, float world_z) const -> bool;

  [[nodiscard]] auto getBridgeCenterPosition(float world_x, float world_z) const
      -> std::optional<QVector3D>;

  [[nodiscard]] auto getBridgeDeckHeight(float world_x, float world_z) const
      -> std::optional<float>;

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

  std::vector<bool> m_onBridge;
  std::vector<QVector3D> m_bridgeCenters;

  [[nodiscard]] auto indexAt(int x, int z) const -> int;
  [[nodiscard]] auto inBounds(int x, int z) const -> bool;

  void precomputeBridgeData();

  [[nodiscard]] static auto
  calculateFeatureHeight(const TerrainFeature &feature, float world_x,
                         float world_z) -> float;
};

} // namespace Game::Map
