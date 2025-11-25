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
  // Ground-type-specific defaults
  static constexpr float kDefaultSnowCoverage = 0.0F;
  static constexpr float kDefaultMoistureLevel = 0.5F;
  static constexpr float kDefaultCrackIntensity = 0.0F;
  static constexpr float kDefaultRockExposure = 0.3F;
  static constexpr float kDefaultGrassSaturation = 1.0F;
  static constexpr float kDefaultSoilRoughness = 0.5F;
  static constexpr float kDefaultSnowColorR = 0.92F;
  static constexpr float kDefaultSnowColorG = 0.94F;
  static constexpr float kDefaultSnowColorB = 0.98F;

  static auto defaultGrassPrimary() -> QVector3D {
    return {kDefaultGrassPrimaryR, kDefaultGrassPrimaryG,
            kDefaultGrassPrimaryB};
  }
  static auto defaultGrassSecondary() -> QVector3D {
    return {kDefaultGrassSecondaryR, kDefaultGrassSecondaryG,
            kDefaultGrassSecondaryB};
  }
  static auto defaultGrassDry() -> QVector3D {
    return {kDefaultGrassDryR, kDefaultGrassDryG, kDefaultGrassDryB};
  }
  static auto defaultSoilColor() -> QVector3D {
    return {kDefaultSoilColorR, kDefaultSoilColorG, kDefaultSoilColorB};
  }
  static auto defaultRockLow() -> QVector3D {
    return {kDefaultRockLowR, kDefaultRockLowG, kDefaultRockLowB};
  }
  static auto defaultRockHigh() -> QVector3D {
    return {kDefaultRockHighR, kDefaultRockHighG, kDefaultRockHighB};
  }
  static auto defaultTint() -> QVector3D {
    return {kDefaultTintComponent, kDefaultTintComponent,
            kDefaultTintComponent};
  }
  static auto defaultLightDirection() -> QVector3D {
    return {kDefaultLightDirX, kDefaultLightDirY, kDefaultLightDirZ};
  }
  static auto defaultSnowColor() -> QVector3D {
    return {kDefaultSnowColorR, kDefaultSnowColorG, kDefaultSnowColorB};
  }

  QVector3D grassPrimary = defaultGrassPrimary();
  float tile_size = kDefaultTileSize;
  QVector3D grassSecondary = defaultGrassSecondary();
  float macroNoiseScale = kDefaultMacroNoiseScale;
  QVector3D grassDry = defaultGrassDry();
  float detail_noiseScale = kDefaultDetailNoiseScale;
  QVector3D soilColor = defaultSoilColor();
  float slopeRockThreshold = kDefaultSlopeRockThreshold;
  QVector3D rockLow = defaultRockLow();
  float slopeRockSharpness = kDefaultSlopeRockSharpness;
  QVector3D rockHigh = defaultRockHigh();
  float soilBlendHeight = kDefaultSoilBlendHeight;
  QVector3D tint = defaultTint();
  float soilBlendSharpness = kDefaultSoilBlendSharpness;

  QVector2D noiseOffset{0.0F, 0.0F};
  float heightNoiseStrength = kDefaultHeightNoiseStrength;
  float heightNoiseFrequency = kDefaultHeightNoiseFrequency;
  float ambientBoost = kDefaultAmbientBoost;
  float rockDetailStrength = kDefaultRockDetailStrength;
  QVector3D light_direction = defaultLightDirection();

  float noiseAngle = 0.0F;

  float microBumpAmp = kDefaultMicroBumpAmp;
  float microBumpFreq = kDefaultMicroBumpFreq;
  float microNormalWeight = kDefaultMicroNormalWeight;

  float albedoJitter = kDefaultAlbedoJitter;

  bool isGroundPlane = false;

  // Ground-type-specific shader parameters
  float snowCoverage = kDefaultSnowCoverage;
  float moistureLevel = kDefaultMoistureLevel;
  float crackIntensity = kDefaultCrackIntensity;
  float rockExposure = kDefaultRockExposure;
  float grassSaturation = kDefaultGrassSaturation;
  float soilRoughness = kDefaultSoilRoughness;
  QVector3D snowColor = defaultSnowColor();
};

} // namespace Render::GL
