#include "cylinder_pipeline.h"
#include "../backend.h"
#include "../mesh.h"
#include "../primitives.h"
#include "../render_constants.h"
#include "gl/shader_cache.h"
#include <GL/gl.h>
#include <QOpenGLContext>
#include <algorithm>
#include <cstddef>
#include <qopenglcontext.h>
#include <qopenglext.h>
#include <qstringliteral.h>

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;
using namespace Render::GL::BufferCapacity;
using namespace Render::GL::Geometry;
using namespace Render::GL::Growth;

CylinderPipeline::CylinderPipeline(ShaderCache *shaderCache)
    : m_shaderCache(shaderCache) {}

CylinderPipeline::~CylinderPipeline() { shutdown(); }

auto CylinderPipeline::initialize() -> bool {
  initializeOpenGLFunctions();

  if (m_shaderCache == nullptr) {
    return false;
  }

  m_cylinderShader = m_shaderCache->get(QStringLiteral("cylinder_instanced"));
  m_fogShader = m_shaderCache->get(QStringLiteral("fog_instanced"));

  if ((m_cylinderShader == nullptr) || (m_fogShader == nullptr)) {
    return false;
  }

  initialize_cylinder_pipeline();
  initialize_fog_pipeline();
  cache_uniforms();

  m_initialized = true;
  return true;
}

void CylinderPipeline::shutdown() {
  shutdown_cylinder_pipeline();
  shutdown_fog_pipeline();
  m_initialized = false;
}

void CylinderPipeline::cache_uniforms() {
  if (m_cylinderShader != nullptr) {
    m_cylinderUniforms.view_proj =
        m_cylinderShader->uniform_handle("u_viewProj");
  }

  if (m_fogShader != nullptr) {
    m_fogUniforms.view_proj = m_fogShader->uniform_handle("u_viewProj");
  }
}

void CylinderPipeline::begin_frame() {
  if (m_cylinderPersistentBuffer.is_valid()) {
    m_cylinderPersistentBuffer.begin_frame();
  }

  if (m_fogPersistentBuffer.is_valid()) {
    m_fogPersistentBuffer.begin_frame();
  }
}

void CylinderPipeline::initialize_cylinder_pipeline() {
  initializeOpenGLFunctions();
  shutdown_cylinder_pipeline();

  Mesh *unit = get_unit_cylinder();
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

  glEnableVertexAttribArray(VertexAttrib::Position);
  glVertexAttribPointer(VertexAttrib::Position, ComponentCount::Vec3, GL_FLOAT,
                        GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, position)));
  glEnableVertexAttribArray(VertexAttrib::Normal);
  glVertexAttribPointer(VertexAttrib::Normal, ComponentCount::Vec3, GL_FLOAT,
                        GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, normal)));
  glEnableVertexAttribArray(VertexAttrib::TexCoord);
  glVertexAttribPointer(VertexAttrib::TexCoord, ComponentCount::Vec2, GL_FLOAT,
                        GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, tex_coord)));

  constexpr std::size_t k_cylinder_persistent_capacity = 10000;
  if (m_cylinderPersistentBuffer.initialize(k_cylinder_persistent_capacity,
                                            BufferCapacity::BuffersInFlight)) {
    m_usePersistentBuffers = true;
    glBindBuffer(GL_ARRAY_BUFFER, m_cylinderPersistentBuffer.buffer());
  } else {
    m_usePersistentBuffers = false;
    glGenBuffers(1, &m_cylinderInstanceBuffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_cylinderInstanceBuffer);
    m_cylinderInstanceCapacity = BufferCapacity::DefaultCylinderInstances;
    glBufferData(GL_ARRAY_BUFFER,
                 m_cylinderInstanceCapacity * sizeof(CylinderInstanceGpu),
                 nullptr, GL_DYNAMIC_DRAW);
  }

  const auto stride = static_cast<GLsizei>(sizeof(CylinderInstanceGpu));
  glEnableVertexAttribArray(VertexAttrib::InstancePosition);
  glVertexAttribPointer(
      VertexAttrib::InstancePosition, ComponentCount::Vec3, GL_FLOAT, GL_FALSE,
      stride, reinterpret_cast<void *>(offsetof(CylinderInstanceGpu, start)));
  glVertexAttribDivisor(VertexAttrib::InstancePosition, 1);

  glEnableVertexAttribArray(VertexAttrib::InstanceScale);
  glVertexAttribPointer(
      VertexAttrib::InstanceScale, ComponentCount::Vec3, GL_FLOAT, GL_FALSE,
      stride, reinterpret_cast<void *>(offsetof(CylinderInstanceGpu, end)));
  glVertexAttribDivisor(VertexAttrib::InstanceScale, 1);

  glEnableVertexAttribArray(VertexAttrib::InstanceColor);
  glVertexAttribPointer(
      VertexAttrib::InstanceColor, 1, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(CylinderInstanceGpu, radius)));
  glVertexAttribDivisor(VertexAttrib::InstanceColor, 1);

  glEnableVertexAttribArray(VertexAttrib::InstanceAlpha);
  glVertexAttribPointer(
      VertexAttrib::InstanceAlpha, 1, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(CylinderInstanceGpu, alpha)));
  glVertexAttribDivisor(VertexAttrib::InstanceAlpha, 1);

  glEnableVertexAttribArray(VertexAttrib::InstanceTint);
  glVertexAttribPointer(
      VertexAttrib::InstanceTint, ComponentCount::Vec3, GL_FLOAT, GL_FALSE,
      stride, reinterpret_cast<void *>(offsetof(CylinderInstanceGpu, color)));
  glVertexAttribDivisor(VertexAttrib::InstanceTint, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  m_cylinderScratch.reserve(m_usePersistentBuffers
                                ? k_cylinder_persistent_capacity
                                : m_cylinderInstanceCapacity);
}

