#include "post_process_pipeline.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QString>
#include <QVector2D>
#include <QVector4D>

#include <algorithm>
#include <cstddef>

namespace Render::GL::BackendPipelines {

namespace {

constexpr unsigned int k_format_rgba16f = 0x881A;
constexpr unsigned int k_format_rgba8 = 0x8058;

} // namespace

PostProcessPipeline::PostProcessPipeline(ShaderCache* shader_cache)
    : m_shader_cache(shader_cache) {
}

PostProcessPipeline::~PostProcessPipeline() {
  shutdown();
}

auto PostProcessPipeline::initialize() -> bool {
  initializeOpenGLFunctions();

  if (m_shader_cache == nullptr) {
    return false;
  }

  m_bright_shader = m_shader_cache->get(QStringLiteral("post_bright"));
  m_blur_shader = m_shader_cache->get(QStringLiteral("post_blur"));
  m_composite_shader = m_shader_cache->get(QStringLiteral("post_composite"));
  m_fxaa_shader = m_shader_cache->get(QStringLiteral("post_fxaa"));
  m_sky_shader = m_shader_cache->get(QStringLiteral("sky"));
  m_godrays_shader = m_shader_cache->get(QStringLiteral("post_godrays"));

  if (m_bright_shader == nullptr || m_blur_shader == nullptr ||
      m_composite_shader == nullptr || m_fxaa_shader == nullptr) {
    qWarning() << "PostProcessPipeline: post shaders unavailable, scene will render "
                  "without grading";
    return false;
  }

  glGenVertexArrays(1, &m_empty_vao);
  m_initialized = true;
  return true;
}

void PostProcessPipeline::shutdown() {
  if (QOpenGLContext::currentContext() != nullptr) {
    initializeOpenGLFunctions();
    release_targets();
    if (m_empty_vao != 0) {
      glDeleteVertexArrays(1, &m_empty_vao);
    }
  }
  m_empty_vao = 0;
  m_initialized = false;
}

void PostProcessPipeline::cache_uniforms() {
}

auto PostProcessPipeline::create_color_target(RenderTarget& target,
                                              int width,
                                              int height,
                                              unsigned int internal_format) -> bool {
  glGenFramebuffers(1, &target.fbo);
  glGenTextures(1, &target.color);

  glBindTexture(GL_TEXTURE_2D, target.color);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               static_cast<GLint>(internal_format),
               width,
               height,
               0,
               GL_RGBA,
               internal_format == k_format_rgba16f ? GL_FLOAT : GL_UNSIGNED_BYTE,
               nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

  glBindFramebuffer(GL_FRAMEBUFFER, target.fbo);
  glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, target.color, 0);

  target.width = width;
  target.height = height;
  return glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
}

auto PostProcessPipeline::ensure_targets(int width, int height) -> bool {
  if (m_scene.fbo != 0 && m_scene.width == width && m_scene.height == height) {
    return true;
  }

  release_targets();

  const int bloom_width = std::max(width / k_bloom_divisor, 1);
  const int bloom_height = std::max(height / k_bloom_divisor, 1);

  m_scene_is_float = true;
  if (!create_color_target(m_scene, width, height, k_format_rgba16f)) {
    release_targets();
    m_scene_is_float = false;
    if (!create_color_target(m_scene, width, height, k_format_rgba8)) {
      release_targets();
      return false;
    }
  }

  glGenTextures(1, &m_scene_depth);
  glBindTexture(GL_TEXTURE_2D, m_scene_depth);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_DEPTH_COMPONENT24,
               width,
               height,
               0,
               GL_DEPTH_COMPONENT,
               GL_FLOAT,
               nullptr);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindFramebuffer(GL_FRAMEBUFFER, m_scene.fbo);
  glFramebufferTexture2D(
      GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_scene_depth, 0);
  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    release_targets();
    return false;
  }

  const unsigned int bloom_format =
      m_scene_is_float ? k_format_rgba16f : k_format_rgba8;
  for (auto& target : m_bloom) {
    if (!create_color_target(target, bloom_width, bloom_height, bloom_format)) {
      release_targets();
      return false;
    }
  }

  if (!create_color_target(m_composite, width, height, k_format_rgba8)) {
    release_targets();
    return false;
  }
  if (!create_color_target(m_rays,
                           std::max(width / k_godray_divisor, 1),
                           std::max(height / k_godray_divisor, 1),
                           k_format_rgba8)) {
    release_targets();
    return false;
  }

  return true;
}

void PostProcessPipeline::release_targets() {
  auto drop = [this](RenderTarget& target) {
    if (target.color != 0) {
      glDeleteTextures(1, &target.color);
    }
    if (target.fbo != 0) {
      glDeleteFramebuffers(1, &target.fbo);
    }
    target = RenderTarget{};
  };

  drop(m_scene);
  drop(m_composite);
  drop(m_rays);
  for (auto& target : m_bloom) {
    drop(target);
  }
  if (m_scene_depth != 0) {
    glDeleteTextures(1, &m_scene_depth);
    m_scene_depth = 0;
  }
}

