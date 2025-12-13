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

void GroundRenderer::update_noise_offset() {
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

auto GroundRenderer::build_params() const -> TerrainChunkParams {
  if (m_cachedParamsValid) {
    return m_cachedParams;
  }

  TerrainChunkParams params;

  const QVector3D primary = m_biomeSettings.grass_primary * 0.97F;
  const QVector3D secondary = m_biomeSettings.grass_secondary * 0.93F;
  const QVector3D dry = m_biomeSettings.grass_dry * 0.90F;
  const QVector3D soil = m_biomeSettings.soil_color * 0.68F;

  params.grass_primary = saturate(primary);
  params.grass_secondary = saturate(secondary);
  params.grass_dry = saturate(dry);
  params.soil_color = saturate(soil);
  params.rock_low = saturate(m_biomeSettings.rock_low);
  params.rock_high = saturate(m_biomeSettings.rock_high);

  params.tint = QVector3D(0.96F, 0.98F, 0.96F);

  params.tile_size = std::max(0.25F, m_tile_size);

  params.macro_noise_scale =
      std::max(0.012F, m_biomeSettings.terrain_macro_noise_scale * 0.60F);
  params.detail_noise_scale =
      std::max(0.045F, m_biomeSettings.terrain_detail_noise_scale * 0.75F);

  params.slope_rock_threshold =
      std::clamp(m_biomeSettings.terrain_rock_threshold + 0.30F, 0.40F, 0.90F);
  params.slope_rock_sharpness =
      std::clamp(m_biomeSettings.terrain_rock_sharpness + 1.5F, 2.0F, 6.0F);

  params.soil_blend_height = m_biomeSettings.terrain_soil_height - 1.25F;
  params.soil_blend_sharpness =
      std::clamp(m_biomeSettings.terrain_soil_sharpness * 0.75F, 1.5F, 5.0F);

  params.noise_offset = m_noiseOffset;
  params.noise_angle = m_noiseAngle;

  float target_amp;
  float target_freq;
  if (m_biomeSettings.ground_irregularity_enabled) {

    target_amp = std::clamp(m_biomeSettings.irregularity_amplitude * 0.85F,
                            0.15F, 0.70F);
    target_freq = std::max(0.45F, m_biomeSettings.irregularity_scale * 2.5F);
  } else {

    target_amp = std::clamp(m_biomeSettings.height_noise_amplitude * 0.22F,
                            0.10F, 0.20F);
    target_freq =
        std::max(0.6F, m_biomeSettings.height_noise_frequency * 1.05F);
  }
  params.height_noise_strength = target_amp;
  params.height_noise_frequency = target_freq;

  params.micro_bump_amp = 0.07F;
  params.micro_bump_freq = 2.2F;
  params.micro_normal_weight = 0.65F;

  params.albedo_jitter = 0.05F;

  params.ambient_boost = m_biomeSettings.terrain_ambient_boost * 0.85F;

  params.rock_detail_strength =
      m_biomeSettings.terrain_rock_detail_strength * 0.18F;

  QVector3D const l(0.35F, 0.85F, 0.42F);
  params.light_direction = l.normalized();

  params.is_ground_plane = true;

  params.snow_coverage = std::clamp(m_biomeSettings.snow_coverage, 0.0F, 1.0F);
  params.moisture_level =
      std::clamp(m_biomeSettings.moisture_level, 0.0F, 1.0F);
  params.crack_intensity =
      std::clamp(m_biomeSettings.crack_intensity, 0.0F, 1.0F);
  params.rock_exposure = std::clamp(m_biomeSettings.rock_exposure, 0.0F, 1.0F);
  params.grass_saturation =
      std::clamp(m_biomeSettings.grass_saturation, 0.0F, 1.5F);
  params.soil_roughness =
      std::clamp(m_biomeSettings.soil_roughness, 0.0F, 1.0F);
  params.snow_color = saturate(m_biomeSettings.snow_color);

  m_cachedParams = params;
  m_cachedParamsValid = true;
  return params;
}

void GroundRenderer::submit(Renderer &renderer, ResourceManager *resources) {
  sync_biome_from_service();

  if (resources == nullptr) {
    return;
  }

  if (m_hasBiome) {
    Mesh *plane = resources->ground();
    if (plane != nullptr) {
      const TerrainChunkParams params = build_params();

      const bool model_changed =
          m_modelDirty || (m_lastSubmittedModel != m_model);
      const bool state_changed =
          (m_lastSubmittedStateVersion != m_stateVersion);
      (void)model_changed;
      (void)state_changed;

      renderer.terrain_chunk(plane, m_model, params, 0x0040U, true, +0.0008F);

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

void GroundRenderer::sync_biome_from_service() {
  auto &service = Game::Map::TerrainService::instance();
  if (!service.isInitialized()) {
    return;
  }
  const auto &current = service.biomeSettings();
  if (!m_hasBiome || !biomeEquals(current, m_biomeSettings)) {
    m_biomeSettings = current;
    m_hasBiome = true;
    update_noise_offset();
    invalidateParamsCache();
  }
}

auto GroundRenderer::biomeEquals(const Game::Map::BiomeSettings &a,
                                 const Game::Map::BiomeSettings &b) -> bool {
  return a.ground_type == b.ground_type && a.grass_primary == b.grass_primary &&
         a.grass_secondary == b.grass_secondary && a.grass_dry == b.grass_dry &&
         a.soil_color == b.soil_color && a.rock_low == b.rock_low &&
         a.rock_high == b.rock_high &&
         a.terrain_macro_noise_scale == b.terrain_macro_noise_scale &&
         a.terrain_detail_noise_scale == b.terrain_detail_noise_scale &&
         a.terrain_soil_height == b.terrain_soil_height &&
         a.terrain_soil_sharpness == b.terrain_soil_sharpness &&
         a.terrain_rock_threshold == b.terrain_rock_threshold &&
         a.terrain_rock_sharpness == b.terrain_rock_sharpness &&
         a.terrain_ambient_boost == b.terrain_ambient_boost &&
         a.terrain_rock_detail_strength == b.terrain_rock_detail_strength &&
         a.height_noise_amplitude == b.height_noise_amplitude &&
         a.height_noise_frequency == b.height_noise_frequency &&
         a.ground_irregularity_enabled == b.ground_irregularity_enabled &&
         a.irregularity_scale == b.irregularity_scale &&
         a.irregularity_amplitude == b.irregularity_amplitude &&
         a.seed == b.seed && a.snow_coverage == b.snow_coverage &&
         a.moisture_level == b.moisture_level &&
         a.crack_intensity == b.crack_intensity &&
         a.rock_exposure == b.rock_exposure &&
         a.grass_saturation == b.grass_saturation &&
         a.soil_roughness == b.soil_roughness && a.snow_color == b.snow_color;
}

} // namespace Render::GL
