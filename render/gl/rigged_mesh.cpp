#include "render/gl/rigged_mesh.h"

#include <QDebug>
#include <QOpenGLContext>
#include <qopenglext.h>

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <utility>

#include "render/gl/draw_tally.h"
#include "render/gl/gl_lifetime.h"
#include "render/gl/platform_gl.h"
#include "render/profiling/asset_counters.h"

namespace Render::GL {

auto RiggedMesh::next_id() noexcept -> std::uint64_t {
  static std::atomic<std::uint64_t> counter{1};
  return counter.fetch_add(1, std::memory_order_relaxed);
}

RiggedMesh::RiggedMesh(std::vector<RiggedVertex> vertices,
                       std::vector<std::uint32_t> indices)
    : m_vertices(std::move(vertices))
    , m_indices(std::move(indices)) {
  Render::Profiling::count_asset(
      Render::Profiling::AssetCounter::RiggedMeshConstructed);
  Render::Profiling::count_asset(Render::Profiling::AssetCounter::RiggedMeshVertexBytes,
                                 (m_vertices.size() * sizeof(RiggedVertex)) +
                                     (m_indices.size() * sizeof(std::uint32_t)));
  const auto vertex_is_rigid = [this](std::uint32_t index) {
    if (index >= m_vertices.size()) {
      return false;
    }
    const auto& weights = m_vertices[index].bone_weights;
    return weights[0] == 1.0F && weights[1] == 0.0F && weights[2] == 0.0F &&
           weights[3] == 0.0F;
  };
  const auto triangle_is_rigid = [&](std::size_t offset) {
    return vertex_is_rigid(m_indices[offset]) &&
           vertex_is_rigid(m_indices[offset + 1U]) &&
           vertex_is_rigid(m_indices[offset + 2U]);
  };

  std::vector<std::uint32_t> partitioned;
  partitioned.reserve(m_indices.size());
  const std::size_t triangle_index_count = m_indices.size() - m_indices.size() % 3U;
  for (std::size_t offset = 0; offset < triangle_index_count; offset += 3U) {
    if (triangle_is_rigid(offset)) {
      partitioned.insert(partitioned.end(),
                         m_indices.begin() + static_cast<std::ptrdiff_t>(offset),
                         m_indices.begin() + static_cast<std::ptrdiff_t>(offset + 3U));
    }
  }
  m_rigid_index_count = partitioned.size();
  for (std::size_t offset = 0; offset < triangle_index_count; offset += 3U) {
    if (!triangle_is_rigid(offset)) {
      partitioned.insert(partitioned.end(),
                         m_indices.begin() + static_cast<std::ptrdiff_t>(offset),
                         m_indices.begin() + static_cast<std::ptrdiff_t>(offset + 3U));
    }
  }
  partitioned.insert(partitioned.end(),
                     m_indices.begin() +
                         static_cast<std::ptrdiff_t>(triangle_index_count),
                     m_indices.end());
  m_indices = std::move(partitioned);
}

RiggedMesh::~RiggedMesh() = default;

void RiggedMesh::setup_buffers() {
  if (!gl_objects_can_be_released()) {
    qWarning() << "RiggedMesh::setup_buffers called without current GL "
                  "context; skipping VAO/VBO creation";
    return;
  }
  m_gpu.initialize();
  if (const auto stale = m_gpu.drain_gl_errors(); stale != 0U) {
    qWarning() << "RiggedMesh::setup_buffers inherited GL error" << stale
               << "from an earlier call";
  }

  m_vao = std::make_unique<VertexArray>();
  m_vbo = std::make_unique<Buffer>(Buffer::Type::Vertex);
  m_ebo = std::make_unique<Buffer>(Buffer::Type::Index);

  m_vao->bind();
  m_vbo->set_data(m_vertices);
  m_ebo->set_data(m_indices);

  m_gpu.configure_vertex_attributes();

  m_vao->unbind();

  if (const auto err = m_gpu.current_gl_error(); err != 0U) {
    qWarning() << "RiggedMesh::setup_buffers GL error" << err;
  }
}

auto RiggedMesh::bind_vao() -> bool {
  if (!m_vao) {
    setup_buffers();
  }
  if (!m_vao || !gl_objects_can_be_released()) {
    return false;
  }
  m_vao->bind();
  return true;
}

auto RiggedMesh::ensure_gl_buffers() -> bool {
  if (!m_vao) {
    setup_buffers();
  }
  return m_vbo != nullptr && m_ebo != nullptr;
}

void RiggedMesh::unbind_vao() {
  if (m_vao) {
    m_vao->unbind();
  }
}

void RiggedMesh::draw() {
  if (!bind_vao()) {
    return;
  }
  m_gpu.draw_triangles(m_indices.size());
  Render::GL::tally_draw(m_indices.size());
  m_vao->unbind();

#ifndef NDEBUG
  if (const auto err = m_gpu.current_gl_error(); err != 0U) {
    qWarning() << "RiggedMesh::draw GL error" << err << "indices" << m_indices.size();
  }
#endif
}

} // namespace Render::GL
