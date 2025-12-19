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

  initialize_grass_geometry();

  cache_uniforms();

  return is_initialized();
}

void TerrainPipeline::shutdown() {
  shutdown_grass_geometry();
  m_grassShader = nullptr;
  m_groundShader = nullptr;
  m_terrainShader = nullptr;
}

void TerrainPipeline::cache_uniforms() {
  cache_grass_uniforms();
  cache_ground_uniforms();
  cache_terrain_uniforms();
}

auto TerrainPipeline::is_initialized() const -> bool {
  return m_grassShader != nullptr && m_groundShader != nullptr &&
         m_terrainShader != nullptr && m_grassVao != 0;
}

void TerrainPipeline::cache_grass_uniforms() {
  if (m_grassShader == nullptr) {
    return;
  }

  m_grassUniforms.view_proj = m_grassShader->uniform_handle("u_viewProj");
  m_grassUniforms.time = m_grassShader->uniform_handle("u_time");
  m_grassUniforms.wind_strength =
      m_grassShader->uniform_handle("u_windStrength");
  m_grassUniforms.wind_speed = m_grassShader->uniform_handle("u_windSpeed");
  m_grassUniforms.soil_color = m_grassShader->uniform_handle("u_soilColor");
  m_grassUniforms.light_dir = m_grassShader->uniform_handle("u_lightDir");
}

void TerrainPipeline::cache_ground_uniforms() {
  if (m_groundShader == nullptr) {
    return;
  }

  m_groundUniforms.mvp = m_groundShader->uniform_handle("u_mvp");
  m_groundUniforms.model = m_groundShader->uniform_handle("u_model");
  m_groundUniforms.grass_primary =
      m_groundShader->uniform_handle("u_grassPrimary");
  m_groundUniforms.grass_secondary =
      m_groundShader->uniform_handle("u_grassSecondary");
  m_groundUniforms.grass_dry = m_groundShader->uniform_handle("u_grassDry");
  m_groundUniforms.soil_color = m_groundShader->uniform_handle("u_soilColor");
  m_groundUniforms.tint = m_groundShader->uniform_handle("u_tint");
  m_groundUniforms.noise_offset =
      m_groundShader->uniform_handle("u_noiseOffset");
  m_groundUniforms.tile_size = m_groundShader->uniform_handle("u_tileSize");
  m_groundUniforms.macro_noise_scale =
      m_groundShader->uniform_handle("u_macroNoiseScale");
  m_groundUniforms.detail_noise_scale =
      m_groundShader->uniform_handle("u_detailNoiseScale");
  m_groundUniforms.soil_blend_height =
      m_groundShader->uniform_handle("u_soilBlendHeight");
  m_groundUniforms.soil_blend_sharpness =
      m_groundShader->uniform_handle("u_soilBlendSharpness");
  m_groundUniforms.height_noise_strength =
      m_groundShader->uniform_handle("u_heightNoiseStrength");
  m_groundUniforms.height_noise_frequency =
      m_groundShader->uniform_handle("u_heightNoiseFrequency");
  m_groundUniforms.ambient_boost =
      m_groundShader->uniform_handle("u_ambientBoost");
  m_groundUniforms.light_dir = m_groundShader->uniform_handle("u_lightDir");

  m_groundUniforms.snow_coverage =
      m_groundShader->uniform_handle("u_snowCoverage");
  m_groundUniforms.moisture_level =
      m_groundShader->uniform_handle("u_moistureLevel");
  m_groundUniforms.crack_intensity =
      m_groundShader->uniform_handle("u_crackIntensity");
  m_groundUniforms.grass_saturation =
      m_groundShader->uniform_handle("u_grassSaturation");
  m_groundUniforms.soil_roughness =
      m_groundShader->uniform_handle("u_soilRoughness");
  m_groundUniforms.snow_color = m_groundShader->uniform_handle("u_snowColor");
}

void TerrainPipeline::cache_terrain_uniforms() {
  if (m_terrainShader == nullptr) {
    return;
  }

  m_terrainUniforms.mvp = m_terrainShader->uniform_handle("u_mvp");
  m_terrainUniforms.model = m_terrainShader->uniform_handle("u_model");
  m_terrainUniforms.grass_primary =
      m_terrainShader->uniform_handle("u_grassPrimary");
  m_terrainUniforms.grass_secondary =
      m_terrainShader->uniform_handle("u_grassSecondary");
  m_terrainUniforms.grass_dry = m_terrainShader->uniform_handle("u_grassDry");
  m_terrainUniforms.soil_color = m_terrainShader->uniform_handle("u_soilColor");
  m_terrainUniforms.rock_low = m_terrainShader->uniform_handle("u_rockLow");
  m_terrainUniforms.rock_high = m_terrainShader->uniform_handle("u_rockHigh");
  m_terrainUniforms.tint = m_terrainShader->uniform_handle("u_tint");
  m_terrainUniforms.noise_offset =
      m_terrainShader->uniform_handle("u_noiseOffset");
  m_terrainUniforms.tile_size = m_terrainShader->uniform_handle("u_tileSize");
  m_terrainUniforms.macro_noise_scale =
      m_terrainShader->uniform_handle("u_macroNoiseScale");
  m_terrainUniforms.detail_noise_scale =
      m_terrainShader->uniform_handle("u_detailNoiseScale");
  m_terrainUniforms.slope_rock_threshold =
      m_terrainShader->uniform_handle("u_slopeRockThreshold");
  m_terrainUniforms.slope_rock_sharpness =
      m_terrainShader->uniform_handle("u_slopeRockSharpness");
  m_terrainUniforms.soil_blend_height =
      m_terrainShader->uniform_handle("u_soilBlendHeight");
  m_terrainUniforms.soil_blend_sharpness =
      m_terrainShader->uniform_handle("u_soilBlendSharpness");
  m_terrainUniforms.height_noise_strength =
      m_terrainShader->uniform_handle("u_heightNoiseStrength");
  m_terrainUniforms.height_noise_frequency =
      m_terrainShader->uniform_handle("u_heightNoiseFrequency");
  m_terrainUniforms.ambient_boost =
      m_terrainShader->uniform_handle("u_ambientBoost");
  m_terrainUniforms.rock_detail_strength =
      m_terrainShader->uniform_handle("u_rockDetailStrength");
  m_terrainUniforms.light_dir = m_terrainShader->uniform_handle("u_lightDir");

  m_terrainUniforms.snow_coverage =
      m_terrainShader->uniform_handle("u_snowCoverage");
  m_terrainUniforms.moisture_level =
      m_terrainShader->uniform_handle("u_moistureLevel");
  m_terrainUniforms.crack_intensity =
      m_terrainShader->uniform_handle("u_crackIntensity");
  m_terrainUniforms.rock_exposure =
      m_terrainShader->uniform_handle("u_rockExposure");
  m_terrainUniforms.grass_saturation =
      m_terrainShader->uniform_handle("u_grassSaturation");
  m_terrainUniforms.soil_roughness =
      m_terrainShader->uniform_handle("u_soilRoughness");
  m_terrainUniforms.snow_color = m_terrainShader->uniform_handle("u_snowColor");
}

void TerrainPipeline::initialize_grass_geometry() {
  auto *gl = QOpenGLContext::currentContext()->extraFunctions();
  if (gl == nullptr) {
    qWarning() << "TerrainPipeline::initialize_grass_geometry: no OpenGL context";
    return;
  }

  shutdown_grass_geometry();

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

void TerrainPipeline::shutdown_grass_geometry() {
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
