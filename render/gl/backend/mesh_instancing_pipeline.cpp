#include "mesh_instancing_pipeline.h"

#include <QDebug>
#include <QOpenGLContext>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>

#include "render/gl/draw_tally.h"
#include "render/gl/gl_resource_tracking.h"
#include "render/gl/mesh.h"
#include "render/static_building_batch.h"

namespace Render::GL::BackendPipelines {

namespace {
constexpr std::size_t k_initial_capacity = 512;
constexpr std::size_t k_max_instances_per_batch = 8192;

constexpr std::size_t k_ring_instances = 4U * k_max_instances_per_batch;
constexpr std::size_t k_max_ring_instances = 32U * k_max_instances_per_batch;

constexpr GLuint k_first_instance_loc = 3;
constexpr GLuint k_instance_vec4_count = sizeof(MeshInstanceGpu) / (4U * sizeof(float));
constexpr GLuint k_building_instance_vec4_count =
    sizeof(BuildingInstanceGpu) / (4U * sizeof(float));
constexpr GLuint k_merged_part_color_loc =
    k_first_instance_loc + k_building_instance_vec4_count;
constexpr GLuint k_merged_part_material_loc = k_merged_part_color_loc + 1U;
} // namespace

MeshInstancingPipeline::MeshInstancingPipeline(GL::ShaderCache*) {
  m_instances.reserve(k_initial_capacity);
}

MeshInstancingPipeline::~MeshInstancingPipeline() {
  shutdown();
}

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

  glGenBuffers(1, &m_instance_buffer);
  note_buffers_created(1);
  if (m_instance_buffer == 0) {
    qWarning() << "MeshInstancingPipeline: failed to create instance buffer";
    return false;
  }

  m_instance_capacity = k_max_instances_per_batch;
  m_ring_capacity_bytes = k_ring_instances * sizeof(MeshInstanceGpu);
  m_ring_offset_bytes = 0;
  glBindBuffer(GL_ARRAY_BUFFER, m_instance_buffer);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(m_ring_capacity_bytes),
               nullptr,
               GL_STREAM_DRAW);
  note_buffer_storage(static_cast<std::size_t>(m_ring_capacity_bytes), false);
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  m_initialized = true;
  return true;
}

void MeshInstancingPipeline::shutdown() {
  if (!m_initialized) {
    return;
  }

  if (QOpenGLContext::currentContext() != nullptr) {
    if (m_instance_buffer != 0) {
      glDeleteBuffers(1, &m_instance_buffer);
      m_instance_buffer = 0;
    }
    for (const auto& [id, buffers] : m_merged) {
      glDeleteVertexArrays(1, &buffers.vao);
      glDeleteBuffers(1, &buffers.vertices);
      glDeleteBuffers(1, &buffers.indices);
    }
  }

  m_merged.clear();
  m_instances.clear();
  m_instance_capacity = 0;
  m_ring_capacity_bytes = 0;
  m_ring_offset_bytes = 0;
  m_current_mesh = nullptr;
  m_initialized = false;
}

void MeshInstancingPipeline::cache_uniforms() {
}

auto MeshInstancingPipeline::is_initialized() const -> bool {
  return m_initialized;
}

void MeshInstancingPipeline::begin_frame() {
  m_instances.clear();
  m_current_mesh = nullptr;
}

void MeshInstancingPipeline::accumulate(const QMatrix4x4& model,
                                        const QVector3D& color,
                                        float alpha) {
  const float* data = model.constData();
  m_instances.push_back(MeshInstanceGpu{
      .model_col0 = {data[0], data[1], data[2], data[12]},
      .model_col1 = {data[4], data[5], data[6], data[13]},
      .model_col2 = {data[8], data[9], data[10], data[14]},
      .color_alpha = {color.x(), color.y(), color.z(), alpha},
  });
}

void MeshInstancingPipeline::begin_batch(Mesh* mesh) {
  m_current_mesh = mesh;
}

