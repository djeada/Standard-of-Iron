#include "ground_renderer.h"
#include "../draw_queue.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "ground/terrain_gpu.h"
#include <algorithm>
#include <cmath>
#include <qmatrix4x4.h>
#include <qvectornd.h>

namespace Render::GL {

namespace {
const QMatrix4x4 k_identity_matrix;

inline auto saturate(const QVector3D &c) -> QVector3D {
  return {std::clamp(c.x(), 0.0F, 1.0F), std::clamp(c.y(), 0.0F, 1.0F),
          std::clamp(c.z(), 0.0F, 1.0F)};
}
} // namespace

static auto clamp01(const QVector3D &c) -> QVector3D { return saturate(c); }

void GroundRenderer::recomputeModel() {
  QMatrix4x4 new_model = k_identity_matrix;
  new_model.translate(0.0F, -0.5F, 0.0F);

  if (m_width > 0 && m_height > 0) {
    const float scale_x = std::sqrt(float(m_width)) * m_tile_size;
    const float scale_z = std::sqrt(float(m_height)) * m_tile_size;
    new_model.scale(scale_x, 1.0F, scale_z);
  } else {
    new_model.scale(m_extent, 1.0F, m_extent);
  }

  if (new_model != m_model) {
    m_model = new_model;
    m_modelDirty = true;
  }
}

void GroundRenderer::updateNoiseOffset() {
  const float span_x = (m_width > 0 ? float(m_width) * m_tile_size : m_extent);
  const float span_z =
      (m_height > 0 ? float(m_height) * m_tile_size : m_extent);
  const auto seed = static_cast<float>(m_biomeSettings.seed % 1024U);

  QVector2D new_offset;
  new_offset.setX(span_x * 0.37F + seed * 0.21F);
  new_offset.setY(span_z * 0.43F + seed * 0.17F);

  m_noiseAngle = std::fmod(seed * 0.6180339887F, 1.0F) * 6.28318530718F;

  if (new_offset != m_noiseOffset) {
    m_noiseOffset = new_offset;
    invalidateParamsCache();
  }
}

auto GroundRenderer::buildParams() const -> TerrainChunkParams {
  if (m_cachedParamsValid) {
    return m_cachedParams;
  }

  TerrainChunkParams params;

  const QVector3D primary = m_biomeSettings.grassPrimary * 0.97F;
  const QVector3D secondary = m_biomeSettings.grassSecondary * 0.93F;
  const QVector3D dry = m_biomeSettings.grassDry * 0.90F;
  const QVector3D soil = m_biomeSettings.soilColor * 0.68F;

  params.grassPrimary = saturate(primary);
  params.grassSecondary = saturate(secondary);
  params.grassDry = saturate(dry);
  params.soilColor = saturate(soil);
  params.rockLow = saturate(m_biomeSettings.rockLow);
  params.rockHigh = saturate(m_biomeSettings.rockHigh);

  params.tint = QVector3D(0.96F, 0.98F, 0.96F);

  params.tile_size = std::max(0.25F, m_tile_size);

  params.macroNoiseScale =
      std::max(0.012F, m_biomeSettings.terrainMacroNoiseScale * 0.60F);
  params.detail_noiseScale =
      std::max(0.045F, m_biomeSettings.terrainDetailNoiseScale * 0.75F);

  params.slopeRockThreshold =
      std::clamp(m_biomeSettings.terrainRockThreshold + 0.30F, 0.40F, 0.90F);
  params.slopeRockSharpness =
      std::clamp(m_biomeSettings.terrainRockSharpness + 1.5F, 2.0F, 6.0F);

  params.soilBlendHeight = m_biomeSettings.terrainSoilHeight - 1.25F;
  params.soilBlendSharpness =
      std::clamp(m_biomeSettings.terrainSoilSharpness * 0.75F, 1.5F, 5.0F);

  params.noiseOffset = m_noiseOffset;
  params.noiseAngle = m_noiseAngle;

  float target_amp;
  float target_freq;
  if (m_biomeSettings.groundIrregularityEnabled) {

    target_amp =
        std::clamp(m_biomeSettings.irregularityAmplitude * 0.85F, 0.15F, 0.70F);
    target_freq = std::max(0.45F, m_biomeSettings.irregularityScale * 2.5F);
  } else {

    target_amp =
        std::clamp(m_biomeSettings.heightNoiseAmplitude * 0.22F, 0.10F, 0.20F);
    target_freq = std::max(0.6F, m_biomeSettings.heightNoiseFrequency * 1.05F);
  }
  params.heightNoiseStrength = target_amp;
  params.heightNoiseFrequency = target_freq;

  params.microBumpAmp = 0.07F;
  params.microBumpFreq = 2.2F;
  params.microNormalWeight = 0.65F;

  params.albedoJitter = 0.05F;

  params.ambientBoost = m_biomeSettings.terrainAmbientBoost * 0.85F;

  params.rockDetailStrength = m_biomeSettings.terrainRockDetailStrength * 0.18F;

  QVector3D const l(0.35F, 0.85F, 0.42F);
  params.light_direction = l.normalized();

  params.isGroundPlane = true;

  // Ground-type-specific shader parameters
  params.snowCoverage = std::clamp(m_biomeSettings.snowCoverage, 0.0F, 1.0F);
  params.moistureLevel = std::clamp(m_biomeSettings.moistureLevel, 0.0F, 1.0F);
  params.crackIntensity =
      std::clamp(m_biomeSettings.crackIntensity, 0.0F, 1.0F);
  params.rockExposure = std::clamp(m_biomeSettings.rockExposure, 0.0F, 1.0F);
  params.grassSaturation =
      std::clamp(m_biomeSettings.grassSaturation, 0.0F, 1.5F);
  params.soilRoughness = std::clamp(m_biomeSettings.soilRoughness, 0.0F, 1.0F);
  params.snowColor = saturate(m_biomeSettings.snowColor);

  m_cachedParams = params;
  m_cachedParamsValid = true;
  return params;
}

void GroundRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  syncBiomeFromService();

