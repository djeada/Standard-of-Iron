#include "banner_pipeline.h"
#include "../backend.h"
#include "../mesh.h"
#include "../shader_cache.h"
#include <QDebug>

namespace Render::GL::BackendPipelines {

auto BannerPipeline::initialize() -> bool {
  if (m_shaderCache == nullptr) {
    qWarning() << "BannerPipeline::initialize: null ShaderCache";
    return false;
  }

  initializeOpenGLFunctions();

  m_bannerShader = m_shaderCache->get("banner");

  if (m_bannerShader == nullptr) {
    qWarning() << "BannerPipeline: Failed to load banner shader";
    return false;
  }

  m_bannerMesh16 = GL::create_plane_mesh(1.0F, 1.0F, 16);
  m_bannerMesh8 = GL::create_plane_mesh(1.0F, 1.0F, 8);

  cache_uniforms();

  return is_initialized();
}

void BannerPipeline::shutdown() {
  m_bannerShader = nullptr;
  m_bannerMesh16.reset();
  m_bannerMesh8.reset();
}

void BannerPipeline::cache_uniforms() { cache_banner_uniforms(); }

auto BannerPipeline::is_initialized() const -> bool {
  return m_bannerShader != nullptr && m_bannerMesh16 != nullptr;
}

auto BannerPipeline::get_banner_mesh(int subdivisions) -> GL::Mesh * {
  if (subdivisions >= 12) {
    return m_bannerMesh16.get();
  }
  return m_bannerMesh8.get();
}

void BannerPipeline::cache_banner_uniforms() {
  if (m_bannerShader == nullptr) {
    return;
  }

  m_bannerUniforms.mvp = m_bannerShader->uniform_handle("u_mvp");
  m_bannerUniforms.model = m_bannerShader->uniform_handle("u_model");
  m_bannerUniforms.time = m_bannerShader->uniform_handle("u_time");
  m_bannerUniforms.windStrength =
      m_bannerShader->uniform_handle("u_windStrength");
  m_bannerUniforms.color = m_bannerShader->uniform_handle("u_color");
  m_bannerUniforms.trimColor = m_bannerShader->uniform_handle("u_trimColor");
  m_bannerUniforms.texture = m_bannerShader->uniform_handle("u_texture");
  m_bannerUniforms.useTexture = m_bannerShader->uniform_handle("u_useTexture");
  m_bannerUniforms.alpha = m_bannerShader->uniform_handle("u_alpha");
}

} // namespace Render::GL::BackendPipelines
