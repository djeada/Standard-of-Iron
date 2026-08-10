#include "ground_marker_pipeline.h"

#include <QOpenGLContext>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include "render/geom/ground_marker_pattern.h"
#include "render/gl/mesh.h"
#include "render/gl/render_constants.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {

constexpr int k_angular_segments = 96;
constexpr int k_radial_segments = 6;
constexpr std::size_t k_default_instance_capacity = 256;
constexpr std::size_t k_capacity_growth = 2;

} // namespace

GroundMarkerPipeline::GroundMarkerPipeline(ShaderCache* shader_cache)
    : m_shader_cache(shader_cache) {
}

GroundMarkerPipeline::~GroundMarkerPipeline() {
  shutdown();
}

auto GroundMarkerPipeline::initialize() -> bool {
  initializeOpenGLFunctions();

  if (m_shader_cache == nullptr) {
    return false;
  }
  m_shader = m_shader_cache->get(QStringLiteral("ground_marker"));
  if (m_shader == nullptr) {
    return false;
  }

  build_mesh();
  cache_uniforms();
  m_initialized = m_mesh.drawable();
  return m_initialized;
}

void GroundMarkerPipeline::shutdown() {
  if (QOpenGLContext::currentContext() != nullptr) {
    initializeOpenGLFunctions();
    release_mesh_buffers(*this, m_mesh);
  }
  m_instance_capacity = 0;
  m_instances_resident = 0;
  m_scratch.clear();
  m_initialized = false;
}

void GroundMarkerPipeline::cache_uniforms() {
  if (m_shader == nullptr) {
    return;
  }
  m_uniforms.time = m_shader->optional_uniform_handle("u_time");
  m_uniforms.ground_offset = m_shader->optional_uniform_handle("u_ground_offset");
  for (std::size_t slot = 0; slot < k_pattern_slots; ++slot) {
    const std::string name = "u_pattern_table[" + std::to_string(slot) + "]";
    m_pattern_handles[slot] = m_shader->optional_uniform_handle(name.c_str());
  }
  m_uniforms.has_height_tex = m_shader->optional_uniform_handle("u_has_height_tex");
  m_uniforms.height_tex = m_shader->optional_uniform_handle("u_height_tex");
  m_uniforms.height_uv_scale = m_shader->optional_uniform_handle("u_height_uv_scale");
  m_uniforms.height_uv_offset = m_shader->optional_uniform_handle("u_height_uv_offset");
  m_uniforms.height_to_world = m_shader->optional_uniform_handle("u_height_to_world");
}

