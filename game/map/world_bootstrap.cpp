#include "world_bootstrap.h"
#include "render/gl/bootstrap.h"
#include "render/ground/ground_renderer.h"

namespace Game {
namespace Map {

bool WorldBootstrap::initialize(Render::GL::Renderer &renderer,
                                Render::GL::Camera &camera,
                                Render::GL::GroundRenderer *ground,
                                QString *outError) {
  if (!Render::GL::RenderBootstrap::initialize(renderer, camera)) {
    if (outError) {
      *outError = "Failed to initialize OpenGL renderer";
    }
    return false;
  }

  if (ground) {
    ground->configureExtent(50.0f);
  }

  return true;
}

void WorldBootstrap::ensureInitialized(bool &initialized,
                                       Render::GL::Renderer &renderer,
                                       Render::GL::Camera &camera,
                                       Render::GL::GroundRenderer *ground,
                                       QString *outError) {
  if (!initialized) {
    initialized = initialize(renderer, camera, ground, outError);
  }
}

} // namespace Map
} // namespace Game
