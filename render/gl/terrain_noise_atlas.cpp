#include "render/gl/terrain_noise_atlas.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QOpenGLExtraFunctions>
#include <QOpenGLFunctions>
#include <QString>
#include <QtGui/qopengl.h>

#include <algorithm>
#include <array>
#include <cstddef>

#include "render/gl/gl_resource_tracking.h"
#include "render/gl/shader.h"

namespace Render::GL {

namespace {

constexpr int k_microdetail_size_full = 1024;
constexpr int k_microdetail_size_low = 512;
constexpr int k_max_atlas_size_full = 4096;
constexpr int k_max_atlas_size_low = 2048;
constexpr float k_microdetail_cells = 32.0F;

struct BakeState {
  GLint framebuffer = 0;
  GLint viewport[4]{};
  GLboolean depth = 0U;
  GLboolean blend = 0U;
  GLboolean cull = 0U;
};

auto capture_bake_state(QOpenGLFunctions* gl) -> BakeState {
  BakeState state;
  gl->glGetIntegerv(GL_FRAMEBUFFER_BINDING, &state.framebuffer);
  gl->glGetIntegerv(GL_VIEWPORT, state.viewport);
  state.depth = gl->glIsEnabled(GL_DEPTH_TEST);
  state.blend = gl->glIsEnabled(GL_BLEND);
  state.cull = gl->glIsEnabled(GL_CULL_FACE);
  return state;
}

void restore_bake_state(QOpenGLFunctions* gl, const BakeState& state) {
  gl->glViewport(
      state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3]);
  if (state.depth != 0U) {
    gl->glEnable(GL_DEPTH_TEST);
  }
  if (state.blend != 0U) {
    gl->glEnable(GL_BLEND);
  }
  if (state.cull != 0U) {
    gl->glEnable(GL_CULL_FACE);
  }
}

void allocate_rgba16f_texture(QOpenGLFunctions* gl,
                              const TextureHandle& texture,
                              int size,
                              GLint wrap) {
  gl->glBindTexture(GL_TEXTURE_2D, texture.id());
  gl->glTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA16F, size, size, 0, GL_RGBA, GL_FLOAT, nullptr);
  note_texture_storage(texture_transfer_bytes(static_cast<std::size_t>(size),
                                              static_cast<std::size_t>(size),
                                              8U),
                       false);
  gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
  gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap);
  gl->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap);
  gl->glBindTexture(GL_TEXTURE_2D, 0);
}

} // namespace

TerrainNoiseAtlas::TerrainNoiseAtlas() = default;
TerrainNoiseAtlas::~TerrainNoiseAtlas() = default;

void TerrainNoiseAtlas::bake_microdetail(bool low_detail) {
  if (m_microdetail_texture) {
    return;
  }
  auto* context = QOpenGLContext::currentContext();
  auto* gl = context != nullptr ? context->functions() : nullptr;
  if (gl == nullptr) {
    return;
  }

  if (m_microdetail_bake_shader == nullptr) {
    m_microdetail_bake_shader = std::make_unique<Shader>();
    if (!m_microdetail_bake_shader->load_from_files(
            QStringLiteral(":/assets/shaders/post_fullscreen.vert"),
            QStringLiteral(":/assets/shaders/terrain_microdetail_bake.frag"))) {
      qWarning() << "TerrainRenderer: microdetail bake shader failed to compile;"
                    " falling back to procedural noise";
      m_microdetail_bake_shader.reset();
      return;
    }
    m_microdetail_bake_shader->set_debug_name(
        QStringLiteral("terrain_microdetail_bake"));
  }

  const int microdetail_size =
      low_detail ? k_microdetail_size_low : k_microdetail_size_full;
  TextureHandle texture = TextureHandle::create();
  allocate_rgba16f_texture(gl, texture, microdetail_size, GL_REPEAT);

  FramebufferHandle fbo = FramebufferHandle::create();
  if (!m_fullscreen_vao) {
    m_fullscreen_vao = VertexArrayHandle::create();
  }

  const BakeState previous = capture_bake_state(gl);

  gl->glBindFramebuffer(GL_FRAMEBUFFER, fbo.id());
  gl->glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture.id(), 0);
  if (gl->glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    qWarning() << "TerrainRenderer: microdetail bake framebuffer incomplete";
    gl->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous.framebuffer));
    return;
  }

  gl->glViewport(0, 0, microdetail_size, microdetail_size);
  gl->glDisable(GL_DEPTH_TEST);
  gl->glDisable(GL_BLEND);
  gl->glDisable(GL_CULL_FACE);

  m_microdetail_bake_shader->use();
  m_microdetail_bake_shader->set_uniform("u_microdetail_cells", k_microdetail_cells);
  context->extraFunctions()->glBindVertexArray(m_fullscreen_vao.id());
  gl->glDrawArrays(GL_TRIANGLES, 0, 3);
  context->extraFunctions()->glBindVertexArray(0);
  m_microdetail_bake_shader->release();

  gl->glBindTexture(GL_TEXTURE_2D, texture.id());
  gl->glGenerateMipmap(GL_TEXTURE_2D);
  gl->glBindTexture(GL_TEXTURE_2D, 0);

  gl->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous.framebuffer));
  fbo.reset();
  restore_bake_state(gl, previous);

  m_microdetail_texture = std::move(texture);
}

