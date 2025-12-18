#pragma once

#include "map_definition.h"

namespace Render::GL {
class Renderer;
class Camera;
} 

namespace Game::Map {

struct Environment {
  static void apply(const MapDefinition &def, Render::GL::Renderer &renderer,
                    Render::GL::Camera &camera);
  static void applyDefault(Render::GL::Renderer &renderer,
                           Render::GL::Camera &camera);
};

} 
