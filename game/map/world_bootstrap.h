#pragma once

namespace Render {
namespace GL {
class Renderer;
class Camera;
class GroundRenderer;
}
}

namespace Game {
namespace Map {

class WorldBootstrap {
public:
  static bool initialize(Render::GL::Renderer &renderer,
                         Render::GL::Camera &camera,
                         Render::GL::GroundRenderer *ground = nullptr);
  
  static void ensureInitialized(bool &initialized,
                                Render::GL::Renderer &renderer,
                                Render::GL::Camera &camera,
                                Render::GL::GroundRenderer *ground = nullptr);
};

}
}
