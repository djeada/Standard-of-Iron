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
  m_grassUniforms.windStrength = m_grassShader->uniformHandle("u_windStrength");
  m_grassUniforms.windSpeed = m_grassShader->uniformHandle("u_windSpeed");
  m_grassUniforms.soilColor = m_grassShader->uniformHandle("u_soilColor");
  m_grassUniforms.light_dir = m_grassShader->uniformHandle("u_lightDir");
}

void TerrainPipeline::cacheGroundUniforms() {
  if (m_groundShader == nullptr) {
    return;
  }

  m_groundUniforms.mvp = m_groundShader->uniformHandle("u_mvp");
  m_groundUniforms.model = m_groundShader->uniformHandle("u_model");
  m_groundUniforms.grassPrimary =
      m_groundShader->uniformHandle("u_grassPrimary");
  m_groundUniforms.grassSecondary =
      m_groundShader->uniformHandle("u_grassSecondary");
  m_groundUniforms.grassDry = m_groundShader->uniformHandle("u_grassDry");
  m_groundUniforms.soilColor = m_groundShader->uniformHandle("u_soilColor");
  m_groundUniforms.tint = m_groundShader->uniformHandle("u_tint");
  m_groundUniforms.noiseOffset = m_groundShader->uniformHandle("u_noiseOffset");
  m_groundUniforms.tile_size = m_groundShader->uniformHandle("u_tileSize");
  m_groundUniforms.macroNoiseScale =
      m_groundShader->uniformHandle("u_macroNoiseScale");
  m_groundUniforms.detail_noiseScale =
      m_groundShader->uniformHandle("u_detailNoiseScale");
  m_groundUniforms.soilBlendHeight =
      m_groundShader->uniformHandle("u_soilBlendHeight");
  m_groundUniforms.soilBlendSharpness =
      m_groundShader->uniformHandle("u_soilBlendSharpness");
  m_groundUniforms.ambientBoost =
      m_groundShader->uniformHandle("u_ambientBoost");
  m_groundUniforms.light_dir = m_groundShader->uniformHandle("u_lightDir");
}

void TerrainPipeline::cacheTerrainUniforms() {
  if (m_terrainShader == nullptr) {
    return;
  }

  m_terrainUniforms.mvp = m_terrainShader->uniformHandle("u_mvp");
  m_terrainUniforms.model = m_terrainShader->uniformHandle("u_model");
  m_terrainUniforms.grassPrimary =
      m_terrainShader->uniformHandle("u_grassPrimary");
  m_terrainUniforms.grassSecondary =
      m_terrainShader->uniformHandle("u_grassSecondary");
  m_terrainUniforms.grassDry = m_terrainShader->uniformHandle("u_grassDry");
  m_terrainUniforms.soilColor = m_terrainShader->uniformHandle("u_soilColor");
  m_terrainUniforms.rockLow = m_terrainShader->uniformHandle("u_rockLow");
  m_terrainUniforms.rockHigh = m_terrainShader->uniformHandle("u_rockHigh");
  m_terrainUniforms.tint = m_terrainShader->uniformHandle("u_tint");
  m_terrainUniforms.noiseOffset =
      m_terrainShader->uniformHandle("u_noiseOffset");
  m_terrainUniforms.tile_size = m_terrainShader->uniformHandle("u_tileSize");
  m_terrainUniforms.macroNoiseScale =
      m_terrainShader->uniformHandle("u_macroNoiseScale");
  m_terrainUniforms.detail_noiseScale =
      m_terrainShader->uniformHandle("u_detailNoiseScale");
  m_terrainUniforms.slopeRockThreshold =
      m_terrainShader->uniformHandle("u_slopeRockThreshold");
  m_terrainUniforms.slopeRockSharpness =
      m_terrainShader->uniformHandle("u_slopeRockSharpness");
  m_terrainUniforms.soilBlendHeight =
      m_terrainShader->uniformHandle("u_soilBlendHeight");
  m_terrainUniforms.soilBlendSharpness =
      m_terrainShader->uniformHandle("u_soilBlendSharpness");
  m_terrainUniforms.heightNoiseStrength =
      m_terrainShader->uniformHandle("u_heightNoiseStrength");
  m_terrainUniforms.heightNoiseFrequency =
      m_terrainShader->uniformHandle("u_heightNoiseFrequency");
  m_terrainUniforms.ambientBoost =
      m_terrainShader->uniformHandle("u_ambientBoost");
  m_terrainUniforms.rockDetailStrength =
      m_terrainShader->uniformHandle("u_rockDetailStrength");
  m_terrainUniforms.light_dir = m_terrainShader->uniformHandle("u_lightDir");
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

  if (m_grassVertexBuffer != 0u) {
    gl->glDeleteBuffers(1, &m_grassVertexBuffer);
    m_grassVertexBuffer = 0;
  }
  if (m_grassVao != 0u) {
    gl->glDeleteVertexArrays(1, &m_grassVao);
    m_grassVao = 0;
  }
  m_grassVertexCount = 0;
}

} // namespace Render::GL::BackendPipelines
