#include "wall_walk_orders.h"

#include <cmath>
#include <limits>
#include <vector>

#include "core/ambient_session.h"
#include "core/component_core.h"
#include "core/component_gameplay.h"
#include "core/wall_walk_geometry.h"
#include "core/world.h"
#include "systems/owner_registry.h"
#include "units/spawn_type.h"

namespace Game::Systems::WallWalk {

namespace {

// How far along an enemy wall a docked tower's bridge still counts as the way
// onto it.
constexpr float k_tower_bridge_reach = 30.0F;

struct BridgeHead {
  int wall_owner{0};
  float x{0.0F};
  float z{0.0F};
};

// Enemy walls `owner_id` can reach: through its (or an ally's) siege towers
// docked against them with the bridge down.
auto bridge_heads(Engine::Core::World& world, int owner_id) -> std::vector<BridgeHead> {
  std::vector<BridgeHead> heads;
  auto const* services = Game::Session::services_for_or_null(world);
  auto allied = [&](int a, int b) {
    return a == b || (services != nullptr && services->owners != nullptr &&
                      services->owners->are_allies(a, b));
  };
  for (auto [id, tower, unit, transform] :
       world.view<const Engine::Core::SiegeTowerComponent,
                  const Engine::Core::UnitComponent,
                  const Engine::Core::TransformComponent>()) {
    (void)id;
    if (unit.health <= 0 || !allied(unit.owner_id, owner_id) ||
        tower.state != Engine::Core::SiegeTowerComponent::State::Docked ||
        tower.ramp < 1.0F) {
      continue;
    }
    auto const* wall_unit =
        world.try_get<Engine::Core::UnitComponent>(tower.docked_wall_id);
    if (wall_unit == nullptr || wall_unit->health <= 0 ||
        allied(wall_unit->owner_id, owner_id)) {
      continue;
    }
    heads.push_back({wall_unit->owner_id, transform.position.x, transform.position.z});
  }
  return heads;
}

} // namespace

auto wall_walk_order_at(Engine::Core::World& world,
                        int owner_id,
                        float x,
                        float z) -> std::optional<WallWalkOrder> {
  bool any_access = false;
  std::optional<WallWalkOrder> best;
  float best_distance = std::numeric_limits<float>::max();
  auto const heads = bridge_heads(world, owner_id);
  for (auto [id, unit, transform, wall] :
       world.view<const Engine::Core::UnitComponent,
                  const Engine::Core::TransformComponent,
                  const Engine::Core::WallSegmentComponent>()) {
    if (unit.health <= 0 || unit.spawn_type != Game::Units::SpawnType::WallSegment ||
        world.has<Engine::Core::PendingRemovalComponent>(id) ||
        (wall.inner_x == 0 && wall.inner_z == 0)) {
      continue;
    }
    float const node_x = transform.position.x;
    float const node_z = transform.position.z;
    if (unit.owner_id == owner_id) {
      any_access = any_access || wall.has_access();
    } else {
      bool reached = false;
      for (auto const& head : heads) {
        reached = reached || (head.wall_owner == unit.owner_id &&
                              std::hypot(head.x - node_x, head.z - node_z) <=
                                  k_tower_bridge_reach);
      }
      if (!reached) {
        continue;
      }
      any_access = true;
    }
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
  if (!any_access) {
    return std::nullopt;
  }
  return best;
}

} // namespace Game::Systems::WallWalk
