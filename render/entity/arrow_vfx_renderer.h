#pragma once

#include <QMatrix4x4>
#include <QVector3D>

namespace Render {
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

namespace Render::GL {

void render_arrows(Renderer *renderer, ResourceManager *resources,
                   const Game::Systems::ArrowSystem &arrow_system);

}
