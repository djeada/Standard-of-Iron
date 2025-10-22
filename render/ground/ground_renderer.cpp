#include "ground_renderer.h"
#include "../draw_queue.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include <algorithm>
#include <cmath>

namespace Render {
namespace GL {

namespace {
const QMatrix4x4 kIdentityMatrix;

inline QVector3D saturate(const QVector3D &c) {
  return QVector3D(std::clamp(c.x(), 0.0f, 1.0f), std::clamp(c.y(), 0.0f, 1.0f),
                   std::clamp(c.z(), 0.0f, 1.0f));
}
} // namespace

static QVector3D clamp01(const QVector3D &c) { return saturate(c); }

void GroundRenderer::recomputeModel() {
  QMatrix4x4 newModel = kIdentityMatrix;
  newModel.translate(0.0f, -0.5f, 0.0f);

  if (m_width > 0 && m_height > 0) {
    const float scaleX = std::sqrt(float(m_width)) * m_tileSize;
    const float scaleZ = std::sqrt(float(m_height)) * m_tileSize;
    newModel.scale(scaleX, 1.0f, scaleZ);
  } else {
    newModel.scale(m_extent, 1.0f, m_extent);
  }

  if (newModel != m_model) {
    m_model = newModel;
    m_modelDirty = true;
  }
}

void GroundRenderer::updateNoiseOffset() {
  const float spanX = (m_width > 0 ? float(m_width) * m_tileSize : m_extent);
  const float spanZ = (m_height > 0 ? float(m_height) * m_tileSize : m_extent);
  const float seed = static_cast<float>(m_biomeSettings.seed % 1024u);

  QVector2D newOffset;
  newOffset.setX(spanX * 0.37f + seed * 0.21f);
  newOffset.setY(spanZ * 0.43f + seed * 0.17f);

  m_noiseAngle = std::fmod(seed * 0.6180339887f, 1.0f) * 6.28318530718f;

  if (newOffset != m_noiseOffset) {
    m_noiseOffset = newOffset;
    invalidateParamsCache();
  }
}

TerrainChunkParams GroundRenderer::buildParams() const {
  if (m_cachedParamsValid) {
    return m_cachedParams;
  }

  TerrainChunkParams params;

  const QVector3D primary = m_biomeSettings.grassPrimary * 0.97f;
  const QVector3D secondary = m_biomeSettings.grassSecondary * 0.93f;
  const QVector3D dry = m_biomeSettings.grassDry * 0.90f;
  const QVector3D soil = m_biomeSettings.soilColor * 0.68f;

  params.grassPrimary = saturate(primary);
  params.grassSecondary = saturate(secondary);
  params.grassDry = saturate(dry);
  params.soilColor = saturate(soil);
  params.rockLow = saturate(m_biomeSettings.rockLow);
  params.rockHigh = saturate(m_biomeSettings.rockHigh);

  params.tint = QVector3D(0.96f, 0.98f, 0.96f);

  params.tileSize = std::max(0.25f, m_tileSize);

  params.macroNoiseScale =
      std::max(0.012f, m_biomeSettings.terrainMacroNoiseScale * 0.60f);
  params.detailNoiseScale =
      std::max(0.045f, m_biomeSettings.terrainDetailNoiseScale * 0.75f);

  params.slopeRockThreshold = 0.72f;
  params.slopeRockSharpness = 4.5f;

  params.soilBlendHeight = -0.65f;
  params.soilBlendSharpness = 2.6f;

  params.noiseOffset = m_noiseOffset;
  params.noiseAngle = m_noiseAngle;

  const float targetAmp =
      std::clamp(m_biomeSettings.heightNoiseAmplitude * 0.22f, 0.10f, 0.20f);
  params.heightNoiseStrength = targetAmp;

  params.heightNoiseFrequency =
      std::max(0.6f, m_biomeSettings.heightNoiseFrequency * 1.05f);

  params.microBumpAmp = 0.07f;
  params.microBumpFreq = 2.2f;
  params.microNormalWeight = 0.65f;

  params.albedoJitter = 0.05f;

  params.ambientBoost = m_biomeSettings.terrainAmbientBoost * 0.85f;

  params.rockDetailStrength = m_biomeSettings.terrainRockDetailStrength * 0.18f;

  QVector3D L(0.35f, 0.85f, 0.42f);
  params.lightDirection = L.normalized();

  params.isGroundPlane = true;

  m_cachedParams = params;
  m_cachedParamsValid = true;
  return params;
}

void GroundRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  if (!resources)
    return;

  if (m_hasBiome) {
    Mesh *plane = resources->ground();
    if (plane) {
      const TerrainChunkParams params = buildParams();

      const bool modelChanged =
          m_modelDirty || (m_lastSubmittedModel != m_model);
      const bool stateChanged = (m_lastSubmittedStateVersion != m_stateVersion);
      (void)modelChanged;
      (void)stateChanged;

      renderer.terrainChunk(plane, m_model, params, 0x0040u, true, +0.0008f);

      m_lastSubmittedModel = m_model;
      m_modelDirty = false;
      m_lastSubmittedStateVersion = m_stateVersion;
      return;
    }
  }

  const float cell = (m_tileSize > 0.0f ? m_tileSize : 1.0f);
  const float extent = (m_width > 0 && m_height > 0)
                           ? std::max(m_width, m_height) * m_tileSize * 0.5f
                           : m_extent;
  renderer.grid(m_model, m_color, cell, 0.06f, extent);
}

} // namespace GL
} // namespace Render
