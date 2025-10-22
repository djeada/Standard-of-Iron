#include "patrol_flags.h"
#include "../../game/core/component.h"
#include "../../game/core/world.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "flag.h"
#include <unordered_set>

namespace Render::GL {

void renderPatrolFlags(Renderer *renderer, ResourceManager *resources,
                       Engine::Core::World &world,
                       const std::optional<QVector3D> &previewWaypoint) {
  if (!renderer || !resources)
    return;

  std::unordered_set<uint64_t> renderedPositions;

  if (previewWaypoint.has_value()) {
    auto flag = Geom::Flag::create(previewWaypoint->x(), previewWaypoint->z(),
                                   QVector3D(0.3f, 1.0f, 0.4f),
                                   QVector3D(0.3f, 0.2f, 0.1f), 0.9f);

    renderer->mesh(resources->unit(), flag.pole, flag.poleColor,
                   resources->white(), 0.8f);
    renderer->mesh(resources->unit(), flag.pennant, flag.pennantColor,
                   resources->white(), 0.8f);
    renderer->mesh(resources->unit(), flag.finial, flag.pennantColor,
                   resources->white(), 0.8f);

    int32_t gridX = static_cast<int32_t>(previewWaypoint->x() * 10.0f);
    int32_t gridZ = static_cast<int32_t>(previewWaypoint->z() * 10.0f);
    uint64_t posHash =
        (static_cast<uint64_t>(gridX) << 32) | static_cast<uint64_t>(gridZ);
    renderedPositions.insert(posHash);
  }

  auto patrolEntities = world.getEntitiesWith<Engine::Core::PatrolComponent>();

  for (auto *entity : patrolEntities) {
    auto *patrol = entity->getComponent<Engine::Core::PatrolComponent>();
    if (!patrol || !patrol->patrolling || patrol->waypoints.empty())
      continue;

    auto *unit = entity->getComponent<Engine::Core::UnitComponent>();
    if (!unit || unit->health <= 0)
      continue;

    for (const auto &waypoint : patrol->waypoints) {

      int32_t gridX = static_cast<int32_t>(waypoint.first * 10.0f);
      int32_t gridZ = static_cast<int32_t>(waypoint.second * 10.0f);
      uint64_t posHash =
          (static_cast<uint64_t>(gridX) << 32) | static_cast<uint64_t>(gridZ);

      if (!renderedPositions.insert(posHash).second) {
        continue;
      }

      auto flag = Geom::Flag::create(waypoint.first, waypoint.second,
                                     QVector3D(0.2f, 0.9f, 0.3f),
                                     QVector3D(0.3f, 0.2f, 0.1f), 0.8f);

      renderer->mesh(resources->unit(), flag.pole, flag.poleColor,
                     resources->white(), 1.0f);
      renderer->mesh(resources->unit(), flag.pennant, flag.pennantColor,
                     resources->white(), 1.0f);
      renderer->mesh(resources->unit(), flag.finial, flag.pennantColor,
                     resources->white(), 1.0f);
    }
  }
}

} 
