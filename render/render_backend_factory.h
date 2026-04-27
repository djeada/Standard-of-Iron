#pragma once

#include "i_render_backend.h"
#include <memory>

namespace Render {

class RenderBackendFactory {
public:
  static auto
  create(ShaderQuality quality) -> std::unique_ptr<GL::IRenderBackend>;
};

} // namespace Render
