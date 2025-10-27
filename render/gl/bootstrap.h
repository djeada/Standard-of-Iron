#pragma once

#include <memory>

class QOpenGLContext;

namespace Render::GL {
class Renderer;
class Camera;
class ResourceManager;

class RenderBootstrap {
public:
  static auto initialize(Renderer &renderer, Camera &camera) -> bool;
};

} // namespace Render::GL
