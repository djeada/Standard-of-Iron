#include "healer_aura_pipeline.h"
#include "../../../game/core/component.h"
#include "../../../game/core/world.h"
#include "../../../game/systems/healing_colors.h"
#include "../backend.h"
#include "../camera.h"
#include "../render_constants.h"
#include "../shader_cache.h"
#include <QDebug>
#include <QMatrix4x4>
#include <QOpenGLContext>
#include <cmath>
#include <numbers>

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {
void clear_gl_errors() {
  while (glGetError() != GL_NO_ERROR) {
  }
}

auto check_gl_error(const char *operation) -> bool {
  GLenum err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "HealerAuraPipeline GL error in" << operation << ":" << err;
    return false;
  }
  return true;
}
} // namespace

auto HealerAuraPipeline::initialize() -> bool {
  if (m_shaderCache == nullptr) {
    qWarning() << "HealerAuraPipeline::initialize: null ShaderCache";
    return false;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  m_auraShader = m_shaderCache->get("healing_aura");
  if (m_auraShader == nullptr) {
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
  shutdown_geometry();
  m_auraShader = nullptr;
}

void HealerAuraPipeline::shutdown_geometry() {
  if (QOpenGLContext::currentContext() == nullptr) {

    m_vao = 0;
    m_vertexBuffer = 0;
    m_indexBuffer = 0;
    m_indexCount = 0;
    return;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  if (m_vao != 0) {
    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
  }
  if (m_vertexBuffer != 0) {
    glDeleteBuffers(1, &m_vertexBuffer);
    m_vertexBuffer = 0;
  }
  if (m_indexBuffer != 0) {
    glDeleteBuffers(1, &m_indexBuffer);
    m_indexBuffer = 0;
  }
  m_indexCount = 0;
}

void HealerAuraPipeline::cache_uniforms() {
  if (m_auraShader == nullptr) {
    return;
  }

  m_uniforms.mvp = m_auraShader->uniform_handle("u_mvp");
  m_uniforms.model = m_auraShader->uniform_handle("u_model");
  m_uniforms.time = m_auraShader->uniform_handle("u_time");
  m_uniforms.auraRadius = m_auraShader->uniform_handle("u_auraRadius");
  m_uniforms.intensity = m_auraShader->uniform_handle("u_intensity");
  m_uniforms.auraColor = m_auraShader->uniform_handle("u_auraColor");
}

auto HealerAuraPipeline::is_initialized() const -> bool {
  return m_auraShader != nullptr && m_vao != 0 && m_indexCount > 0;
}

struct AuraVertex {
  float position[3];
  float normal[3];
  float tex_coord[2];
};

auto HealerAuraPipeline::create_dome_geometry() -> bool {
  initializeOpenGLFunctions();
  shutdown_geometry();
  clear_gl_errors();

  std::vector<AuraVertex> vertices;
  std::vector<unsigned int> indices;

  constexpr int stacks = 8;
  constexpr int slices = 16;
  constexpr float pi = std::numbers::pi_v<float>;

  vertices.reserve(static_cast<size_t>((stacks + 1) * (slices + 1)));

  for (int i = 0; i <= stacks; ++i) {
    float phi =
        (static_cast<float>(i) / static_cast<float>(stacks)) * pi * 0.5F;
    float y = std::sin(phi);
    float r = std::cos(phi);

    for (int j = 0; j <= slices; ++j) {
      float theta =
          (static_cast<float>(j) / static_cast<float>(slices)) * pi * 2.0F;
      float x = r * std::cos(theta);
      float z = r * std::sin(theta);

      AuraVertex v;
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
      unsigned int curr = static_cast<unsigned int>(i * (slices + 1) + j);
      unsigned int next = curr + static_cast<unsigned int>(slices + 1);

      indices.push_back(curr);
      indices.push_back(next);
      indices.push_back(curr + 1);

      indices.push_back(curr + 1);
      indices.push_back(next);
      indices.push_back(next + 1);
    }
  }

  glGenVertexArrays(1, &m_vao);
  if (!check_gl_error("glGenVertexArrays") || m_vao == 0) {
    return false;
  }

  glBindVertexArray(m_vao);
  if (!check_gl_error("glBindVertexArray")) {
    glDeleteVertexArrays(1, &m_vao);
    m_vao = 0;
    return false;
  }

  glGenBuffers(1, &m_vertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_vertexBuffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(AuraVertex)),
               vertices.data(), GL_STATIC_DRAW);
  if (!check_gl_error("vertex buffer")) {
    shutdown_geometry();
    return false;
  }

  glGenBuffers(1, &m_indexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_indexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
               indices.data(), GL_STATIC_DRAW);
  if (!check_gl_error("index buffer")) {
    shutdown_geometry();
    return false;
  }

  m_indexCount = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(VertexAttrib::Position);
  glVertexAttribPointer(
      VertexAttrib::Position, ComponentCount::Vec3, GL_FLOAT, GL_FALSE,
      sizeof(AuraVertex),
      reinterpret_cast<void *>(offsetof(AuraVertex, position)));

  glEnableVertexAttribArray(VertexAttrib::Normal);
  glVertexAttribPointer(VertexAttrib::Normal, ComponentCount::Vec3, GL_FLOAT,
                        GL_FALSE, sizeof(AuraVertex),
                        reinterpret_cast<void *>(offsetof(AuraVertex, normal)));

  glEnableVertexAttribArray(VertexAttrib::TexCoord);
  glVertexAttribPointer(
      VertexAttrib::TexCoord, ComponentCount::Vec2, GL_FLOAT, GL_FALSE,
      sizeof(AuraVertex),
      reinterpret_cast<void *>(offsetof(AuraVertex, tex_coord)));

  glBindVertexArray(0);

  if (!check_gl_error("vertex attributes")) {
    shutdown_geometry();
    return false;
  }

  return true;
}

void HealerAuraPipeline::collect_healers(Engine::Core::World *world) {
  m_healerData.clear();

  if (world == nullptr) {
    return;
  }

  auto healers = world->get_entities_with<Engine::Core::HealerComponent>();

  for (auto *healer : healers) {
    if (healer->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto *transform = healer->get_component<Engine::Core::TransformComponent>();
    auto *healer_comp = healer->get_component<Engine::Core::HealerComponent>();
    auto *unit_comp = healer->get_component<Engine::Core::UnitComponent>();

    if (transform == nullptr || healer_comp == nullptr) {
      continue;
    }

    if (unit_comp != nullptr && unit_comp->health <= 0) {
      continue;
    }

    // Only Carthage healers use the aura of power (circular green aura)
    // Roman healers use healing beams instead
    if (unit_comp != nullptr && 
        !Game::Systems::uses_healing_aura(unit_comp->nation_id)) {
      continue;
    }

    HealerAuraData data;
    data.position = QVector3D(transform->position.x, transform->position.y,
                              transform->position.z);
    data.radius = healer_comp->healing_range;
    data.is_active = healer_comp->is_healing_active;

    data.intensity = data.is_active ? 1.0F : 0.5F;

    data.color = Game::Systems::get_healing_color(unit_comp->nation_id);

    m_healerData.push_back(data);
  }
}

void HealerAuraPipeline::render(const Camera &cam, float animation_time) {
  if (!is_initialized() || m_healerData.empty()) {
    return;
  }

  initializeOpenGLFunctions();
  clear_gl_errors();

  GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
  GLboolean depthTestEnabled = glIsEnabled(GL_DEPTH_TEST);
  GLboolean blendEnabled = glIsEnabled(GL_BLEND);
  GLboolean depthMaskEnabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskEnabled);

  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);

  m_auraShader->use();
  glBindVertexArray(m_vao);

  for (const auto &data : m_healerData) {
    render_aura(data, cam, animation_time);
  }

  glBindVertexArray(0);

  glDepthMask(depthMaskEnabled);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  if (!blendEnabled) {
    glDisable(GL_BLEND);
  }
  if (depthTestEnabled) {
    glEnable(GL_DEPTH_TEST);
  } else {
    glDisable(GL_DEPTH_TEST);
  }
  if (cullEnabled) {
    glEnable(GL_CULL_FACE);
  }
}

void HealerAuraPipeline::render_aura(const HealerAuraData &data,
                                     const Camera &cam, float animation_time) {

  QMatrix4x4 model;
  model.setToIdentity();
  model.translate(data.position);

  model.scale(data.radius);

  QMatrix4x4 vp = cam.get_projection_matrix() * cam.get_view_matrix();
  QMatrix4x4 mvp = vp * model;

  m_auraShader->set_uniform(m_uniforms.mvp, mvp);
  m_auraShader->set_uniform(m_uniforms.model, model);
  m_auraShader->set_uniform(m_uniforms.time, animation_time);

  m_auraShader->set_uniform(m_uniforms.auraRadius, 1.0F);
  m_auraShader->set_uniform(m_uniforms.intensity, data.intensity);
  m_auraShader->set_uniform(m_uniforms.auraColor, data.color);

  glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
}

void HealerAuraPipeline::render_single_aura(const QVector3D &position,
                                            const QVector3D &color,
                                            float radius, float intensity,
                                            float time,
                                            const QMatrix4x4 &view_proj) {
  if (!is_initialized()) {
    return;
  }
  if (intensity < 0.01F) {
    return;
  }

  initializeOpenGLFunctions();
  ;

  GLboolean cullEnabled = glIsEnabled(GL_CULL_FACE);
  GLboolean depthMaskEnabled = GL_TRUE;
  glGetBooleanv(GL_DEPTH_WRITEMASK, &depthMaskEnabled);

  glDisable(GL_CULL_FACE);
  glEnable(GL_DEPTH_TEST);
  glDepthMask(GL_FALSE);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE);

  m_auraShader->use();
  glBindVertexArray(m_vao);

  QMatrix4x4 model;
  model.setToIdentity();
  model.translate(position);
  model.scale(radius);

  QMatrix4x4 mvp = view_proj * model;

  m_auraShader->set_uniform(m_uniforms.mvp, mvp);
  m_auraShader->set_uniform(m_uniforms.model, model);
  m_auraShader->set_uniform(m_uniforms.time, time);
  m_auraShader->set_uniform(m_uniforms.auraRadius, 1.0F);
  m_auraShader->set_uniform(m_uniforms.intensity, intensity);
  m_auraShader->set_uniform(m_uniforms.auraColor, color);

  glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);

  glBindVertexArray(0);

  glDepthMask(depthMaskEnabled);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  if (cullEnabled) {
    glEnable(GL_CULL_FACE);
  }
}

} // namespace Render::GL::BackendPipelines
