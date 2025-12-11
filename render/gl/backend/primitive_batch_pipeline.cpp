#include "primitive_batch_pipeline.h"
#include "../backend.h"
#include "../mesh.h"
#include "../primitives.h"
#include "../render_constants.h"
#include <GL/gl.h>
#include <QOpenGLContext>
#include <algorithm>
#include <cstddef>

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

PrimitiveBatchPipeline::PrimitiveBatchPipeline(ShaderCache *shaderCache)
    : m_shaderCache(shaderCache) {}

PrimitiveBatchPipeline::~PrimitiveBatchPipeline() { shutdown(); }

auto PrimitiveBatchPipeline::initialize() -> bool {
  initializeOpenGLFunctions();

  if (m_shaderCache == nullptr) {
    return false;
  }

  m_shader = m_shaderCache->get(QStringLiteral("primitive_instanced"));
  if (m_shader == nullptr) {
    return false;
  }

  initializeSphereVao();
  initializeCylinderVao();
  initializeConeVao();
  cacheUniforms();

  m_initialized = true;
  return true;
}

void PrimitiveBatchPipeline::shutdown() {
  shutdownVaos();
  m_initialized = false;
}

void PrimitiveBatchPipeline::cacheUniforms() {
  if (m_shader != nullptr) {
    m_uniforms.view_proj = m_shader->uniformHandle("u_viewProj");
    m_uniforms.light_dir = m_shader->uniformHandle("u_lightDir");
    m_uniforms.ambient_strength = m_shader->uniformHandle("u_ambientStrength");
  }
}

void PrimitiveBatchPipeline::beginFrame() {}