void CylinderPipeline::shutdown_cylinder_pipeline() {

  if (QOpenGLContext::currentContext() == nullptr) {

    m_cylinderVao = 0;
    m_cylinderVertexBuffer = 0;
    m_cylinderIndexBuffer = 0;
    m_cylinderInstanceBuffer = 0;
    m_cylinderIndexCount = 0;
    m_cylinderInstanceCapacity = 0;
    m_cylinderScratch.clear();
    return;
  }

  initializeOpenGLFunctions();

  m_cylinderPersistentBuffer.destroy();

  if (m_cylinderInstanceBuffer != 0U) {
    glDeleteBuffers(1, &m_cylinderInstanceBuffer);
    m_cylinderInstanceBuffer = 0;
  }
  if (m_cylinderVertexBuffer != 0U) {
    glDeleteBuffers(1, &m_cylinderVertexBuffer);
    m_cylinderVertexBuffer = 0;
  }
  if (m_cylinderIndexBuffer != 0U) {
    glDeleteBuffers(1, &m_cylinderIndexBuffer);
    m_cylinderIndexBuffer = 0;
  }
  if (m_cylinderVao != 0U) {
    glDeleteVertexArrays(1, &m_cylinderVao);
    m_cylinderVao = 0;
  }
  m_cylinderIndexCount = 0;
  m_cylinderInstanceCapacity = 0;
  m_cylinderScratch.clear();
}

void CylinderPipeline::upload_cylinder_instances(std::size_t count) {
  if (count == 0) {
    return;
  }

  initializeOpenGLFunctions();

  if (m_usePersistentBuffers && m_cylinderPersistentBuffer.is_valid()) {
    if (count > m_cylinderPersistentBuffer.capacity()) {
      count = m_cylinderPersistentBuffer.capacity();
    }

    m_cylinderPersistentBuffer.write(m_cylinderScratch.data(), count);
    glBindBuffer(GL_ARRAY_BUFFER, m_cylinderPersistentBuffer.buffer());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return;
  }

  if (m_cylinderInstanceBuffer == 0U) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_cylinderInstanceBuffer);
  if (count > m_cylinderInstanceCapacity) {
    m_cylinderInstanceCapacity = std::max<std::size_t>(
        count, (m_cylinderInstanceCapacity != 0U)
                   ? m_cylinderInstanceCapacity * Growth::CapacityMultiplier
                   : count);
    glBufferData(GL_ARRAY_BUFFER,
                 m_cylinderInstanceCapacity * sizeof(CylinderInstanceGpu),
                 nullptr, GL_DYNAMIC_DRAW);
    m_cylinderScratch.reserve(m_cylinderInstanceCapacity);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(CylinderInstanceGpu),
                  m_cylinderScratch.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void CylinderPipeline::draw_cylinders(std::size_t count) {
  if ((m_cylinderVao == 0U) || m_cylinderIndexCount == 0 || count == 0) {
    return;
  }

  initializeOpenGLFunctions();
  glBindVertexArray(m_cylinderVao);
  glDrawElementsInstanced(GL_TRIANGLES, m_cylinderIndexCount, GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

void CylinderPipeline::initialize_fog_pipeline() {
  initializeOpenGLFunctions();
  shutdown_fog_pipeline();

  const Vertex vertices[Geometry::QuadVertexCount] = {
      {{-0.5F, 0.0F, -0.5F}, {0.0F, 1.0F, 0.0F}, {0.0F, 0.0F}},
      {{0.5F, 0.0F, -0.5F}, {0.0F, 1.0F, 0.0F}, {1.0F, 0.0F}},
      {{-0.5F, 0.0F, 0.5F}, {0.0F, 1.0F, 0.0F}, {0.0F, 1.0F}},
      {{0.5F, 0.0F, 0.5F}, {0.0F, 1.0F, 0.0F}, {1.0F, 1.0F}},
  };

  const unsigned int indices[Geometry::QuadIndexCount] = {0, 1, 2, 2, 1, 3};

  glGenVertexArrays(1, &m_fogVao);
  glBindVertexArray(m_fogVao);

  glGenBuffers(1, &m_fogVertexBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_fogVertexBuffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glGenBuffers(1, &m_fogIndexBuffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_fogIndexBuffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);
  m_fogIndexCount = Geometry::QuadIndexCount;

  glEnableVertexAttribArray(VertexAttrib::Position);
  glVertexAttribPointer(VertexAttrib::Position, ComponentCount::Vec3, GL_FLOAT,
                        GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, position)));
  glEnableVertexAttribArray(VertexAttrib::Normal);
  glVertexAttribPointer(VertexAttrib::Normal, ComponentCount::Vec3, GL_FLOAT,
                        GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, normal)));
  glEnableVertexAttribArray(VertexAttrib::TexCoord);
  glVertexAttribPointer(VertexAttrib::TexCoord, ComponentCount::Vec2, GL_FLOAT,
                        GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, tex_coord)));

  glGenBuffers(1, &m_fogInstanceBuffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_fogInstanceBuffer);
  m_fogInstanceCapacity = BufferCapacity::DefaultFogInstances;
  glBufferData(GL_ARRAY_BUFFER, m_fogInstanceCapacity * sizeof(FogInstanceGpu),
               nullptr, GL_DYNAMIC_DRAW);

  const auto stride = static_cast<GLsizei>(sizeof(FogInstanceGpu));
  glEnableVertexAttribArray(VertexAttrib::InstancePosition);
  glVertexAttribPointer(
      VertexAttrib::InstancePosition, ComponentCount::Vec3, GL_FLOAT, GL_FALSE,
      stride, reinterpret_cast<void *>(offsetof(FogInstanceGpu, center)));
  glVertexAttribDivisor(VertexAttrib::InstancePosition, 1);

  glEnableVertexAttribArray(VertexAttrib::InstanceScale);
  glVertexAttribPointer(
      VertexAttrib::InstanceScale, 1, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(FogInstanceGpu, size)));
  glVertexAttribDivisor(VertexAttrib::InstanceScale, 1);

  glEnableVertexAttribArray(VertexAttrib::InstanceColor);
  glVertexAttribPointer(
      VertexAttrib::InstanceColor, ComponentCount::Vec3, GL_FLOAT, GL_FALSE,
      stride, reinterpret_cast<void *>(offsetof(FogInstanceGpu, color)));
  glVertexAttribDivisor(VertexAttrib::InstanceColor, 1);

  glEnableVertexAttribArray(VertexAttrib::InstanceAlpha);
  glVertexAttribPointer(
      VertexAttrib::InstanceAlpha, 1, GL_FLOAT, GL_FALSE, stride,
      reinterpret_cast<void *>(offsetof(FogInstanceGpu, alpha)));
  glVertexAttribDivisor(VertexAttrib::InstanceAlpha, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  m_fogScratch.reserve(m_fogInstanceCapacity);
}

