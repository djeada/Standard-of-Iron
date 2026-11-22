#include "render/backend/ground_marker_pipeline.h"

#include <QOpenGLContext>

#include <algorithm>
#include <cmath>
#include <numbers>
#include <string>
#include <vector>

#include "render/backend/state_scopes.h"
#include "render/backend/uniform_helpers.h"
#include "render/geom/ground_marker_pattern.h"
#include "render/geom/ground_marker_tessellation.h"
#include "render/gl/gl_resource_tracking.h"
#include "render/gl/mesh.h"
#include "render/gl/render_constants.h"
#include "render/gl/texture.h"

namespace Render::GL::BackendPipelines {

using namespace Render::GL::VertexAttrib;
using namespace Render::GL::ComponentCount;

namespace {

constexpr int k_radial_segments = 6;
constexpr std::size_t k_default_instance_capacity = 256;
constexpr std::size_t k_capacity_growth = 2;

constexpr float k_ground_marker_offset = 0.06F;

constexpr float k_ground_marker_slope_clearance = 0.14F;
constexpr float k_ground_marker_max_slope_lift = 4.0F;

constexpr float k_ground_marker_min_pixels = 1.6F;
constexpr float k_ground_marker_max_thickness_gain = 6.0F;

auto ground_marker_world_per_cell(const TerrainSurfaceCmd::HeightResources& height)
    -> float {
  if (height.uv_scale.x() <= 0.0F || height.texel_size.x() <= 0.0F) {
    return 1.0F;
  }
  return height.texel_size.x() / height.uv_scale.x();
}

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

  build_meshes();
  cache_uniforms();
  m_initialized =
      std::all_of(m_meshes.begin(), m_meshes.end(), [](const StaticMeshBuffers& mesh) {
        return mesh.drawable();
      });
  return m_initialized;
}

void GroundMarkerPipeline::shutdown() {
  for (StaticMeshBuffers& mesh : m_meshes) {
    mesh.reset();
  }
  m_instance_buffer.reset();
  m_instance_capacity = 0;
  m_instances_resident = 0;
  for (auto& bucket : m_scratch) {
    bucket.clear();
  }
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
  m_uniforms.angular_step = m_shader->optional_uniform_handle("u_angular_step");
  m_uniforms.height_world_per_texel =
      m_shader->optional_uniform_handle("u_height_world_per_texel");
  m_uniforms.slope_clearance = m_shader->optional_uniform_handle("u_slope_clearance");
  m_uniforms.max_slope_lift = m_shader->optional_uniform_handle("u_max_slope_lift");
  m_uniforms.half_viewport = m_shader->optional_uniform_handle("u_half_viewport");
  m_uniforms.min_marker_pixels =
      m_shader->optional_uniform_handle("u_min_marker_pixels");
  m_uniforms.max_thickness_gain =
      m_shader->optional_uniform_handle("u_max_thickness_gain");
}

void GroundMarkerPipeline::build_meshes() {
  initializeOpenGLFunctions();

  for (StaticMeshBuffers& mesh : m_meshes) {
    mesh.reset();
  }
  m_instance_buffer.reset();

  m_instance_buffer = BufferHandle::create();
  glBindBuffer(GL_ARRAY_BUFFER, m_instance_buffer.id());
  m_instance_capacity = k_default_instance_capacity;
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(m_instance_capacity * sizeof(InstanceGpu)),
               nullptr,
               GL_DYNAMIC_DRAW);
  note_buffer_storage(
      static_cast<std::size_t>(m_instance_capacity * sizeof(InstanceGpu)), false);