void GroundMarkerPipeline::build_mesh() {
  initializeOpenGLFunctions();
  release_mesh_buffers(*this, m_mesh);

  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(
      static_cast<std::size_t>((k_angular_segments + 1) * (k_radial_segments + 1)));
  indices.reserve(static_cast<std::size_t>(k_angular_segments * k_radial_segments * 6));

  constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;
  const float radial_span =
      Render::Geom::k_marker_geometry_outer - Render::Geom::k_marker_geometry_inner;

  for (int ring = 0; ring <= k_radial_segments; ++ring) {
    const float t = static_cast<float>(ring) / static_cast<float>(k_radial_segments);
    const float radial = Render::Geom::k_marker_geometry_inner + t * radial_span;
    for (int segment = 0; segment <= k_angular_segments; ++segment) {
      const float u =
          static_cast<float>(segment) / static_cast<float>(k_angular_segments);
      const float angle = u * k_two_pi;
      vertices.push_back(
          {{std::cos(angle), 0.0F, std::sin(angle)}, {0.0F, 1.0F, 0.0F}, {u, radial}});
    }
  }

  const auto row_stride = static_cast<unsigned int>(k_angular_segments + 1);
  for (int ring = 0; ring < k_radial_segments; ++ring) {
    for (int segment = 0; segment < k_angular_segments; ++segment) {
      const auto base = static_cast<unsigned int>(ring) * row_stride +
                        static_cast<unsigned int>(segment);
      const unsigned int next = base + 1U;
      const unsigned int above = base + row_stride;
      const unsigned int above_next = above + 1U;
      indices.insert(indices.end(), {base, above, next, next, above, above_next});
    }
  }

  glGenVertexArrays(1, &m_mesh.vao);
  glBindVertexArray(m_mesh.vao);

  glGenBuffers(1, &m_mesh.vertex_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_mesh.vertex_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
               vertices.data(),
               GL_STATIC_DRAW);

  glGenBuffers(1, &m_mesh.index_buffer);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_mesh.index_buffer);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
               indices.data(),
               GL_STATIC_DRAW);
  m_mesh.index_count = static_cast<GLsizei>(indices.size());
  m_mesh.vertex_count = static_cast<GLsizei>(vertices.size());

  glEnableVertexAttribArray(VertexAttrib::position);
  glVertexAttribPointer(VertexAttrib::position,
                        ComponentCount::vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(Vertex),
                        reinterpret_cast<void*>(offsetof(Vertex, position)));
  glEnableVertexAttribArray(VertexAttrib::normal);
  glVertexAttribPointer(VertexAttrib::normal,
                        ComponentCount::vec3,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(Vertex),
                        reinterpret_cast<void*>(offsetof(Vertex, normal)));
  glEnableVertexAttribArray(VertexAttrib::tex_coord);
  glVertexAttribPointer(VertexAttrib::tex_coord,
                        ComponentCount::vec2,
                        GL_FLOAT,
                        GL_FALSE,
                        sizeof(Vertex),
                        reinterpret_cast<void*>(offsetof(Vertex, tex_coord)));

  glGenBuffers(1, &m_mesh.instance_buffer);
  glBindBuffer(GL_ARRAY_BUFFER, m_mesh.instance_buffer);
  m_instance_capacity = k_default_instance_capacity;
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(m_instance_capacity * sizeof(InstanceGpu)),
               nullptr,
               GL_DYNAMIC_DRAW);

  const auto stride = static_cast<GLsizei>(sizeof(InstanceGpu));
  glEnableVertexAttribArray(VertexAttrib::instance_position);
  glVertexAttribPointer(VertexAttrib::instance_position,
                        ComponentCount::vec4,
                        GL_FLOAT,
                        GL_FALSE,
                        stride,
                        reinterpret_cast<void*>(offsetof(InstanceGpu, center_radius)));
  glVertexAttribDivisor(VertexAttrib::instance_position, 1);

  glEnableVertexAttribArray(VertexAttrib::instance_scale);
  glVertexAttribPointer(VertexAttrib::instance_scale,
                        ComponentCount::vec4,
                        GL_FLOAT,
                        GL_FALSE,
                        stride,
                        reinterpret_cast<void*>(offsetof(InstanceGpu, color_alpha)));
  glVertexAttribDivisor(VertexAttrib::instance_scale, 1);

  glEnableVertexAttribArray(VertexAttrib::instance_color);
  glVertexAttribPointer(VertexAttrib::instance_color,
                        ComponentCount::vec4,
                        GL_FLOAT,
                        GL_FALSE,
                        stride,
                        reinterpret_cast<void*>(offsetof(InstanceGpu, shape)));
  glVertexAttribDivisor(VertexAttrib::instance_color, 1);

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

  m_scratch.reserve(m_instance_capacity);
}

void GroundMarkerPipeline::upload_pattern_table() {
  if (m_shader == nullptr) {
    return;
  }

  for (std::size_t i = 0; i < Render::Geom::k_ground_marker_patterns.size(); ++i) {
    const auto& spec = Render::Geom::k_ground_marker_patterns[i];
    const std::size_t first = i * 2;
    const std::size_t second = first + 1;
    if (second >= k_pattern_slots) {
      break;
    }
    if (m_pattern_handles[first] != Shader::InvalidUniform) {
      m_shader->set_uniform(m_pattern_handles[first],
                            QVector4D(spec.dash_count,
                                      spec.dash_duty,
                                      spec.second_ring_start,
                                      spec.second_ring_end));
    }
    if (m_pattern_handles[second] != Shader::InvalidUniform) {
      m_shader->set_uniform(m_pattern_handles[second],
                            QVector4D(spec.tick_count, spec.tick_length, 0.0F, 0.0F));
    }
  }
}

void GroundMarkerPipeline::upload_instances(std::size_t count) {
  m_instances_resident = 0;
  count = std::min(count, m_scratch.size());
  if (count == 0 || m_mesh.instance_buffer == 0U) {
    return;
  }

  initializeOpenGLFunctions();
  glBindBuffer(GL_ARRAY_BUFFER, m_mesh.instance_buffer);
  if (count > m_instance_capacity) {
    m_instance_capacity =
        std::max<std::size_t>(count, m_instance_capacity * k_capacity_growth);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_instance_capacity * sizeof(InstanceGpu)),
                 nullptr,
                 GL_DYNAMIC_DRAW);
    m_scratch.reserve(m_instance_capacity);
  }
  glBufferSubData(GL_ARRAY_BUFFER,
                  0,
                  static_cast<GLsizeiptr>(count * sizeof(InstanceGpu)),
                  m_scratch.data());
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  m_instances_resident = count;
}

void GroundMarkerPipeline::draw(std::size_t count) {
  count = m_draw_guard.clamp(count, m_instances_resident);
  if (!m_mesh.drawable() || count == 0) {
    return;
  }

  initializeOpenGLFunctions();
  glBindVertexArray(m_mesh.vao);
  glDrawElementsInstanced(GL_TRIANGLES,
                          m_mesh.index_count,
                          GL_UNSIGNED_INT,
                          nullptr,
                          static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

} // namespace Render::GL::BackendPipelines
