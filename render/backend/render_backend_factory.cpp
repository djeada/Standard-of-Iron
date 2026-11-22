#include "render/backend/render_backend_factory.h"

#include "render/backend/backend.h"
#include "render/backend/software_backend.h"

namespace Render {

auto RenderBackendFactory::create(ShaderQuality quality)
    -> std::unique_ptr<GL::IRenderBackend> {
  if (quality == ShaderQuality::None) {
    return std::make_unique<GL::SoftwareBackend>();
  }
  return std::make_unique<GL::Backend>(quality);
}

} // namespace Render
