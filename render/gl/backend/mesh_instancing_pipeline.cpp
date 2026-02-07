#include "mesh_instancing_pipeline.h"
#include "../backend.h"
#include "../mesh.h"
#include "../texture.h"
#include <QDebug>
#include <QOpenGLContext>
#include <cstring>

namespace Render::GL::BackendPipelines {

namespace {
constexpr std::size_t k_initial_capacity = 512;
constexpr std::size_t k_max_instances_per_batch = 8192;

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
  m_initialized = false;
}

void MeshInstancingPipeline::cache_uniforms() {}

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

void MeshInstancingPipeline::flush() {
  if (m_instances.empty()) {
    return;
  }
  if (m_currentMesh == nullptr || m_currentShader == nullptr ||
      !m_initialized) {
    qWarning() << "MeshInstancingPipeline::flush called with invalid state:"
               << "mesh=" << m_currentMesh << "shader=" << m_currentShader
               << "initialized=" << m_initialized
               << "instances=" << m_instances.size();
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
  GLsizeiptr const upload_size =
      static_cast<GLsizeiptr>(count * sizeof(MeshInstanceGpu));

  void *mapped =
      glMapBufferRange(GL_ARRAY_BUFFER, 0, upload_size,
                       GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT);
  if (mapped != nullptr) {
    std::memcpy(mapped, m_instances.data(),
                static_cast<std::size_t>(upload_size));
    glUnmapBuffer(GL_ARRAY_BUFFER);
  } else {
    glBufferSubData(GL_ARRAY_BUFFER, 0, upload_size, m_instances.data());
  }

  if (!m_currentMesh->bind_vao()) {
    m_instances.clear();
    return;
  }

  if (!m_currentMesh->bind_vao()) {
    m_instances.clear();
    return;
  }

  setup_instance_attributes();

  if (m_currentTexture != nullptr) {
    m_currentTexture->bind(0);
  }

  m_currentMesh->draw_instanced_raw(count);

  glVertexAttribDivisor(k_instance_model_col0_loc, 0);
  glVertexAttribDivisor(k_instance_model_col1_loc, 0);
  glVertexAttribDivisor(k_instance_model_col2_loc, 0);
  glVertexAttribDivisor(k_instance_color_alpha_loc, 0);
  glDisableVertexAttribArray(k_instance_model_col0_loc);
  glDisableVertexAttribArray(k_instance_model_col1_loc);
  glDisableVertexAttribArray(k_instance_model_col2_loc);
  glDisableVertexAttribArray(k_instance_color_alpha_loc);

  m_currentMesh->unbind_vao();
  glBindBuffer(GL_ARRAY_BUFFER, 0);

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
