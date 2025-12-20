#include "mesh_instancing_pipeline.h"
#include "../backend.h"
#include "../mesh.h"
#include "../texture.h"
#include <QDebug>
#include <QOpenGLContext>
#include <cstring>

namespace Render::GL::BackendPipelines {

namespace {
constexpr std::size_t k_initial_capacity = 256;
constexpr std::size_t k_max_instances_per_batch = 4096;

// Vertex attribute locations for instance data
// These must match the shader layout
constexpr GLuint k_instance_model_col0_loc = 4;
constexpr GLuint k_instance_model_col1_loc = 5;
constexpr GLuint k_instance_model_col2_loc = 6;
constexpr GLuint k_instance_model_col3_loc = 7;
constexpr GLuint k_instance_color_alpha_loc = 8;
constexpr GLuint k_instance_material_loc = 9;
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

  // Create instance buffer
  glGenBuffers(1, &m_instanceBuffer);
  if (m_instanceBuffer == 0) {
    qWarning() << "MeshInstancingPipeline: failed to create instance buffer";
    return false;
  }

  m_instanceCapacity = k_initial_capacity;
  glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(m_instanceCapacity *
                                       sizeof(MeshInstanceGpu)),
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

void MeshInstancingPipeline::cache_uniforms() {
  // Uniforms are cached per-shader in the character pipeline
  // This pipeline delegates uniform handling to the existing infrastructure
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
                                        int material_id) {
  MeshInstanceGpu inst{};

  // Copy model matrix columns
  const float *data = model.constData();
  inst.model_col0[0] = data[0];
  inst.model_col0[1] = data[1];
  inst.model_col0[2] = data[2];
  inst.model_col0[3] = data[3];
  inst.model_col1[0] = data[4];
  inst.model_col1[1] = data[5];
  inst.model_col1[2] = data[6];
  inst.model_col1[3] = data[7];
  inst.model_col2[0] = data[8];
  inst.model_col2[1] = data[9];
  inst.model_col2[2] = data[10];
  inst.model_col2[3] = data[11];
  inst.model_col3[0] = data[12];
  inst.model_col3[1] = data[13];
  inst.model_col3[2] = data[14];
  inst.model_col3[3] = data[15];

  inst.color[0] = color.x();
  inst.color[1] = color.y();
  inst.color[2] = color.z();
  inst.alpha = alpha;
  inst.material_id = material_id;

  m_instances.push_back(inst);
}

void MeshInstancingPipeline::begin_batch(Mesh *mesh, Shader *shader,
                                         Texture *texture) {
  m_currentMesh = mesh;
  m_currentShader = shader;
  m_currentTexture = texture;
}

void MeshInstancingPipeline::flush(const QMatrix4x4 &view_proj) {
  if (m_instances.empty() || m_currentMesh == nullptr ||
      m_currentShader == nullptr || !m_initialized) {
    m_instances.clear();
    return;
  }

  const std::size_t count = m_instances.size();

  // Resize buffer if needed
  if (count > m_instanceCapacity) {
    std::size_t newCapacity = m_instanceCapacity;
    while (newCapacity < count) {
      newCapacity *= 2;
    }
    newCapacity = std::min(newCapacity, k_max_instances_per_batch);

    glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(newCapacity * sizeof(MeshInstanceGpu)), nullptr,
        GL_DYNAMIC_DRAW);
    m_instanceCapacity = newCapacity;
  }

  // Upload instance data
  glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
  glBufferSubData(GL_ARRAY_BUFFER, 0,
                  static_cast<GLsizeiptr>(count * sizeof(MeshInstanceGpu)),
                  m_instances.data());

  // Set up instance attributes
  setup_instance_attributes();

  // Bind texture if present
  if (m_currentTexture != nullptr) {
    m_currentTexture->bind(0);
  }

  // Draw instanced
  m_currentMesh->draw_instanced(count);

  // Clean up instance attributes
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  for (GLuint loc = k_instance_model_col0_loc; loc <= k_instance_material_loc;
       ++loc) {
    glDisableVertexAttribArray(loc);
  }

  m_instances.clear();
}

auto MeshInstancingPipeline::instance_count() const -> std::size_t {
  return m_instances.size();
}

auto MeshInstancingPipeline::has_pending() const -> bool {
  return !m_instances.empty();
}

void MeshInstancingPipeline::upload_instances() {
  if (m_instances.empty()) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);
  glBufferSubData(
      GL_ARRAY_BUFFER, 0,
      static_cast<GLsizeiptr>(m_instances.size() * sizeof(MeshInstanceGpu)),
      m_instances.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void MeshInstancingPipeline::setup_instance_attributes() {
  glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);

  const auto stride = static_cast<GLsizei>(sizeof(MeshInstanceGpu));

  // Model matrix column 0
  glEnableVertexAttribArray(k_instance_model_col0_loc);
  glVertexAttribPointer(
      k_instance_model_col0_loc, 4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(MeshInstanceGpu, model_col0)));
  glVertexAttribDivisor(k_instance_model_col0_loc, 1);

  // Model matrix column 1
  glEnableVertexAttribArray(k_instance_model_col1_loc);
  glVertexAttribPointer(
      k_instance_model_col1_loc, 4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(MeshInstanceGpu, model_col1)));
  glVertexAttribDivisor(k_instance_model_col1_loc, 1);

  // Model matrix column 2
  glEnableVertexAttribArray(k_instance_model_col2_loc);
  glVertexAttribPointer(
      k_instance_model_col2_loc, 4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(MeshInstanceGpu, model_col2)));
  glVertexAttribDivisor(k_instance_model_col2_loc, 1);

  // Model matrix column 3
  glEnableVertexAttribArray(k_instance_model_col3_loc);
  glVertexAttribPointer(
      k_instance_model_col3_loc, 4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(MeshInstanceGpu, model_col3)));
  glVertexAttribDivisor(k_instance_model_col3_loc, 1);

  // Color and alpha packed together
  glEnableVertexAttribArray(k_instance_color_alpha_loc);
  glVertexAttribPointer(
      k_instance_color_alpha_loc, 4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(MeshInstanceGpu, color)));
  glVertexAttribDivisor(k_instance_color_alpha_loc, 1);

  // Material ID as integer attribute
  glEnableVertexAttribArray(k_instance_material_loc);
  glVertexAttribIPointer(
      k_instance_material_loc, 1, GL_INT, stride,
      reinterpret_cast<void *>(offsetof(MeshInstanceGpu, material_id)));
  glVertexAttribDivisor(k_instance_material_loc, 1);
}

} // namespace Render::GL::BackendPipelines
