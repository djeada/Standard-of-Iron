#include "patrol_system.h"

#include <QVector3D>

#include <cmath>
#include <cstdint>

#include "../core/component_gameplay.h"
#include "../core/system_context.h"
#include "../core/world.h"
#include "../core/world_spatial_index.h"
#include "command_service.h"

namespace Game::Systems {

namespace {

constexpr float k_patrol_engagement_radius = 5.0F;

constexpr float k_patrol_goal_epsilon_sq = 0.01F;

constexpr std::uint64_t k_enemy_scan_interval_ticks = 8;

auto already_walking_to(const Engine::Core::MovementComponent& movement,
                        float target_x,
                        float target_z) -> bool {
  if (!movement.get_has_target() || !movement.has_waypoints() ||
      !movement.get_has_requested_goal()) {
    return false;
  }
  float const dx = movement.get_requested_goal_x() - target_x;
  float const dz = movement.get_requested_goal_z() - target_z;
  return (dx * dx) + (dz * dz) <= k_patrol_goal_epsilon_sq;
}

} // namespace

void PatrolSystem::run(Engine::Core::SystemContext& context) {
  auto& index = context.spatial_index();
  index.refresh(context.world());
  const std::uint64_t tick = context.world().tick_id();

  for (auto [entity, patrol_ref, movement_ref, transform_ref, unit_ref] :
       context.entity_view<Engine::Core::PatrolComponent,
                           Engine::Core::MovementComponent,
                           Engine::Core::TransformComponent,
                           Engine::Core::UnitComponent>()) {
    auto* patrol = &patrol_ref;
    const auto& transform = transform_ref;
    const auto& unit = unit_ref;
    const auto& movement = movement_ref;

    if (!patrol->patrolling || patrol->waypoints.size() < 2) {
      continue;
    }

    if (unit.health <= 0) {
      patrol->patrolling = false;
      continue;
    }

    auto* attack_target =
        context.try_get<Engine::Core::AttackTargetComponent>(entity.get_id());
    if ((attack_target != nullptr) && attack_target->target_id != 0) {

      continue;
    }

    Engine::Core::EntityID nearest_enemy = Engine::Core::NULL_ENTITY;

    const bool scan_this_tick = !movement.get_has_target() ||
                                (tick + static_cast<std::uint64_t>(entity.get_id())) %
                                        k_enemy_scan_interval_ticks ==
                                    0;
    if (scan_this_tick) {
      float nearest_dist_sq = k_patrol_engagement_radius * k_patrol_engagement_radius;
      index.for_each_in_radius(
          transform.position.x,
          transform.position.z,
          k_patrol_engagement_radius,
          [&](const Engine::Core::WorldSpatialIndex::Entry& candidate) {
            if (!candidate.is(Engine::Core::WorldSpatialIndex::k_alive) ||
                candidate.is(Engine::Core::WorldSpatialIndex::k_building)) {
              return;
            }
            if (candidate.owner_id == unit.owner_id) {
              return;
            }
            const float dx = candidate.x - transform.position.x;
            const float dz = candidate.z - transform.position.z;
            const float dist_sq = dx * dx + dz * dz;
            if (dist_sq < nearest_dist_sq) {
              nearest_dist_sq = dist_sq;
              nearest_enemy = candidate.id;
            }
          });
    }

    if (nearest_enemy != Engine::Core::NULL_ENTITY) {
      if (attack_target == nullptr) {
        attack_target =
            context.emplace<Engine::Core::AttackTargetComponent>(entity.get_id());
      }
      if (attack_target != nullptr) {
        attack_target->target_id = nearest_enemy;
        attack_target->should_chase = false;
      }

      continue;
    }

    auto waypoint = patrol->waypoints[patrol->current_waypoint];
    float target_x = waypoint.first;
    float target_z = waypoint.second;

    float const dx = target_x - transform.position.x;
    float const dz = target_z - transform.position.z;
    float const dist_sq = dx * dx + dz * dz;

    if (dist_sq < 1.0F) {

      patrol->current_waypoint =
          (patrol->current_waypoint + 1) % patrol->waypoints.size();
      waypoint = patrol->waypoints[patrol->current_waypoint];
      target_x = waypoint.first;
      target_z = waypoint.second;
    }

    if (already_walking_to(movement, target_x, target_z)) {

      continue;
    }

    Game::Systems::CommandService::MoveOptions options;
    options.kind = Game::Systems::MoveOrderKind::ScriptedMove;
    Game::Systems::CommandService::move_unit(
        context.world(), entity.get_id(), QVector3D(target_x, 0.0F, target_z), options);
  }
}

auto PatrolSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<UnitComponent,
                                     TransformComponent,
                                     BuildingComponent,
                                     PendingRemovalComponent>{},
                               Writes<PatrolComponent,
                                      AttackTargetComponent,
                                      MovementComponent,
                                      AttackComponent>{});
}

} // namespace Game::Systems
