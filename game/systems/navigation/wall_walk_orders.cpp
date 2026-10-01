#include "wall_walk_orders.h"

#include <cmath>
#include <limits>

#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/wall_walk_geometry.h"
#include "core/world.h"
#include "units/spawn_type.h"

namespace Game::Systems::WallWalk {

auto wall_walk_order_at(Engine::Core::World& world,
                        int owner_id,
                        float x,
                        float z) -> std::optional<WallWalkOrder> {
  bool any_stair = false;
  std::optional<WallWalkOrder> best;
  float best_distance = std::numeric_limits<float>::max();
  for (auto [id, unit, transform, wall] :
       world.view<const Engine::Core::UnitComponent,
                  const Engine::Core::TransformComponent,
                  const Engine::Core::WallSegmentComponent>()) {
    if (unit.owner_id != owner_id || unit.health <= 0 ||
        unit.spawn_type != Game::Units::SpawnType::WallSegment ||
        world.has<Engine::Core::PendingRemovalComponent>(id) ||
        (wall.inner_x == 0 && wall.inner_z == 0)) {
      continue;
    }
    any_stair = any_stair || wall.has_stair;
    float const node_x = transform.position.x;
    float const node_z = transform.position.z;
    if (!is_wall_walk_order(node_x, node_z, wall.inner_x, wall.inner_z, x, z)) {
      continue;
    }
    float const distance = std::hypot(node_x - x, node_z - z);
    if (distance >= best_distance) {
      continue;
    }
    best_distance = distance;
    auto const lane = lane_point(node_x, node_z, wall.inner_x, wall.inner_z);
    bool const runs_x = wall.inner_z != 0;
    WallWalkOrder order;
    order.along_x = runs_x ? 1.0F : 0.0F;
    order.along_z = runs_x ? 0.0F : 1.0F;
    order.x = runs_x ? x : lane.x;
    order.z = runs_x ? lane.z : z;
    best = order;
  }
  if (!any_stair) {
    return std::nullopt;
  }
  return best;
}

} // namespace Game::Systems::WallWalk
