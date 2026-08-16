#pragma once

#include "game/map/map_definition.h"

namespace Render::GL {
class Renderer;
class Camera;
} // namespace Render::GL

namespace App::Core {

struct Environment {
  static void apply(const Game::Map::MapDefinition& def,
                    Render::GL::Renderer& renderer,
                    Render::GL::Camera& camera);
  static void apply_default(Render::GL::Renderer& renderer, Render::GL::Camera& camera);
};

} // namespace App::Core
