#include "terrain_pipeline.h"
#include "../backend.h"
#include "../render_constants.h"
#include "../shader_cache.h"
#include <GL/gl.h>
#include <QDebug>
#include <QOpenGLExtraFunctions>
#include <cstddef>
#include <qglobal.h>
#include <qopenglext.h>
#include <qvectornd.h>

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;
using namespace Render::GL::Geometry;

auto TerrainPipeline::initialize() -> bool {
  if (m_shaderCache == nullptr) {
    qWarning() << "TerrainPipeline::initialize: null ShaderCache";
    return false;
  }

  auto *gl = QOpenGLContext::currentContext()->extraFunctions();

  m_grassShader = m_shaderCache->get("grass_instanced");
  m_groundShader = m_shaderCache->get("ground_plane");
  m_terrainShader = m_shaderCache->get("terrain_chunk");

  if (m_grassShader == nullptr) {
    qWarning() << "TerrainPipeline: Failed to load grass_instanced shader";
  }
  if (m_groundShader == nullptr) {
    qWarning() << "TerrainPipeline: Failed to load ground_plane shader";
  }
  if (m_terrainShader == nullptr) {
    qWarning() << "TerrainPipeline: Failed to load basic (terrain) shader";
  }

  initializeGrassGeometry();

  cacheUniforms();

  return isInitialized();
}

void TerrainPipeline::shutdown() {
  shutdownGrassGeometry();
  m_grassShader = nullptr;
  m_groundShader = nullptr;
  m_terrainShader = nullptr;
}

void TerrainPipeline::cacheUniforms() {
  cacheGrassUniforms();
  cacheGroundUniforms();
  cacheTerrainUniforms();
}

auto TerrainPipeline::isInitialized() const -> bool {
  return m_grassShader != nullptr && m_groundShader != nullptr &&
         m_terrainShader != nullptr && m_grassVao != 0;
}

void TerrainPipeline::cacheGrassUniforms() {
  if (m_grassShader == nullptr) {
    return;
  }

  m_grassUniforms.view_proj = m_grassShader->uniformHandle("u_viewProj");
  m_grassUniforms.time = m_grassShader->uniformHandle("u_time");
  m_grassUniforms.wind_strength =
      m_grassShader->uniformHandle("u_windStrength");
  m_grassUniforms.wind_speed = m_grassShader->uniformHandle("u_windSpeed");
  m_grassUniforms.soil_color = m_grassShader->uniformHandle("u_soilColor");
  m_grassUniforms.light_dir = m_grassShader->uniformHandle("u_lightDir");
}

void TerrainPipeline::cacheGroundUniforms() {
  if (m_groundShader == nullptr) {
    return;
  }

  m_groundUniforms.mvp = m_groundShader->uniformHandle("u_mvp");
  m_groundUniforms.model = m_groundShader->uniformHandle("u_model");
  m_groundUniforms.grass_primary =
      m_groundShader->uniformHandle("u_grassPrimary");
  m_groundUniforms.grass_secondary =
      m_groundShader->uniformHandle("u_grassSecondary");
  m_groundUniforms.grass_dry = m_groundShader->uniformHandle("u_grassDry");
  m_groundUniforms.soil_color = m_groundShader->uniformHandle("u_soilColor");
  m_groundUniforms.tint = m_groundShader->uniformHandle("u_tint");
  m_groundUniforms.noise_offset =
      m_groundShader->uniformHandle("u_noiseOffset");
  m_groundUniforms.tile_size = m_groundShader->uniformHandle("u_tileSize");
  m_groundUniforms.macro_noise_scale =
      m_groundShader->uniformHandle("u_macroNoiseScale");
  m_groundUniforms.detail_noise_scale =
      m_groundShader->uniformHandle("u_detailNoiseScale");
  m_groundUniforms.soil_blend_height =
      m_groundShader->uniformHandle("u_soilBlendHeight");
  m_groundUniforms.soil_blend_sharpness =
      m_groundShader->uniformHandle("u_soilBlendSharpness");
  m_groundUniforms.height_noise_strength =
      m_groundShader->uniformHandle("u_heightNoiseStrength");
  m_groundUniforms.height_noise_frequency =
      m_groundShader->uniformHandle("u_heightNoiseFrequency");
  m_groundUniforms.ambient_boost =
      m_groundShader->uniformHandle("u_ambientBoost");
  m_groundUniforms.light_dir = m_groundShader->uniformHandle("u_lightDir");

  m_groundUniforms.snow_coverage =
      m_groundShader->uniformHandle("u_snowCoverage");
  m_groundUniforms.moisture_level =
      m_groundShader->uniformHandle("u_moistureLevel");
  m_groundUniforms.crack_intensity =
      m_groundShader->uniformHandle("u_crackIntensity");
  m_groundUniforms.grass_saturation =
      m_groundShader->uniformHandle("u_grassSaturation");
  m_groundUniforms.soil_roughness =
      m_groundShader->uniformHandle("u_soilRoughness");
  m_groundUniforms.snow_color = m_groundShader->uniformHandle("u_snowColor");
}

