#pragma once

#include <QVector2D>
#include <QVector3D>

namespace Render::GL {

struct TerrainChunkParams {
  static constexpr float kDefaultGrassPrimaryR = 0.30F;
  static constexpr float kDefaultGrassPrimaryG = 0.60F;
  static constexpr float kDefaultGrassPrimaryB = 0.28F;
  static constexpr float kDefaultTileSize = 1.0F;
  static constexpr float kDefaultGrassSecondaryR = 0.44F;
  static constexpr float kDefaultGrassSecondaryG = 0.70F;
  static constexpr float kDefaultGrassSecondaryB = 0.32F;
  static constexpr float kDefaultMacroNoiseScale = 0.035F;
  static constexpr float kDefaultGrassDryR = 0.72F;
  static constexpr float kDefaultGrassDryG = 0.66F;
  static constexpr float kDefaultGrassDryB = 0.48F;
  static constexpr float kDefaultDetailNoiseScale = 0.16F;
  static constexpr float kDefaultSoilColorR = 0.30F;
  static constexpr float kDefaultSoilColorG = 0.26F;
  static constexpr float kDefaultSoilColorB = 0.20F;
  static constexpr float kDefaultSlopeRockThreshold = 0.45F;
  static constexpr float kDefaultRockLowR = 0.48F;
  static constexpr float kDefaultRockLowG = 0.46F;
  static constexpr float kDefaultRockLowB = 0.44F;
  static constexpr float kDefaultSlopeRockSharpness = 3.0F;
  static constexpr float kDefaultRockHighR = 0.70F;
  static constexpr float kDefaultRockHighG = 0.71F;
  static constexpr float kDefaultRockHighB = 0.75F;
  static constexpr float kDefaultSoilBlendHeight = 0.6F;
  static constexpr float kDefaultTintComponent = 1.0F;
  static constexpr float kDefaultSoilBlendSharpness = 3.5F;
  static constexpr float kDefaultHeightNoiseStrength = 0.05F;
  static constexpr float kDefaultHeightNoiseFrequency = 0.1F;
  static constexpr float kDefaultAmbientBoost = 1.05F;
  static constexpr float kDefaultRockDetailStrength = 0.35F;
  static constexpr float kDefaultLightDirX = 0.35F;
  static constexpr float kDefaultLightDirY = 0.8F;
  static constexpr float kDefaultLightDirZ = 0.45F;
  static constexpr float kDefaultMicroBumpAmp = 0.07F;
  static constexpr float kDefaultMicroBumpFreq = 2.2F;
  static constexpr float kDefaultMicroNormalWeight = 0.65F;
  static constexpr float kDefaultAlbedoJitter = 0.05F;

  static constexpr float kDefaultSnowCoverage = 0.0F;
  static constexpr float kDefaultMoistureLevel = 0.5F;
  static constexpr float kDefaultCrackIntensity = 0.0F;
  static constexpr float kDefaultRockExposure = 0.3F;
  static constexpr float kDefaultGrassSaturation = 1.0F;
  static constexpr float kDefaultSoilRoughness = 0.5F;
  static constexpr float kDefaultSnowColorR = 0.92F;
  static constexpr float kDefaultSnowColorG = 0.94F;
  static constexpr float kDefaultSnowColorB = 0.98F;

  static auto default_grass_primary() -> QVector3D {
    return {kDefaultGrassPrimaryR, kDefaultGrassPrimaryG,
            kDefaultGrassPrimaryB};
  }
  static auto default_grass_secondary() -> QVector3D {
    return {kDefaultGrassSecondaryR, kDefaultGrassSecondaryG,
            kDefaultGrassSecondaryB};
  }
  static auto default_grass_dry() -> QVector3D {
    return {kDefaultGrassDryR, kDefaultGrassDryG, kDefaultGrassDryB};
  }
  static auto default_soil_color() -> QVector3D {
    return {kDefaultSoilColorR, kDefaultSoilColorG, kDefaultSoilColorB};
  }
  static auto default_rock_low() -> QVector3D {
    return {kDefaultRockLowR, kDefaultRockLowG, kDefaultRockLowB};
  }
  static auto default_rock_high() -> QVector3D {
    return {kDefaultRockHighR, kDefaultRockHighG, kDefaultRockHighB};
  }
  static auto default_tint() -> QVector3D {
    return {kDefaultTintComponent, kDefaultTintComponent,
            kDefaultTintComponent};
  }
  static auto default_light_direction() -> QVector3D {
    return {kDefaultLightDirX, kDefaultLightDirY, kDefaultLightDirZ};
  }
  static auto default_snow_color() -> QVector3D {
    return {kDefaultSnowColorR, kDefaultSnowColorG, kDefaultSnowColorB};
  }

  QVector3D grass_primary = default_grass_primary();
  float tile_size = kDefaultTileSize;
  QVector3D grass_secondary = default_grass_secondary();
  float macro_noise_scale = kDefaultMacroNoiseScale;
  QVector3D grass_dry = default_grass_dry();
  float detail_noise_scale = kDefaultDetailNoiseScale;
  QVector3D soil_color = default_soil_color();
  float slope_rock_threshold = kDefaultSlopeRockThreshold;
  QVector3D rock_low = default_rock_low();
  float slope_rock_sharpness = kDefaultSlopeRockSharpness;
  QVector3D rock_high = default_rock_high();
  float soil_blend_height = kDefaultSoilBlendHeight;
  QVector3D tint = default_tint();
  float soil_blend_sharpness = kDefaultSoilBlendSharpness;

  QVector2D noise_offset{0.0F, 0.0F};
  float height_noise_strength = kDefaultHeightNoiseStrength;
  float height_noise_frequency = kDefaultHeightNoiseFrequency;
  float ambient_boost = kDefaultAmbientBoost;
  float rock_detail_strength = kDefaultRockDetailStrength;
  QVector3D light_direction = default_light_direction();

  float noise_angle = 0.0F;

  float micro_bump_amp = kDefaultMicroBumpAmp;
  float micro_bump_freq = kDefaultMicroBumpFreq;
  float micro_normal_weight = kDefaultMicroNormalWeight;

  float albedo_jitter = kDefaultAlbedoJitter;

  bool is_ground_plane = false;

  float snow_coverage = kDefaultSnowCoverage;
  float moisture_level = kDefaultMoistureLevel;
  float crack_intensity = kDefaultCrackIntensity;
  float rock_exposure = kDefaultRockExposure;
  float grass_saturation = kDefaultGrassSaturation;
  float soil_roughness = kDefaultSoilRoughness;
  QVector3D snow_color = default_snow_color();
};

} // namespace Render::GL
