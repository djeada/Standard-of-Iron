#pragma once

#include "map_definition.h"

namespace Render {
namespace GL {
class Renderer;
class Camera;
} // namespace GL
} // namespace Render

namespace Game {
namespace Map {

struct Environment {
  static void apply(const MapDefinition &def, Render::GL::Renderer &renderer,
                    Render::GL::Camera &camera);
  static void applyDefault(Render::GL::Renderer &renderer,
                           Render::GL::Camera &camera);
};

} // namespace Map
} // namespace Game