  if (resources == nullptr) {
    return;
  }

  if (m_hasBiome) {
    Mesh *plane = resources->ground();
    if (plane != nullptr) {
      const TerrainChunkParams params = buildParams();

      const bool model_changed =
          m_modelDirty || (m_lastSubmittedModel != m_model);
      const bool state_changed =
          (m_lastSubmittedStateVersion != m_stateVersion);
      (void)model_changed;
      (void)state_changed;

      renderer.terrainChunk(plane, m_model, params, 0x0040U, true, +0.0008F);

      m_lastSubmittedModel = m_model;
      m_modelDirty = false;
      m_lastSubmittedStateVersion = m_stateVersion;
      return;
    }
  }

  const float cell = (m_tile_size > 0.0F ? m_tile_size : 1.0F);
  const float extent = (m_width > 0 && m_height > 0)
                           ? std::max(m_width, m_height) * m_tile_size * 0.5F
                           : m_extent;
  renderer.grid(m_model, m_color, cell, 0.06F, extent);
}

void GroundRenderer::syncBiomeFromService() {
  auto &service = Game::Map::TerrainService::instance();
  if (!service.isInitialized()) {
    return;
  }
  const auto &current = service.biomeSettings();
  if (!m_hasBiome || !biomeEquals(current, m_biomeSettings)) {
    m_biomeSettings = current;
    m_hasBiome = true;
    updateNoiseOffset();
    invalidateParamsCache();
  }
}

auto GroundRenderer::biomeEquals(const Game::Map::BiomeSettings &a,
                                 const Game::Map::BiomeSettings &b) -> bool {
  return a.groundType == b.groundType && a.grassPrimary == b.grassPrimary &&
         a.grassSecondary == b.grassSecondary && a.grassDry == b.grassDry &&
         a.soilColor == b.soilColor && a.rockLow == b.rockLow &&
         a.rockHigh == b.rockHigh &&
         a.terrainMacroNoiseScale == b.terrainMacroNoiseScale &&
         a.terrainDetailNoiseScale == b.terrainDetailNoiseScale &&
         a.terrainSoilHeight == b.terrainSoilHeight &&
         a.terrainSoilSharpness == b.terrainSoilSharpness &&
         a.terrainRockThreshold == b.terrainRockThreshold &&
         a.terrainRockSharpness == b.terrainRockSharpness &&
         a.terrainAmbientBoost == b.terrainAmbientBoost &&
         a.terrainRockDetailStrength == b.terrainRockDetailStrength &&
         a.heightNoiseAmplitude == b.heightNoiseAmplitude &&
         a.heightNoiseFrequency == b.heightNoiseFrequency &&
         a.groundIrregularityEnabled == b.groundIrregularityEnabled &&
         a.irregularityScale == b.irregularityScale &&
         a.irregularityAmplitude == b.irregularityAmplitude &&
         a.seed == b.seed && a.snowCoverage == b.snowCoverage &&
         a.moistureLevel == b.moistureLevel &&
         a.crackIntensity == b.crackIntensity &&
         a.rockExposure == b.rockExposure &&
         a.grassSaturation == b.grassSaturation &&
         a.soilRoughness == b.soilRoughness && a.snowColor == b.snowColor;
}

} // namespace Render::GL
