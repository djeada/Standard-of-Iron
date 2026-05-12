#include "banner_pipeline.h"
#include "../backend.h"
#include "../mesh.h"
#include "../shader_cache.h"
#include <QDebug>

namespace Render::GL::BackendPipelines {

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

  m_banner_mesh16 = GL::create_plane_mesh(1.0F, 1.0F, 16);
  m_banner_mesh8 = GL::create_plane_mesh(1.0F, 1.0F, 8);

  cache_uniforms();

  return is_initialized();
}

void BannerPipeline::shutdown() {
  m_banner_shader = nullptr;
  m_banner_mesh16.reset();
  m_banner_mesh8.reset();
}

void BannerPipeline::cache_uniforms() { cache_banner_uniforms(); }

auto BannerPipeline::is_initialized() const -> bool {
  return m_banner_shader != nullptr && m_banner_mesh16 != nullptr;
}

auto BannerPipeline::get_banner_mesh(int subdivisions) -> GL::Mesh * {
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
  m_banner_uniforms.wind_strength =
      m_banner_shader->uniform_handle("u_wind_strength");
  m_banner_uniforms.color = m_banner_shader->uniform_handle("u_color");
  m_banner_uniforms.trim_color = m_banner_shader->uniform_handle("u_trim_color");
  m_banner_uniforms.texture = m_banner_shader->uniform_handle("u_texture");
  m_banner_uniforms.use_texture =
      m_banner_shader->uniform_handle("u_use_texture");
  m_banner_uniforms.alpha = m_banner_shader->uniform_handle("u_alpha");
}

} // namespace Render::GL::BackendPipelines
