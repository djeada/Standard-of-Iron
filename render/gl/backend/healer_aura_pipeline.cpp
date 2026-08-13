#include "healer_aura_pipeline.h"

#include <QDebug>
#include <QMatrix4x4>
#include <QOpenGLContext>

#include <cmath>
#include <numbers>

#include "gl_error_check.h"
#include "render/gl/render_constants.h"
#include "render/gl/shader_cache.h"
#include "render/gl/state_scopes.h"
#include "static_mesh_upload.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {

auto check_gl_error(const char* operation) -> bool {
  return BackendPipelines::check_gl_error("HealerAuraPipeline", operation);
}

} // namespace

auto HealerAuraPipeline::initialize() -> bool {
  if (m_shader_cache == nullptr) {
    qWarning() << "HealerAuraPipeline::initialize: null ShaderCache";
    return false;
  }

  clear_gl_errors();

  m_aura_shader = m_shader_cache->get("healing_aura");
  if (m_aura_shader == nullptr) {
    qWarning() << "HealerAuraPipeline: Failed to get healing_aura shader";
    return false;
  }

  cache_uniforms();

  if (!create_dome_geometry()) {
    qWarning() << "HealerAuraPipeline: Failed to create dome geometry";
    return false;
  }

  qInfo() << "HealerAuraPipeline initialized successfully";
  return is_initialized();
}

void HealerAuraPipeline::shutdown() {
  release_geometry();
  m_aura_shader = nullptr;
}

void HealerAuraPipeline::release_geometry() {
  if (QOpenGLContext::currentContext() != nullptr) {
    initializeOpenGLFunctions();
    clear_gl_errors();
  }
  release_mesh_buffers(*this, m_mesh);
}

void HealerAuraPipeline::cache_uniforms() {
  if (m_aura_shader == nullptr) {
    return;
  }

  m_uniforms.mvp = m_aura_shader->uniform_handle("u_mvp");
  m_uniforms.model = m_aura_shader->uniform_handle("u_model");
  m_uniforms.time = m_aura_shader->uniform_handle("u_time");
  m_uniforms.aura_radius = m_aura_shader->uniform_handle("u_aura_radius");
  m_uniforms.intensity = m_aura_shader->uniform_handle("u_intensity");
  m_uniforms.aura_color = m_aura_shader->uniform_handle("u_aura_color");
}

auto HealerAuraPipeline::is_initialized() const -> bool {
  return m_aura_shader != nullptr && m_mesh.vao != 0 && m_mesh.index_count > 0;
}

struct AuraVertex {
  float position[3];
  float normal[3];
  float tex_coord[2];
};

auto HealerAuraPipeline::create_dome_geometry() -> bool {
  initializeOpenGLFunctions();
  release_geometry();
  clear_gl_errors();

  std::vector<AuraVertex> vertices;
  std::vector<unsigned int> indices;

  constexpr int stacks = 8;
  constexpr int slices = 16;
  constexpr float pi = std::numbers::pi_v<float>;

  vertices.reserve(static_cast<size_t>((stacks + 1) * (slices + 1)));

  for (int i = 0; i <= stacks; ++i) {
    float const phi = (static_cast<float>(i) / static_cast<float>(stacks)) * pi * 0.5F;
    float const y = std::sin(phi);
    float const r = std::cos(phi);

    for (int j = 0; j <= slices; ++j) {
      float const theta =
          (static_cast<float>(j) / static_cast<float>(slices)) * pi * 2.0F;
      float const x = r * std::cos(theta);
      float const z = r * std::sin(theta);

      AuraVertex v{};
      v.position[0] = x;
      v.position[1] = y;
      v.position[2] = z;
      v.normal[0] = x;
      v.normal[1] = y;
      v.normal[2] = z;
      v.tex_coord[0] = static_cast<float>(j) / static_cast<float>(slices);
      v.tex_coord[1] = static_cast<float>(i) / static_cast<float>(stacks);
      vertices.push_back(v);
    }
  }

  indices.reserve(static_cast<size_t>(stacks * slices * 6));
  for (int i = 0; i < stacks; ++i) {
    for (int j = 0; j < slices; ++j) {
      auto const curr = static_cast<unsigned int>(i * (slices + 1) + j);
      unsigned int const next = curr + static_cast<unsigned int>(slices + 1);

      indices.push_back(curr);
      indices.push_back(next);
      indices.push_back(curr + 1);

      indices.push_back(curr + 1);
      indices.push_back(next);
      indices.push_back(next + 1);
    }
  }

  return upload_static_effect_mesh(*this,
                                   m_mesh,
                                   "HealerAuraPipeline dome",
                                   vertices.data(),
                                   vertices.size(),
                                   sizeof(AuraVertex),
                                   k_position_normal_texcoord_layout,
                                   indices);
}

void HealerAuraPipeline::render_aura_batch(const AuraInstanceData* instances,
                                           std::size_t count,
                                           const QMatrix4x4& view_proj) {
  if (!is_initialized() || instances == nullptr || count == 0) {
    return;
  }

  CullFaceScope const cull(false);
  DepthTestScope const depth_test(true);
  DepthMaskScope const depth_mask(false);
  BlendScope const blend(true);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);

  m_aura_shader->use();
  glBindVertexArray(m_mesh.vao);

  for (std::size_t idx = 0; idx < count; ++idx) {
    const AuraInstanceData& inst = instances[idx];
    if (inst.intensity < 0.01F) {
      continue;
    }

    QMatrix4x4 model;
    model.setToIdentity();
    model.translate(inst.position);
    model.scale(inst.radius);

    QMatrix4x4 const mvp = view_proj * model;

    m_aura_shader->set_uniform(m_uniforms.mvp, mvp);
    m_aura_shader->set_uniform(m_uniforms.model, model);
    m_aura_shader->set_uniform(m_uniforms.time, inst.time);
    m_aura_shader->set_uniform(m_uniforms.aura_radius, 1.0F);
    m_aura_shader->set_uniform(m_uniforms.intensity, inst.intensity);
    m_aura_shader->set_uniform(m_uniforms.aura_color, inst.color);

    glDrawElements(GL_TRIANGLES, m_mesh.index_count, GL_UNSIGNED_INT, nullptr);
  }

  glBindVertexArray(0);
}

} // namespace Render::GL::BackendPipelines
