#include "terrain_pipeline.h"
#include "../backend.h"
#include "../render_constants.h"
#include "../shader_cache.h"
#include "../state_scopes.h"
#include "../../draw_queue.h"
#include "../../ground/grass_gpu.h"
#include "../../ground/terrain_gpu.h"
#include <GL/gl.h>
#include <QDebug>
#include <QOpenGLExtraFunctions>
#include <cstddef>
#include <memory>
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

  cache_uniforms();

  return is_initialized();
}

void TerrainPipeline::shutdown() {
  shutdownGrassGeometry();
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

void TerrainPipeline::render_grass(const GL::DrawQueue &queue, std::size_t &i,
                                    const QMatrix4x4 &view_proj,
                                    GL::Backend *backend) {
  const auto &grass = std::get<GrassBatchCmdIndex>(queue.get_sorted(i));
  if (!grass.instance_buffer || grass.instance_count == 0 || !m_grassShader ||
      m_grassVao == 0 || m_grassVertexCount == 0) {
    return;
  }

  DepthMaskScope const depth_mask(false);
  BlendScope const blend(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  GLboolean const prev_cull = glIsEnabled(GL_CULL_FACE);
  if (prev_cull != 0U) {
    glDisable(GL_CULL_FACE);
  }

  backend->bind_shader(m_grassShader);
  backend->set_view_proj_uniform(m_grassShader, m_grassUniforms.view_proj,
                                  view_proj);

  if (m_grassUniforms.time != GL::Shader::InvalidUniform) {
    m_grassShader->set_uniform(m_grassUniforms.time, grass.params.time);
  }
  if (m_grassUniforms.wind_strength != GL::Shader::InvalidUniform) {
    m_grassShader->set_uniform(m_grassUniforms.wind_strength,
                               grass.params.wind_strength);
  }
  if (m_grassUniforms.wind_speed != GL::Shader::InvalidUniform) {
    m_grassShader->set_uniform(m_grassUniforms.wind_speed,
                               grass.params.wind_speed);
  }
  if (m_grassUniforms.soil_color != GL::Shader::InvalidUniform) {
    m_grassShader->set_uniform(m_grassUniforms.soil_color,
                               grass.params.soil_color);
  }
  if (m_grassUniforms.light_dir != GL::Shader::InvalidUniform) {
    QVector3D light_dir = grass.params.light_direction;
    if (!light_dir.isNull()) {
      light_dir.normalize();
    }
    m_grassShader->set_uniform(m_grassUniforms.light_dir, light_dir);
  }

  glBindVertexArray(m_grassVao);
  grass.instance_buffer->bind();
  const auto stride = static_cast<GLsizei>(sizeof(GrassInstanceGpu));
  glVertexAttribPointer(
      TexCoord, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(GrassInstanceGpu, pos_height)));
  glVertexAttribPointer(
      InstancePosition, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(GrassInstanceGpu, color_width)));
  glVertexAttribPointer(
      InstanceScale, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(GrassInstanceGpu, sway_params)));
  grass.instance_buffer->unbind();

  glDrawArraysInstanced(GL_TRIANGLES, 0, m_grassVertexCount,
                        static_cast<GLsizei>(grass.instance_count));
  glBindVertexArray(0);

  if (prev_cull != 0U) {
    glEnable(GL_CULL_FACE);
  }
}

#define SET_UNIFORM_IF_VALID(shader, uniform, value)                          \
  if ((uniform) != GL::Shader::InvalidUniform) {                              \
    (shader)->set_uniform((uniform), (value));                                \
  }

