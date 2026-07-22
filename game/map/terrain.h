#pragma once

#include <QString>
#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

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
  float terrain_ambient_boost = 0.96F;
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

struct TerrainSurfaceProfile {
  GroundType ground_type = GroundType::ForestMud;
  QVector3D grass_primary{0.30F, 0.60F, 0.28F};
  QVector3D grass_secondary{0.44F, 0.70F, 0.32F};
  QVector3D grass_dry{0.72F, 0.66F, 0.48F};
  QVector3D soil_color{0.28F, 0.24F, 0.18F};
  QVector3D rock_low{0.48F, 0.46F, 0.44F};
  QVector3D rock_high{0.68F, 0.69F, 0.73F};
  float height_noise_amplitude = 0.16F;
  float height_noise_frequency = 0.05F;
  float terrain_macro_noise_scale = 0.035F;
  float terrain_detail_noise_scale = 0.14F;
  float terrain_soil_height = 0.6F;
  float terrain_soil_sharpness = 3.8F;
  float terrain_rock_threshold = 0.42F;
  float terrain_rock_sharpness = 3.1F;
  float terrain_ambient_boost = 0.96F;
  float terrain_rock_detail_strength = 0.35F;
  bool ground_irregularity_enabled = true;
  float irregularity_scale = 0.15F;
  float irregularity_amplitude = 0.08F;
  std::uint32_t seed = 1337U;
};

struct TerrainScatterProfile {
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
  float plant_density = 0.5F;
  float spawn_edge_padding = 0.08F;
  float background_scatter_radius = 0.35F;
  std::uint32_t seed = 1337U;
};

struct ClimateProfile {
  float snow_coverage = 0.0F;
  float moisture_level = 0.5F;
  float crack_intensity = 0.0F;
  float rock_exposure = 0.3F;
  float grass_saturation = 1.0F;
  float soil_roughness = 0.5F;
  QVector3D snow_color{0.92F, 0.94F, 0.98F};
};

struct WindProfile {
  float sway_strength = 0.25F;
  float sway_speed = 1.4F;
  float background_sway_variance = 0.2F;
};

struct BiomeProfiles {
  TerrainSurfaceProfile surface;
  TerrainScatterProfile scatter;
  ClimateProfile climate;
  WindProfile wind;
};

struct TerrainScatterRules {
  bool allow_pines = true;
  bool allow_olives = false;
  float pine_base_density = 0.2F;
  float pine_density_scale = 0.3F;
  float pine_scale_min = 1.9F;
  float pine_scale_max = 3.4F;
  float olive_base_density = 0.05F;
  float olive_density_scale = 0.08F;
  float olive_scale_min = 3.2F;
  float olive_scale_max = 5.8F;
};

inline auto
make_surface_profile(const BiomeSettings& settings) -> TerrainSurfaceProfile {
  TerrainSurfaceProfile profile;
  profile.ground_type = settings.ground_type;
  profile.grass_primary = settings.grass_primary;
  profile.grass_secondary = settings.grass_secondary;
  profile.grass_dry = settings.grass_dry;
  profile.soil_color = settings.soil_color;
  profile.rock_low = settings.rock_low;
  profile.rock_high = settings.rock_high;
  profile.height_noise_amplitude = settings.height_noise_amplitude;
  profile.height_noise_frequency = settings.height_noise_frequency;
  profile.terrain_macro_noise_scale = settings.terrain_macro_noise_scale;
  profile.terrain_detail_noise_scale = settings.terrain_detail_noise_scale;
  profile.terrain_soil_height = settings.terrain_soil_height;
  profile.terrain_soil_sharpness = settings.terrain_soil_sharpness;
  profile.terrain_rock_threshold = settings.terrain_rock_threshold;
  profile.terrain_rock_sharpness = settings.terrain_rock_sharpness;
  profile.terrain_ambient_boost = settings.terrain_ambient_boost;
  profile.terrain_rock_detail_strength = settings.terrain_rock_detail_strength;
  profile.ground_irregularity_enabled = settings.ground_irregularity_enabled;
  profile.irregularity_scale = settings.irregularity_scale;
  profile.irregularity_amplitude = settings.irregularity_amplitude;
  profile.seed = settings.seed;
  return profile;
}

