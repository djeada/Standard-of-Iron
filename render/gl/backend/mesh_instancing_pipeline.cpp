#include "mesh_instancing_pipeline.h"
#include "../backend.h"
#include "../mesh.h"
#include "../texture.h"
#include <QDebug>
#include <QOpenGLContext>
#include <QStringLiteral>
#include <cstring>

namespace Render::GL::BackendPipelines {

namespace {
constexpr std::size_t k_initial_capacity = 256;
constexpr std::size_t k_max_instances_per_batch = 4096;

constexpr GLuint k_instance_model_col0_loc = 3;
constexpr GLuint k_instance_model_col1_loc = 4;
constexpr GLuint k_instance_model_col2_loc = 5;
constexpr GLuint k_instance_color_alpha_loc = 6;
} // namespace

MeshInstancingPipeline::MeshInstancingPipeline(GL::Backend *backend,
                                               GL::ShaderCache *shader_cache)
    : m_backend(backend), m_shaderCache(shader_cache) {
  m_instances.reserve(k_initial_capacity);
}

MeshInstancingPipeline::~MeshInstancingPipeline() { shutdown(); }

auto MeshInstancingPipeline::initialize() -> bool {
  if (m_initialized) {
    return true;
  }

  if (QOpenGLContext::currentContext() == nullptr) {
    qWarning() << "MeshInstancingPipeline::initialize called without GL "
                  "context";
    return false;
  }

  initializeOpenGLFunctions();

  glGenBuffers(1, &m_instanceBuffer);
  if (m_instanceBuffer == 0) {
    qWarning() << "MeshInstancingPipeline: failed to create instance buffer";
    return false;
  }

  m_instanceCapacity = k_initial_capacity;
  glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
  glBufferData(
      GL_ARRAY_BUFFER,
      static_cast<GLsizeiptr>(m_instanceCapacity * sizeof(MeshInstanceGpu)),
      nullptr, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  // Load the instanced shader for mesh batching
  if (m_shaderCache != nullptr) {
    m_instancedShader =
        m_shaderCache->get(QStringLiteral("spearman_instanced"));
    if (m_instancedShader != nullptr) {
      cache_uniforms();
      qInfo() << "MeshInstancingPipeline: loaded instanced shader";
    } else {
      qWarning()
          << "MeshInstancingPipeline: spearman_instanced shader not found";
    }
  }

  m_initialized = true;
  qInfo() << "MeshInstancingPipeline initialized with capacity"
          << m_instanceCapacity;
  return true;
}

void MeshInstancingPipeline::shutdown() {
  if (!m_initialized) {
    return;
  }

  if (QOpenGLContext::currentContext() != nullptr) {
    if (m_instanceBuffer != 0) {
      glDeleteBuffers(1, &m_instanceBuffer);
      m_instanceBuffer = 0;
    }
  }

  m_instances.clear();
  m_currentMesh = nullptr;
  m_currentShader = nullptr;
  m_currentTexture = nullptr;
  m_instancedShader = nullptr;
  m_initialized = false;
}

void MeshInstancingPipeline::cache_uniforms() {
  if (m_instancedShader == nullptr) {
    return;
  }

  m_uniforms.view_proj =
      m_instancedShader->uniform_location(QStringLiteral("u_viewProj"));
  m_uniforms.texture =
      m_instancedShader->uniform_location(QStringLiteral("u_texture"));
  m_uniforms.use_texture =
      m_instancedShader->uniform_location(QStringLiteral("u_useTexture"));
}

auto MeshInstancingPipeline::is_initialized() const -> bool {
  return m_initialized;
}

void MeshInstancingPipeline::begin_frame() {
  m_instances.clear();
  m_currentMesh = nullptr;
  m_currentShader = nullptr;
  m_currentTexture = nullptr;
}

auto MeshInstancingPipeline::can_batch(Mesh *mesh, Shader *shader,
                                       Texture *texture) const -> bool {
  if (m_instances.empty()) {
    return true;
  }
  if (m_instances.size() >= k_max_instances_per_batch) {
    return false;
  }
  return mesh == m_currentMesh && shader == m_currentShader &&
         texture == m_currentTexture;
}

void MeshInstancingPipeline::accumulate(const QMatrix4x4 &model,
                                        const QVector3D &color, float alpha,
                                        int) {
  MeshInstanceGpu inst{};

  const float *data = model.constData();

  inst.model_col0[0] = data[0];
  inst.model_col0[1] = data[1];
  inst.model_col0[2] = data[2];
  inst.model_col0[3] = data[12];

  inst.model_col1[0] = data[4];
  inst.model_col1[1] = data[5];
  inst.model_col1[2] = data[6];
  inst.model_col1[3] = data[13];

  inst.model_col2[0] = data[8];
  inst.model_col2[1] = data[9];
  inst.model_col2[2] = data[10];
  inst.model_col2[3] = data[14];

  inst.color_alpha[0] = color.x();
  inst.color_alpha[1] = color.y();
  inst.color_alpha[2] = color.z();
  inst.color_alpha[3] = alpha;

  m_instances.push_back(inst);
}

void MeshInstancingPipeline::begin_batch(Mesh *mesh, Shader *shader,
                                         Texture *texture) {
  m_currentMesh = mesh;
  m_currentShader = shader;
  m_currentTexture = texture;
}

void MeshInstancingPipeline::flush(const QMatrix4x4 &view_proj) {
  if (m_instances.empty()) {
    return;
  }
  if (m_currentMesh == nullptr || !m_initialized) {
    qWarning() << "MeshInstancingPipeline::flush called with invalid state:"
               << "mesh=" << m_currentMesh
               << "initialized=" << m_initialized
               << "instances=" << m_instances.size();
    m_instances.clear();
    return;
  }

  // Use the instanced shader if available, otherwise fall back
  Shader *active_shader =
      (m_instancedShader != nullptr) ? m_instancedShader : m_currentShader;
  if (active_shader == nullptr) {
    qWarning()
        << "MeshInstancingPipeline::flush: no shader available for instancing";
    m_instances.clear();
    return;
  }

  const std::size_t count = m_instances.size();

  if (count > m_instanceCapacity) {
    std::size_t newCapacity = m_instanceCapacity;
    while (newCapacity < count) {
      newCapacity *= 2;
    }
    newCapacity = std::min(newCapacity, k_max_instances_per_batch);

    glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(newCapacity * sizeof(MeshInstanceGpu)),
                 nullptr, GL_DYNAMIC_DRAW);
    m_instanceCapacity = newCapacity;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
  glBufferSubData(GL_ARRAY_BUFFER, 0,
                  static_cast<GLsizeiptr>(count * sizeof(MeshInstanceGpu)),
                  m_instances.data());

  // Use the instanced shader
  active_shader->use();

  // Set shared view-projection uniform
  if (m_uniforms.view_proj != Shader::InvalidUniform) {
    active_shader->set_uniform(m_uniforms.view_proj, view_proj);
  }

  // Set texture uniforms
  bool has_texture = (m_currentTexture != nullptr);
  if (m_uniforms.use_texture != Shader::InvalidUniform) {
    active_shader->set_uniform(m_uniforms.use_texture, has_texture);
  }
  if (has_texture) {
    m_currentTexture->bind(0);
    if (m_uniforms.texture != Shader::InvalidUniform) {
      active_shader->set_uniform(m_uniforms.texture, 0);
    }
  }

  setup_instance_attributes();

  m_currentMesh->draw_instanced(count);

  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glDisableVertexAttribArray(k_instance_model_col0_loc);
  glDisableVertexAttribArray(k_instance_model_col1_loc);
  glDisableVertexAttribArray(k_instance_model_col2_loc);
  glDisableVertexAttribArray(k_instance_color_alpha_loc);

  m_instances.clear();
}

auto MeshInstancingPipeline::instance_count() const -> std::size_t {
  return m_instances.size();
}

auto MeshInstancingPipeline::has_pending() const -> bool {
  return !m_instances.empty();
}

void MeshInstancingPipeline::setup_instance_attributes() {
  glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);

  const auto stride = static_cast<GLsizei>(sizeof(MeshInstanceGpu));

  glEnableVertexAttribArray(k_instance_model_col0_loc);
  glVertexAttribPointer(
      k_instance_model_col0_loc, 4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(MeshInstanceGpu, model_col0)));
  glVertexAttribDivisor(k_instance_model_col0_loc, 1);

  glEnableVertexAttribArray(k_instance_model_col1_loc);
  glVertexAttribPointer(
      k_instance_model_col1_loc, 4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(MeshInstanceGpu, model_col1)));
  glVertexAttribDivisor(k_instance_model_col1_loc, 1);

  glEnableVertexAttribArray(k_instance_model_col2_loc);
  glVertexAttribPointer(
      k_instance_model_col2_loc, 4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(MeshInstanceGpu, model_col2)));
  glVertexAttribDivisor(k_instance_model_col2_loc, 1);

  glEnableVertexAttribArray(k_instance_color_alpha_loc);
  glVertexAttribPointer(
      k_instance_color_alpha_loc, 4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(MeshInstanceGpu, color_alpha)));
  glVertexAttribDivisor(k_instance_color_alpha_loc, 1);
}

} // namespace Render::GL::BackendPipelines
