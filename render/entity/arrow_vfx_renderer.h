#pragma once

#include <QMatrix4x4>
#include <QVector3D>

namespace Render { namespace GL { class Renderer; class ResourceManager; } }
namespace Game { namespace Systems { class ArrowSystem; } }

namespace Render::GL {

// Draws projectile arrows from ArrowSystem using the shared Renderer/ResourceManager.
void renderArrows(Renderer* renderer, ResourceManager* resources, const Game::Systems::ArrowSystem& arrowSystem);

} // namespace Render::GL
