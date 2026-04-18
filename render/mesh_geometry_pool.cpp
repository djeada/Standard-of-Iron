#include "mesh_geometry_pool.h"
#include <QOpenGLContext>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLVersionFunctionsFactory>

namespace Render {

void MeshGeometryPool::upload_entry(PooledMeshEntry &entry) {
  auto *gl = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(
      QOpenGLContext::currentContext());
  if (gl == nullptr) return;

  gl->glGenVertexArrays(1, &entry.info.vao);
  gl->glGenBuffers(1, &entry.info.vbo);

  gl->glBindVertexArray(entry.info.vao);
  gl->glBindBuffer(GL_ARRAY_BUFFER, entry.info.vbo);
  gl->glBufferData(GL_ARRAY_BUFFER,
                   static_cast<GLsizeiptr>(entry.vertices.size() * sizeof(PooledVertex)),
                   entry.vertices.data(), GL_STATIC_DRAW);

  // Position: location 0
  gl->glEnableVertexAttribArray(0);
  gl->glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(PooledVertex),
                            reinterpret_cast<const void *>(offsetof(PooledVertex, position)));
  // Normal: location 1
  gl->glEnableVertexAttribArray(1);
  gl->glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(PooledVertex),
                            reinterpret_cast<const void *>(offsetof(PooledVertex, normal)));
  // Texcoord: location 2
  gl->glEnableVertexAttribArray(2);
  gl->glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(PooledVertex),
                            reinterpret_cast<const void *>(offsetof(PooledVertex, texcoord)));

  if (!entry.indices.empty()) {
    gl->glGenBuffers(1, &entry.info.ebo);
    gl->glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, entry.info.ebo);
    gl->glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(entry.indices.size() * sizeof(std::uint32_t)),
                     entry.indices.data(), GL_STATIC_DRAW);
  }

  gl->glBindVertexArray(0);
  entry.info.uploaded = true;
}

void MeshGeometryPool::release_gpu_resources() {
  auto *ctx = QOpenGLContext::currentContext();
  if (ctx == nullptr) return;

  auto *gl = QOpenGLVersionFunctionsFactory::get<QOpenGLFunctions_3_3_Core>(ctx);
  if (gl == nullptr) return;

  std::lock_guard lock(m_mutex);
  for (auto &[id, entry] : m_entries) {
    if (entry.info.uploaded) {
      if (entry.info.vao != 0) gl->glDeleteVertexArrays(1, &entry.info.vao);
      if (entry.info.vbo != 0) gl->glDeleteBuffers(1, &entry.info.vbo);
      if (entry.info.ebo != 0) gl->glDeleteBuffers(1, &entry.info.ebo);
      entry.info.vao = 0;
      entry.info.vbo = 0;
      entry.info.ebo = 0;
      entry.info.uploaded = false;
    }
  }
}

} // namespace Render