void PostProcessPipeline::set_depth_range(float near_plane, float far_plane) noexcept {
  m_near_plane = near_plane;
  m_far_plane = far_plane;
}

void PostProcessPipeline::set_atmosphere(const QMatrix4x4& inverse_view_projection,
                                         const QVector3D& camera_position,
                                         float fog_start,
                                         float fog_end,
                                         float time) noexcept {
  m_inverse_view_proj = inverse_view_projection;
  m_camera_position = camera_position;
  m_fog_start = fog_start;
  m_fog_end = fog_end;
  m_mist_time = time;
}

void PostProcessPipeline::set_sun_screen(const QVector2D& sun_screen,
                                         float sun_visibility) noexcept {
  m_sun_screen = sun_screen;
  m_sun_visibility = std::clamp(sun_visibility, 0.0F, 1.0F);
}

void PostProcessPipeline::set_mist(const std::vector<Render::MistVolume>& volumes) {
  m_mist_volumes = volumes;
  if (m_mist_volumes.size() > static_cast<std::size_t>(Render::k_max_mist_volumes)) {
    m_mist_volumes.resize(Render::k_max_mist_volumes);
  }
  m_mist_dirty = true;
}

void PostProcessPipeline::draw_sky(const QMatrix4x4& view_projection,
                                   const QVector3D& camera_position) {
  if (!m_initialized || m_sky_shader == nullptr) {
    return;
  }

  const GLboolean depth_was_enabled = glIsEnabled(GL_DEPTH_TEST);
  const GLboolean cull_was_enabled = glIsEnabled(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDepthMask(GL_FALSE);

  m_sky_shader->use();
  m_sky_shader->set_uniform("u_inverse_view_proj", view_projection.inverted());
  (void)camera_position;
  draw_fullscreen();

  glDepthMask(GL_TRUE);
  if (depth_was_enabled == GL_TRUE) {
    glEnable(GL_DEPTH_TEST);
  }
  if (cull_was_enabled == GL_TRUE) {
    glEnable(GL_CULL_FACE);
  }
}

auto PostProcessPipeline::begin_scene(int width, int height) -> bool {
  m_capturing = false;
  if (!m_initialized || m_targets_failed || width <= 0 || height <= 0) {
    return false;
  }

  GLint bound_fbo = 0;
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &bound_fbo);
  glGetIntegerv(GL_VIEWPORT, m_output_viewport.data());

  if (!ensure_targets(width, height)) {
    m_targets_failed = true;
    qWarning() << "PostProcessPipeline: could not create a" << width << "x" << height
               << "scene target; falling back to direct rendering";
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(bound_fbo));
    return false;
  }

  m_output_fbo = bound_fbo;
  glBindFramebuffer(GL_FRAMEBUFFER, m_scene.fbo);
  glViewport(0, 0, width, height);
  m_capturing = true;
  return true;
}

void PostProcessPipeline::draw_fullscreen() {
  glBindVertexArray(m_empty_vao);
  glDrawArrays(GL_TRIANGLES, 0, 3);
  glBindVertexArray(0);
}

void PostProcessPipeline::blur_bloom_once() {
  const auto blur_width = static_cast<float>(m_bloom[0].width);
  const auto blur_height = static_cast<float>(m_bloom[0].height);

  glBindFramebuffer(GL_FRAMEBUFFER, m_bloom[1].fbo);
  glViewport(0, 0, m_bloom[1].width, m_bloom[1].height);
  m_blur_shader->set_uniform("u_texel_step", QVector2D(1.0F / blur_width, 0.0F));
  glBindTexture(GL_TEXTURE_2D, m_bloom[0].color);
  draw_fullscreen();

  glBindFramebuffer(GL_FRAMEBUFFER, m_bloom[0].fbo);
  glViewport(0, 0, m_bloom[0].width, m_bloom[0].height);
  m_blur_shader->set_uniform("u_texel_step", QVector2D(0.0F, 1.0F / blur_height));
  glBindTexture(GL_TEXTURE_2D, m_bloom[1].color);
  draw_fullscreen();
}