void PrimitiveBatchPipeline::setupInstanceAttributes(GLuint vao,
                                                     GLuint instanceBuffer) {
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, instanceBuffer);

  const auto stride = static_cast<GLsizei>(sizeof(GL::PrimitiveInstanceGpu));

  glEnableVertexAttribArray(3);
  glVertexAttribPointer(
      3, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(GL::PrimitiveInstanceGpu, model_col0)));
  glVertexAttribDivisor(3, 1);

  glEnableVertexAttribArray(4);
  glVertexAttribPointer(
      4, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(GL::PrimitiveInstanceGpu, model_col1)));
  glVertexAttribDivisor(4, 1);

  glEnableVertexAttribArray(5);
  glVertexAttribPointer(
      5, Vec4, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(GL::PrimitiveInstanceGpu, model_col2)));
  glVertexAttribDivisor(5, 1);

  glEnableVertexAttribArray(6);
  glVertexAttribPointer(6, Vec4, GL_FLOAT, GL_FALSE, stride,
                        reinterpret_cast<void *>(
                            offsetof(GL::PrimitiveInstanceGpu, color_alpha)));
  glVertexAttribDivisor(6, 1);

  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::initializeSphereVao() {
  Mesh *unit = getUnitSphere();
  if (unit == nullptr) {
    return;
  }

  const auto &vertices = unit->getVertices();
  const auto &indices = unit->getIndices();
  if (vertices.empty() || indices.empty()) {
    return;
  }

  glGenVertexArrays(1, &m_sphereVao);
  glBindVertexArray(m_sphereVao);

  glGenBuffers(1, &m_sphereVertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_sphereVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
               vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &m_sphereIndexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_sphereIndexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);
  m_sphereIndexCount = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(Position, Vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, position)));
  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(Normal, Vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, normal)));
  glEnableVertexAttribArray(TexCoord);
  glVertexAttribPointer(TexCoord, Vec2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, tex_coord)));

  glGenBuffers(1, &m_sphereInstanceBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_sphereInstanceBuffer);
  m_sphereInstanceCapacity = kDefaultInstanceCapacity;
  glBufferData(GL_ARRAY_BUFFER,
               m_sphereInstanceCapacity * sizeof(GL::PrimitiveInstanceGpu),
               nullptr, GL_DYNAMIC_DRAW);

  setupInstanceAttributes(m_sphereVao, m_sphereInstanceBuffer);
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::initializeCylinderVao() {
  Mesh *unit = getUnitCylinder();
  if (unit == nullptr) {
    return;
  }

  const auto &vertices = unit->getVertices();
  const auto &indices = unit->getIndices();
  if (vertices.empty() || indices.empty()) {
    return;
  }

  glGenVertexArrays(1, &m_cylinderVao);
  glBindVertexArray(m_cylinderVao);

  glGenBuffers(1, &m_cylinderVertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_cylinderVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
               vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &m_cylinderIndexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cylinderIndexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);
  m_cylinderIndexCount = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(Position, Vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, position)));
  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(Normal, Vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, normal)));
  glEnableVertexAttribArray(TexCoord);
  glVertexAttribPointer(TexCoord, Vec2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, tex_coord)));

  glGenBuffers(1, &m_cylinderInstanceBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_cylinderInstanceBuffer);
  m_cylinderInstanceCapacity = kDefaultInstanceCapacity;
  glBufferData(GL_ARRAY_BUFFER,
               m_cylinderInstanceCapacity * sizeof(GL::PrimitiveInstanceGpu),
               nullptr, GL_DYNAMIC_DRAW);

  setupInstanceAttributes(m_cylinderVao, m_cylinderInstanceBuffer);
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::initializeConeVao() {
  Mesh *unit = getUnitCone();
  if (unit == nullptr) {
    return;
  }

  const auto &vertices = unit->getVertices();
  const auto &indices = unit->getIndices();
  if (vertices.empty() || indices.empty()) {
    return;
  }

  glGenVertexArrays(1, &m_coneVao);
  glBindVertexArray(m_coneVao);

  glGenBuffers(1, &m_coneVertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_coneVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
               vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &m_coneIndexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_coneIndexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);
  m_coneIndexCount = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(Position, Vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, position)));
  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(Normal, Vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, normal)));
  glEnableVertexAttribArray(TexCoord);
  glVertexAttribPointer(TexCoord, Vec2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, tex_coord)));

  glGenBuffers(1, &m_coneInstanceBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_coneInstanceBuffer);
  m_coneInstanceCapacity = kDefaultInstanceCapacity;
  glBufferData(GL_ARRAY_BUFFER,
               m_coneInstanceCapacity * sizeof(GL::PrimitiveInstanceGpu),
               nullptr, GL_DYNAMIC_DRAW);

  setupInstanceAttributes(m_coneVao, m_coneInstanceBuffer);
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::shutdownVaos() {
  if (m_sphereVao != 0) {
    glDeleteVertexArrays(1, &m_sphereVao);
    m_sphereVao = 0;
  }
  if (m_sphereVertexBuffer != 0) {
    glDeleteBuffers(1, &m_sphereVertexBuffer);
    m_sphereVertexBuffer = 0;
  }
  if (m_sphereIndexBuffer != 0) {
    glDeleteBuffers(1, &m_sphereIndexBuffer);
    m_sphereIndexBuffer = 0;
  }
  if (m_sphereInstanceBuffer != 0) {
    glDeleteBuffers(1, &m_sphereInstanceBuffer);
    m_sphereInstanceBuffer = 0;
  }

  if (m_cylinderVao != 0) {
    glDeleteVertexArrays(1, &m_cylinderVao);
    m_cylinderVao = 0;
  }
  if (m_cylinderVertexBuffer != 0) {
    glDeleteBuffers(1, &m_cylinderVertexBuffer);
    m_cylinderVertexBuffer = 0;
  }
  if (m_cylinderIndexBuffer != 0) {
    glDeleteBuffers(1, &m_cylinderIndexBuffer);
    m_cylinderIndexBuffer = 0;
  }
  if (m_cylinderInstanceBuffer != 0) {
    glDeleteBuffers(1, &m_cylinderInstanceBuffer);
    m_cylinderInstanceBuffer = 0;
  }

  if (m_coneVao != 0) {
    glDeleteVertexArrays(1, &m_coneVao);
    m_coneVao = 0;
  }
  if (m_coneVertexBuffer != 0) {
    glDeleteBuffers(1, &m_coneVertexBuffer);
    m_coneVertexBuffer = 0;
  }
  if (m_coneIndexBuffer != 0) {
    glDeleteBuffers(1, &m_coneIndexBuffer);
    m_coneIndexBuffer = 0;
  }
  if (m_coneInstanceBuffer != 0) {
    glDeleteBuffers(1, &m_coneInstanceBuffer);
    m_coneInstanceBuffer = 0;
  }
}

