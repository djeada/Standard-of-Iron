#pragma once

#include <QString>

namespace Render::GL {
class Renderer;
class Camera;
class GroundRenderer;
} // namespace Render::GL

namespace Game::Map {

class WorldBootstrap {
public:
  static auto initialize(Render::GL::Renderer &renderer,
                         Render::GL::Camera &camera,
                         Render::GL::GroundRenderer *ground = nullptr,
                         QString *out_error = nullptr) -> bool;

  static void ensureInitialized(bool &initialized,
                                Render::GL::Renderer &renderer,
                                Render::GL::Camera &camera,
                                Render::GL::GroundRenderer *ground = nullptr,
                                QString *out_error = nullptr);
};

} // namespace Game::Map
