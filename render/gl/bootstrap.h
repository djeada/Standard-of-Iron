#pragma once

#include <memory>

class QOpenGLContext;

namespace Render { namespace GL {
class Renderer;
class Camera;
class ResourceManager;

class RenderBootstrap {
public:
    // Initializes GL renderer and binds camera. Returns false if no valid GL context or renderer init fails.
    static bool initialize(Renderer& renderer,
                           Camera& camera);
};

} } // namespace Render::GL
