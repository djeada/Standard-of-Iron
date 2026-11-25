#pragma once

#include "render/geom/mesh.h"
#include "render/gl/mesh_gpu_table.h"

namespace Render::GL {

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

template <typename VisitMeshes>
[[nodiscard]] auto prewarm_meshes(MeshGpuTable& table,
                                  VisitMeshes&& visit_meshes) -> bool {
  bool ready = true;
  visit_meshes([&](const Mesh& mesh) { ready = table.prewarm(mesh) && ready; });
  return ready;
}

} // namespace Render::GL