  for (std::size_t lod = 0; lod < k_lod_count; ++lod) {
    build_mesh(m_meshes[lod], Render::Geom::marker_segment_count(lod));
    m_scratch[lod].reserve(k_default_instance_capacity);
  }

  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

void GroundMarkerPipeline::build_mesh(StaticMeshBuffers& mesh, int angular_segments) {
  std::vector<Vertex> vertices;
  std::vector<unsigned int> indices;
  vertices.reserve(
      static_cast<std::size_t>((angular_segments + 1) * (k_radial_segments + 1)));
  indices.reserve(static_cast<std::size_t>(angular_segments * k_radial_segments * 6));

  constexpr float k_two_pi = 2.0F * std::numbers::pi_v<float>;
  const float radial_span =
      Render::Geom::k_marker_geometry_outer - Render::Geom::k_marker_geometry_inner;

  for (int ring = 0; ring <= k_radial_segments; ++ring) {
    const float t = static_cast<float>(ring) / static_cast<float>(k_radial_segments);
    const float radial = Render::Geom::k_marker_geometry_inner + t * radial_span;
    for (int segment = 0; segment <= angular_segments; ++segment) {
      const float u =
          static_cast<float>(segment) / static_cast<float>(angular_segments);
      const float angle = u * k_two_pi;
      vertices.push_back(
          {{std::cos(angle), 0.0F, std::sin(angle)}, {0.0F, 1.0F, 0.0F}, {u, radial}});
    }
  }

  const auto row_stride = static_cast<unsigned int>(angular_segments + 1);
  for (int ring = 0; ring < k_radial_segments; ++ring) {
    for (int segment = 0; segment < angular_segments; ++segment) {
      const auto base = static_cast<unsigned int>(ring) * row_stride +
                        static_cast<unsigned int>(segment);
      const unsigned int next = base + 1U;
      const unsigned int above = base + row_stride;
      const unsigned int above_next = above + 1U;
      indices.insert(indices.end(), {base, above, next, next, above, above_next});
    }
  }

  mesh.vao = VertexArrayHandle::create();
  glBindVertexArray(mesh.vao.id());

  mesh.vertex_buffer = BufferHandle::create();
  glBindBuffer(GL_ARRAY_BUFFER, mesh.vertex_buffer.id());
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
               vertices.data(),
               GL_STATIC_DRAW);
  note_buffer_storage(static_cast<std::size_t>(vertices.size() * sizeof(Vertex)),
                      vertices.data() != nullptr);

  mesh.index_buffer = BufferHandle::create();
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mesh.index_buffer.id());
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
               indices.data(),
               GL_STATIC_DRAW);
  note_buffer_storage(static_cast<std::size_t>(indices.size() * sizeof(unsigned int)),
                      indices.data() != nullptr);
  mesh.index_count = static_cast<GLsizei>(indices.size());
  mesh.vertex_count = static_cast<GLsizei>(vertices.size());

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

  glBindBuffer(GL_ARRAY_BUFFER, m_instance_buffer.id());
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
}