void TerrainPipeline::apply_ground_uniforms(GL::Shader *shader,
                                             const TerrainChunkParams &params,
                                             const QMatrix4x4 &mvp,
                                             const QMatrix4x4 &model) {
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.mvp, mvp);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.model, model);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.grass_primary,
                       params.grass_primary);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.grass_secondary,
                       params.grass_secondary);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.grass_dry, params.grass_dry);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.soil_color, params.soil_color);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.tint, params.tint);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.noise_offset,
                       params.noise_offset);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.tile_size, params.tile_size);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.macro_noise_scale,
                       params.macro_noise_scale);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.detail_noise_scale,
                       params.detail_noise_scale);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.soil_blend_height,
                       params.soil_blend_height);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.soil_blend_sharpness,
                       params.soil_blend_sharpness);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.height_noise_strength,
                       params.height_noise_strength);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.height_noise_frequency,
                       params.height_noise_frequency);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.ambient_boost,
                       params.ambient_boost);

  if (m_groundUniforms.light_dir != GL::Shader::InvalidUniform) {
    QVector3D light_dir = params.light_direction;
    if (!light_dir.isNull()) {
      light_dir.normalize();
    }
    shader->set_uniform(m_groundUniforms.light_dir, light_dir);
  }

  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.snow_coverage,
                       params.snow_coverage);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.moisture_level,
                       params.moisture_level);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.crack_intensity,
                       params.crack_intensity);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.grass_saturation,
                       params.grass_saturation);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.soil_roughness,
                       params.soil_roughness);
  SET_UNIFORM_IF_VALID(shader, m_groundUniforms.snow_color, params.snow_color);
}

void TerrainPipeline::apply_terrain_uniforms(GL::Shader *shader,
                                              const TerrainChunkParams &params,
                                              const QMatrix4x4 &mvp,
                                              const QMatrix4x4 &model) {
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.mvp, mvp);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.model, model);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.grass_primary,
                       params.grass_primary);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.grass_secondary,
                       params.grass_secondary);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.grass_dry, params.grass_dry);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.soil_color,
                       params.soil_color);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.rock_low, params.rock_low);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.rock_high, params.rock_high);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.tint, params.tint);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.noise_offset,
                       params.noise_offset);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.tile_size, params.tile_size);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.macro_noise_scale,
                       params.macro_noise_scale);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.detail_noise_scale,
                       params.detail_noise_scale);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.slope_rock_threshold,
                       params.slope_rock_threshold);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.slope_rock_sharpness,
                       params.slope_rock_sharpness);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.soil_blend_height,
                       params.soil_blend_height);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.soil_blend_sharpness,
                       params.soil_blend_sharpness);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.height_noise_strength,
                       params.height_noise_strength);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.height_noise_frequency,
                       params.height_noise_frequency);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.ambient_boost,
                       params.ambient_boost);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.rock_detail_strength,
                       params.rock_detail_strength);

  if (m_terrainUniforms.light_dir != GL::Shader::InvalidUniform) {
    QVector3D light_dir = params.light_direction;
    if (!light_dir.isNull()) {
      light_dir.normalize();
    }
    shader->set_uniform(m_terrainUniforms.light_dir, light_dir);
  }

  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.snow_coverage,
                       params.snow_coverage);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.moisture_level,
                       params.moisture_level);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.crack_intensity,
                       params.crack_intensity);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.rock_exposure,
                       params.rock_exposure);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.grass_saturation,
                       params.grass_saturation);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.soil_roughness,
                       params.soil_roughness);
  SET_UNIFORM_IF_VALID(shader, m_terrainUniforms.snow_color, params.snow_color);
}

void TerrainPipeline::render_terrain_chunk(const GL::DrawQueue &queue,
                                            std::size_t &i,
                                            const QMatrix4x4 &view_proj,
                                            GL::Backend *backend) {
  const auto &terrain = std::get<TerrainChunkCmdIndex>(queue.get_sorted(i));

  GL::Shader *active_shader =
      terrain.params.is_ground_plane ? m_groundShader : m_terrainShader;

  if (!terrain.mesh || !active_shader) {
    return;
  }

  backend->bind_shader(active_shader);

  const QMatrix4x4 mvp = view_proj * terrain.model;

  if (terrain.params.is_ground_plane) {
    apply_ground_uniforms(active_shader, terrain.params, mvp, terrain.model);
  } else {
    apply_terrain_uniforms(active_shader, terrain.params, mvp, terrain.model);
  }

  DepthMaskScope const depth_mask(terrain.depth_write);
  std::unique_ptr<PolygonOffsetScope> poly_scope;
  if (terrain.depth_bias != 0.0F) {
    poly_scope =
        std::make_unique<PolygonOffsetScope>(terrain.depth_bias, terrain.depth_bias);
  }

  terrain.mesh->draw();
}

#undef SET_UNIFORM_IF_VALID

} // namespace Render::GL::BackendPipelines
