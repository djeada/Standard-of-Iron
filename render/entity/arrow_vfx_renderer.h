#pragma once

#include <QMatrix4x4>
#include <QVector3D>

namespace Render {
namespace GL {
class Renderer;
class ResourceManager;
} // namespace GL
} // namespace Render
namespace Game {
namespace Systems {
class ArrowSystem;
}
} // namespace Game

namespace Render::GL {

void renderArrows(Renderer *renderer, ResourceManager *resources,
                  const Game::Systems::ArrowSystem &arrow_system);

}
