#pragma once

#include <memory>

class QOpenGLContext;

namespace Render {
namespace GL {
class Renderer;
class Camera;
class ResourceManager;

class RenderBootstrap {
public:
  static bool initialize(Renderer &renderer, Camera &camera);
};

} // namespace GL
} // namespace Render