void CylinderPipeline::shutdown_fog_pipeline() {

  if (QOpenGLContext::currentContext() == nullptr) {

    m_fogVao = 0;
    m_fogVertexBuffer = 0;
    m_fogIndexBuffer = 0;
    m_fogInstanceBuffer = 0;
    m_fogIndexCount = 0;
    m_fogInstanceCapacity = 0;
    m_fogScratch.clear();
    return;
  }

  initializeOpenGLFunctions();

  if (m_fogInstanceBuffer != 0U) {
    glDeleteBuffers(1, &m_fogInstanceBuffer);
    m_fogInstanceBuffer = 0;
  }
  if (m_fogVertexBuffer != 0U) {
    glDeleteBuffers(1, &m_fogVertexBuffer);
    m_fogVertexBuffer = 0;
  }
  if (m_fogIndexBuffer != 0U) {
    glDeleteBuffers(1, &m_fogIndexBuffer);
    m_fogIndexBuffer = 0;
  }
  if (m_fogVao != 0U) {
    glDeleteVertexArrays(1, &m_fogVao);
    m_fogVao = 0;
  }
  m_fogIndexCount = 0;
  m_fogInstanceCapacity = 0;
  m_fogScratch.clear();
}

void CylinderPipeline::upload_fog_instances(std::size_t count) {
  if ((m_fogInstanceBuffer == 0U) || count == 0) {
    return;
  }

  initializeOpenGLFunctions();
  glBindBuffer(GL_ARRAY_BUFFER, m_fogInstanceBuffer);
  if (count > m_fogInstanceCapacity) {
    m_fogInstanceCapacity = std::max<std::size_t>(
        count, (m_fogInstanceCapacity != 0U)
                   ? m_fogInstanceCapacity * Growth::CapacityMultiplier
                   : count);
    glBufferData(GL_ARRAY_BUFFER,
                 m_fogInstanceCapacity * sizeof(FogInstanceGpu), nullptr,
                 GL_DYNAMIC_DRAW);
    m_fogScratch.reserve(m_fogInstanceCapacity);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(FogInstanceGpu),
                  m_fogScratch.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void CylinderPipeline::draw_fog(std::size_t count) {
  if ((m_fogVao == 0U) || m_fogIndexCount == 0 || count == 0) {
    return;
  }

  initializeOpenGLFunctions();
  glBindVertexArray(m_fogVao);
  glDrawElementsInstanced(GL_TRIANGLES, m_fogIndexCount, GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

} 
