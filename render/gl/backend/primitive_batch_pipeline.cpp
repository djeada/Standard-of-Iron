#include "primitive_batch_pipeline.h"

#include <QOpenGLContext>

#include <algorithm>
#include <cstddef>

#include "render/gl/backend.h"
#include "render/gl/mesh.h"
#include "render/gl/platform_gl.h"
#include "render/gl/primitives.h"
#include "render/gl/render_constants.h"
#include "render/gl/vertex_attrib_layout.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

PrimitiveBatchPipeline::PrimitiveBatchPipeline(ShaderCache* shader_cache)
    : m_shader_cache(shader_cache) {
}

PrimitiveBatchPipeline::~PrimitiveBatchPipeline() {
  shutdown();
}

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
    m_uniforms.view_proj = m_shader->optional_uniform_handle("u_view_proj");
    m_uniforms.light_dir = m_shader->optional_uniform_handle("u_light_dir");
    m_uniforms.ambient_strength =
        m_shader->optional_uniform_handle("u_ambient_strength");
  }
}

void PrimitiveBatchPipeline::begin_frame() {
}

void PrimitiveBatchPipeline::setup_instance_attributes(GLuint vao,
                                                       GLuint instance_buffer) {
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, instance_buffer);

  const auto stride = static_cast<GLsizei>(sizeof(GL::PrimitiveInstanceGpu));

  apply_vertex_attrib_layout({{3,
                               vec4,
                               GL_FLOAT,
                               GL_FALSE,
                               stride,
                               offsetof(GL::PrimitiveInstanceGpu, model_col0),
                               1,
                               true},
                              {4,
                               vec4,
                               GL_FLOAT,
                               GL_FALSE,
                               stride,
                               offsetof(GL::PrimitiveInstanceGpu, model_col1),
                               1,
                               true},
                              {5,
                               vec4,
                               GL_FLOAT,
                               GL_FALSE,
                               stride,
                               offsetof(GL::PrimitiveInstanceGpu, model_col2),
                               1,
                               true},
                              {6,
                               vec4,
                               GL_FLOAT,
                               GL_FALSE,
                               stride,
                               offsetof(GL::PrimitiveInstanceGpu, color_alpha),
                               1,
                               true}});

  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::initialize_sphere_vao() {
  Mesh* unit = get_unit_sphere();
  if (unit == nullptr) {
    return;
  }

  const auto& vertices = unit->get_vertices();
  const auto& indices = unit->get_indices();
  if (vertices.empty() || indices.empty()) {
    return;
  }

  glGenVertexArrays(1, &m_sphere_mesh.vao);
  glBindVertexArray(m_sphere_mesh.vao);

  glGenBuffers(1, &m_sphere_mesh.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_sphere_mesh.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               vertices.size() * sizeof(Vertex),
               vertices.data(),
               GL_STATIC_DRAW);

  glGenBuffers(1, &m_sphere_mesh.index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_sphere_mesh.index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               indices.size() * sizeof(unsigned int),
               indices.data(),
               GL_STATIC_DRAW);
  m_sphere_mesh.index_count = static_cast<GLsizei>(indices.size());

  apply_vertex_attrib_layout(
      {{position, vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, position)},
       {normal, vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, normal)},
       {tex_coord,
        vec2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        offsetof(Vertex, tex_coord)}});

  glGenBuffers(1, &m_sphere_mesh.instance_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_sphere_mesh.instance_buffer);
  m_sphere_instance_capacity = k_default_instance_capacity;
  glBufferData(GL_ARRAY_BUFFER,
               m_sphere_instance_capacity * sizeof(GL::PrimitiveInstanceGpu),
               nullptr,
               GL_DYNAMIC_DRAW);

  setup_instance_attributes(m_sphere_mesh.vao, m_sphere_mesh.instance_buffer);
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::initialize_cylinder_vao() {
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

  apply_vertex_attrib_layout(
      {{position, vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, position)},
       {normal, vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, normal)},
       {tex_coord,
        vec2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        offsetof(Vertex, tex_coord)}});

  glGenBuffers(1, &m_cylinder_mesh.instance_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_cylinder_mesh.instance_buffer);
  m_cylinder_instance_capacity = k_default_instance_capacity;
  glBufferData(GL_ARRAY_BUFFER,
               m_cylinder_instance_capacity * sizeof(GL::PrimitiveInstanceGpu),
               nullptr,
               GL_DYNAMIC_DRAW);

  setup_instance_attributes(m_cylinder_mesh.vao, m_cylinder_mesh.instance_buffer);
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::initialize_cone_vao() {
  Mesh* unit = get_unit_cone();
  if (unit == nullptr) {
    return;
  }

  const auto& vertices = unit->get_vertices();
  const auto& indices = unit->get_indices();
  if (vertices.empty() || indices.empty()) {
    return;
  }

  glGenVertexArrays(1, &m_cone_mesh.vao);
  glBindVertexArray(m_cone_mesh.vao);

  glGenBuffers(1, &m_cone_mesh.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_cone_mesh.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               vertices.size() * sizeof(Vertex),
               vertices.data(),
               GL_STATIC_DRAW);

  glGenBuffers(1, &m_cone_mesh.index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_cone_mesh.index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               indices.size() * sizeof(unsigned int),
               indices.data(),
               GL_STATIC_DRAW);
  m_cone_mesh.index_count = static_cast<GLsizei>(indices.size());

  apply_vertex_attrib_layout(
      {{position, vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, position)},
       {normal, vec3, GL_FLOAT, GL_FALSE, sizeof(Vertex), offsetof(Vertex, normal)},
       {tex_coord,
        vec2,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        offsetof(Vertex, tex_coord)}});

  glGenBuffers(1, &m_cone_mesh.instance_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_cone_mesh.instance_buffer);
  m_cone_instance_capacity = k_default_instance_capacity;
  glBufferData(GL_ARRAY_BUFFER,
               m_cone_instance_capacity * sizeof(GL::PrimitiveInstanceGpu),
               nullptr,
               GL_DYNAMIC_DRAW);

  setup_instance_attributes(m_cone_mesh.vao, m_cone_mesh.instance_buffer);
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::shutdown_vaos() {
  for (StaticMeshBuffers* mesh : {&m_sphere_mesh, &m_cylinder_mesh, &m_cone_mesh}) {
    release_mesh_buffers(*this, *mesh);
  }
}

