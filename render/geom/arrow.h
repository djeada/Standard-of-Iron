
#pragma once
#include "../gl/mesh.h"
#include <QMatrix4x4>
#include <QVector3D>

namespace Render {
namespace Geom {
class Arrow {
public:
  static GL::Mesh *get();
};
} 

namespace GL {
class Renderer;
class ResourceManager;
} 
} 

namespace Game {
namespace Systems {
class ArrowSystem;
}
} 

namespace Render {
namespace GL {
void renderArrows(Renderer *renderer, ResourceManager *resources,
                  const Game::Systems::ArrowSystem &arrowSystem);
}
} 