void MeshInstancingPipeline::flush() {
  if (m_instances.empty()) {
    return;
  }
  if (m_current_mesh == nullptr || !m_initialized) {
    qWarning() << "MeshInstancingPipeline::flush called with invalid state:" << "mesh="
               << m_current_mesh << "initialized=" << m_initialized
               << "instances=" << m_instances.size();
    m_instances.clear();
    return;
  }

  const std::size_t count = m_instances.size();

  if (m_instance_capacity == 0 || m_ring_capacity_bytes == 0) {
    m_instances.clear();
    return;
  }

  if (!bind_mesh(m_current_mesh)) {
    m_instances.clear();
    return;
  }

  for (std::size_t offset = 0; offset < count; offset += m_instance_capacity) {
    const std::size_t chunk = std::min(count - offset, m_instance_capacity);
    std::size_t byte_offset = 0;
    const bool uploaded = upload(
        m_instances.data() + offset, chunk * sizeof(MeshInstanceGpu), byte_offset);
    const std::size_t drawable = m_draw_guard.clamp(chunk, uploaded ? chunk : 0U);
    if (drawable > 0) {
      point_instance_attributes(byte_offset);
      m_current_mesh->draw_bound(drawable);
    }
  }

  m_current_mesh->unbind_vao();
  glBindBuffer(GL_ARRAY_BUFFER, 0);

  m_instances.clear();
}

auto MeshInstancingPipeline::merged_vao(const MergedBuildingMesh& mesh) -> GLuint {
  auto [it, inserted] = m_merged.try_emplace(mesh.id);
  MergedBuffers& buffers = it->second;
  if (!inserted) {
    return buffers.vao;
  }
  glGenVertexArrays(1, &buffers.vao);
  note_vertex_arrays_created(1);
  glGenBuffers(1, &buffers.vertices);
  glGenBuffers(1, &buffers.indices);
  note_buffers_created(2);
  glBindVertexArray(buffers.vao);
  glBindBuffer(GL_ARRAY_BUFFER, buffers.vertices);
  const std::size_t vertex_bytes = mesh.vertices.size() * sizeof(MergedBuildingVertex);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(vertex_bytes),
               mesh.vertices.data(),
               GL_STATIC_DRAW);
  note_buffer_storage(vertex_bytes, true);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, buffers.indices);
  const std::size_t index_bytes = mesh.indices.size() * sizeof(std::uint32_t);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(index_bytes),
               mesh.indices.data(),
               GL_STATIC_DRAW);
  note_buffer_storage(index_bytes, true);

  struct Attribute {
    GLuint location;
    GLint components;
    std::size_t offset;
  };
  const auto stride = static_cast<GLsizei>(sizeof(MergedBuildingVertex));
  for (const Attribute& attribute :
       {Attribute{0, 3, offsetof(MergedBuildingVertex, position)},
        Attribute{1, 3, offsetof(MergedBuildingVertex, normal)},
        Attribute{2, 2, offsetof(MergedBuildingVertex, tex_coord)},
        Attribute{
            k_merged_part_color_loc, 4, offsetof(MergedBuildingVertex, color_alpha)},
        Attribute{
            k_merged_part_material_loc, 4, offsetof(MergedBuildingVertex, material)}}) {
    glEnableVertexAttribArray(attribute.location);
    glVertexAttribPointer(attribute.location,
                          attribute.components,
                          GL_FLOAT,
                          GL_FALSE,
                          stride,
                          reinterpret_cast<void*>(attribute.offset));
  }
  for (GLuint slot = 0; slot < k_building_instance_vec4_count; ++slot) {
    glEnableVertexAttribArray(k_first_instance_loc + slot);
    glVertexAttribDivisor(k_first_instance_loc + slot, 1);
  }
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  return buffers.vao;
}

