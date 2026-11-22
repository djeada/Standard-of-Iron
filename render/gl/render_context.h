#pragma once

#include <memory>

#include "render/gl/instance_buffer_table.h"
#include "render/gl/mesh_gpu_table.h"
#include "render/gl/rigged_mesh_gpu_table.h"
#include "render/gl/texture_table.h"

namespace Render::GL {

class ResourceManager;
class ShaderCache;

// The GPU state one backend owns: shared textures and meshes, compiled
// shaders, and its copies of every Mesh, InstanceBlock and TextureImage it has
// drawn. It is created on the
// backend's context and destroyed with the backend, so two backends in one
// process never share GL objects and nothing outlives the context that made it.
class RenderContext {
public:
  RenderContext();
  ~RenderContext();

  RenderContext(const RenderContext&) = delete;
  auto operator=(const RenderContext&) -> RenderContext& = delete;
  RenderContext(RenderContext&&) = delete;
  auto operator=(RenderContext&&) -> RenderContext& = delete;

  // Creates the resources and loads the default shaders. Needs a current
  // context. False if the resources could not be created.
  auto initialize() -> bool;

  [[nodiscard]] auto resources() const noexcept -> ResourceManager* {
    return m_resources.get();
  }
  [[nodiscard]] auto shaders() const noexcept -> ShaderCache* {
    return m_shaders.get();
  }
  [[nodiscard]] auto meshes() noexcept -> MeshGpuTable& { return m_meshes; }
  [[nodiscard]] auto instances() noexcept -> InstanceBufferTable& {
    return m_instances;
  }
  [[nodiscard]] auto textures() noexcept -> TextureTable& { return m_textures; }
  [[nodiscard]] auto rigged_meshes() noexcept -> RiggedMeshGpuTable& {
    return m_rigged_meshes;
  }

private:
  // Destroyed bottom-up: textures, instance buffers and uploaded meshes, then
  // shaders,
  // then shared resources.
  std::unique_ptr<ResourceManager> m_resources;
  std::unique_ptr<ShaderCache> m_shaders;
  MeshGpuTable m_meshes;
  InstanceBufferTable m_instances;
  TextureTable m_textures;
  RiggedMeshGpuTable m_rigged_meshes;
};

} // namespace Render::GL
