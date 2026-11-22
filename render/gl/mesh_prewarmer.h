#pragma once

#include "render/geom/mesh.h"
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

// Uploads every mesh `visit_meshes` reports into `table`. `visit_meshes` takes a
// MeshVisitor, so a domain renderer can list its meshes without seeing GL.
template <typename VisitMeshes>
[[nodiscard]] auto prewarm_meshes(MeshGpuTable& table,
                                  VisitMeshes&& visit_meshes) -> bool {
  bool ready = true;
  visit_meshes([&](const Mesh& mesh) { ready = table.prewarm(mesh) && ready; });
  return ready;
}

} // namespace Render::GL
