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
    : m_shader_cache(shaderCache) {}

PrimitiveBatchPipeline::~PrimitiveBatchPipeline() { shutdown(); }

auto PrimitiveBatchPipeline::initialize() -> bool {
  initializeOpenGLFunctions();

  if (m_shader_cache == nullptr) {
    return false;
  }

  m_shader = m_shader_cache->get(QStringLiteral("primitive_instanced"));
  if (m_shader == nullptr) {
    return false;
  }

  initialize_sphere_vao();
  initialize_cylinder_vao();
  initialize_cone_vao();
  cache_uniforms();

  m_initialized = true;
  return true;
}

void PrimitiveBatchPipeline::shutdown() {
  shutdown_vaos();
  m_initialized = false;
}

void PrimitiveBatchPipeline::cache_uniforms() {
  if (m_shader != nullptr) {
    m_uniforms.view_proj = m_shader->uniform_handle("u_viewProj");
    m_uniforms.light_dir = m_shader->uniform_handle("u_lightDir");
    m_uniforms.ambient_strength = m_shader->uniform_handle("u_ambientStrength");
  }
}

void PrimitiveBatchPipeline::begin_frame() {}

void PrimitiveBatchPipeline::setup_instance_attributes(GLuint vao,
                                                       GLuint instance_buffer) {
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, instance_buffer);

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

void PrimitiveBatchPipeline::initialize_sphere_vao() {
  Mesh *unit = get_unit_sphere();
  if (unit == nullptr) {
    return;
  }

  const auto &vertices = unit->getVertices();
  const auto &indices = unit->getIndices();
  if (vertices.empty() || indices.empty()) {
    return;
  }

  glGenVertexArrays(1, &m_sphere_vao);
  glBindVertexArray(m_sphere_vao);

  glGenBuffers(1, &m_sphere_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_sphere_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
               vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &m_sphere_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_sphere_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);
  m_sphere_index_count = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(Position, Vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, position)));
  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(Normal, Vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, normal)));
  glEnableVertexAttribArray(TexCoord);
  glVertexAttribPointer(TexCoord, Vec2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, tex_coord)));

  glGenBuffers(1, &m_sphere_instance_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_sphere_instance_buffer);
  m_sphere_instance_capacity = k_default_instance_capacity;
  glBufferData(GL_ARRAY_BUFFER,
               m_sphere_instance_capacity * sizeof(GL::PrimitiveInstanceGpu),
               nullptr, GL_DYNAMIC_DRAW);

  setup_instance_attributes(m_sphere_vao, m_sphere_instance_buffer);
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::initialize_cylinder_vao() {
  Mesh *unit = get_unit_cylinder();
  if (unit == nullptr) {
    return;
  }

  const auto &vertices = unit->getVertices();
  const auto &indices = unit->getIndices();
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

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(Position, Vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, position)));
  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(Normal, Vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, normal)));
  glEnableVertexAttribArray(TexCoord);
  glVertexAttribPointer(TexCoord, Vec2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, tex_coord)));

  glGenBuffers(1, &m_cylinder_instance_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_cylinder_instance_buffer);
  m_cylinder_instance_capacity = k_default_instance_capacity;
  glBufferData(GL_ARRAY_BUFFER,
               m_cylinder_instance_capacity * sizeof(GL::PrimitiveInstanceGpu),
               nullptr, GL_DYNAMIC_DRAW);

  setup_instance_attributes(m_cylinder_vao, m_cylinder_instance_buffer);
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::initialize_cone_vao() {
  Mesh *unit = get_unit_cone();
  if (unit == nullptr) {
    return;
  }

  const auto &vertices = unit->getVertices();
  const auto &indices = unit->getIndices();
  if (vertices.empty() || indices.empty()) {
    return;
  }

  glGenVertexArrays(1, &m_cone_vao);
  glBindVertexArray(m_cone_vao);

  glGenBuffers(1, &m_cone_vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_cone_vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex),
               vertices.data(), GL_STATIC_DRAW);

  glGenBuffers(1, &m_cone_index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cone_index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int),
               indices.data(), GL_STATIC_DRAW);
  m_cone_index_count = static_cast<GLsizei>(indices.size());

  glEnableVertexAttribArray(Position);
  glVertexAttribPointer(Position, Vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, position)));
  glEnableVertexAttribArray(Normal);
  glVertexAttribPointer(Normal, Vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, normal)));
  glEnableVertexAttribArray(TexCoord);
  glVertexAttribPointer(TexCoord, Vec2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                        reinterpret_cast<void *>(offsetof(Vertex, tex_coord)));

  glGenBuffers(1, &m_cone_instance_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_cone_instance_buffer);
  m_cone_instance_capacity = k_default_instance_capacity;
  glBufferData(GL_ARRAY_BUFFER,
               m_cone_instance_capacity * sizeof(GL::PrimitiveInstanceGpu),
               nullptr, GL_DYNAMIC_DRAW);

  setup_instance_attributes(m_cone_vao, m_cone_instance_buffer);
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::shutdown_vaos() {
  if (m_sphere_vao != 0) {
    glDeleteVertexArrays(1, &m_sphere_vao);
    m_sphere_vao = 0;
  }
  if (m_sphere_vertex_buffer != 0) {
    glDeleteBuffers(1, &m_sphere_vertex_buffer);
    m_sphere_vertex_buffer = 0;
  }
  if (m_sphere_index_buffer != 0) {
    glDeleteBuffers(1, &m_sphere_index_buffer);
    m_sphere_index_buffer = 0;
  }
  if (m_sphere_instance_buffer != 0) {
    glDeleteBuffers(1, &m_sphere_instance_buffer);
    m_sphere_instance_buffer = 0;
  }

  if (m_cylinder_vao != 0) {
    glDeleteVertexArrays(1, &m_cylinder_vao);
    m_cylinder_vao = 0;
  }
  if (m_cylinder_vertex_buffer != 0) {
    glDeleteBuffers(1, &m_cylinder_vertex_buffer);
    m_cylinder_vertex_buffer = 0;
  }
  if (m_cylinder_index_buffer != 0) {
    glDeleteBuffers(1, &m_cylinder_index_buffer);
    m_cylinder_index_buffer = 0;
  }
  if (m_cylinder_instance_buffer != 0) {
    glDeleteBuffers(1, &m_cylinder_instance_buffer);
    m_cylinder_instance_buffer = 0;
  }

  if (m_cone_vao != 0) {
    glDeleteVertexArrays(1, &m_cone_vao);
    m_cone_vao = 0;
  }
  if (m_cone_vertex_buffer != 0) {
    glDeleteBuffers(1, &m_cone_vertex_buffer);
    m_cone_vertex_buffer = 0;
  }
  if (m_cone_index_buffer != 0) {
    glDeleteBuffers(1, &m_cone_index_buffer);
    m_cone_index_buffer = 0;
  }
  if (m_cone_instance_buffer != 0) {
    glDeleteBuffers(1, &m_cone_instance_buffer);
    m_cone_instance_buffer = 0;
  }
}

