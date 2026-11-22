#pragma once

#include <memory>

#include "render/gl/gl_capabilities.h"

class QOpenGLContext;

namespace Render::GL {
class Renderer;
class Camera;
class DrawResources;

class RenderBootstrap {
public:
  static auto initialize(Renderer& renderer, Camera& camera) -> bool;

  static void remember_adapter(const GLCapabilities::AdapterDescription& adapter);
  [[nodiscard]] static auto adapter() -> const GLCapabilities::AdapterDescription&;
};

} // namespace Render::GL
