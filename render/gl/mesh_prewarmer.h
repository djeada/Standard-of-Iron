#pragma once

#include <QOpenGLContext>

#include "render/gl/mesh.h"

namespace Render::GL {

template <typename Range, typename MeshOf>
[[nodiscard]] auto prewarm_mesh_buffers(Range& meshes, MeshOf mesh_of) -> bool {
  if (QOpenGLContext::currentContext() == nullptr) {
    return false;
  }
  bool ready = true;
  for (auto& entry : meshes) {
    auto* mesh = mesh_of(entry);
    if (mesh == nullptr) {
      continue;
    }
    if (mesh->bind_vao()) {
      mesh->unbind_vao();
    } else {
      ready = false;
    }
  }
  return ready;
}

} // namespace Render::GL
