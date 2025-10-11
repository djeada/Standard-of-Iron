#pragma once

#include <QVector2D>
#include <QVector3D>

namespace Render::GL {

struct TerrainChunkParams {

  QVector3D grassPrimary{0.30f, 0.60f, 0.28f};
  float tileSize = 1.0f;
  QVector3D grassSecondary{0.44f, 0.70f, 0.32f};
  float macroNoiseScale = 0.035f;
  QVector3D grassDry{0.72f, 0.66f, 0.48f};
  float detailNoiseScale = 0.16f;
  QVector3D soilColor{0.30f, 0.26f, 0.20f};
  float slopeRockThreshold = 0.45f;
  QVector3D rockLow{0.48f, 0.46f, 0.44f};
  float slopeRockSharpness = 3.0f;
  QVector3D rockHigh{0.70f, 0.71f, 0.75f};
  float soilBlendHeight = 0.6f;
  QVector3D tint{1.0f, 1.0f, 1.0f};
  float soilBlendSharpness = 3.5f;

  QVector2D noiseOffset{0.0f, 0.0f};
  float heightNoiseStrength = 0.05f;
  float heightNoiseFrequency = 0.1f;
  float ambientBoost = 1.05f;
  float rockDetailStrength = 0.35f;
  QVector3D lightDirection{0.35f, 0.8f, 0.45f};

  float noiseAngle = 0.0f;

  float microBumpAmp = 0.07f;
  float microBumpFreq = 2.2f;
  float microNormalWeight = 0.65f;

  float albedoJitter = 0.05f;
  
  // Flag to indicate if this is the flat ground plane (not elevated terrain)
  bool isGroundPlane = false;
  
  float mapHalfWidth = 25.0f;
  float mapHalfHeight = 25.0f;
};

} // namespace Render::GL
