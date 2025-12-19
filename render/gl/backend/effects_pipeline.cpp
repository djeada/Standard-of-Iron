#include "effects_pipeline.h"
#include "../backend.h"
#include "../shader_cache.h"
#include "../state_scopes.h"
#include "../../draw_queue.h"
#include "../../geom/selection_disc.h"
#include "../../geom/selection_ring.h"
#include <QDebug>
#include <qglobal.h>

namespace Render::GL::BackendPipelines {

auto EffectsPipeline::initialize() -> bool {
  if (m_shaderCache == nullptr) {
    qWarning() << "EffectsPipeline::initialize: null ShaderCache";
    return false;
  }

  m_basicShader = m_shaderCache->get("basic");
  m_gridShader = m_shaderCache->get("grid");

  if (m_basicShader == nullptr) {
    qWarning() << "EffectsPipeline: Failed to load basic shader";
  }
  if (m_gridShader == nullptr) {
    qWarning() << "EffectsPipeline: Failed to load grid shader";
  }

  cache_uniforms();

  return is_initialized();
}

void EffectsPipeline::shutdown() {
  m_basicShader = nullptr;
  m_gridShader = nullptr;
}

void EffectsPipeline::cache_uniforms() {
  cache_basic_uniforms();
  cache_grid_uniforms();
}

auto EffectsPipeline::is_initialized() const -> bool {
  return m_basicShader != nullptr && m_gridShader != nullptr;
}

void EffectsPipeline::cache_basic_uniforms() {
  if (m_basicShader == nullptr) {
    return;
  }

  m_basicUniforms.mvp = m_basicShader->uniform_handle("u_mvp");
  m_basicUniforms.model = m_basicShader->uniform_handle("u_model");
  m_basicUniforms.texture = m_basicShader->uniform_handle("u_texture");
  m_basicUniforms.useTexture = m_basicShader->uniform_handle("u_useTexture");
  m_basicUniforms.color = m_basicShader->uniform_handle("u_color");
  m_basicUniforms.alpha = m_basicShader->uniform_handle("u_alpha");
}

void EffectsPipeline::cache_grid_uniforms() {
  if (m_gridShader == nullptr) {
    return;
  }

  m_gridUniforms.mvp = m_gridShader->uniform_handle("u_mvp");
  m_gridUniforms.model = m_gridShader->uniform_handle("u_model");
  m_gridUniforms.gridColor = m_gridShader->uniform_handle("u_gridColor");
  m_gridUniforms.lineColor = m_gridShader->uniform_handle("u_lineColor");
  m_gridUniforms.cellSize = m_gridShader->uniform_handle("u_cellSize");
  m_gridUniforms.thickness = m_gridShader->uniform_handle("u_thickness");
}

void EffectsPipeline::render_grid(const GL::DrawQueue &queue, std::size_t &i,
                                   const QMatrix4x4 &view_proj) {
  if (!m_gridShader) {
    return;
  }
  const auto &gc = std::get<GridCmdIndex>(queue.get_sorted(i));

  m_backend->bind_shader(m_gridShader);

  m_gridShader->set_uniform(m_gridUniforms.mvp, gc.mvp);
  m_gridShader->set_uniform(m_gridUniforms.model, gc.model);
  m_gridShader->set_uniform(m_gridUniforms.gridColor, gc.color);
  const QVector3D k_grid_line_color(0.22F, 0.25F, 0.22F);
  m_gridShader->set_uniform(m_gridUniforms.lineColor, k_grid_line_color);
  m_gridShader->set_uniform(m_gridUniforms.cellSize, gc.cell_size);
  m_gridShader->set_uniform(m_gridUniforms.thickness, gc.thickness);

  if (auto *resources = m_backend->resources()) {
    if (auto *plane = resources->ground()) {
      plane->draw();
    }
  }
}

void EffectsPipeline::render_selection_ring(const GL::DrawQueue &queue,
                                             std::size_t &i,
                                             const QMatrix4x4 &view_proj) {
  const auto &sc = std::get<SelectionRingCmdIndex>(queue.get_sorted(i));
  Mesh *ring = Render::Geom::SelectionRing::get();
  if (!ring || !m_basicShader) {
    return;
  }

  m_backend->bind_shader(m_basicShader);
  m_basicShader->set_uniform(m_basicUniforms.useTexture, false);
  m_basicShader->set_uniform(m_basicUniforms.color, sc.color);

  DepthMaskScope const depth_mask(false);
  PolygonOffsetScope const poly(-1.0F, -1.0F);
  BlendScope const blend(true);

  {
    QMatrix4x4 m = sc.model;
    m.scale(1.08F, 1.0F, 1.08F);
    const QMatrix4x4 mvp = view_proj * m;
    m_basicShader->set_uniform(m_basicUniforms.mvp, mvp);
    m_basicShader->set_uniform(m_basicUniforms.model, m);
    m_basicShader->set_uniform(m_basicUniforms.alpha, sc.alpha_outer);
    ring->draw();
  }

  {
    const QMatrix4x4 mvp = view_proj * sc.model;
    m_basicShader->set_uniform(m_basicUniforms.mvp, mvp);
    m_basicShader->set_uniform(m_basicUniforms.model, sc.model);
    m_basicShader->set_uniform(m_basicUniforms.alpha, sc.alpha_inner);
    ring->draw();
  }
}

void EffectsPipeline::render_selection_smoke(const GL::DrawQueue &queue,
                                              std::size_t &i,
                                              const QMatrix4x4 &view_proj) {
  const auto &sm = std::get<SelectionSmokeCmdIndex>(queue.get_sorted(i));
  Mesh *disc = Render::Geom::SelectionDisc::get();
  if (!disc || !m_basicShader) {
    return;
  }

  m_backend->bind_shader(m_basicShader);
  m_basicShader->set_uniform(m_basicUniforms.useTexture, false);
  m_basicShader->set_uniform(m_basicUniforms.color, sm.color);

  DepthMaskScope const depth_mask(false);
  DepthTestScope const depth_test(true);
  PolygonOffsetScope const poly(-1.0F, -1.0F);
  BlendScope const blend(true);

  for (int i = 0; i < 7; ++i) {
    float const scale = 1.35F + 0.12F * i;
    float const a = sm.base_alpha * (1.0F - 0.09F * i);
    QMatrix4x4 m = sm.model;
    m.translate(0.0F, 0.02F, 0.0F);
    m.scale(scale, 1.0F, scale);
    const QMatrix4x4 mvp = view_proj * m;
    m_basicShader->set_uniform(m_basicUniforms.mvp, mvp);
    m_basicShader->set_uniform(m_basicUniforms.model, m);
    m_basicShader->set_uniform(m_basicUniforms.alpha, a);
    disc->draw();
  }
}

} // namespace Render::GL::BackendPipelines