void TerrainPipeline::cacheTerrainUniforms() {
  if (m_terrainShader == nullptr) {
    return;
  }

  m_terrainUniforms.mvp = m_terrainShader->uniformHandle("u_mvp");
  m_terrainUniforms.model = m_terrainShader->uniformHandle("u_model");
  m_terrainUniforms.grass_primary =
      m_terrainShader->uniformHandle("u_grassPrimary");
  m_terrainUniforms.grass_secondary =
      m_terrainShader->uniformHandle("u_grassSecondary");
  m_terrainUniforms.grass_dry = m_terrainShader->uniformHandle("u_grassDry");
  m_terrainUniforms.soil_color = m_terrainShader->uniformHandle("u_soilColor");
  m_terrainUniforms.rock_low = m_terrainShader->uniformHandle("u_rockLow");
  m_terrainUniforms.rock_high = m_terrainShader->uniformHandle("u_rockHigh");
  m_terrainUniforms.tint = m_terrainShader->uniformHandle("u_tint");
  m_terrainUniforms.noise_offset =
      m_terrainShader->uniformHandle("u_noiseOffset");
  m_terrainUniforms.tile_size = m_terrainShader->uniformHandle("u_tileSize");
  m_terrainUniforms.macro_noise_scale =
      m_terrainShader->uniformHandle("u_macroNoiseScale");
  m_terrainUniforms.detail_noise_scale =
      m_terrainShader->uniformHandle("u_detailNoiseScale");
  m_terrainUniforms.slope_rock_threshold =
      m_terrainShader->uniformHandle("u_slopeRockThreshold");
  m_terrainUniforms.slope_rock_sharpness =
      m_terrainShader->uniformHandle("u_slopeRockSharpness");
  m_terrainUniforms.soil_blend_height =
      m_terrainShader->uniformHandle("u_soilBlendHeight");
  m_terrainUniforms.soil_blend_sharpness =
      m_terrainShader->uniformHandle("u_soilBlendSharpness");
  m_terrainUniforms.height_noise_strength =
      m_terrainShader->uniformHandle("u_heightNoiseStrength");
  m_terrainUniforms.height_noise_frequency =
      m_terrainShader->uniformHandle("u_heightNoiseFrequency");
  m_terrainUniforms.ambient_boost =
      m_terrainShader->uniformHandle("u_ambientBoost");
  m_terrainUniforms.rock_detail_strength =
      m_terrainShader->uniformHandle("u_rockDetailStrength");
  m_terrainUniforms.light_dir = m_terrainShader->uniformHandle("u_lightDir");

  m_terrainUniforms.snow_coverage =
      m_terrainShader->uniformHandle("u_snowCoverage");
  m_terrainUniforms.moisture_level =
      m_terrainShader->uniformHandle("u_moistureLevel");
  m_terrainUniforms.crack_intensity =
      m_terrainShader->uniformHandle("u_crackIntensity");
  m_terrainUniforms.rock_exposure =
      m_terrainShader->uniformHandle("u_rockExposure");
  m_terrainUniforms.grass_saturation =
      m_terrainShader->uniformHandle("u_grassSaturation");
  m_terrainUniforms.soil_roughness =
      m_terrainShader->uniformHandle("u_soilRoughness");
  m_terrainUniforms.snow_color = m_terrainShader->uniformHandle("u_snowColor");
}

void TerrainPipeline::initializeGrassGeometry() {
  auto *gl = QOpenGLContext::currentContext()->extraFunctions();
  if (gl == nullptr) {
    qWarning() << "TerrainPipeline::initializeGrassGeometry: no OpenGL context";
    return;
  }

  shutdownGrassGeometry();

  struct GrassVertex {
    QVector3D position;
    QVector2D uv;
  };

  const GrassVertex blade_vertices[6] = {
      {{-0.5F, 0.0F, 0.0F}, {0.0F, 0.0F}},
      {{0.5F, 0.0F, 0.0F}, {1.0F, 0.0F}},
      {{-0.35F, 1.0F, 0.0F}, {0.1F, 1.0F}},
      {{-0.35F, 1.0F, 0.0F}, {0.1F, 1.0F}},
      {{0.5F, 0.0F, 0.0F}, {1.0F, 0.0F}},
      {{0.35F, 1.0F, 0.0F}, {0.9F, 1.0F}},
  };

  gl->glGenVertexArrays(1, &m_grassVao);
  gl->glBindVertexArray(m_grassVao);

  gl->glGenBuffers(1, &m_grassVertexBuffer);
  gl->glBindBuffer(GL_ARRAY_BUFFER, m_grassVertexBuffer);
  gl->glBufferData(GL_ARRAY_BUFFER, sizeof(blade_vertices), blade_vertices,
                   GL_STATIC_DRAW);
  m_grassVertexCount = GrassBladeVertexCount;

  gl->glEnableVertexAttribArray(Position);
  gl->glVertexAttribPointer(
      Position, Vec3, GL_FLOAT, GL_FALSE, sizeof(GrassVertex),
      reinterpret_cast<void *>(offsetof(GrassVertex, position)));

  gl->glEnableVertexAttribArray(Normal);
  gl->glVertexAttribPointer(
      Normal, Vec2, GL_FLOAT, GL_FALSE, sizeof(GrassVertex),
      reinterpret_cast<void *>(offsetof(GrassVertex, uv)));

  gl->glEnableVertexAttribArray(TexCoord);
  gl->glVertexAttribDivisor(TexCoord, 1);
  gl->glEnableVertexAttribArray(InstancePosition);
  gl->glVertexAttribDivisor(InstancePosition, 1);
  gl->glEnableVertexAttribArray(InstanceScale);
  gl->glVertexAttribDivisor(InstanceScale, 1);

  gl->glBindVertexArray(0);
  gl->glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void TerrainPipeline::shutdownGrassGeometry() {
  auto *gl = QOpenGLContext::currentContext()->extraFunctions();
  if (gl == nullptr) {
    return;
  }

  if (m_grassVertexBuffer != 0U) {
    gl->glDeleteBuffers(1, &m_grassVertexBuffer);
    m_grassVertexBuffer = 0;
  }
  if (m_grassVao != 0U) {
    gl->glDeleteVertexArrays(1, &m_grassVao);
    m_grassVao = 0;
  }
  m_grassVertexCount = 0;
}

} // namespace Render::GL::BackendPipelines
