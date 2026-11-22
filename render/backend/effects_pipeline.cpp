#include "render/backend/effects_pipeline.h"

#include <QDebug>
#include <qglobal.h>

#include "render/backend/state_scopes.h"
#include "render/gl/mesh.h"
#include "render/gl/shader_cache.h"
#include "scene/camera.h"

namespace Render::GL::BackendPipelines {

namespace {
const QVector3D k_grid_line_color(0.22F, 0.25F, 0.22F);
} // namespace

auto EffectsPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "EffectsPipeline::initialize: null ShaderCache";
    return false;
  }

  m_basic_shader = m_shader_cache->get("basic");
  m_basic_instanced_shader = m_shader_cache->get("basic_instanced");
  m_grid_shader = m_shader_cache->get("grid");

  if (m_basic_shader == nullptr) {
    qWarning() << "EffectsPipeline: Failed to load basic shader";
  }
  if (m_grid_shader == nullptr) {
    qWarning() << "EffectsPipeline: Failed to load grid shader";
  }

  cache_uniforms();

  return is_initialized();
}

void EffectsPipeline::shutdown() {
  m_basic_shader = nullptr;
  m_basic_instanced_shader = nullptr;
  m_grid_shader = nullptr;
}

void EffectsPipeline::cache_uniforms() {
  cache_basic_uniforms();
  cache_basic_instanced_uniforms();
  cache_grid_uniforms();
}

auto EffectsPipeline::is_initialized() const -> bool {
  return m_basic_shader != nullptr && m_grid_shader != nullptr;
}

void EffectsPipeline::draw_grid(const GridCmd& grid,
                                Mesh* ground,
                                PipelineDrawContext& context) {
  if (m_grid_shader == nullptr) {
    return;
  }

  if (context.state.bound_shader != m_grid_shader) {
    m_grid_shader->use();
    context.state.bound_shader = m_grid_shader;
  }

  m_grid_shader->set_uniform(m_grid_uniforms.mvp, grid.mvp);
  m_grid_shader->set_uniform(m_grid_uniforms.model, grid.model);
  m_grid_shader->set_uniform(m_grid_uniforms.grid_color, grid.color);
  m_grid_shader->set_uniform(m_grid_uniforms.line_color, k_grid_line_color);
  m_grid_shader->set_uniform(m_grid_uniforms.cell_size, grid.cell_size);
  m_grid_shader->set_uniform(m_grid_uniforms.thickness, grid.thickness);

  if (ground != nullptr) {
    context.meshes.draw(*ground);
  }
}

void EffectsPipeline::draw_selection_smoke(const SelectionSmokeCmd& smoke,
                                           Mesh* disc,
                                           PipelineDrawContext& context) {
  const QMatrix4x4& view_proj = context.frame.view_proj;

  if (context.state.bound_shader != m_basic_shader) {
    m_basic_shader->use();
    context.state.bound_shader = m_basic_shader;
  }
  m_basic_shader->set_uniform(m_basic_uniforms.use_texture, false);
  m_basic_shader->set_uniform(m_basic_uniforms.instanced, false);
  m_basic_shader->set_uniform(m_basic_uniforms.view_proj, view_proj);
  if (m_basic_uniforms.camera_pos != GL::Shader::InvalidUniform) {
    m_basic_shader->set_uniform(m_basic_uniforms.camera_pos,
                                context.frame.camera->get_position());
  }
  m_basic_shader->set_uniform(m_basic_uniforms.color, smoke.color);
  DepthMaskScope const depth_mask(false);
  DepthTestScope const depth_test(true);

  PolygonOffsetScope const poly(-1.0F, -1.0F);
  BlendScope const blend(true);
  for (int i = 0; i < 7; ++i) {
    float const scale = 1.35F + 0.12F * i;
    float const a = smoke.base_alpha * (1.0F - 0.09F * i);
    QMatrix4x4 m = smoke.model;
    m.translate(0.0F, 0.02F, 0.0F);
    m.scale(scale, 1.0F, scale);
    const QMatrix4x4 mvp = view_proj * m;
    m_basic_shader->set_uniform(m_basic_uniforms.mvp, mvp);
    m_basic_shader->set_uniform(m_basic_uniforms.model, m);
    m_basic_shader->set_uniform(m_basic_uniforms.alpha, a);
    context.meshes.draw(*disc);
  }
}

void EffectsPipeline::cache_basic_uniforms() {
  if (m_basic_shader == nullptr) {
    return;
  }

  m_basic_uniforms.mvp = m_basic_shader->optional_uniform_handle("u_mvp");
  m_basic_uniforms.model = m_basic_shader->uniform_handle("u_model");
  m_basic_uniforms.view_proj = m_basic_shader->optional_uniform_handle("u_view_proj");
  m_basic_uniforms.texture = m_basic_shader->uniform_handle("u_texture");
  m_basic_uniforms.use_texture = m_basic_shader->uniform_handle("u_use_texture");
  m_basic_uniforms.color = m_basic_shader->uniform_handle("u_color");
  m_basic_uniforms.alpha = m_basic_shader->uniform_handle("u_alpha");
  m_basic_uniforms.instanced = m_basic_shader->optional_uniform_handle("u_instanced");
  m_basic_uniforms.camera_pos = m_basic_shader->optional_uniform_handle("u_camera_pos");
}

void EffectsPipeline::cache_basic_instanced_uniforms() {
  if (m_basic_instanced_shader == nullptr) {
    return;
  }
  m_basic_instanced_uniforms.view_proj =
      m_basic_instanced_shader->optional_uniform_handle("u_view_proj");
  m_basic_instanced_uniforms.use_texture =
      m_basic_instanced_shader->optional_uniform_handle("u_use_texture");
}

void EffectsPipeline::cache_grid_uniforms() {
  if (m_grid_shader == nullptr) {
    return;
  }

  m_grid_uniforms.mvp = m_grid_shader->optional_uniform_handle("u_mvp");
  m_grid_uniforms.model = m_grid_shader->uniform_handle("u_model");
  m_grid_uniforms.grid_color = m_grid_shader->uniform_handle("u_grid_color");
  m_grid_uniforms.line_color = m_grid_shader->uniform_handle("u_line_color");
  m_grid_uniforms.cell_size = m_grid_shader->uniform_handle("u_cell_size");
  m_grid_uniforms.thickness = m_grid_shader->uniform_handle("u_thickness");
}

} // namespace Render::GL::BackendPipelines
