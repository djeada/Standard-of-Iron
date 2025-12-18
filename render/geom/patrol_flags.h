#pragma once

#include <QVector3D>
#include <optional>

namespace Engine::Core {
class World;
}
namespace Render::GL {
class Renderer;
class ResourceManager;
} 

namespace Render::GL {

void renderPatrolFlags(
    Renderer *renderer, ResourceManager *resources, Engine::Core::World &world,
    const std::optional<QVector3D> &preview_waypoint = std::nullopt);

}
