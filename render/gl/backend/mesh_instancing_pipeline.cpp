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
// These match the primitive_instanced.vert shader layout
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
  // This pipeline uses per-instance vertex attributes for model/color/alpha,
  // not uniforms. The view-projection uniform is set by the shader's existing
  // uniform binding logic in the Backend. Only the u_viewProj and u_lightDir
  // uniforms are needed for instanced shaders, and those are set when the
  // shader is bound. No additional uniform caching is required here.
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
                                        int /*material_id*/) {
  MeshInstanceGpu inst{};

  // Pack model matrix columns with translation in w components
  // This matches the primitive_instanced.vert layout:
  // mat4 = mat4(vec4(col0.xyz, 0), vec4(col1.xyz, 0), vec4(col2.xyz, 0),
  //             vec4(col0.w, col1.w, col2.w, 1))
  const float *data = model.constData();
  // Column 0: rotation/scale column 0, translation.x
  inst.model_col0[0] = data[0];
  inst.model_col0[1] = data[1];
  inst.model_col0[2] = data[2];
  inst.model_col0[3] = data[12]; // translation.x
  // Column 1: rotation/scale column 1, translation.y
  inst.model_col1[0] = data[4];
  inst.model_col1[1] = data[5];
  inst.model_col1[2] = data[6];
  inst.model_col1[3] = data[13]; // translation.y
  // Column 2: rotation/scale column 2, translation.z
  inst.model_col2[0] = data[8];
  inst.model_col2[1] = data[9];
  inst.model_col2[2] = data[10];
  inst.model_col2[3] = data[14]; // translation.z

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
  if (m_currentMesh == nullptr || m_currentShader == nullptr ||
      !m_initialized) {
    qWarning() << "MeshInstancingPipeline::flush called with invalid state:"
               << "mesh=" << m_currentMesh << "shader=" << m_currentShader
               << "initialized=" << m_initialized
               << "instances=" << m_instances.size();
    m_instances.clear();
    return;
  }

  Q_UNUSED(view_proj); // view_proj is set by shader's existing uniform binding

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
  for (GLuint loc = k_instance_model_col0_loc;
       loc <= k_instance_color_alpha_loc; ++loc) {
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

void MeshInstancingPipeline::setup_instance_attributes() {
  glBindBuffer(GL_ARRAY_BUFFER, m_instanceBuffer);

  const auto stride = static_cast<GLsizei>(sizeof(MeshInstanceGpu));

  // Model matrix column 0 (xyz = rotation/scale col 0, w = translation.x)
  glEnableVertexAttribArray(k_instance_model_col0_loc);
  glVertexAttribPointer(
      k_instance_model_col0_loc, 4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(MeshInstanceGpu, model_col0)));
  glVertexAttribDivisor(k_instance_model_col0_loc, 1);

  // Model matrix column 1 (xyz = rotation/scale col 1, w = translation.y)
  glEnableVertexAttribArray(k_instance_model_col1_loc);
  glVertexAttribPointer(
      k_instance_model_col1_loc, 4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(MeshInstanceGpu, model_col1)));
  glVertexAttribDivisor(k_instance_model_col1_loc, 1);

  // Model matrix column 2 (xyz = rotation/scale col 2, w = translation.z)
  glEnableVertexAttribArray(k_instance_model_col2_loc);
  glVertexAttribPointer(
      k_instance_model_col2_loc, 4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(MeshInstanceGpu, model_col2)));
  glVertexAttribDivisor(k_instance_model_col2_loc, 1);

  // Color and alpha packed together
  glEnableVertexAttribArray(k_instance_color_alpha_loc);
  glVertexAttribPointer(
      k_instance_color_alpha_loc, 4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(MeshInstanceGpu, color_alpha)));
  glVertexAttribDivisor(k_instance_color_alpha_loc, 1);
}

} // namespace Render::GL::BackendPipelines
