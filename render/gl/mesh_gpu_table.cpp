#include "render/gl/mesh_gpu_table.h"

#include <QDebug>

#include <cstddef>
#include <sstream>

#include "render/gl/draw_tally.h"
#include "render/gl/gl_lifetime.h"
#include "render/gl/mesh.h"
#include "render/gl/platform_gl.h"
#include "render/gl/vertex_attrib_layout.h"
#include "render/profiling/gl_creation_trace.h"

namespace Render::GL {

MeshGpuTable::MeshGpuTable() = default;

MeshGpuTable::~MeshGpuTable() = default;

auto MeshGpuTable::upload(const Mesh& mesh, Entry& entry) -> bool {
  if (!gl_objects_can_be_released()) {
    qWarning() << "MeshGpuTable: upload requested without a current GL context; "
                  "skipping VAO/VBO creation";
    return false;
  }
  initializeOpenGLFunctions();
  const auto& vertices = mesh.get_vertices();
  const auto& indices = mesh.get_indices();
  bool reported_stale = false;
  for (GLenum stale = glGetError(); stale != GL_NO_ERROR; stale = glGetError()) {
    if (!reported_stale) {
      reported_stale = true;
      qWarning() << "MeshGpuTable: upload inherited GL error" << stale
                 << "from an earlier call";
    }
  }
  if (Render::Profiling::gl_creation_trace_enabled()) {
    std::ostringstream fingerprint;
    fingerprint << "vertices=" << vertices.size() << " indices=" << indices.size()
                << " radius=" << mesh.bounds_radius();
    Render::Profiling::record_mesh_upload(fingerprint.str());
  }
  entry.instance_layout_ready = false;
  entry.lifetime = mesh.lifetime();
  entry.vao = std::make_unique<VertexArray>();
  entry.vbo = std::make_unique<Buffer>(Buffer::Type::Vertex);
  entry.ebo = std::make_unique<Buffer>(Buffer::Type::Index);

  entry.vao->bind();
  entry.vbo->set_data(vertices);
  entry.ebo->set_data(indices);

  constexpr GLsizei k_stride = sizeof(Vertex);
  constexpr auto offset_of = [](auto member_ptr) -> std::size_t {
    return reinterpret_cast<std::size_t>(
        &(reinterpret_cast<Vertex const*>(0)->*member_ptr));
  };
  auto const pos_off = offset_of(&Vertex::position);
  auto const norm_off = offset_of(&Vertex::normal);
  auto const tex_off = offset_of(&Vertex::tex_coord);

  apply_vertex_attrib_layout({{0, 3, GL_FLOAT, GL_FALSE, k_stride, pos_off},
                              {1, 3, GL_FLOAT, GL_FALSE, k_stride, norm_off},
                              {2, 2, GL_FLOAT, GL_FALSE, k_stride, tex_off}});
  entry.vao->set_index_buffer(*entry.ebo);

  entry.vao->unbind();

  GLenum const err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "MeshGpuTable: upload GL error" << err;
  }
  return true;
}

auto MeshGpuTable::prepare(const Mesh& mesh, const char* caller_name) -> Entry* {
  Entry& entry = m_entries[mesh.id()];
  if (entry.vao && entry.vao->id() != 0U) {
    const GlShareGroup owner = entry.vao->share_group();
    const GlShareGroup current = current_gl_share_group();
    if (owner != k_unknown_share_group && current != k_unknown_share_group &&
        owner != current) {
      entry.vao.reset();
      entry.vbo.reset();
      entry.ebo.reset();
    }
  }
  if (!entry.vao) {
    (void)upload(mesh, entry);
  }
  if (!entry.vao) {
    m_entries.erase(mesh.id());
    qWarning() << caller_name << "has no vertex array; skipping draw" << "indices"
               << mesh.get_indices().size() << "context"
               << (gl_objects_can_be_released() ? "current" : "absent");
    return nullptr;
  }
  entry.vao->bind();
  initializeOpenGLFunctions();

#ifndef NDEBUG
  GLenum const pre_err = glGetError();
  if (pre_err != GL_NO_ERROR) {
    qWarning() << caller_name << "pre-draw GL error" << pre_err << "vao"
               << entry.vao->id() << "indices" << mesh.get_indices().size();
  }
#endif
  return &entry;
}

auto MeshGpuTable::bind(const Mesh& mesh) -> bool {
  return prepare(mesh, "MeshGpuTable::bind") != nullptr;
}

void MeshGpuTable::unbind() {
  if (!gl_objects_can_be_released()) {
    return;
  }
  initializeOpenGLFunctions();
  glBindVertexArray(0);
}

void MeshGpuTable::draw_bound(const Mesh& mesh, std::size_t instance_count) {
  const auto index_count = static_cast<GLsizei>(mesh.get_indices().size());
  if (instance_count == 1U) {
    glDrawElements(GL_TRIANGLES, index_count, GL_UNSIGNED_INT, nullptr);
  } else {
    glDrawElementsInstanced(GL_TRIANGLES,
                            index_count,
                            GL_UNSIGNED_INT,
                            nullptr,
                            static_cast<GLsizei>(instance_count));
  }
  tally_draw(mesh.get_indices().size(), instance_count);
#ifndef NDEBUG
  GLenum const err = glGetError();
  if (err != GL_NO_ERROR) {
    qWarning() << "MeshGpuTable::draw_bound GL error" << err << "indices"
               << mesh.get_indices().size() << "instances" << instance_count;
  }
#endif
}

void MeshGpuTable::draw(const Mesh& mesh) {
  if (prepare(mesh, "MeshGpuTable::draw") == nullptr) {
    return;
  }
  draw_bound(mesh);
  glBindVertexArray(0);
}

auto MeshGpuTable::claim_instance_layout(const Mesh& mesh) -> bool {
  const auto it = m_entries.find(mesh.id());
  if (it == m_entries.end()) {
    return false;
  }
  return !std::exchange(it->second.instance_layout_ready, true);
}

auto MeshGpuTable::prewarm(const Mesh& mesh) -> bool {
  if (prepare(mesh, "MeshGpuTable::prewarm") == nullptr) {
    return false;
  }
  glBindVertexArray(0);
  return true;
}

void MeshGpuTable::collect_retired() {
  for (auto it = m_entries.begin(); it != m_entries.end();) {
    if (it->second.lifetime.expired()) {
      it = m_entries.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace Render::GL
