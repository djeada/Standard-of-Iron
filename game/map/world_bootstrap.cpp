#include "world_bootstrap.h"
#include "render/gl/bootstrap.h"
#include "render/ground/ground_renderer.h"

namespace Game::Map {

auto WorldBootstrap::initialize(Render::GL::Renderer &renderer,
                                Render::GL::Camera &camera,
                                Render::GL::GroundRenderer *ground,
                                QString *out_error) -> bool {
  if (!Render::GL::RenderBootstrap::initialize(renderer, camera)) {
    if (out_error != nullptr) {
      *out_error = "Failed to initialize OpenGL renderer";
    }
    return false;
  }

  if (ground != nullptr) {
    ground->configureExtent(50.0F);
  }

  return true;
}

void WorldBootstrap::ensureInitialized(bool &initialized,
                                       Render::GL::Renderer &renderer,
                                       Render::GL::Camera &camera,
                                       Render::GL::GroundRenderer *ground,
                                       QString *out_error) {
  if (!initialized) {
    initialized = initialize(renderer, camera, ground, out_error);
  }
}

} // namespace Game::Map
