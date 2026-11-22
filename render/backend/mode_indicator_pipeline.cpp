#include "render/backend/mode_indicator_pipeline.h"

#include <QDebug>
#include <QOpenGLContext>

#include "render/backend/gl_error_check.h"
#include "render/backend/mesh_instancing_pipeline.h"
#include "render/backend/state_scopes.h"
#include "render/geom/mode_indicator.h"
#include "render/gl/mesh.h"
#include "render/gl/shader_cache.h"

namespace Render::GL::BackendPipelines {

namespace {

auto check_gl_error(const char* operation) -> bool {
  return BackendPipelines::check_gl_error("ModeIndicatorPipeline", operation);
}

} // namespace

auto ModeIndicatorPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "ModeIndicatorPipeline::initialize: null ShaderCache";
    return false;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  m_indicator_shader = m_shader_cache->get("mode_indicator");
  if (m_indicator_shader == nullptr) {
    qWarning() << "ModeIndicatorPipeline: Failed to get mode_indicator shader";
    return false;
  }

  cache_uniforms();

  m_instanced_shader = m_shader_cache->get("mode_indicator_instanced");
  if (m_instanced_shader != nullptr) {
    cache_instanced_uniforms();
  }

  return is_initialized();
}

void ModeIndicatorPipeline::shutdown() {
  m_indicator_shader = nullptr;
  m_instanced_shader = nullptr;
}

void ModeIndicatorPipeline::cache_uniforms() {
  if (m_indicator_shader == nullptr) {
    return;
  }

  m_uniforms.mvp = m_indicator_shader->uniform_handle("u_mvp");
  m_uniforms.mode_color = m_indicator_shader->uniform_handle("u_mode_color");
  m_uniforms.alpha = m_indicator_shader->uniform_handle("u_alpha");
  m_uniforms.time = m_indicator_shader->uniform_handle("u_time");
}

void ModeIndicatorPipeline::cache_instanced_uniforms() {
  if (m_instanced_shader == nullptr) {
    return;
  }
  m_instanced_uniforms.time = m_instanced_shader->uniform_handle("u_time");
}

auto ModeIndicatorPipeline::is_initialized() const -> bool {
  return m_indicator_shader != nullptr;
}

void ModeIndicatorPipeline::draw_indicators(const PreparedBatch& prepared,
                                            MeshInstancingPipeline* instancing,
                                            PipelineDrawContext& context) {
  if (!is_initialized()) {
    return;
  }

  const auto& queue = context.queue;
  const float animation_time = context.frame.animation_time;

  auto resolve_glyph = [](int mode_type) -> Mesh* {
    auto const index = static_cast<std::size_t>(mode_type);
    if (index >= Render::Geom::k_indicator_kind_count) {
      return nullptr;
    }
    return Render::Geom::ModeIndicator::mesh_for(
        static_cast<Render::Geom::IndicatorKind>(index));
  };

  if (prepared.kind == PreparedBatchKind::ModeIndicatorInstanced &&
      instancing != nullptr && instancing->is_initialized() &&
      m_instanced_shader != nullptr) {
    const auto& first_mc =
        std::get<ModeIndicatorCmdIndex>(queue.get_sorted(prepared.start));

    Mesh* const batch_mesh = resolve_glyph(first_mc.mode_type);
    if (batch_mesh != nullptr) {
      GL::Shader* inst_shader = m_instanced_shader;
      inst_shader->use();
      if (m_instanced_uniforms.time != GL::Shader::InvalidUniform) {
        inst_shader->set_uniform(m_instanced_uniforms.time, animation_time);
      }

      instancing->begin_batch(batch_mesh);
      for (std::size_t idx = prepared.start; idx < prepared.start + prepared.count;
           ++idx) {
        const auto& mc = std::get<ModeIndicatorCmdIndex>(queue.get_sorted(idx));
        instancing->accumulate(mc.model, mc.color, mc.alpha);
      }

      DepthMaskScope const depth_mask(false);
      BlendScope const blend(true);
      DepthTestScope const depth_test(false);
      CullFaceScope const cull(false);
      glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

      instancing->flush(context.meshes);
      context.state.bound_shader = nullptr;
    }
    return;
  }

  const auto& mc = std::get<ModeIndicatorCmdIndex>(queue.get_sorted(prepared.start));
  Mesh* const indicator_mesh = resolve_glyph(mc.mode_type);
  if (indicator_mesh == nullptr) {
    return;
  }

  render_indicator(context.meshes,
                   indicator_mesh,
                   mc.model,
                   context.frame.view_proj,
                   mc.color,
                   mc.alpha,
                   animation_time);

  context.state.bound_shader = nullptr;
}

void ModeIndicatorPipeline::render_indicator(MeshGpuTable& meshes,
                                             Mesh* mesh,
                                             const QMatrix4x4& model,
                                             const QMatrix4x4& view_proj,
                                             const QVector3D& color,
                                             float alpha,
                                             float time) {
  if (!is_initialized() || mesh == nullptr) {
    return;
  }

  DepthMaskScope const depth_mask(false);
  BlendScope const blend(true);
  DepthTestScope const depth_test(false);
  CullFaceScope const cull(false);

  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  m_indicator_shader->use();

  QMatrix4x4 mvp = view_proj * model;
  m_indicator_shader->set_uniform(m_uniforms.mvp, mvp);
  m_indicator_shader->set_uniform(m_uniforms.mode_color, color);
  m_indicator_shader->set_uniform(m_uniforms.alpha, alpha);
  m_indicator_shader->set_uniform(m_uniforms.time, time);

  meshes.draw(*mesh);

  check_gl_error("render_indicator");
}

} // namespace Render::GL::BackendPipelines
