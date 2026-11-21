#pragma once

#include <memory>

#include "render/i_render_backend.h"

namespace Render {

class RenderBackendFactory {
public:
  static auto create(ShaderQuality quality) -> std::unique_ptr<GL::IRenderBackend>;
};

} // namespace Render
