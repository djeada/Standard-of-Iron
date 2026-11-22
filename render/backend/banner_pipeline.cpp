#include "render/backend/banner_pipeline.h"

#include <QDebug>

#include "render/backend/state_scopes.h"
#include "render/gl/mesh.h"
#include "render/gl/shader_cache.h"
#include "render/gl/texture.h"
#include "scene/camera.h"

namespace Render::GL::BackendPipelines {

void BannerPipeline::draw_banners(const PreparedBatch& prepared,
                                  const MeshCmd& head,
                                  bool is_transparent,
                                  GL::Texture* fallback_texture,
                                  PipelineDrawContext& context) {
  Shader* const active_shader = m_banner_shader;
  const auto& queue = context.queue;
  const auto& banner = m_banner_uniforms;
  if (is_transparent) {
    glDepthFunc(GL_LEQUAL);
  }
  CullFaceScope const banner_cull(false);
  if (context.state.bound_shader != active_shader) {
    active_shader->use();
    active_shader->set_uniform(banner.time, context.frame.animation_time);
    active_shader->set_uniform(banner.wind_strength,
                               context.frame.banner_wind_strength);
    active_shader->set_uniform(banner.light_direction, context.frame.light_direction);
    active_shader->set_uniform(banner.camera_pos, context.frame.camera->get_position());
    active_shader->set_uniform(banner.ambient_strength, context.frame.ambient_strength);
    context.state.bound_shader = active_shader;
  }
  GL::Texture* const tex_to_use =
      (head.texture != nullptr) ? head.texture : fallback_texture;
  if ((tex_to_use != nullptr) && tex_to_use != context.state.bound_texture) {
    tex_to_use->bind(0);
    context.state.bound_texture = tex_to_use;
  }
  active_shader->set_uniform(banner.texture, 0);
  active_shader->set_uniform(banner.use_texture, head.texture != nullptr);
  if (context.meshes.bind(*head.mesh)) {
    for (std::size_t j = prepared.start; j < prepared.end(); ++j) {
      const auto& single = std::get<MeshCmd>(queue.get_sorted(j));
      active_shader->set_uniform(banner.mvp, context.frame.view_proj * single.model);
      active_shader->set_uniform(banner.model, single.model);
      active_shader->set_uniform(banner.color, single.color);
      active_shader->set_uniform(banner.trim_color,
                                 single.has_trim_color ? single.trim_color
                                                       : (single.color * 0.7F));
      active_shader->set_uniform(banner.alpha, single.alpha);
      context.meshes.draw_bound(*head.mesh);
    }
    context.meshes.unbind();
  }
  if (is_transparent) {
    glDepthFunc(GL_LESS);
  }
}

auto BannerPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "BannerPipeline::initialize: null ShaderCache";
    return false;
  }

  initializeOpenGLFunctions();

  m_banner_shader = m_shader_cache->get("banner");

  if (m_banner_shader == nullptr) {
    qWarning() << "BannerPipeline: Failed to load banner shader";
    return false;
  }

  m_banner_mesh16 = GL::create_plane_mesh(1.0F, 1.0F, 24);
  m_banner_mesh8 = GL::create_plane_mesh(1.0F, 1.0F, 12);

  cache_uniforms();

  return is_initialized();
}

void BannerPipeline::shutdown() {
  m_banner_shader = nullptr;
  m_banner_mesh16.reset();
  m_banner_mesh8.reset();
}

void BannerPipeline::cache_uniforms() {
  cache_banner_uniforms();
}

auto BannerPipeline::is_initialized() const -> bool {
  return m_banner_shader != nullptr && m_banner_mesh16 != nullptr;
}

auto BannerPipeline::get_banner_mesh(int subdivisions) -> GL::Mesh* {
  if (subdivisions >= 12) {
    return m_banner_mesh16.get();
  }
  return m_banner_mesh8.get();
}

void BannerPipeline::cache_banner_uniforms() {
  if (m_banner_shader == nullptr) {
    return;
  }

  m_banner_uniforms.mvp = m_banner_shader->uniform_handle("u_mvp");
  m_banner_uniforms.model = m_banner_shader->uniform_handle("u_model");
  m_banner_uniforms.time = m_banner_shader->uniform_handle("u_time");
  m_banner_uniforms.wind_strength = m_banner_shader->uniform_handle("u_wind_strength");
  m_banner_uniforms.light_direction =
      m_banner_shader->optional_uniform_handle("u_light_direction");
  m_banner_uniforms.camera_pos = m_banner_shader->uniform_handle("u_camera_pos");
  m_banner_uniforms.ambient_strength =
      m_banner_shader->optional_uniform_handle("u_ambient_strength");
  m_banner_uniforms.color = m_banner_shader->uniform_handle("u_color");
  m_banner_uniforms.trim_color = m_banner_shader->uniform_handle("u_trim_color");
  m_banner_uniforms.texture = m_banner_shader->uniform_handle("u_texture");
  m_banner_uniforms.use_texture = m_banner_shader->uniform_handle("u_use_texture");
  m_banner_uniforms.alpha = m_banner_shader->uniform_handle("u_alpha");
}

} // namespace Render::GL::BackendPipelines
