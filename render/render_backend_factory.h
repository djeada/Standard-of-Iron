#pragma once

#include <memory>

#include "i_render_backend.h"

namespace Render {

class RenderBackendFactory {
public:
  static auto create(ShaderQuality quality) -> std::unique_ptr<GL::IRenderBackend>;
};

} // namespace Render