void GroundMarkerPipeline::draw_markers(const PreparedBatch& prepared,
                                        PipelineDrawContext& context) {
  if (!is_initialized()) {
    return;
  }

  const auto& queue = context.queue;
  const std::size_t i = prepared.start;
  const std::size_t batch_end = prepared.end();

  DepthMaskScope const depth_mask(false);
  DepthTestScope const depth_test(true);
  PolygonOffsetScope const poly(-1.0F, -1.0F);
  BlendScope const blend(true);
  CullFaceScope const cull(false);

  Shader* marker_shader = m_shader;
  marker_shader->use();
  context.state.bound_shader = marker_shader;
  context.state.bound_texture = nullptr;

  const auto& uniforms = m_uniforms;
  upload_pattern_table();
  set_uniform_if_valid(*marker_shader, uniforms.time, context.frame.animation_time);
  set_uniform_if_valid(*marker_shader, uniforms.ground_offset, k_ground_marker_offset);
  set_uniform_if_valid(
      *marker_shader,
      uniforms.half_viewport,
      QVector2D(static_cast<float>(context.frame.viewport_width) * 0.5F,
                static_cast<float>(context.frame.viewport_height) * 0.5F));
  set_uniform_if_valid(
      *marker_shader, uniforms.min_marker_pixels, k_ground_marker_min_pixels);
  set_uniform_if_valid(
      *marker_shader, uniforms.max_thickness_gain, k_ground_marker_max_thickness_gain);

  const auto& height = std::get<GroundMarkerCmdIndex>(queue.get_sorted(i)).height;
  const bool has_height = height.enabled && height.texture != nullptr;
  set_uniform_if_valid(*marker_shader, uniforms.has_height_tex, has_height ? 1 : 0);
  float world_per_cell = 1.0F;
  if (has_height) {
    height.texture->bind(TextureUnit::terrain_height);
    context.state.bound_texture = height.texture;
    set_uniform_if_valid(
        *marker_shader, uniforms.height_tex, TextureUnit::terrain_height);
    set_uniform_if_valid(*marker_shader, uniforms.height_uv_scale, height.uv_scale);
    set_uniform_if_valid(*marker_shader, uniforms.height_uv_offset, height.uv_offset);
    set_uniform_if_valid(*marker_shader, uniforms.height_to_world, height.to_world);

    world_per_cell = ground_marker_world_per_cell(height);
    set_uniform_if_valid(*marker_shader,
                         uniforms.height_world_per_texel,
                         QVector2D(world_per_cell, world_per_cell));
    set_uniform_if_valid(
        *marker_shader, uniforms.slope_clearance, k_ground_marker_slope_clearance);
    set_uniform_if_valid(
        *marker_shader, uniforms.max_slope_lift, k_ground_marker_max_slope_lift);
  }

  const auto marker_instance = [](const GroundMarkerCmd& marker,
                                  float alpha) -> InstanceGpu {
    return {.center_radius = QVector4D(marker.center, marker.outer_radius),
            .color_alpha = QVector4D(marker.color, alpha),
            .shape = QVector4D(marker.thickness,
                               static_cast<float>(marker.pattern),
                               marker.focused ? 1.0F : 0.0F,
                               marker.phase)};
  };

  begin_batch();
  bool any_occluded = false;
  for (std::size_t j = i; j < batch_end; ++j) {
    const auto& marker = std::get<GroundMarkerCmdIndex>(queue.get_sorted(j));
    any_occluded = any_occluded || marker.occluded_alpha > 0.0F;
    add_instance(
        marker_instance(marker, marker.alpha), marker.outer_radius, world_per_cell);
  }
  flush_batch();

  if (any_occluded) {

    DepthFuncScope const depth_func(GL_GREATER);
    begin_batch();
    for (std::size_t j = i; j < batch_end; ++j) {
      const auto& marker = std::get<GroundMarkerCmdIndex>(queue.get_sorted(j));
      if (marker.occluded_alpha <= 0.0F) {
        continue;
      }
      add_instance(marker_instance(marker, marker.alpha * marker.occluded_alpha),
                   marker.outer_radius,
                   world_per_cell);
    }
    flush_batch();
  }
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

void GroundMarkerPipeline::begin_batch() {
  for (auto& bucket : m_scratch) {
    bucket.clear();
  }
}

void GroundMarkerPipeline::add_instance(const InstanceGpu& instance,
                                        float outer_radius,
                                        float world_per_cell) {
  const std::size_t lod =
      Render::Geom::marker_segment_lod(outer_radius, world_per_cell);
  m_scratch[std::min(lod, k_lod_count - 1U)].push_back(instance);
}

auto GroundMarkerPipeline::batched_instance_count() const -> std::size_t {
  std::size_t total = 0;
  for (const auto& bucket : m_scratch) {
    total += bucket.size();
  }
  return total;
}

void GroundMarkerPipeline::flush_batch() {
  if (m_shader == nullptr) {
    return;
  }
  for (std::size_t lod = 0; lod < k_lod_count; ++lod) {
    if (m_scratch[lod].empty()) {
      continue;
    }
    if (m_uniforms.angular_step != Shader::InvalidUniform) {
      m_shader->set_uniform(m_uniforms.angular_step,
                            Render::Geom::marker_angular_step(lod));
    }
    upload_instances(lod, m_scratch[lod].size());
    draw(lod, m_scratch[lod].size());
  }
}

void GroundMarkerPipeline::upload_instances(std::size_t lod, std::size_t count) {
  m_instances_resident = 0;
  count = std::min(count, m_scratch[lod].size());
  if (count == 0 || !m_instance_buffer) {
    return;
  }

  initializeOpenGLFunctions();
  glBindBuffer(GL_ARRAY_BUFFER, m_instance_buffer.id());
  if (count > m_instance_capacity) {
    m_instance_capacity =
        std::max<std::size_t>(count, m_instance_capacity * k_capacity_growth);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_instance_capacity * sizeof(InstanceGpu)),
                 nullptr,
                 GL_DYNAMIC_DRAW);
    note_buffer_storage(
        static_cast<std::size_t>(m_instance_capacity * sizeof(InstanceGpu)), false);
  }
  glBufferSubData(GL_ARRAY_BUFFER,
                  0,
                  static_cast<GLsizeiptr>(count * sizeof(InstanceGpu)),
                  m_scratch[lod].data());
  note_buffer_transfer(static_cast<std::size_t>(count * sizeof(InstanceGpu)));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  m_instances_resident = count;
}

void GroundMarkerPipeline::draw(std::size_t lod, std::size_t count) {
  count = m_draw_guard.clamp(count, m_instances_resident);
  const StaticMeshBuffers& mesh = m_meshes[lod];
  if (!mesh.drawable() || count == 0) {
    return;
  }

  initializeOpenGLFunctions();
  glBindVertexArray(mesh.vao.id());
  glDrawElementsInstanced(GL_TRIANGLES,
                          mesh.index_count,
                          GL_UNSIGNED_INT,
                          nullptr,
                          static_cast<GLsizei>(count));
  glBindVertexArray(0);
}

} // namespace Render::GL::BackendPipelines
