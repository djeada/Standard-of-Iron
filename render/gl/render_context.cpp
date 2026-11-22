#include "render/gl/render_context.h"

#include <array>

#include "render/gl/mesh_prewarmer.h"
#include "render/gl/resources.h"
#include "render/gl/shader_cache.h"

namespace Render::GL {

RenderContext::RenderContext() = default;

RenderContext::~RenderContext() = default;

auto RenderContext::initialize() -> bool {
  m_resources = std::make_unique<ResourceManager>();
  if (!m_resources->initialize()) {
    return false;
  }
  std::array shared{m_resources->quad(), m_resources->ground(), m_resources->unit()};
  if (!prewarm_mesh_buffers(m_meshes, shared, [](const Mesh* mesh) { return mesh; })) {
    return false;
  }
  m_shaders = std::make_unique<ShaderCache>();
  m_shaders->initialize_defaults();
  return true;
}

} // namespace Render::GL
