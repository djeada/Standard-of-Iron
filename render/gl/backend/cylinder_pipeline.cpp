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

CylinderPipeline::CylinderPipeline(ShaderCache *shader_cache)
    : m_shader_cache(shader_cache) {}

CylinderPipeline::~CylinderPipeline() { shutdown(); }

auto CylinderPipeline::initialize() -> bool {
  initializeOpenGLFunctions();

  if (m_shader_cache == nullptr) {
    return false;
  }

  m_cylinder_shader = m_shader_cache->get(QStringLiteral("cylinder_instanced"));
  m_fog_shader = m_shader_cache->get(QStringLiteral("fog_instanced"));

  if ((m_cylinder_shader == nullptr) || (m_fog_shader == nullptr)) {
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
  if (m_cylinder_shader != nullptr) {
    m_cylinder_uniforms.view_proj =
        m_cylinder_shader->uniform_handle("u_viewProj");
  }

  if (m_fog_shader != nullptr) {
    m_fog_uniforms.view_proj = m_fog_shader->uniform_handle("u_viewProj");
    m_fog_uniforms.time = m_fog_shader->optional_uniform_handle("u_time");
  }
}

void CylinderPipeline::begin_frame() {
  if (m_cylinder_persistent_buffer.is_valid()) {
    m_cylinder_persistent_buffer.begin_frame();
  }

  if (m_fog_persistent_buffer.is_valid()) {
    m_fog_persistent_buffer.begin_frame();
  }
}

void CylinderPipeline::initialize_cylinder_pipeline() {
  initializeOpenGLFunctions();
  shutdown_cylinder_pipeline();

  Mesh *unit = get_unit_cylinder();
  if (unit == nullptr) {
    return;
  }

  const auto &vertices = unit->get_vertices();
  const auto &indices = unit->get_indices();
  if (vertices.empty() || indices.empty()) {
    return;
  }

  glGenVertexArrays(1, &m_cylinder_vao);
  glBindVertexArray(m_cylinder_vao);

  glGenBuffers(1, &m_cylinder_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_cylinder_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
               vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &m_cylinder_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cylinder_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);
  m_cylinder_index_count = static_cast<GLsizei>(indices.size());

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
  if (m_cylinder_persistent_buffer.initialize(k_cylinder_persistent_capacity,
                                            BufferCapacity::BuffersInFlight)) {
    m_use_persistent_buffers = true;
    glBindBuffer(GL_ARRAY_BUFFER, m_cylinder_persistent_buffer.buffer());
  } else {
    m_use_persistent_buffers = false;
    glGenBuffers(1, &m_cylinder_instance_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_cylinder_instance_buffer);
    m_cylinder_instance_capacity = BufferCapacity::DefaultCylinderInstances;
    glBufferData(GL_ARRAY_BUFFER,
                 m_cylinder_instance_capacity * sizeof(CylinderInstanceGpu),
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

  m_cylinder_scratch.reserve(m_use_persistent_buffers
                                ? k_cylinder_persistent_capacity
                                : m_cylinder_instance_capacity);
}

void CylinderPipeline::shutdown_cylinder_pipeline() {

  if (QOpenGLContext::currentContext() == nullptr) {

    m_cylinder_vao = 0;
    m_cylinder_vertex_buffer = 0;
    m_cylinder_index_buffer = 0;
    m_cylinder_instance_buffer = 0;
    m_cylinder_index_count = 0;
    m_cylinder_instance_capacity = 0;
    m_cylinder_scratch.clear();
    return;
  }

  initializeOpenGLFunctions();

  m_cylinder_persistent_buffer.destroy();

  if (m_cylinder_instance_buffer != 0U) {
    glDeleteBuffers(1, &m_cylinder_instance_buffer);
    m_cylinder_instance_buffer = 0;
  }
  if (m_cylinder_vertex_buffer != 0U) {
    glDeleteBuffers(1, &m_cylinder_vertex_buffer);
    m_cylinder_vertex_buffer = 0;
  }
  if (m_cylinder_index_buffer != 0U) {
    glDeleteBuffers(1, &m_cylinder_index_buffer);
    m_cylinder_index_buffer = 0;
  }
  if (m_cylinder_vao != 0U) {
    glDeleteVertexArrays(1, &m_cylinder_vao);
    m_cylinder_vao = 0;
  }
  m_cylinder_index_count = 0;
  m_cylinder_instance_capacity = 0;
  m_cylinder_scratch.clear();
}

void CylinderPipeline::upload_cylinder_instances(std::size_t count) {
  if (count == 0) {
    return;
  }

  initializeOpenGLFunctions();

  if (m_use_persistent_buffers && m_cylinder_persistent_buffer.is_valid()) {
    if (count > m_cylinder_persistent_buffer.capacity()) {
      count = m_cylinder_persistent_buffer.capacity();
    }

    m_cylinder_persistent_buffer.write(m_cylinder_scratch.data(), count);
    glBindBuffer(GL_ARRAY_BUFFER, m_cylinder_persistent_buffer.buffer());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return;
  }

  if (m_cylinder_instance_buffer == 0U) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_cylinder_instance_buffer);
  if (count > m_cylinder_instance_capacity) {
    m_cylinder_instance_capacity = std::max<std::size_t>(
        count, (m_cylinder_instance_capacity != 0U)
                   ? m_cylinder_instance_capacity * Growth::CapacityMultiplier
                   : count);
    glBufferData(GL_ARRAY_BUFFER,
                 m_cylinder_instance_capacity * sizeof(CylinderInstanceGpu),
                 nullptr, GL_DYNAMIC_DRAW);
    m_cylinder_scratch.reserve(m_cylinder_instance_capacity);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(CylinderInstanceGpu),
                  m_cylinder_scratch.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void CylinderPipeline::draw_cylinders(std::size_t count) {
  if ((m_cylinder_vao == 0U) || m_cylinder_index_count == 0 || count == 0) {
    return;
  }

  initializeOpenGLFunctions();
  glBindVertexArray(m_cylinder_vao);
  glDrawElementsInstanced(GL_TRIANGLES, m_cylinder_index_count, GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

void CylinderPipeline::initialize_fog_pipeline() {
  initializeOpenGLFunctions();
  shutdown_fog_pipeline();

  constexpr int k_fog_grid_segments = 4;
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(static_cast<std::size_t>((k_fog_grid_segments + 1) *
                                            (k_fog_grid_segments + 1)));
  indices.reserve(
      static_cast<std::size_t>(k_fog_grid_segments * k_fog_grid_segments * 6));

  for (int z = 0; z <= k_fog_grid_segments; ++z) {
    const float v = static_cast<float>(z) / k_fog_grid_segments;
    for (int x = 0; x <= k_fog_grid_segments; ++x) {
      const float u = static_cast<float>(x) / k_fog_grid_segments;
      vertices.push_back(
          {{u - 0.5F, 0.0F, v - 0.5F}, {0.0F, 1.0F, 0.0F}, {u, v}});
    }
  }

  for (int z = 0; z < k_fog_grid_segments; ++z) {
    for (int x = 0; x < k_fog_grid_segments; ++x) {
      const auto i0 =
          static_cast<unsigned int>(z * (k_fog_grid_segments + 1) + x);
      const auto i1 = i0 + 1U;
      const auto i2 =
          static_cast<unsigned int>((z + 1) * (k_fog_grid_segments + 1) + x);
      const auto i3 = i2 + 1U;
      indices.insert(indices.end(), {i0, i1, i2, i2, i1, i3});
    }
  }

  glGenVertexArrays(1, &m_fog_vao);
  glBindVertexArray(m_fog_vao);

  glGenBuffers(1, &m_fog_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_fog_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
               vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &m_fog_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_fog_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);
  m_fog_index_count = static_cast<GLsizei>(indices.size());

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

  glGenBuffers(1, &m_fog_instance_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_fog_instance_buffer);
  m_fog_instance_capacity = BufferCapacity::DefaultFogInstances;
  glBufferData(GL_ARRAY_BUFFER, m_fog_instance_capacity * sizeof(FogInstanceGpu),
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

  m_fog_scratch.reserve(m_fog_instance_capacity);
}

void CylinderPipeline::shutdown_fog_pipeline() {

  if (QOpenGLContext::currentContext() == nullptr) {

    m_fog_vao = 0;
    m_fog_vertex_buffer = 0;
    m_fog_index_buffer = 0;
    m_fog_instance_buffer = 0;
    m_fog_index_count = 0;
    m_fog_instance_capacity = 0;
    m_fog_scratch.clear();
    return;
  }

  initializeOpenGLFunctions();

  if (m_fog_instance_buffer != 0U) {
    glDeleteBuffers(1, &m_fog_instance_buffer);
    m_fog_instance_buffer = 0;
  }
  if (m_fog_vertex_buffer != 0U) {
    glDeleteBuffers(1, &m_fog_vertex_buffer);
    m_fog_vertex_buffer = 0;
  }
  if (m_fog_index_buffer != 0U) {
    glDeleteBuffers(1, &m_fog_index_buffer);
    m_fog_index_buffer = 0;
  }
  if (m_fog_vao != 0U) {
    glDeleteVertexArrays(1, &m_fog_vao);
    m_fog_vao = 0;
  }
  m_fog_index_count = 0;
  m_fog_instance_capacity = 0;
  m_fog_scratch.clear();
}

void CylinderPipeline::upload_fog_instances(std::size_t count) {
  if ((m_fog_instance_buffer == 0U) || count == 0) {
    return;
  }

  initializeOpenGLFunctions();
  glBindBuffer(GL_ARRAY_BUFFER, m_fog_instance_buffer);
  if (count > m_fog_instance_capacity) {
    m_fog_instance_capacity = std::max<std::size_t>(
        count, (m_fog_instance_capacity != 0U)
                   ? m_fog_instance_capacity * Growth::CapacityMultiplier
                   : count);
    glBufferData(GL_ARRAY_BUFFER,
                 m_fog_instance_capacity * sizeof(FogInstanceGpu), nullptr,
                 GL_DYNAMIC_DRAW);
    m_fog_scratch.reserve(m_fog_instance_capacity);
  }
  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(FogInstanceGpu),
                  m_fog_scratch.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void CylinderPipeline::bind_fog_instance_buffer(GL::Buffer *instance_buffer) {
  if (instance_buffer == nullptr || m_fog_vao == 0U) {
    return;
  }

  initializeOpenGLFunctions();
  glBindVertexArray(m_fog_vao);
  instance_buffer->bind();

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
  instance_buffer->unbind();
}

void CylinderPipeline::draw_fog(std::size_t count) {
  if ((m_fog_vao == 0U) || m_fog_index_count == 0 || count == 0) {
    return;
  }

  initializeOpenGLFunctions();
  glBindVertexArray(m_fog_vao);
  glDrawElementsInstanced(GL_TRIANGLES, m_fog_index_count, GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

} // namespace Render::GL::BackendPipelines