void MeshInstancingPipeline::draw_merged(const MergedBuildingMesh& mesh,
                                         std::size_t instance_byte_offset,
                                         std::size_t instance_count,
                                         std::uint32_t first_index,
                                         std::uint32_t index_count) {
  if (!m_initialized || instance_count == 0 || index_count == 0) {
    return;
  }
  const GLuint vao = merged_vao(mesh);
  if (vao == 0) {
    return;
  }
  glBindVertexArray(vao);
  glBindBuffer(GL_ARRAY_BUFFER, m_instance_buffer);
  const auto stride = static_cast<GLsizei>(sizeof(BuildingInstanceGpu));
  for (GLuint slot = 0; slot < k_building_instance_vec4_count; ++slot) {
    glVertexAttribPointer(k_first_instance_loc + slot,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          stride,
                          reinterpret_cast<void*>(static_cast<std::uintptr_t>(
                              instance_byte_offset + slot * 4U * sizeof(float))));
  }
  glDrawElementsInstanced(GL_TRIANGLES,
                          static_cast<GLsizei>(index_count),
                          GL_UNSIGNED_INT,
                          reinterpret_cast<void*>(static_cast<std::uintptr_t>(
                              std::size_t{first_index} * sizeof(std::uint32_t))),
                          static_cast<GLsizei>(instance_count));
  tally_draw(index_count, instance_count);
  glBindVertexArray(0);
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

auto MeshInstancingPipeline::bind_mesh(Mesh* mesh) -> bool {
  if (mesh == nullptr || !mesh->bind_vao()) {
    return false;
  }
  if (mesh->claim_instance_layout()) {
    for (GLuint slot = 0; slot < k_instance_vec4_count; ++slot) {
      glEnableVertexAttribArray(k_first_instance_loc + slot);
      glVertexAttribDivisor(k_first_instance_loc + slot, 1);
    }
  }
  return true;
}

auto MeshInstancingPipeline::upload(const void* data,
                                    std::size_t bytes,
                                    std::size_t& byte_offset) -> bool {
  byte_offset = 0;
  if (data == nullptr || bytes == 0 || m_instance_buffer == 0 ||
      bytes > k_max_ring_instances * sizeof(MeshInstanceGpu)) {
    return false;
  }

  glBindBuffer(GL_ARRAY_BUFFER, m_instance_buffer);
  if (m_ring_offset_bytes + bytes > m_ring_capacity_bytes) {
    while (m_ring_capacity_bytes < bytes) {
      m_ring_capacity_bytes *= 2U;
    }
    m_ring_capacity_bytes = std::min(m_ring_capacity_bytes * 2U,
                                     k_max_ring_instances * sizeof(MeshInstanceGpu));
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(m_ring_capacity_bytes),
                 nullptr,
                 GL_STREAM_DRAW);
    note_buffer_storage(static_cast<std::size_t>(m_ring_capacity_bytes), false);
    m_ring_offset_bytes = 0;
  }

  void* mapped = glMapBufferRange(GL_ARRAY_BUFFER,
                                  static_cast<GLintptr>(m_ring_offset_bytes),
                                  static_cast<GLsizeiptr>(bytes),
                                  GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT |
                                      GL_MAP_UNSYNCHRONIZED_BIT);
  if (mapped != nullptr) {
    note_mapped_buffer_range(bytes);
    std::memcpy(mapped, data, bytes);
    glUnmapBuffer(GL_ARRAY_BUFFER);
  } else {
    glBufferSubData(GL_ARRAY_BUFFER,
                    static_cast<GLintptr>(m_ring_offset_bytes),
                    static_cast<GLsizeiptr>(bytes),
                    data);
  }
  note_buffer_transfer(bytes);

  byte_offset = m_ring_offset_bytes;
  m_ring_offset_bytes += bytes;
  return true;
}

void MeshInstancingPipeline::point_instance_attributes(std::size_t byte_offset) {
  const auto stride = static_cast<GLsizei>(sizeof(MeshInstanceGpu));
  for (GLuint slot = 0; slot < k_instance_vec4_count; ++slot) {
    glVertexAttribPointer(k_first_instance_loc + slot,
                          4,
                          GL_FLOAT,
                          GL_FALSE,
                          stride,
                          reinterpret_cast<void*>(static_cast<std::uintptr_t>(
                              byte_offset + slot * 4U * sizeof(float))));
  }
}

} // namespace Render::GL::BackendPipelines