inline auto
make_scatter_profile(const BiomeSettings& settings) -> TerrainScatterProfile {
  TerrainScatterProfile profile;
  profile.ground_type = settings.ground_type;
  profile.grass_primary = settings.grass_primary;
  profile.grass_secondary = settings.grass_secondary;
  profile.grass_dry = settings.grass_dry;
  profile.soil_color = settings.soil_color;
  profile.rock_low = settings.rock_low;
  profile.rock_high = settings.rock_high;
  profile.patch_density = settings.patch_density;
  profile.patch_jitter = settings.patch_jitter;
  profile.background_blade_density = settings.background_blade_density;
  profile.blade_height_min = settings.blade_height_min;
  profile.blade_height_max = settings.blade_height_max;
  profile.blade_width_min = settings.blade_width_min;
  profile.blade_width_max = settings.blade_width_max;
  profile.plant_density = settings.plant_density;
  profile.spawn_edge_padding = settings.spawn_edge_padding;
  profile.background_scatter_radius = settings.background_scatter_radius;
  profile.seed = settings.seed;
  return profile;
}

inline auto make_climate_profile(const BiomeSettings& settings) -> ClimateProfile {
  ClimateProfile profile;
  profile.snow_coverage = settings.snow_coverage;
  profile.moisture_level = settings.moisture_level;
  profile.crack_intensity = settings.crack_intensity;
  profile.rock_exposure = settings.rock_exposure;
  profile.grass_saturation = settings.grass_saturation;
  profile.soil_roughness = settings.soil_roughness;
  profile.snow_color = settings.snow_color;
  return profile;
}

inline auto make_wind_profile(const BiomeSettings& settings) -> WindProfile {
  WindProfile profile;
  profile.sway_strength = settings.sway_strength;
  profile.sway_speed = settings.sway_speed;
  profile.background_sway_variance = settings.background_sway_variance;
  return profile;
}

inline auto make_biome_profiles(const BiomeSettings& settings) -> BiomeProfiles {
  BiomeProfiles profiles;
  profiles.surface = make_surface_profile(settings);
  profiles.scatter = make_scatter_profile(settings);
  profiles.climate = make_climate_profile(settings);
  profiles.wind = make_wind_profile(settings);
  return profiles;
}

inline auto make_scatter_rules(GroundType ground_type) -> TerrainScatterRules {
  TerrainScatterRules rules;
  switch (ground_type) {
  case GroundType::GrassDry:
    rules.allow_pines = false;
    rules.allow_olives = true;
    rules.olive_base_density = 0.12F;
    rules.olive_density_scale = 0.15F;
    rules.olive_scale_min = 3.6F;
    rules.olive_scale_max = 6.4F;
    break;
  case GroundType::ForestMud:
    rules.pine_base_density = 0.32F;
    rules.pine_density_scale = 0.42F;
    rules.pine_scale_min = 2.1F;
    rules.pine_scale_max = 3.8F;
    break;
  case GroundType::SoilFertile:
    rules.allow_pines = false;
    rules.allow_olives = true;
    rules.olive_base_density = 0.08F;
    rules.olive_density_scale = 0.12F;
    rules.olive_scale_min = 3.0F;
    rules.olive_scale_max = 5.4F;
    break;
  case GroundType::SoilRocky:
    rules.allow_pines = false;
    rules.allow_olives = false;
    break;
  case GroundType::AlpineMix:
    rules.pine_base_density = 0.10F;
    rules.pine_density_scale = 0.20F;
    rules.pine_scale_min = 1.7F;
    rules.pine_scale_max = 3.0F;
    break;
  }
  return rules;
}

