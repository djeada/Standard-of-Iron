
#pragma once
#include "../gl/mesh.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render {
namespace Geom {
class Arrow {
public:
  static auto get() -> GL::Mesh *;
};
} // namespace Geom

namespace GL {
class Renderer;
class ResourceManager;
} // namespace GL
} // namespace Render

namespace Game::Systems {
class ArrowSystem;
}

namespace Render::GL {
void render_arrows(Renderer *renderer, ResourceManager *resources,
                  const Game::Systems::ArrowSystem &arrow_system);
}
