#include "terrain_biome_codec.h"

#include <QJsonObject>
#include <QString>
#include <QVector3D>
#include <qstringliteral.h>

#include <cstdint>

#include "../map/biome_settings.h"

namespace Engine::Core::TerrainCodec {

namespace {
void write_biome_palette(const Game::Map::TerrainSurfaceProfile& surface,
                         const Game::Map::ClimateProfile& climate,
                         QJsonObject& biome_obj) {
  biome_obj["grassPrimaryR"] = surface.grass_primary.x();
  biome_obj["grassPrimaryG"] = surface.grass_primary.y();
  biome_obj["grassPrimaryB"] = surface.grass_primary.z();
  biome_obj["grassSecondaryR"] = surface.grass_secondary.x();
  biome_obj["grassSecondaryG"] = surface.grass_secondary.y();
  biome_obj["grassSecondaryB"] = surface.grass_secondary.z();
  biome_obj["grassDryR"] = surface.grass_dry.x();
  biome_obj["grassDryG"] = surface.grass_dry.y();
  biome_obj["grassDryB"] = surface.grass_dry.z();
  biome_obj["soilColorR"] = surface.soil_color.x();
  biome_obj["soilColorG"] = surface.soil_color.y();
  biome_obj["soilColorB"] = surface.soil_color.z();
  biome_obj["rockLowR"] = surface.rock_low.x();
  biome_obj["rockLowG"] = surface.rock_low.y();
  biome_obj["rockLowB"] = surface.rock_low.z();
  biome_obj["rockHighR"] = surface.rock_high.x();
  biome_obj["rockHighG"] = surface.rock_high.y();
  biome_obj["rockHighB"] = surface.rock_high.z();
  biome_obj["snowColorR"] = climate.snow_color.x();
  biome_obj["snowColorG"] = climate.snow_color.y();
  biome_obj["snowColorB"] = climate.snow_color.z();
}

void write_biome_scatter(const Game::Map::TerrainScatterProfile& scatter,
                         const Game::Map::WindProfile& wind,
                         QJsonObject& biome_obj) {
  biome_obj["patchDensity"] = scatter.patch_density;
  biome_obj["patchJitter"] = scatter.patch_jitter;
  biome_obj["backgroundBladeDensity"] = scatter.background_blade_density;
  biome_obj["bladeHeightMin"] = scatter.blade_height_min;
  biome_obj["bladeHeightMax"] = scatter.blade_height_max;
  biome_obj["bladeWidthMin"] = scatter.blade_width_min;
  biome_obj["bladeWidthMax"] = scatter.blade_width_max;
  biome_obj["sway_strength"] = wind.sway_strength;
  biome_obj["sway_speed"] = wind.sway_speed;
  biome_obj["backgroundSwayVariance"] = wind.background_sway_variance;
  biome_obj["backgroundScatterRadius"] = scatter.background_scatter_radius;
  biome_obj["plant_density"] = scatter.plant_density;
  biome_obj["spawnEdgePadding"] = scatter.spawn_edge_padding;
}

void write_biome_surface(const Game::Map::TerrainSurfaceProfile& surface,
                         QJsonObject& biome_obj) {
  biome_obj["heightNoiseAmplitude"] = surface.height_noise_amplitude;
  biome_obj["heightNoiseFrequency"] = surface.height_noise_frequency;
  biome_obj["terrainMacroNoiseScale"] = surface.terrain_macro_noise_scale;
  biome_obj["terrainDetailNoiseScale"] = surface.terrain_detail_noise_scale;
  biome_obj["terrainSoilHeight"] = surface.terrain_soil_height;
  biome_obj["terrainSoilSharpness"] = surface.terrain_soil_sharpness;
  biome_obj["terrainRockThreshold"] = surface.terrain_rock_threshold;
  biome_obj["terrainRockSharpness"] = surface.terrain_rock_sharpness;
  biome_obj["terrainAmbientBoost"] = surface.terrain_ambient_boost;
  biome_obj["terrainRockDetailStrength"] = surface.terrain_rock_detail_strength;
  biome_obj["seed"] = static_cast<qint64>(surface.seed);
}

void write_biome_climate(const Game::Map::ClimateProfile& climate,
                         QJsonObject& biome_obj) {
  biome_obj["snowCoverage"] = climate.snow_coverage;
  biome_obj["moistureLevel"] = climate.moisture_level;
  biome_obj["crackIntensity"] = climate.crack_intensity;
  biome_obj["rockExposure"] = climate.rock_exposure;
  biome_obj["grassSaturation"] = climate.grass_saturation;
  biome_obj["soilRoughness"] = climate.soil_roughness;
}

auto read_biome_color(const QJsonObject& biome_obj,
                      const QString& base,
                      const QVector3D& fallback) -> QVector3D {
  const auto r_key = base + QStringLiteral("R");
  const auto g_key = base + QStringLiteral("G");
  const auto b_key = base + QStringLiteral("B");
  const float r =
      static_cast<float>(biome_obj[r_key].toDouble(static_cast<double>(fallback.x())));
  const float g =
      static_cast<float>(biome_obj[g_key].toDouble(static_cast<double>(fallback.y())));
  const float b =
      static_cast<float>(biome_obj[b_key].toDouble(static_cast<double>(fallback.z())));
  return {r, g, b};
}

void read_biome_palette(const QJsonObject& biome_obj,
                        const Game::Map::BiomeSettings& default_biome,
                        Game::Map::BiomeSettings& biome) {
  biome.grass_primary = read_biome_color(
      biome_obj, QStringLiteral("grassPrimary"), default_biome.grass_primary);
  biome.grass_secondary = read_biome_color(
      biome_obj, QStringLiteral("grassSecondary"), default_biome.grass_secondary);
  biome.grass_dry =
      read_biome_color(biome_obj, QStringLiteral("grassDry"), default_biome.grass_dry);
  biome.soil_color = read_biome_color(
      biome_obj, QStringLiteral("soilColor"), default_biome.soil_color);
  biome.rock_low =
      read_biome_color(biome_obj, QStringLiteral("rockLow"), default_biome.rock_low);
  biome.rock_high =
      read_biome_color(biome_obj, QStringLiteral("rockHigh"), default_biome.rock_high);

  biome.snow_color = read_biome_color(
      biome_obj, QStringLiteral("snowColor"), default_biome.snow_color);
}

void read_biome_scatter(const QJsonObject& biome_obj,
                        const Game::Map::BiomeSettings& default_biome,
                        Game::Map::BiomeSettings& biome) {
  biome.patch_density = static_cast<float>(
      biome_obj["patchDensity"].toDouble(default_biome.patch_density));
  biome.patch_jitter =
      static_cast<float>(biome_obj["patchJitter"].toDouble(default_biome.patch_jitter));
  biome.background_blade_density =
      static_cast<float>(biome_obj["backgroundBladeDensity"].toDouble(
          default_biome.background_blade_density));
  biome.blade_height_min = static_cast<float>(
      biome_obj["bladeHeightMin"].toDouble(default_biome.blade_height_min));
  biome.blade_height_max = static_cast<float>(
      biome_obj["bladeHeightMax"].toDouble(default_biome.blade_height_max));
  biome.blade_width_min = static_cast<float>(
      biome_obj["bladeWidthMin"].toDouble(default_biome.blade_width_min));
  biome.blade_width_max = static_cast<float>(
      biome_obj["bladeWidthMax"].toDouble(default_biome.blade_width_max));
  biome.sway_strength = static_cast<float>(
      biome_obj["sway_strength"].toDouble(default_biome.sway_strength));
  biome.sway_speed =
      static_cast<float>(biome_obj["sway_speed"].toDouble(default_biome.sway_speed));
  biome.background_sway_variance =
      static_cast<float>(biome_obj["backgroundSwayVariance"].toDouble(
          default_biome.background_sway_variance));
  biome.background_scatter_radius =
      static_cast<float>(biome_obj["backgroundScatterRadius"].toDouble(
          default_biome.background_scatter_radius));
  biome.plant_density = static_cast<float>(
      biome_obj["plant_density"].toDouble(default_biome.plant_density));
  biome.spawn_edge_padding = static_cast<float>(
      biome_obj["spawnEdgePadding"].toDouble(default_biome.spawn_edge_padding));
}

void read_biome_surface(const QJsonObject& biome_obj,
                        const Game::Map::BiomeSettings& default_biome,
                        Game::Map::BiomeSettings& biome) {
  biome.height_noise_amplitude = static_cast<float>(
      biome_obj["heightNoiseAmplitude"].toDouble(default_biome.height_noise_amplitude));
  biome.height_noise_frequency = static_cast<float>(
      biome_obj["heightNoiseFrequency"].toDouble(default_biome.height_noise_frequency));
  biome.terrain_macro_noise_scale =
      static_cast<float>(biome_obj["terrainMacroNoiseScale"].toDouble(
          default_biome.terrain_macro_noise_scale));
  biome.terrain_detail_noise_scale =
      static_cast<float>(biome_obj["terrainDetailNoiseScale"].toDouble(
          default_biome.terrain_detail_noise_scale));
  biome.terrain_soil_height = static_cast<float>(
      biome_obj["terrainSoilHeight"].toDouble(default_biome.terrain_soil_height));
  biome.terrain_soil_sharpness = static_cast<float>(
      biome_obj["terrainSoilSharpness"].toDouble(default_biome.terrain_soil_sharpness));
  biome.terrain_rock_threshold = static_cast<float>(
      biome_obj["terrainRockThreshold"].toDouble(default_biome.terrain_rock_threshold));
  biome.terrain_rock_sharpness = static_cast<float>(
      biome_obj["terrainRockSharpness"].toDouble(default_biome.terrain_rock_sharpness));
  biome.terrain_ambient_boost = static_cast<float>(
      biome_obj["terrainAmbientBoost"].toDouble(default_biome.terrain_ambient_boost));
  biome.terrain_rock_detail_strength =
      static_cast<float>(biome_obj["terrainRockDetailStrength"].toDouble(
          default_biome.terrain_rock_detail_strength));
  if (biome_obj.contains("seed")) {
    biome.seed =
        static_cast<std::uint32_t>(biome_obj["seed"].toVariant().toULongLong());
  } else {
    biome.seed = default_biome.seed;
  }
}

void read_biome_climate(const QJsonObject& biome_obj,
                        const Game::Map::BiomeSettings& default_biome,
                        Game::Map::BiomeSettings& biome) {
  biome.snow_coverage = static_cast<float>(
      biome_obj["snowCoverage"].toDouble(default_biome.snow_coverage));
  biome.moisture_level = static_cast<float>(
      biome_obj["moistureLevel"].toDouble(default_biome.moisture_level));
  biome.crack_intensity = static_cast<float>(
      biome_obj["crackIntensity"].toDouble(default_biome.crack_intensity));
  biome.rock_exposure = static_cast<float>(
      biome_obj["rockExposure"].toDouble(default_biome.rock_exposure));
  biome.grass_saturation = static_cast<float>(
      biome_obj["grassSaturation"].toDouble(default_biome.grass_saturation));
  biome.soil_roughness = static_cast<float>(
      biome_obj["soilRoughness"].toDouble(default_biome.soil_roughness));
}

} // namespace

void write_biome(const Game::Map::BiomeSettings& biome, QJsonObject& terrain_obj) {
  const auto profiles = Game::Map::make_biome_profiles(biome);
  QJsonObject biome_obj;
  write_biome_palette(profiles.surface, profiles.climate, biome_obj);
  write_biome_scatter(profiles.scatter, profiles.wind, biome_obj);
  write_biome_surface(profiles.surface, biome_obj);
  write_biome_climate(profiles.climate, biome_obj);
  terrain_obj["biome"] = biome_obj;
}

void read_biome(const QJsonObject& json, Game::Map::BiomeSettings& biome) {
  if (!json.contains("biome")) {
    return;
  }
  const auto biome_obj = json["biome"].toObject();
  const Game::Map::BiomeSettings default_biome{};
  read_biome_palette(biome_obj, default_biome, biome);
  read_biome_scatter(biome_obj, default_biome, biome);
  read_biome_surface(biome_obj, default_biome, biome);
  read_biome_climate(biome_obj, default_biome, biome);
}

} // namespace Engine::Core::TerrainCodec
