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

  // Load nation-specific instanced shaders and map them to original shaders
  if (m_shaderCache != nullptr) {
    // Carthage spearman mapping
    Shader *carthageOriginal =
        m_shaderCache->get(QStringLiteral("spearman_carthage"));
    Shader *carthageInstanced =
        m_shaderCache->get(QStringLiteral("spearman_instanced_carthage"));
    if (carthageOriginal != nullptr && carthageInstanced != nullptr) {
      InstancedShaderInfo info;
      info.shader = carthageInstanced;
      carthageInstanced->use();
      info.uniforms.view_proj =
          carthageInstanced->optional_uniform_handle("u_viewProj");
      info.uniforms.texture =
          carthageInstanced->optional_uniform_handle("u_texture");
      info.uniforms.use_texture =
          carthageInstanced->optional_uniform_handle("u_useTexture");
      info.uniforms.material_id =
          carthageInstanced->optional_uniform_handle("u_materialId");
      m_shaderMap[carthageOriginal] = info;
      qInfo() << "MeshInstancingPipeline: mapped spearman_carthage -> "
                 "spearman_instanced_carthage";
    }

    // Roman Republic spearman mapping
    Shader *romeOriginal =
        m_shaderCache->get(QStringLiteral("spearman_roman_republic"));
    Shader *romeInstanced =
        m_shaderCache->get(QStringLiteral("spearman_instanced_roman_republic"));
    if (romeOriginal != nullptr && romeInstanced != nullptr) {
      InstancedShaderInfo info;
      info.shader = romeInstanced;
      romeInstanced->use();
      info.uniforms.view_proj =
          romeInstanced->optional_uniform_handle("u_viewProj");
      info.uniforms.texture =
          romeInstanced->optional_uniform_handle("u_texture");
      info.uniforms.use_texture =
          romeInstanced->optional_uniform_handle("u_useTexture");
      info.uniforms.material_id =
          romeInstanced->optional_uniform_handle("u_materialId");
      m_shaderMap[romeOriginal] = info;
      qInfo() << "MeshInstancingPipeline: mapped spearman_roman_republic -> "
                 "spearman_instanced_roman_republic";
    }
  }

  m_initialized = true;
  qInfo() << "MeshInstancingPipeline initialized with capacity"
          << m_instanceCapacity << "and" << m_shaderMap.size()
          << "shader mappings";
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
  m_shaderMap.clear();
  m_activeInstancedShader = nullptr;
  m_initialized = false;
}

void MeshInstancingPipeline::cache_uniforms() {
  // Uniforms are cached per-shader in the shader map during initialize()
}

auto MeshInstancingPipeline::is_initialized() const -> bool {
  return m_initialized;
}

void MeshInstancingPipeline::begin_frame() {
  m_instances.clear();
  m_currentMesh = nullptr;
  m_currentShader = nullptr;
  m_currentTexture = nullptr;
  m_currentMaterialId = 0;
  m_activeInstancedShader = nullptr;
}

auto MeshInstancingPipeline::can_batch(Mesh *mesh, Shader *shader,
                                       Texture *texture, int material_id) const -> bool {
  if (m_instances.empty()) {
    return true;
  }
  if (m_instances.size() >= k_max_instances_per_batch) {
    return false;
  }
  return mesh == m_currentMesh && shader == m_currentShader &&
         texture == m_currentTexture && material_id == m_currentMaterialId;
}

void MeshInstancingPipeline::accumulate(const QMatrix4x4 &model,
                                        const QVector3D &color, float alpha,
                                        int material_id) {
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
  
  // Store material_id for this batch (all instances in a batch share the same material_id)
  m_currentMaterialId = material_id;
}

void MeshInstancingPipeline::begin_batch(Mesh *mesh, Shader *shader,
                                         Texture *texture) {
  m_currentMesh = mesh;
  m_currentShader = shader;
  m_currentTexture = texture;

  // Look up the instanced shader for this original shader
  auto it = m_shaderMap.find(shader);
  if (it != m_shaderMap.end()) {
    m_activeInstancedShader = it->second.shader;
    m_activeUniforms = it->second.uniforms;
  } else {
    m_activeInstancedShader = nullptr;
  }
}

void MeshInstancingPipeline::flush(const QMatrix4x4 &view_proj) {
  if (m_instances.empty()) {
    return;
  }
  if (m_currentMesh == nullptr || !m_initialized) {
    qWarning() << "MeshInstancingPipeline::flush called with invalid state:"
               << "mesh=" << m_currentMesh << "initialized=" << m_initialized
               << "instances=" << m_instances.size();
    m_instances.clear();
    return;
  }

  // Use the mapped instanced shader, or fall back
  Shader *active_shader = m_activeInstancedShader;
  if (active_shader == nullptr) {
    qWarning()
        << "MeshInstancingPipeline::flush: no instanced shader for original";
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

  // IMPORTANT: instance attribute pointers are VAO state.
  // Bind the mesh VAO before calling setup_instance_attributes(), otherwise
  // the attributes get installed on VAO=0 and the instanced draw reads zeros.
  if (!m_currentMesh->bind("MeshInstancingPipeline::flush")) {
    m_instances.clear();
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return;
  }

  // Use the instanced shader
  active_shader->use();

  // Set shared view-projection uniform
  if (m_activeUniforms.view_proj != Shader::InvalidUniform) {
    active_shader->set_uniform(m_activeUniforms.view_proj, view_proj);
  }

  // Set texture uniforms
  bool has_texture = (m_currentTexture != nullptr);
  if (m_activeUniforms.use_texture != Shader::InvalidUniform) {
    active_shader->set_uniform(m_activeUniforms.use_texture, has_texture);
  }
  if (has_texture) {
    m_currentTexture->bind(0);
    if (m_activeUniforms.texture != Shader::InvalidUniform) {
      active_shader->set_uniform(m_activeUniforms.texture, 0);
    }
  }
  
  // Set material_id uniform
  if (m_activeUniforms.material_id != Shader::InvalidUniform) {
    active_shader->set_uniform(m_activeUniforms.material_id, m_currentMaterialId);
  }

  setup_instance_attributes();

  glDrawElementsInstanced(
      GL_TRIANGLES,
      static_cast<GLsizei>(m_currentMesh->get_indices().size()),
      GL_UNSIGNED_INT,
      nullptr,
      static_cast<GLsizei>(count));

  // Clean up per-instance attribs on this VAO
  glVertexAttribDivisor(k_instance_model_col0_loc, 0);
  glVertexAttribDivisor(k_instance_model_col1_loc, 0);
  glVertexAttribDivisor(k_instance_model_col2_loc, 0);
  glVertexAttribDivisor(k_instance_color_alpha_loc, 0);
  glDisableVertexAttribArray(k_instance_model_col0_loc);
  glDisableVertexAttribArray(k_instance_model_col1_loc);
  glDisableVertexAttribArray(k_instance_model_col2_loc);
  glDisableVertexAttribArray(k_instance_color_alpha_loc);

  m_currentMesh->unbind();
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  m_instances.clear();
}

auto MeshInstancingPipeline::instance_count() const -> std::size_t {
  return m_instances.size();
}

auto MeshInstancingPipeline::has_pending() const -> bool {
  return !m_instances.empty();
}

auto MeshInstancingPipeline::get_instanced_shader(Shader *original_shader) const
    -> Shader * {
  auto it = m_shaderMap.find(original_shader);
  return (it != m_shaderMap.end()) ? it->second.shader : nullptr;
}

auto MeshInstancingPipeline::has_instanced_shaders() const -> bool {
  return !m_shaderMap.empty();
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
