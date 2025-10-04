#pragma once

#include <QVector3D>
#include <optional>

namespace Engine::Core { class World; }
namespace Render::GL { 
    class Renderer; 
    class ResourceManager;
}

namespace Render::GL {

/**
 * Render patrol waypoint flags for all units with active patrol routes.
 * Called during the rendering phase to visualize patrol waypoints.
 * 
 * @param previewWaypoint Optional first waypoint being placed (shows during placement)
 */
void renderPatrolFlags(
    Renderer* renderer, 
    ResourceManager* resources, 
    Engine::Core::World& world,
    const std::optional<QVector3D>& previewWaypoint = std::nullopt
);

} // namespace Render::GL
