#include "render/gl/rigged_mesh_gpu_table.h"

#include <QDebug>

#include "render/geom/rigged_mesh.h"
#include "render/gl/draw_tally.h"
#include "render/gl/gl_lifetime.h"

namespace Render::GL {

RiggedMeshGpuTable::RiggedMeshGpuTable() = default;
RiggedMeshGpuTable::~RiggedMeshGpuTable() = default;

auto RiggedMeshGpuTable::entry_for(const RiggedMesh& mesh) -> Entry* {
  if (auto it = m_entries.find(mesh.id()); it != m_entries.end()) {
    return &it->second;
  }
  if (!gl_objects_can_be_released()) {
    qWarning() << "RiggedMeshGpuTable: no current GL context; skipping VAO/VBO "
                  "creation";
    return nullptr;
  }
  if (!m_gpu_ready) {
    m_gpu.initialize();
    m_gpu_ready = true;
  }
  if (const auto stale = m_gpu.drain_gl_errors(); stale != 0U) {
    qWarning() << "RiggedMeshGpuTable: inherited GL error" << stale
               << "from an earlier call";
  }

  Entry entry;
  entry.lifetime = mesh.lifetime();
  entry.vao = std::make_unique<VertexArray>();
  entry.vbo = std::make_unique<Buffer>(Buffer::Type::Vertex);
  entry.ebo = std::make_unique<Buffer>(Buffer::Type::Index);

  entry.vao->bind();
  entry.vbo->set_data(mesh.get_vertices());
  entry.ebo->set_data(mesh.get_indices());
  m_gpu.configure_vertex_attributes();
  entry.vao->unbind();

  if (const auto err = m_gpu.current_gl_error(); err != 0U) {
    qWarning() << "RiggedMeshGpuTable: upload GL error" << err;
  }
  return &m_entries.emplace(mesh.id(), std::move(entry)).first->second;
}

auto RiggedMeshGpuTable::ensure(const RiggedMesh& mesh) -> bool {
  const Entry* entry = entry_for(mesh);
  return entry != nullptr && entry->vbo != nullptr && entry->ebo != nullptr;
}

void RiggedMeshGpuTable::draw(const RiggedMesh& mesh) {
  Entry* entry = entry_for(mesh);
  if (entry == nullptr || !gl_objects_can_be_released()) {
    return;
  }
  entry->vao->bind();
  m_gpu.draw_triangles(mesh.index_count());
  Render::GL::tally_draw(mesh.index_count());
  entry->vao->unbind();

#ifndef NDEBUG
  if (const auto err = m_gpu.current_gl_error(); err != 0U) {
    qWarning() << "RiggedMeshGpuTable::draw GL error" << err << "indices"
               << mesh.index_count();
  }
#endif
}

auto RiggedMeshGpuTable::vertex_buffer(const RiggedMesh& mesh) -> Buffer* {
  auto it = m_entries.find(mesh.id());
  return it != m_entries.end() ? it->second.vbo.get() : nullptr;
}

auto RiggedMeshGpuTable::index_buffer(const RiggedMesh& mesh) -> Buffer* {
  auto it = m_entries.find(mesh.id());
  return it != m_entries.end() ? it->second.ebo.get() : nullptr;
}

void RiggedMeshGpuTable::collect_retired() {
  for (auto it = m_entries.begin(); it != m_entries.end();) {
    if (it->second.lifetime.expired()) {
      it = m_entries.erase(it);
    } else {
      ++it;
    }
  }
}

} // namespace Render::GL