void PrimitiveBatchPipeline::upload_sphere_instances(
    const GL::PrimitiveInstanceGpu *data, std::size_t count) {
  if (count == 0 || data == nullptr) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_sphere_instance_buffer);

  if (count > m_sphere_instance_capacity) {
    m_sphere_instance_capacity =
        static_cast<std::size_t>(count * k_growth_factor);
    glBufferData(GL_ARRAY_BUFFER,
                 m_sphere_instance_capacity * sizeof(GL::PrimitiveInstanceGpu),
                 nullptr, GL_DYNAMIC_DRAW);
  }

  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(GL::PrimitiveInstanceGpu),
                  data);
}

void PrimitiveBatchPipeline::upload_cylinder_instances(
    const GL::PrimitiveInstanceGpu *data, std::size_t count) {
  if (count == 0 || data == nullptr) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_cylinder_instance_buffer);

  if (count > m_cylinder_instance_capacity) {
    m_cylinder_instance_capacity =
        static_cast<std::size_t>(count * k_growth_factor);
    glBufferData(GL_ARRAY_BUFFER,
                 m_cylinder_instance_capacity *
                     sizeof(GL::PrimitiveInstanceGpu),
                 nullptr, GL_DYNAMIC_DRAW);
  }

  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(GL::PrimitiveInstanceGpu),
                  data);
}

void PrimitiveBatchPipeline::upload_cone_instances(
    const GL::PrimitiveInstanceGpu *data, std::size_t count) {
  if (count == 0 || data == nullptr) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_cone_instance_buffer);

  if (count > m_cone_instance_capacity) {
    m_cone_instance_capacity =
        static_cast<std::size_t>(count * k_growth_factor);
    glBufferData(GL_ARRAY_BUFFER,
                 m_cone_instance_capacity * sizeof(GL::PrimitiveInstanceGpu),
                 nullptr, GL_DYNAMIC_DRAW);
  }

  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(GL::PrimitiveInstanceGpu),
                  data);
}

void PrimitiveBatchPipeline::draw_spheres(std::size_t count,
                                          const QMatrix4x4 &view_proj) {
  if (count == 0 || m_sphere_vao == 0 || m_shader == nullptr) {
    return;
  }

  m_shader->use();
  m_shader->set_uniform(m_uniforms.view_proj, view_proj);
  m_shader->set_uniform(m_uniforms.light_dir, QVector3D(0.35F, 0.8F, 0.45F));
  m_shader->set_uniform(m_uniforms.ambient_strength, 0.3F);

  glBindVertexArray(m_sphere_vao);
  glDrawElementsInstanced(GL_TRIANGLES, m_sphere_index_count, GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::draw_cylinders(std::size_t count,
                                            const QMatrix4x4 &view_proj) {
  if (count == 0 || m_cylinder_vao == 0 || m_shader == nullptr) {
    return;
  }

  m_shader->use();
  m_shader->set_uniform(m_uniforms.view_proj, view_proj);
  m_shader->set_uniform(m_uniforms.light_dir, QVector3D(0.35F, 0.8F, 0.45F));
  m_shader->set_uniform(m_uniforms.ambient_strength, 0.3F);

  glBindVertexArray(m_cylinder_vao);
  glDrawElementsInstanced(GL_TRIANGLES, m_cylinder_index_count, GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::draw_cones(std::size_t count,
                                        const QMatrix4x4 &view_proj) {
  if (count == 0 || m_cone_vao == 0 || m_shader == nullptr) {
    return;
  }

  m_shader->use();
  m_shader->set_uniform(m_uniforms.view_proj, view_proj);
  m_shader->set_uniform(m_uniforms.light_dir, QVector3D(0.35F, 0.8F, 0.45F));
  m_shader->set_uniform(m_uniforms.ambient_strength, 0.3F);

  glBindVertexArray(m_cone_vao);
  glDrawElementsInstanced(GL_TRIANGLES, m_cone_index_count, GL_UNSIGNED_INT,
                          nullptr, static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

} 
