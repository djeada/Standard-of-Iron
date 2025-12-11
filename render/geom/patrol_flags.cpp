#include "patrol_flags.h"
#include "../../game/core/component.h"
#include "../../game/core/world.h"
#include "../gl/resources.h"
#include "../scene_renderer.h"
#include "flag.h"
#include <cstdint>
#include <optional>
#include <qvectornd.h>
#include <unordered_set>

namespace Render::GL {

constexpr float k_position_grid_precision = 10.0F;
constexpr int k_position_hash_shift = 32;

void renderPatrolFlags(Renderer *renderer, ResourceManager *resources,
                       Engine::Core::World &world,
                       const std::optional<QVector3D> &preview_waypoint) {
  if ((renderer == nullptr) || (resources == nullptr)) {
    return;
  }

  std::unordered_set<uint64_t> rendered_positions;

  if (preview_waypoint.has_value()) {
    auto flag = Geom::Flag::create(preview_waypoint->x(), preview_waypoint->z(),
                                   QVector3D(0.3F, 1.0F, 0.4F),
                                   QVector3D(0.3F, 0.2F, 0.1F), 0.9F);

    renderer->mesh(resources->unit(), flag.pole, flag.poleColor,
                   resources->white(), 0.8F);
    renderer->mesh(resources->unit(), flag.pennant, flag.pennantColor,
                   resources->white(), 0.8F);
    renderer->mesh(resources->unit(), flag.finial, flag.pennantColor,
                   resources->white(), 0.8F);

    auto const grid_x =
        static_cast<int32_t>(preview_waypoint->x() * k_position_grid_precision);
    auto const grid_z =
        static_cast<int32_t>(preview_waypoint->z() * k_position_grid_precision);
    uint64_t const pos_hash =
        (static_cast<uint64_t>(grid_x) << k_position_hash_shift) |
        static_cast<uint64_t>(grid_z);
    rendered_positions.insert(pos_hash);
  }

  auto patrol_entities =
      world.get_entities_with<Engine::Core::PatrolComponent>();

  for (auto *entity : patrol_entities) {
    auto *patrol = entity->get_component<Engine::Core::PatrolComponent>();
    if ((patrol == nullptr) || !patrol->patrolling ||
        patrol->waypoints.empty()) {
      continue;
    }

    auto *unit = entity->get_component<Engine::Core::UnitComponent>();
    if ((unit == nullptr) || unit->health <= 0) {
      continue;
    }

    for (const auto &waypoint : patrol->waypoints) {

      auto const grid_x =
          static_cast<int32_t>(waypoint.first * k_position_grid_precision);
      auto const grid_z =
          static_cast<int32_t>(waypoint.second * k_position_grid_precision);
      uint64_t const pos_hash =
          (static_cast<uint64_t>(grid_x) << k_position_hash_shift) |
          static_cast<uint64_t>(grid_z);

      if (!rendered_positions.insert(pos_hash).second) {
        continue;
      }

      auto flag = Geom::Flag::create(waypoint.first, waypoint.second,
                                     QVector3D(0.2F, 0.9F, 0.3F),
                                     QVector3D(0.3F, 0.2F, 0.1F), 0.8F);

      renderer->mesh(resources->unit(), flag.pole, flag.poleColor,
                     resources->white(), 1.0F);
      renderer->mesh(resources->unit(), flag.pennant, flag.pennantColor,
                     resources->white(), 1.0F);
      renderer->mesh(resources->unit(), flag.finial, flag.pennantColor,
                     resources->white(), 1.0F);
    }
  }
}

} // namespace Render::GL