void PrimitiveBatchPipeline::upload_sphere_instances(
    const GL::PrimitiveInstanceGpu* data, std::size_t count) {
  m_sphere_instances_resident = 0;
  if (count == 0 || data == nullptr || m_sphere_mesh.instance_buffer == 0) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_sphere_mesh.instance_buffer);

  if (count > m_sphere_instance_capacity) {
    m_sphere_instance_capacity = static_cast<std::size_t>(count * k_growth_factor);
    glBufferData(GL_ARRAY_BUFFER,
                 m_sphere_instance_capacity * sizeof(GL::PrimitiveInstanceGpu),
                 nullptr,
                 GL_DYNAMIC_DRAW);
  }

  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(GL::PrimitiveInstanceGpu), data);
  m_sphere_instances_resident = count;
}

void PrimitiveBatchPipeline::upload_cylinder_instances(
    const GL::PrimitiveInstanceGpu* data, std::size_t count) {
  m_cylinder_instances_resident = 0;
  if (count == 0 || data == nullptr || m_cylinder_mesh.instance_buffer == 0) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_cylinder_mesh.instance_buffer);

  if (count > m_cylinder_instance_capacity) {
    m_cylinder_instance_capacity = static_cast<std::size_t>(count * k_growth_factor);
    glBufferData(GL_ARRAY_BUFFER,
                 m_cylinder_instance_capacity * sizeof(GL::PrimitiveInstanceGpu),
                 nullptr,
                 GL_DYNAMIC_DRAW);
  }

  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(GL::PrimitiveInstanceGpu), data);
  m_cylinder_instances_resident = count;
}

void PrimitiveBatchPipeline::upload_cone_instances(const GL::PrimitiveInstanceGpu* data,
                                                   std::size_t count) {
  m_cone_instances_resident = 0;
  if (count == 0 || data == nullptr || m_cone_mesh.instance_buffer == 0) {
    return;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_cone_mesh.instance_buffer);

  if (count > m_cone_instance_capacity) {
    m_cone_instance_capacity = static_cast<std::size_t>(count * k_growth_factor);
    glBufferData(GL_ARRAY_BUFFER,
                 m_cone_instance_capacity * sizeof(GL::PrimitiveInstanceGpu),
                 nullptr,
                 GL_DYNAMIC_DRAW);
  }

  glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(GL::PrimitiveInstanceGpu), data);
  m_cone_instances_resident = count;
}

void PrimitiveBatchPipeline::draw_spheres(std::size_t count,
                                          const QMatrix4x4& view_proj,
                                          const QVector3D& light_dir,
                                          float ambient_strength) {
  count = m_sphere_draw_guard.clamp(count, m_sphere_instances_resident);
  if (count == 0 || m_sphere_mesh.vao == 0 || m_shader == nullptr) {
    return;
  }

  m_shader->use();
  m_shader->set_uniform(m_uniforms.view_proj, view_proj);
  m_shader->set_uniform(m_uniforms.light_dir, light_dir);
  m_shader->set_uniform(m_uniforms.ambient_strength, ambient_strength);

  glBindVertexArray(m_sphere_mesh.vao);
  glDrawElementsInstanced(GL_TRIANGLES,
                          m_sphere_mesh.index_count,
                          GL_UNSIGNED_INT,
                          nullptr,
                          static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::draw_cylinders(std::size_t count,
                                            const QMatrix4x4& view_proj,
                                            const QVector3D& light_dir,
                                            float ambient_strength) {
  count = m_cylinder_draw_guard.clamp(count, m_cylinder_instances_resident);
  if (count == 0 || m_cylinder_mesh.vao == 0 || m_shader == nullptr) {
    return;
  }

  m_shader->use();
  m_shader->set_uniform(m_uniforms.view_proj, view_proj);
  m_shader->set_uniform(m_uniforms.light_dir, light_dir);
  m_shader->set_uniform(m_uniforms.ambient_strength, ambient_strength);

  glBindVertexArray(m_cylinder_mesh.vao);
  glDrawElementsInstanced(GL_TRIANGLES,
                          m_cylinder_mesh.index_count,
                          GL_UNSIGNED_INT,
                          nullptr,
                          static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

void PrimitiveBatchPipeline::draw_cones(std::size_t count,
                                        const QMatrix4x4& view_proj,
                                        const QVector3D& light_dir,
                                        float ambient_strength) {
  count = m_cone_draw_guard.clamp(count, m_cone_instances_resident);
  if (count == 0 || m_cone_mesh.vao == 0 || m_shader == nullptr) {
    return;
  }

  m_shader->use();
  m_shader->set_uniform(m_uniforms.view_proj, view_proj);
  m_shader->set_uniform(m_uniforms.light_dir, light_dir);
  m_shader->set_uniform(m_uniforms.ambient_strength, ambient_strength);

  glBindVertexArray(m_cone_mesh.vao);
  glDrawElementsInstanced(GL_TRIANGLES,
                          m_cone_mesh.index_count,
                          GL_UNSIGNED_INT,
                          nullptr,
                          static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

} // namespace Render::GL::BackendPipelines
