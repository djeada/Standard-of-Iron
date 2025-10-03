#include "patrol_flags.h"
#include "flag.h"
#include "../scene_renderer.h"
#include "../gl/resources.h"
#include "../../game/core/world.h"
#include "../../game/core/component.h"
#include <unordered_set>

namespace Render::GL {

void renderPatrolFlags(
    Renderer* renderer, 
    ResourceManager* resources, 
    Engine::Core::World& world,
    const std::optional<QVector3D>& previewWaypoint
) {
    if (!renderer || !resources) return;
    
    // Track unique waypoint positions to avoid rendering duplicate flags
    // when multiple units patrol the same route
    std::unordered_set<uint64_t> renderedPositions;
    
    // Render preview waypoint (first waypoint during placement) with slightly different appearance
    if (previewWaypoint.has_value()) {
        auto flag = Geom::Flag::create(
            previewWaypoint->x(),
            previewWaypoint->z(),
            QVector3D(0.3f, 1.0f, 0.4f),  // Brighter green for preview
            QVector3D(0.3f, 0.2f, 0.1f),  // Dark wood pole
            0.9f                           // Slightly larger to indicate it's temporary
        );
        
        renderer->mesh(resources->unit(), flag.pole, flag.poleColor, resources->white(), 0.8f);      // Slightly transparent
        renderer->mesh(resources->unit(), flag.pennant, flag.pennantColor, resources->white(), 0.8f);
        renderer->mesh(resources->unit(), flag.finial, flag.pennantColor, resources->white(), 0.8f);
        
        // Mark this position as rendered to avoid duplicates
        int32_t gridX = static_cast<int32_t>(previewWaypoint->x() * 10.0f);
        int32_t gridZ = static_cast<int32_t>(previewWaypoint->z() * 10.0f);
        uint64_t posHash = (static_cast<uint64_t>(gridX) << 32) | static_cast<uint64_t>(gridZ);
        renderedPositions.insert(posHash);
    }
    
    auto patrolEntities = world.getEntitiesWith<Engine::Core::PatrolComponent>();
    
    for (auto* entity : patrolEntities) {
        auto* patrol = entity->getComponent<Engine::Core::PatrolComponent>();
        if (!patrol || !patrol->patrolling || patrol->waypoints.empty()) continue;
        
        // Only render flags for alive units
        auto* unit = entity->getComponent<Engine::Core::UnitComponent>();
        if (!unit || unit->health <= 0) continue;
        
        // Render flags at each waypoint
        for (const auto& waypoint : patrol->waypoints) {
            // Create a simple hash for position deduplication (grid-snapped to 0.1 units)
            int32_t gridX = static_cast<int32_t>(waypoint.first * 10.0f);
            int32_t gridZ = static_cast<int32_t>(waypoint.second * 10.0f);
            uint64_t posHash = (static_cast<uint64_t>(gridX) << 32) | static_cast<uint64_t>(gridZ);
            
            // Skip if we've already rendered a flag at this position
            if (renderedPositions.find(posHash) != renderedPositions.end()) {
                continue;
            }
            renderedPositions.insert(posHash);
            
            // Create green patrol flag (different from yellow rally flags)
            auto flag = Geom::Flag::create(
                waypoint.first,
                waypoint.second,
                QVector3D(0.2f, 0.9f, 0.3f),  // Green flag for patrol
                QVector3D(0.3f, 0.2f, 0.1f),  // Dark wood pole
                0.8f                           // Slightly smaller than rally flags
            );
            
            // Submit flag geometry to renderer
            renderer->mesh(resources->unit(), flag.pole, flag.poleColor, resources->white(), 1.0f);
            renderer->mesh(resources->unit(), flag.pennant, flag.pennantColor, resources->white(), 1.0f);
            renderer->mesh(resources->unit(), flag.finial, flag.pennantColor, resources->white(), 1.0f);
        }
    }
}

} // namespace Render::GL