auto TerrainNoiseAtlas::bake_atlas(const AtlasBakeParams& params) -> bool {
  auto* context = QOpenGLContext::currentContext();
  auto* gl = context != nullptr ? context->functions() : nullptr;
  if (gl == nullptr || params.width <= 0 || params.height <= 0) {
    return false;
  }

  constexpr int k_texels_per_tile = 2;
  const int max_atlas_size =
      params.low_detail ? k_max_atlas_size_low : k_max_atlas_size_full;
  const int requested = std::max(params.width, params.height) * k_texels_per_tile;
  const int atlas_size = std::clamp(requested, 256, max_atlas_size);

  if (m_atlas_bake_shader == nullptr) {
    m_atlas_bake_shader = std::make_unique<Shader>();
    if (!m_atlas_bake_shader->load_from_files(
            QStringLiteral(":/assets/shaders/post_fullscreen.vert"),
            QStringLiteral(":/assets/shaders/terrain_field_bake.frag"))) {
      qWarning() << "TerrainRenderer: terrain field bake shader failed to compile;"
                    " falling back to procedural noise";
      m_atlas_bake_shader.reset();
      return true;
    }
    m_atlas_bake_shader->set_debug_name(QStringLiteral("terrain_field_bake"));
  }

  if (!m_atlas_texture || !m_atlas_detail_texture || m_atlas_size != atlas_size) {
    m_atlas_texture.reset();
    m_atlas_detail_texture.reset();
    m_atlas_texture = TextureHandle::create();
    allocate_rgba16f_texture(gl, m_atlas_texture, atlas_size, GL_CLAMP_TO_EDGE);
    m_atlas_detail_texture = TextureHandle::create();
    allocate_rgba16f_texture(gl, m_atlas_detail_texture, atlas_size, GL_CLAMP_TO_EDGE);
    m_atlas_size = atlas_size;
  }

  if (!m_atlas_fbo) {
    m_atlas_fbo = FramebufferHandle::create();
  }
  if (!m_fullscreen_vao) {
    m_fullscreen_vao = VertexArrayHandle::create();
  }

  const BakeState previous = capture_bake_state(gl);

  gl->glBindFramebuffer(GL_FRAMEBUFFER, m_atlas_fbo.id());
  gl->glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_atlas_texture.id(), 0);
  gl->glFramebufferTexture2D(GL_FRAMEBUFFER,
                             GL_COLOR_ATTACHMENT1,
                             GL_TEXTURE_2D,
                             m_atlas_detail_texture.id(),
                             0);
  const std::array<GLenum, 2> bake_targets{GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
  context->extraFunctions()->glDrawBuffers(static_cast<GLsizei>(bake_targets.size()),
                                           bake_targets.data());
  const GLenum status = gl->glCheckFramebufferStatus(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    qWarning() << "TerrainRenderer: terrain field bake framebuffer incomplete"
               << Qt::hex << status;
    gl->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous.framebuffer));
    return true;
  }

  gl->glViewport(0, 0, atlas_size, atlas_size);
  gl->glDisable(GL_DEPTH_TEST);
  gl->glDisable(GL_BLEND);
  gl->glDisable(GL_CULL_FACE);

  m_atlas_bake_shader->use();
  const QVector2D bake_world_size(static_cast<float>(params.width) * params.tile_size,
                                  static_cast<float>(params.height) * params.tile_size);
  m_atlas_bake_shader->set_uniform("u_bake_world_min", bake_world_size * -0.5F);
  m_atlas_bake_shader->set_uniform("u_bake_world_size", bake_world_size);
  m_atlas_bake_shader->set_uniform("u_noise_offset", params.noise_offset);
  m_atlas_bake_shader->set_uniform("u_tile_size", params.tile_size);
  m_atlas_bake_shader->set_uniform("u_macro_noise_scale", params.macro_noise_scale);

  context->extraFunctions()->glBindVertexArray(m_fullscreen_vao.id());
  gl->glDrawArrays(GL_TRIANGLES, 0, 3);
  context->extraFunctions()->glBindVertexArray(0);
  m_atlas_bake_shader->release();

  gl->glBindTexture(GL_TEXTURE_2D, m_atlas_texture.id());
  gl->glGenerateMipmap(GL_TEXTURE_2D);
  gl->glBindTexture(GL_TEXTURE_2D, m_atlas_detail_texture.id());
  gl->glGenerateMipmap(GL_TEXTURE_2D);
  gl->glBindTexture(GL_TEXTURE_2D, 0);

  gl->glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(previous.framebuffer));
  restore_bake_state(gl, previous);

  qInfo() << "TerrainRenderer: baked terrain noise atlas" << atlas_size << "squared";
  return true;
}

} // namespace Render::GL