inline void apply_ground_type_defaults(BiomeSettings& settings,
                                       GroundType ground_type) {
  settings.ground_type = ground_type;
  switch (ground_type) {
  case GroundType::ForestMud:

    settings.grass_primary = QVector3D(0.28F, 0.58F, 0.26F);
    settings.grass_secondary = QVector3D(0.40F, 0.68F, 0.30F);
    settings.grass_dry = QVector3D(0.76F, 0.70F, 0.48F);
    settings.soil_color = QVector3D(0.28F, 0.24F, 0.18F);
    settings.rock_low = QVector3D(0.48F, 0.46F, 0.44F);
    settings.rock_high = QVector3D(0.68F, 0.69F, 0.73F);
    settings.terrain_ambient_boost = 0.96F;
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
    settings.grass_saturation = 1.10F;
    settings.soil_roughness = 0.55F;
    settings.snow_color = QVector3D(0.92F, 0.94F, 0.98F);
    break;

  case GroundType::GrassDry:

    settings.grass_primary = QVector3D(0.54F, 0.56F, 0.30F);
    settings.grass_secondary = QVector3D(0.60F, 0.62F, 0.35F);
    settings.grass_dry = QVector3D(0.78F, 0.72F, 0.45F);
    settings.soil_color = QVector3D(0.52F, 0.44F, 0.32F);
    settings.rock_low = QVector3D(0.62F, 0.58F, 0.52F);
    settings.rock_high = QVector3D(0.78F, 0.75F, 0.70F);
    settings.terrain_ambient_boost = 1.06F;
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
    settings.grass_saturation = 0.90F;
    settings.soil_roughness = 0.72F;
    settings.snow_color = QVector3D(0.92F, 0.94F, 0.98F);
    break;

  case GroundType::SoilRocky:

    settings.grass_primary = QVector3D(0.38F, 0.46F, 0.27F);
    settings.grass_secondary = QVector3D(0.45F, 0.54F, 0.31F);
    settings.grass_dry = QVector3D(0.58F, 0.52F, 0.38F);
    settings.soil_color = QVector3D(0.55F, 0.48F, 0.38F);
    settings.rock_low = QVector3D(0.52F, 0.50F, 0.46F);
    settings.rock_high = QVector3D(0.72F, 0.70F, 0.66F);
    settings.terrain_ambient_boost = 0.96F;
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
    settings.grass_saturation = 1.00F;
    settings.soil_roughness = 0.85F;
    settings.snow_color = QVector3D(0.92F, 0.94F, 0.98F);
    break;

  case GroundType::AlpineMix:

    settings.grass_primary = QVector3D(0.31F, 0.42F, 0.29F);
    settings.grass_secondary = QVector3D(0.36F, 0.48F, 0.34F);
    settings.grass_dry = QVector3D(0.50F, 0.48F, 0.42F);
    settings.soil_color = QVector3D(0.42F, 0.40F, 0.38F);
    settings.rock_low = QVector3D(0.58F, 0.60F, 0.64F);
    settings.rock_high = QVector3D(0.88F, 0.90F, 0.94F);
    settings.terrain_ambient_boost = 1.12F;
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
    settings.grass_saturation = 0.95F;
    settings.soil_roughness = 0.62F;
    settings.snow_color = QVector3D(0.94F, 0.96F, 1.0F);
    break;

  case GroundType::SoilFertile:

    settings.grass_primary = QVector3D(0.24F, 0.56F, 0.22F);
    settings.grass_secondary = QVector3D(0.33F, 0.65F, 0.29F);
    settings.grass_dry = QVector3D(0.52F, 0.48F, 0.32F);
    settings.soil_color = QVector3D(0.20F, 0.16F, 0.12F);
    settings.rock_low = QVector3D(0.38F, 0.36F, 0.34F);
    settings.rock_high = QVector3D(0.52F, 0.50F, 0.48F);
    settings.terrain_ambient_boost = 0.94F;
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
    settings.grass_saturation = 1.10F;
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

  float rotation_deg = 0.0F;
};

enum class WaterElevationMode : std::uint8_t {
  Terrain,
  Authored,
};

struct RiverSegment {
  QVector3D start;
  QVector3D end;
  float width = 2.0F;
  WaterElevationMode elevation_mode = WaterElevationMode::Terrain;
};

struct Lake {
  QVector3D center;
  float width = 8.0F;
  float depth = 8.0F;
  float rotation_deg = 0.0F;
  WaterElevationMode elevation_mode = WaterElevationMode::Terrain;
};

[[nodiscard]] inline auto lake_boundary_scale(const Lake& lake,
                                              float local_angle) noexcept -> float {
  const float phase = lake.center.x() * 0.071F + lake.center.z() * 0.113F;
  return std::clamp(1.0F + std::sin(local_angle * 3.0F + phase) * 0.055F +
                        std::sin(local_angle * 7.0F - phase * 1.7F) * 0.025F,
                    0.90F,
                    1.10F);
}

[[nodiscard]] inline auto point_in_lake(const Lake& lake,
                                        float world_x,
                                        float world_z,
                                        float padding = 0.0F) noexcept -> bool {
  constexpr float deg_to_rad = 0.01745329251994329577F;
  const float angle = -lake.rotation_deg * deg_to_rad;
  const float cos_a = std::cos(angle);
  const float sin_a = std::sin(angle);
  const float dx = world_x - lake.center.x();
  const float dz = world_z - lake.center.z();
  const float local_x = dx * cos_a - dz * sin_a;
  const float local_z = dx * sin_a + dz * cos_a;
  const float half_width = std::max(lake.width * 0.5F + padding, 0.0001F);
  const float half_depth = std::max(lake.depth * 0.5F + padding, 0.0001F);
  const float normalized_x = local_x / half_width;
  const float normalized_z = local_z / half_depth;
  const float radial = std::hypot(normalized_x, normalized_z);
  const float local_angle = std::atan2(normalized_z, normalized_x);
  return radial <= lake_boundary_scale(lake, local_angle);
}

[[nodiscard]] inline auto point_on_lake_boundary(const Lake& lake,
                                                 float world_x,
                                                 float world_z,
                                                 float tolerance) noexcept -> bool {
  const float shoreline_band = std::max(tolerance, 0.0F);
  return point_in_lake(lake, world_x, world_z, shoreline_band) &&
         !point_in_lake(lake, world_x, world_z, -shoreline_band);
}

// Finds the first shoreline point while travelling from dry ground toward a lake.
// The two points must bracket the shoreline. Keeping this here makes map loading,
// validation, pathing, banks, minimaps, and rendering agree on the same irregular
// lake outline.
[[nodiscard]] inline auto
lake_boundary_intersection(const Lake& lake,
                           QVector3D dry_point,
                           QVector3D wet_point) noexcept -> std::optional<QVector3D> {
  if (point_in_lake(lake, dry_point.x(), dry_point.z()) ||
      !point_in_lake(lake, wet_point.x(), wet_point.z())) {
    return std::nullopt;
  }

  for (int iteration = 0; iteration < 28; ++iteration) {
    const QVector3D midpoint = (dry_point + wet_point) * 0.5F;
    if (point_in_lake(lake, midpoint.x(), midpoint.z())) {
      wet_point = midpoint;
    } else {
      dry_point = midpoint;
    }
  }
  return (dry_point + wet_point) * 0.5F;
}

struct RoadSegment {
  QVector3D start;
  QVector3D end;
  float width = 3.0F;
  QString style = QStringLiteral("default");
};

inline constexpr float k_min_bridge_width = 8.0F;

struct Bridge {
  QVector3D start;
  QVector3D end;
  float width = k_min_bridge_width;
  float height = 0.5F;
};

inline constexpr float k_bridge_riverbank_visual_padding = 1.0F;

[[nodiscard]] inline auto xz_cross(const QVector3D& a,
                                   const QVector3D& b) noexcept -> float {
  return a.x() * b.z() - a.z() * b.x();
}

[[nodiscard]] inline auto bridge_required_half_length_for_river(
    const Bridge& bridge, const RiverSegment& river) noexcept -> std::optional<float> {
  QVector3D const bridge_vec = bridge.end - bridge.start;
  QVector3D const river_vec = river.end - river.start;
  float const bridge_len = std::hypot(bridge_vec.x(), bridge_vec.z());
  float const river_len = std::hypot(river_vec.x(), river_vec.z());
  if (bridge_len < 1.0e-4F || river_len < 1.0e-4F) {
    return std::nullopt;
  }

  float const cross = xz_cross(bridge_vec, river_vec);
  float const sin_angle = std::abs(cross) / (bridge_len * river_len);
  if (sin_angle < 1.0e-4F) {
    return std::nullopt;
  }

  QVector3D const diff = river.start - bridge.start;
  float const t = xz_cross(diff, river_vec) / cross;
  float const s = xz_cross(diff, bridge_vec) / cross;
  if (t < 0.0F || t > 1.0F || s < 0.0F || s > 1.0F) {
    return std::nullopt;
  }

  float const effective_river_half_width =
      river.width * 0.5F + k_bridge_riverbank_visual_padding;
  return effective_river_half_width / sin_angle;
}

inline void extend_bridge_to_span_riverbanks(Bridge& bridge,
                                             const std::vector<RiverSegment>& rivers) {
  for (const RiverSegment& river : rivers) {
    QVector3D bridge_vec = bridge.end - bridge.start;
    float const bridge_len = std::hypot(bridge_vec.x(), bridge_vec.z());
    if (bridge_len < 1.0e-4F) {
      continue;
    }

    auto const required_half = bridge_required_half_length_for_river(bridge, river);
    if (!required_half.has_value()) {
      continue;
    }

    QVector3D dir(bridge_vec.x() / bridge_len, 0.0F, bridge_vec.z() / bridge_len);
    QVector3D const river_vec = river.end - river.start;
    float const cross = xz_cross(bridge_vec, river_vec);
    QVector3D const diff = river.start - bridge.start;
    float const t = xz_cross(diff, river_vec) / cross;
    float const before = std::max(0.0F, *required_half - t * bridge_len);
    float const after = std::max(0.0F, *required_half - (1.0F - t) * bridge_len);
    if (before > 0.0F) {
      bridge.start -= dir * before;
    }
    if (after > 0.0F) {
      bridge.end += dir * after;
    }
  }
}

inline constexpr float k_road_surface_y_offset = 0.02F;

[[nodiscard]] inline auto road_surface_world_y(float terrain_height) -> float {
  return terrain_height + k_road_surface_y_offset;
}

[[nodiscard]] inline auto bridge_arch_curve(float t) -> float {
  float const clamped_t = std::clamp(t, 0.0F, 1.0F);
  return 4.0F * clamped_t * (1.0F - clamped_t);
}

inline constexpr float k_min_bridge_deck_rise = 0.72F;
// Lift only the visible/contact surface above the terrain relief used to
// derive navigation slopes. This keeps bridges clear of the water sheet
// without making their approaches less attractive to pathfinding.
inline constexpr float k_bridge_deck_visual_lift = 0.33F;

[[nodiscard]] inline auto bridge_effective_height(const Bridge& bridge) -> float {
  return std::max(bridge.height, k_min_bridge_deck_rise);
}

[[nodiscard]] inline auto bridge_deck_world_y(const Bridge& bridge, float t) -> float {
  float const clamped_t = std::clamp(t, 0.0F, 1.0F);
  float const base_y = bridge.start.y() * (1.0F - clamped_t) +
                       bridge.end.y() * clamped_t;
  float const effective_height = bridge_effective_height(bridge);
  float const arch_height =
      effective_height * bridge_arch_curve(clamped_t) * 0.8F;
  return base_y + effective_height + arch_height * 0.3F +
         k_bridge_deck_visual_lift;
}

[[nodiscard]] inline auto bridge_crossing_entry_margin(float bridge_width,
                                                       float tile_size) -> float {
  return std::max(tile_size * 2.0F, bridge_width);
}

[[nodiscard]] inline auto
bridge_crossing_alignment_half_width(float bridge_width, float tile_size) -> float {
  return std::max(bridge_width * 0.5F + tile_size, tile_size * 1.5F);
}

struct TerrainField {
  int width = 0;
  int height = 0;
  float tile_size = 1.0F;
  std::vector<float> heights;
  std::vector<float> slopes;
  std::vector<float> curvature;

  void clear();

  [[nodiscard]] auto empty() const -> bool;

  [[nodiscard]] auto sample_height_at(float gx, float gz) const -> float;

  [[nodiscard]] auto sample_slope_at(int grid_x, int grid_z) const -> float;

  [[nodiscard]] auto sample_curvature_at(int grid_x, int grid_z) const -> float;
};

class TerrainHeightMap {
public:
  TerrainHeightMap(int width, int height, float tile_size);

  void build_from_features(const std::vector<TerrainFeature>& features);

  void add_river_segments(const std::vector<RiverSegment>& river_segments);

  void add_lakes(const std::vector<Lake>& lakes);

  [[nodiscard]] auto get_height_at(float world_x, float world_z) const -> float;

  [[nodiscard]] auto get_base_height_at(float world_x, float world_z) const -> float;

  [[nodiscard]] auto get_height_at_grid(int grid_x, int grid_z) const -> float;

  [[nodiscard]] auto is_walkable(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto isHillEntrance(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto getTerrainType(int grid_x, int grid_z) const -> TerrainType;

  [[nodiscard]] auto
  isRiverOrNearby(int grid_x, int grid_z, int margin = 1) const -> bool;

  [[nodiscard]] auto get_width() const -> int { return m_width; }
  [[nodiscard]] auto get_height() const -> int { return m_height; }
  [[nodiscard]] auto get_tile_size() const -> float { return m_tile_size; }

  [[nodiscard]] auto get_height_data() const -> const std::vector<float>& {
    return m_heights;
  }
  [[nodiscard]] auto getTerrainTypes() const -> const std::vector<TerrainType>& {
    return m_terrain_types;
  }
  [[nodiscard]] auto getHillEntrances() const -> const std::vector<bool>& {
    return m_hill_entrances;
  }
  [[nodiscard]] auto get_river_segments() const -> const std::vector<RiverSegment>& {
    return m_river_segments;
  }
  [[nodiscard]] auto get_lakes() const -> const std::vector<Lake>& { return m_lakes; }

  void add_bridges(const std::vector<Bridge>& bridges);
  [[nodiscard]] auto get_bridges() const -> const std::vector<Bridge>& {
    return m_bridges;
  }

  [[nodiscard]] auto isOnBridge(float world_x, float world_z) const -> bool;

  [[nodiscard]] auto isBridgeCell(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto isBridgeCenterline(int grid_x, int grid_z) const -> bool;

  [[nodiscard]] auto getBridgeCenterPosition(float world_x, float world_z) const
      -> std::optional<QVector3D>;

  [[nodiscard]] auto getBridgeTraversalPosition(float world_x, float world_z) const
      -> std::optional<QVector3D>;

  [[nodiscard]] auto getBridgeDeckHeight(float world_x,
                                         float world_z) const -> std::optional<float>;

  void apply_biome_variation(const BiomeSettings& settings);

  void restore_from_data(const std::vector<float>& heights,
                         const std::vector<TerrainType>& terrain_types,
                         const std::vector<RiverSegment>& rivers,
                         const std::vector<Bridge>& bridges,
                         const std::vector<Lake>& lakes = {});

private:
  int m_width;
  int m_height;
  float m_tile_size;

  std::vector<float> m_heights;
  std::vector<TerrainType> m_terrain_types;
  std::vector<bool> m_hill_entrances;
  std::vector<bool> m_hill_walkable;
  std::vector<RiverSegment> m_river_segments;
  std::vector<Lake> m_lakes;
  std::vector<Bridge> m_bridges;

  std::vector<bool> m_on_bridge;
  std::vector<bool> m_bridge_centerline;
  std::vector<QVector3D> m_bridge_centers;

  [[nodiscard]] auto indexAt(int x, int z) const -> int;
  [[nodiscard]] auto in_bounds(int x, int z) const -> bool;

  void precompute_bridge_data();

  [[nodiscard]] static auto calculateFeatureHeight(const TerrainFeature& feature,
                                                   float world_x,
                                                   float world_z) -> float;
};

} // namespace Game::Map
