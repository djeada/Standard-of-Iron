#include "render_backend_factory.h"

#include "gl/backend.h"
#include "software_backend.h"

namespace Render {

auto RenderBackendFactory::create(ShaderQuality quality)
    -> std::unique_ptr<GL::IRenderBackend> {
  if (quality == ShaderQuality::None) {
    return std::make_unique<GL::SoftwareBackend>();
  }
  return std::make_unique<GL::Backend>(quality);
}

} // namespace Render
