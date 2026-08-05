#include "cylinder_pipeline.h"

#include <QOpenGLContext>
#include <qopenglcontext.h>
#include <qopenglext.h>
#include <qstringliteral.h>

#include <GL/gl.h>
#include <algorithm>
#include <cstddef>

#include "../backend.h"
#include "../mesh.h"
#include "../primitives.h"
#include "../render_constants.h"
#include "gl/shader_cache.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;
using namespace Render::GL::BufferCapacity;
using namespace Render::GL::Geometry;
using namespace Render::GL::Growth;

CylinderPipeline::CylinderPipeline(ShaderCache* shader_cache)
    : m_shader_cache(shader_cache) {
}

CylinderPipeline::~CylinderPipeline() {
  shutdown();
}

namespace {

void apply_mesh_vertex_layout(QOpenGLFunctions_3_3_Core& gl) {
  gl.glEnableVertexAttribArray(VertexAttrib::position);
  gl.glVertexAttribPointer(VertexAttrib::position,
                           ComponentCount::vec3,
                           GL_FLOAT,
                           GL_FALSE,
                           sizeof(Vertex),
                           reinterpret_cast<void*>(offsetof(Vertex, position)));
  gl.glEnableVertexAttribArray(VertexAttrib::normal);
  gl.glVertexAttribPointer(VertexAttrib::normal,
                           ComponentCount::vec3,
                           GL_FLOAT,
                           GL_FALSE,
                           sizeof(Vertex),
                           reinterpret_cast<void*>(offsetof(Vertex, normal)));
  gl.glEnableVertexAttribArray(VertexAttrib::tex_coord);
  gl.glVertexAttribPointer(VertexAttrib::tex_coord,
                           ComponentCount::vec2,
                           GL_FLOAT,
                           GL_FALSE,
                           sizeof(Vertex),
                           reinterpret_cast<void*>(offsetof(Vertex, tex_coord)));
}

void apply_fog_instance_layout(QOpenGLFunctions_3_3_Core& gl) {
  const auto stride = static_cast<GLsizei>(sizeof(CylinderPipeline::FogInstanceGpu));
  gl.glEnableVertexAttribArray(VertexAttrib::instance_position);
  gl.glVertexAttribPointer(
      VertexAttrib::instance_position,
      ComponentCount::vec3,
      GL_FLOAT,
      GL_FALSE,
      stride,
      reinterpret_cast<void*>(offsetof(CylinderPipeline::FogInstanceGpu, center)));
  gl.glVertexAttribDivisor(VertexAttrib::instance_position, 1);

  gl.glEnableVertexAttribArray(VertexAttrib::instance_scale);
  gl.glVertexAttribPointer(
      VertexAttrib::instance_scale,
      1,
      GL_FLOAT,
      GL_FALSE,
      stride,
      reinterpret_cast<void*>(offsetof(CylinderPipeline::FogInstanceGpu, size)));
  gl.glVertexAttribDivisor(VertexAttrib::instance_scale, 1);

  gl.glEnableVertexAttribArray(VertexAttrib::instance_color);
  gl.glVertexAttribPointer(
      VertexAttrib::instance_color,
      ComponentCount::vec3,
      GL_FLOAT,
      GL_FALSE,
      stride,
      reinterpret_cast<void*>(offsetof(CylinderPipeline::FogInstanceGpu, color)));
  gl.glVertexAttribDivisor(VertexAttrib::instance_color, 1);

  gl.glEnableVertexAttribArray(VertexAttrib::instance_alpha);
  gl.glVertexAttribPointer(
      VertexAttrib::instance_alpha,
      1,
      GL_FLOAT,
      GL_FALSE,
      stride,
      reinterpret_cast<void*>(offsetof(CylinderPipeline::FogInstanceGpu, alpha)));
  gl.glVertexAttribDivisor(VertexAttrib::instance_alpha, 1);
}

} // namespace

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
        m_cylinder_shader->optional_uniform_handle("u_view_proj");
  }

  if (m_fog_shader != nullptr) {
    m_fog_uniforms.view_proj = m_fog_shader->optional_uniform_handle("u_view_proj");
    m_fog_uniforms.time = m_fog_shader->optional_uniform_handle("u_time");
    m_fog_uniforms.mask_tex = m_fog_shader->optional_uniform_handle("u_fog_mask_tex");
    m_fog_uniforms.mask_size = m_fog_shader->optional_uniform_handle("u_fog_mask_size");
    m_fog_uniforms.mask_tile_size =
        m_fog_shader->optional_uniform_handle("u_fog_mask_tile_size");
    m_fog_uniforms.has_mask = m_fog_shader->optional_uniform_handle("u_has_fog_mask");
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

  Mesh* unit = get_unit_cylinder();
  if (unit == nullptr) {
    return;
  }

  const auto& vertices = unit->get_vertices();
  const auto& indices = unit->get_indices();
  if (vertices.empty() || indices.empty()) {
    return;
  }

  glGenVertexArrays(1, &m_cylinder_mesh.vao);
  glBindVertexArray(m_cylinder_mesh.vao);

  glGenBuffers(1, &m_cylinder_mesh.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_cylinder_mesh.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               vertices.size() * sizeof(Vertex),
               vertices.data(),
               GL_STATIC_DRAW);

  glGenBuffers(1, &m_cylinder_mesh.index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cylinder_mesh.index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               indices.size() * sizeof(unsigned int),
               indices.data(),
               GL_STATIC_DRAW);
  m_cylinder_mesh.index_count = static_cast<GLsizei>(indices.size());

  apply_mesh_vertex_layout(*this);
  constexpr std::size_t k_cylinder_persistent_capacity = 10000;
  if (m_cylinder_persistent_buffer.initialize(k_cylinder_persistent_capacity,
                                              BufferCapacity::buffers_in_flight)) {
    m_use_persistent_buffers = true;
    glBindBuffer(GL_ARRAY_BUFFER, m_cylinder_persistent_buffer.buffer());
  } else {
    m_use_persistent_buffers = false;
    glGenBuffers(1, &m_cylinder_mesh.instance_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, m_cylinder_mesh.instance_buffer);
    m_cylinder_instance_capacity = BufferCapacity::default_cylinder_instances;
    glBufferData(GL_ARRAY_BUFFER,
                 m_cylinder_instance_capacity * sizeof(CylinderInstanceGpu),
                 nullptr,
                 GL_DYNAMIC_DRAW);
  }

  const auto stride = static_cast<GLsizei>(sizeof(CylinderInstanceGpu));
  glEnableVertexAttribArray(VertexAttrib::instance_position);
  glVertexAttribPointer(VertexAttrib::instance_position,
                        ComponentCount::vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        stride,
                        reinterpret_cast<void*>(offsetof(CylinderInstanceGpu, start)));
  glVertexAttribDivisor(VertexAttrib::instance_position, 1);

  glEnableVertexAttribArray(VertexAttrib::instance_scale);
  glVertexAttribPointer(VertexAttrib::instance_scale,
                        ComponentCount::vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        stride,
                        reinterpret_cast<void*>(offsetof(CylinderInstanceGpu, end)));
  glVertexAttribDivisor(VertexAttrib::instance_scale, 1);

  glEnableVertexAttribArray(VertexAttrib::instance_color);
  glVertexAttribPointer(VertexAttrib::instance_color,
                        1,
                        GL_FLOAT,
                        GL_FALSE,
                        stride,
                        reinterpret_cast<void*>(offsetof(CylinderInstanceGpu, radius)));
  glVertexAttribDivisor(VertexAttrib::instance_color, 1);

  glEnableVertexAttribArray(VertexAttrib::instance_alpha);
  glVertexAttribPointer(VertexAttrib::instance_alpha,
                        1,
                        GL_FLOAT,
                        GL_FALSE,
                        stride,
                        reinterpret_cast<void*>(offsetof(CylinderInstanceGpu, alpha)));
  glVertexAttribDivisor(VertexAttrib::instance_alpha, 1);

  glEnableVertexAttribArray(VertexAttrib::instance_tint);
  glVertexAttribPointer(VertexAttrib::instance_tint,
                        ComponentCount::vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        stride,
                        reinterpret_cast<void*>(offsetof(CylinderInstanceGpu, color)));
  glVertexAttribDivisor(VertexAttrib::instance_tint, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  m_cylinder_scratch.reserve(m_use_persistent_buffers ? k_cylinder_persistent_capacity
                                                      : m_cylinder_instance_capacity);
}

void CylinderPipeline::shutdown_cylinder_pipeline() {
  if (QOpenGLContext::currentContext() != nullptr) {
    initializeOpenGLFunctions();
    m_cylinder_persistent_buffer.destroy();
  }
  release_mesh_buffers(*this, m_cylinder_mesh);
  m_cylinder_instance_capacity = 0;
  m_cylinder_scratch.clear();
}

void CylinderPipeline::upload_cylinder_instances(std::size_t count) {
  m_cylinder_instances_resident = 0;
  count = std::min(count, m_cylinder_scratch.size());
  if (count == 0) {
    return;
  }

  initializeOpenGLFunctions();

  if (m_use_persistent_buffers && m_cylinder_persistent_buffer.is_valid()) {
    count = std::min(count, m_cylinder_persistent_buffer.capacity());

    m_cylinder_persistent_buffer.write(m_cylinder_scratch.data(), count);
    glBindBuffer(GL_ARRAY_BUFFER, m_cylinder_persistent_buffer.buffer());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    m_cylinder_instances_resident = count;
    return;
  }

  if (m_cylinder_mesh.instance_buffer == 0U) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_cylinder_mesh.instance_buffer);
  if (count > m_cylinder_instance_capacity) {
    m_cylinder_instance_capacity = std::max<std::size_t>(
        count,
        (m_cylinder_instance_capacity != 0U)
            ? m_cylinder_instance_capacity * Growth::capacity_multiplier
            : count);
    glBufferData(GL_ARRAY_BUFFER,
                 m_cylinder_instance_capacity * sizeof(CylinderInstanceGpu),
                 nullptr,
                 GL_DYNAMIC_DRAW);
    m_cylinder_scratch.reserve(m_cylinder_instance_capacity);
  }
  glBufferSubData(GL_ARRAY_BUFFER,
                  0,
                  count * sizeof(CylinderInstanceGpu),
                  m_cylinder_scratch.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  m_cylinder_instances_resident = count;
}

void CylinderPipeline::draw_cylinders(std::size_t count) {
  count = m_cylinder_draw_guard.clamp(count, m_cylinder_instances_resident);
  if ((m_cylinder_mesh.vao == 0U) || m_cylinder_mesh.index_count == 0 || count == 0) {
    return;
  }

  initializeOpenGLFunctions();
  glBindVertexArray(m_cylinder_mesh.vao);
  glDrawElementsInstanced(GL_TRIANGLES,
                          m_cylinder_mesh.index_count,
                          GL_UNSIGNED_INT,
                          nullptr,
                          static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

void CylinderPipeline::initialize_fog_pipeline() {
  initializeOpenGLFunctions();
  shutdown_fog_pipeline();

  constexpr int k_fog_grid_segments = 4;
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(
      static_cast<std::size_t>((k_fog_grid_segments + 1) * (k_fog_grid_segments + 1)));
  indices.reserve(
      static_cast<std::size_t>(k_fog_grid_segments * k_fog_grid_segments * 6));

  for (int z = 0; z <= k_fog_grid_segments; ++z) {
    const float v = static_cast<float>(z) / k_fog_grid_segments;
    for (int x = 0; x <= k_fog_grid_segments; ++x) {
      const float u = static_cast<float>(x) / k_fog_grid_segments;
      vertices.push_back({{u - 0.5F, 0.0F, v - 0.5F}, {0.0F, 1.0F, 0.0F}, {u, v}});
    }
  }

  for (int z = 0; z < k_fog_grid_segments; ++z) {
    for (int x = 0; x < k_fog_grid_segments; ++x) {
      const auto i0 = static_cast<unsigned int>(z * (k_fog_grid_segments + 1) + x);
      const auto i1 = i0 + 1U;
      const auto i2 =
          static_cast<unsigned int>((z + 1) * (k_fog_grid_segments + 1) + x);
      const auto i3 = i2 + 1U;
      indices.insert(indices.end(), {i0, i1, i2, i2, i1, i3});
    }
  }

  glGenVertexArrays(1, &m_fog_mesh.vao);
  glBindVertexArray(m_fog_mesh.vao);

  glGenBuffers(1, &m_fog_mesh.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_fog_mesh.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               vertices.size() * sizeof(Vertex),
               vertices.data(),
               GL_STATIC_DRAW);

  glGenBuffers(1, &m_fog_mesh.index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_fog_mesh.index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               indices.size() * sizeof(unsigned int),
               indices.data(),
               GL_STATIC_DRAW);
  m_fog_mesh.index_count = static_cast<GLsizei>(indices.size());

  apply_mesh_vertex_layout(*this);
  glGenBuffers(1, &m_fog_mesh.instance_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_fog_mesh.instance_buffer);
  m_fog_instance_capacity = BufferCapacity::default_fog_instances;
  glBufferData(GL_ARRAY_BUFFER,
               m_fog_instance_capacity * sizeof(FogInstanceGpu),
               nullptr,
               GL_DYNAMIC_DRAW);

  apply_fog_instance_layout(*this);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  m_fog_scratch.reserve(m_fog_instance_capacity);
}

void CylinderPipeline::shutdown_fog_pipeline() {
  if (QOpenGLContext::currentContext() != nullptr) {
    initializeOpenGLFunctions();
    m_fog_persistent_buffer.destroy();
  }
  release_mesh_buffers(*this, m_fog_mesh);
  m_fog_instance_capacity = 0;
  m_fog_scratch.clear();
}

void CylinderPipeline::upload_fog_instances(std::size_t count) {
  m_fog_instances_resident = 0;
  count = std::min(count, m_fog_scratch.size());
  if ((m_fog_mesh.instance_buffer == 0U) || count == 0) {
    return;
  }

  initializeOpenGLFunctions();
  glBindBuffer(GL_ARRAY_BUFFER, m_fog_mesh.instance_buffer);
  if (count > m_fog_instance_capacity) {
    m_fog_instance_capacity = std::max<std::size_t>(
        count,
        (m_fog_instance_capacity != 0U)
            ? m_fog_instance_capacity * Growth::capacity_multiplier
            : count);
    glBufferData(GL_ARRAY_BUFFER,
                 m_fog_instance_capacity * sizeof(FogInstanceGpu),
                 nullptr,
                 GL_DYNAMIC_DRAW);
    m_fog_scratch.reserve(m_fog_instance_capacity);
  }
  glBufferSubData(
      GL_ARRAY_BUFFER, 0, count * sizeof(FogInstanceGpu), m_fog_scratch.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  m_fog_instances_resident = count;
}

void CylinderPipeline::bind_fog_instance_buffer(GL::Buffer* instance_buffer) {
  m_fog_instances_resident = 0;
  if (instance_buffer == nullptr || m_fog_mesh.vao == 0U) {
    return;
  }

  m_fog_instances_resident = instance_buffer->size_bytes() / sizeof(FogInstanceGpu);

  initializeOpenGLFunctions();
  glBindVertexArray(m_fog_mesh.vao);
  instance_buffer->bind();

  apply_fog_instance_layout(*this);
  glBindVertexArray(0);
  instance_buffer->unbind();
}

void CylinderPipeline::draw_fog(std::size_t count) {
  count = m_fog_draw_guard.clamp(count, m_fog_instances_resident);
  if ((m_fog_mesh.vao == 0U) || m_fog_mesh.index_count == 0 || count == 0) {
    return;
  }

  initializeOpenGLFunctions();
  glBindVertexArray(m_fog_mesh.vao);
  glDrawElementsInstanced(GL_TRIANGLES,
                          m_fog_mesh.index_count,
                          GL_UNSIGNED_INT,
                          nullptr,
                          static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

} // namespace Render::GL::BackendPipelines
