#include "render/render_backend_factory.h"

#include "render/gl/backend.h"
#include "render/software_backend.h"

namespace Render {

auto RenderBackendFactory::create(ShaderQuality quality)
    -> std::unique_ptr<GL::IRenderBackend> {
  if (quality == ShaderQuality::None) {
    return std::make_unique<GL::SoftwareBackend>();
  }
  return std::make_unique<GL::Backend>(quality);
}

} // namespace Render