void PrimitiveBatchPipeline::uploadSphereInstances(
    const GL::PrimitiveInstanceGpu *data, std::size_t count) {
  if (count == 0 || data == nullptr) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_sphereInstanceBuffer);

  if (count > m_sphereInstanceCapacity) {
    m_sphereInstanceCapacity = static_cast<std::size_t>(count * kGrowthFactor);
    glBufferData(GL_ARRAY_BUFFER,
                 m_sphereInstanceCapacity * sizeof(GL::PrimitiveInstanceGpu),
                 nullptr, GL_DYNAMIC_DRAW);
  }

  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(GL::PrimitiveInstanceGpu),
                  data);
}

void PrimitiveBatchPipeline::uploadCylinderInstances(
    const GL::PrimitiveInstanceGpu *data, std::size_t count) {
  if (count == 0 || data == nullptr) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_cylinderInstanceBuffer);

  if (count > m_cylinderInstanceCapacity) {
    m_cylinderInstanceCapacity =
        static_cast<std::size_t>(count * kGrowthFactor);
    glBufferData(GL_ARRAY_BUFFER,
                 m_cylinderInstanceCapacity * sizeof(GL::PrimitiveInstanceGpu),
                 nullptr, GL_DYNAMIC_DRAW);
  }

  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(GL::PrimitiveInstanceGpu),
                  data);
}

void PrimitiveBatchPipeline::uploadConeInstances(
    const GL::PrimitiveInstanceGpu *data, std::size_t count) {
  if (count == 0 || data == nullptr) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_coneInstanceBuffer);

  if (count > m_coneInstanceCapacity) {
    m_coneInstanceCapacity = static_cast<std::size_t>(count * kGrowthFactor);
    glBufferData(GL_ARRAY_BUFFER,
                 m_coneInstanceCapacity * sizeof(GL::PrimitiveInstanceGpu),
                 nullptr, GL_DYNAMIC_DRAW);
  }

  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(GL::PrimitiveInstanceGpu),
                  data);
}

void PrimitiveBatchPipeline::drawSpheres(std::size_t count,
                                         const QMatrix4x4 &viewProj) {
  if (count == 0 || m_sphereVao == 0 || m_shader == nullptr) {
    return;
  }

  m_shader->use();
  m_shader->setUniform(m_uniforms.view_proj, viewProj);
  m_shader->setUniform(m_uniforms.light_dir, QVector3D(0.35F, 0.8F, 0.45F));
  m_shader->setUniform(m_uniforms.ambient_strength, 0.3F);

  glBindVertexArray(m_sphereVao);
  glDrawElementsInstanced(GL_TRIANGLES, m_sphereIndexCount, GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::drawCylinders(std::size_t count,
                                           const QMatrix4x4 &viewProj) {
  if (count == 0 || m_cylinderVao == 0 || m_shader == nullptr) {
    return;
  }

  m_shader->use();
  m_shader->setUniform(m_uniforms.view_proj, viewProj);
  m_shader->setUniform(m_uniforms.light_dir, QVector3D(0.35F, 0.8F, 0.45F));
  m_shader->setUniform(m_uniforms.ambient_strength, 0.3F);

  glBindVertexArray(m_cylinderVao);
  glDrawElementsInstanced(GL_TRIANGLES, m_cylinderIndexCount, GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::drawCones(std::size_t count,
                                       const QMatrix4x4 &viewProj) {
  if (count == 0 || m_coneVao == 0 || m_shader == nullptr) {
    return;
  }

  m_shader->use();
  m_shader->setUniform(m_uniforms.view_proj, viewProj);
  m_shader->setUniform(m_uniforms.light_dir, QVector3D(0.35F, 0.8F, 0.45F));
  m_shader->setUniform(m_uniforms.ambient_strength, 0.3F);

  glBindVertexArray(m_coneVao);
  glDrawElementsInstanced(GL_TRIANGLES, m_coneIndexCount, GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

} // namespace Render::GL::BackendPipelines
