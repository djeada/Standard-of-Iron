
#pragma once
#include "../gl/mesh.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render {
namespace Geom {
class Arrow {
public:
    static GL::Mesh* get();
};
} // namespace Geom

namespace GL {
class Renderer;
class ResourceManager;
} // namespace GL
} // namespace Render

namespace Game {
namespace Systems {
class ArrowSystem;
} // namespace Systems
} // namespace Game

namespace Render {
namespace GL {
void renderArrows(Renderer* renderer,
                  ResourceManager* resources,
                  const Game::Systems::ArrowSystem& arrowSystem);
} // namespace GL
} // namespace Render