void PostProcessPipeline::resolve_scene() {
  if (!m_capturing) {
    return;
  }
  m_capturing = false;

  const GLboolean depth_was_enabled = glIsEnabled(GL_DEPTH_TEST);
  const GLboolean blend_was_enabled = glIsEnabled(GL_BLEND);
  const GLboolean cull_was_enabled = glIsEnabled(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_BLEND);
  glDisable(GL_CULL_FACE);

  glActiveTexture(GL_TEXTURE0);

  if (m_passes.bloom) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloom[0].fbo);
    glViewport(0, 0, m_bloom[0].width, m_bloom[0].height);
    m_bright_shader->use();
    m_bright_shader->set_uniform("u_scene", 0);
    m_bright_shader->set_uniform("u_threshold", k_bloom_threshold);
    m_bright_shader->set_uniform("u_knee", k_bloom_knee);
    glBindTexture(GL_TEXTURE_2D, m_scene.color);
    draw_fullscreen();

    m_blur_shader->use();
    m_blur_shader->set_uniform("u_source", 0);
    blur_bloom_once();
    blur_bloom_once();
  }

  const bool rays_enabled =
      m_passes.godrays && m_godrays_shader != nullptr && m_rays.fbo != 0;
  if (rays_enabled) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_rays.fbo);
    glViewport(0, 0, m_rays.width, m_rays.height);
    m_godrays_shader->use();
    m_godrays_shader->set_uniform("u_depth", 0);
    m_godrays_shader->set_uniform("u_depth_range",
                                  QVector2D(m_near_plane, m_far_plane));
    m_godrays_shader->set_uniform("u_sun_screen", m_sun_screen);
    m_godrays_shader->set_uniform("u_sun_visibility", m_sun_visibility);
    glBindTexture(GL_TEXTURE_2D, m_scene_depth);
    draw_fullscreen();
  }

  if (m_passes.fxaa) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_composite.fbo);
    glViewport(0, 0, m_composite.width, m_composite.height);
  } else {
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(m_output_fbo));
    glViewport(m_output_viewport[0],
               m_output_viewport[1],
               m_output_viewport[2],
               m_output_viewport[3]);
  }
  m_composite_shader->use();
  m_composite_shader->set_uniform("u_scene", 0);
  m_composite_shader->set_uniform("u_bloom", 1);
  m_composite_shader->set_uniform("u_bloom_intensity",
                                  m_passes.bloom ? k_bloom_intensity : 0.0F);
  m_composite_shader->set_uniform("u_vignette_strength", k_vignette_strength);
  m_composite_shader->set_uniform("u_depth", 2);
  m_composite_shader->set_uniform("u_rays", 3);
  m_composite_shader->set_uniform("u_godray_strength",
                                  rays_enabled ? k_godray_strength : 0.0F);
  m_composite_shader->set_uniform("u_depth_range",
                                  QVector2D(m_near_plane, m_far_plane));

  m_composite_shader->set_uniform(
      m_composite_shader->optional_uniform_handle("u_inverse_resolution"),
      QVector2D(1.0F / static_cast<float>(m_composite.width),
                1.0F / static_cast<float>(m_composite.height)));
  m_composite_shader->set_uniform(
      m_composite_shader->optional_uniform_handle("u_ground_ao_radius"),
      k_ground_ao_radius);
  m_composite_shader->set_uniform(
      m_composite_shader->optional_uniform_handle("u_ground_ao_strength"),
      m_passes.ambient_occlusion ? k_ground_ao_strength : 0.0F);
  m_composite_shader->set_uniform("u_inverse_view_proj", m_inverse_view_proj);
  m_composite_shader->set_uniform("u_camera_position", m_camera_position);
  m_composite_shader->set_uniform("u_fog_range", QVector2D(m_fog_start, m_fog_end));
  m_composite_shader->set_uniform("u_time", m_mist_time);
  m_composite_shader->set_uniform(
      "u_ground_fog",
      QVector4D(
          m_ground_fog.floor_y, m_ground_fog.ceiling_y, m_ground_fog.strength, 0.0F));
  if (m_mist_dirty) {
    const int mist_count = static_cast<int>(m_mist_volumes.size());
    m_composite_shader->set_uniform("u_mist_count", mist_count);
    for (int i = 0; i < mist_count; ++i) {
      const auto& volume = m_mist_volumes[static_cast<std::size_t>(i)];
      m_composite_shader->set_uniform(
          QStringLiteral("u_mist_seg[%1]").arg(i),
          QVector4D(
              volume.start.x(), volume.start.z(), volume.end.x(), volume.end.z()));
      m_composite_shader->set_uniform(
          QStringLiteral("u_mist_info[%1]").arg(i),
          QVector4D(volume.radius,
                    volume.strength,
                    static_cast<float>(static_cast<int>(volume.kind)),
                    volume.start.y()));
    }
    m_mist_dirty = false;
  }
  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, rays_enabled ? m_rays.color : 0);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, m_scene_depth);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, m_bloom[0].color);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, m_scene.color);
  draw_fullscreen();

  if (m_passes.fxaa) {
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(m_output_fbo));
    glViewport(m_output_viewport[0],
               m_output_viewport[1],
               m_output_viewport[2],
               m_output_viewport[3]);
    m_fxaa_shader->use();
    m_fxaa_shader->set_uniform("u_source", 0);
    m_fxaa_shader->set_uniform(
        m_fxaa_shader->optional_uniform_handle("u_inverse_resolution"),
        QVector2D(1.0F / static_cast<float>(m_composite.width),
                  1.0F / static_cast<float>(m_composite.height)));
    glBindTexture(GL_TEXTURE_2D, m_composite.color);
    draw_fullscreen();
  }

  glActiveTexture(GL_TEXTURE3);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE2);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, 0);

  if (depth_was_enabled == GL_TRUE) {
    glEnable(GL_DEPTH_TEST);
  }
  if (blend_was_enabled == GL_TRUE) {
    glEnable(GL_BLEND);
  }
  if (cull_was_enabled == GL_TRUE) {
    glEnable(GL_CULL_FACE);
  }
}

} // namespace Render::GL::BackendPipelines
