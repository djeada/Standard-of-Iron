#pragma once

#include "render/gl/mesh.h"
#include "render/gl/mesh_gpu_table.h"

namespace Render::GL {

// Uploads every mesh in `meshes` into `table` ahead of the first frame that
// draws it. False if any could not be uploaded (no context, or a GL error).
template <typename Range, typename MeshOf>
[[nodiscard]] auto
prewarm_mesh_buffers(MeshGpuTable& table, Range& meshes, MeshOf mesh_of) -> bool {
  bool ready = true;
  for (auto& entry : meshes) {
    const Mesh* mesh = mesh_of(entry);
    if (mesh == nullptr) {
      continue;
    }
    ready = table.prewarm(*mesh) && ready;
  }
  return ready;
}

} // namespace Render::GL
