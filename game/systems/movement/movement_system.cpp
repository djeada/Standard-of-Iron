#include "movement_system.h"

#include <QVector3D>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <tuple>
#include <vector>

#include "movement_orders_assignment.h"
#include "movement_system_collision.h"
#include "movement_system_gates.h"
#include "movement_system_motor.h"
#include "movement_system_mover.h"
#include "route_follow_system.h"
#include "systems/combat_system/structure_combat.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/pathfinding.h"

namespace Game::Systems {

namespace {

constexpr float k_escape_arrival_radius = 0.6F;

}

void MovementSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  if (auto* pathfinder = NavGrid::get_pathfinder()) {
    std::uint64_t const revision = pathfinder->obstruction_revision();
    if (revision != m_obstruction_revision) {
      m_obstruction_revision = revision;

      pathfinder->update_navigation_grid();
      auto entities = world->collect_entities_with<Engine::Core::MovementComponent>();
      repath_after_obstruction_release(*world, entities);
    }
  }

  m_duel_footwork.advance(delta_time);

  m_path_requests.process(*world, &MovementSystem::apply_deferred_route);
  world->each<Engine::Core::MovementComponent>(
      [this, world, delta_time](Engine::Core::EntityID id,
                                Engine::Core::MovementComponent&) {
        move_unit(world->get_entity(id), world, delta_time);
      });

  world->spatial_index().invalidate();
}

void MovementSystem::repath_after_obstruction_release(
    Engine::Core::World& world, const std::vector<Engine::Core::Entity*>& movers) {
  auto* pathfinder = NavGrid::get_pathfinder();
  auto const release = pathfinder != nullptr ? pathfinder->last_obstruction_release()
                                             : Pathfinding::ObstructionRelease{};

  struct RepathCandidate {
    Engine::Core::EntityID entity_id{0};
    QVector3D goal;
    float distance_to_release_sq{0.0F};
  };

  std::vector<RepathCandidate> candidates;
  for (auto* entity : movers) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto const* movement = entity->get_component<Engine::Core::MovementComponent>();
    auto const* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || unit->health <= 0 || movement == nullptr ||
        transform == nullptr || !movement->get_has_target() ||
        !movement->get_order_fell_short()) {
      continue;
    }
    float const dx = transform->position.x - release.center.x();
    float const dz = transform->position.z - release.center.z();
    candidates.push_back(
        {.entity_id = entity->get_id(),
         .goal = QVector3D(
             movement->get_requested_goal_x(), 0.0F, movement->get_requested_goal_z()),
         .distance_to_release_sq = release.located ? (dx * dx) + (dz * dz) : 0.0F});
  }

  std::size_t const repaths =
      std::min(candidates.size(), PathRequestQueue::k_requests_per_tick);
  std::partial_sort(candidates.begin(),
                    candidates.begin() + static_cast<std::ptrdiff_t>(repaths),
                    candidates.end(),
                    [](const RepathCandidate& lhs, const RepathCandidate& rhs) {
                      return std::tie(lhs.distance_to_release_sq, lhs.entity_id) <
                             std::tie(rhs.distance_to_release_sq, rhs.entity_id);
                    });
  for (std::size_t index = 0; index < repaths; ++index) {
    Assignment::retarget_unit(
        world, candidates[index].entity_id, candidates[index].goal);
  }
}

void MovementSystem::apply_deferred_route(Engine::Core::TransformComponent& transform,
                                          Engine::Core::MovementComponent& movement,
                                          const QVector3D& target,
                                          bool precise_arrival) {
  Assignment::assign_navigation_target(
      NavGrid::get_pathfinder(), transform, movement, target);
  movement.precise_arrival = precise_arrival;
}

void MovementSystem::move_unit(Engine::Core::Entity* entity,
                               Engine::Core::World* world,
                               float delta_time) {
  Engine::Core::EntityID const id = entity->get_id();
  auto* transform = world->try_get<Engine::Core::TransformComponent>(id);
  auto* movement = world->try_get<Engine::Core::MovementComponent>(id);
  auto* unit = world->try_get<Engine::Core::UnitComponent>(id);

  if ((transform == nullptr) || (movement == nullptr) || (unit == nullptr)) {
    return;
  }

  auto* facts =
      Engine::Core::get_or_add_component<Engine::Core::MovementFactsComponent>(entity);
  if (facts == nullptr) {
    return;
  }

  Mover mover{*world,
              *entity,
              *transform,
              *movement,
              *unit,
              *facts,
              delta_time,
              transform->position.x,
              transform->position.z};

  if (unit->health <= 0 || world->has<Engine::Core::PendingRemovalComponent>(id)) {
    return;
  }

  if (movement->get_escape_active()) {
    float const to_exit_x = movement->get_escape_x() - transform->position.x;
    float const to_exit_z = movement->get_escape_z() - transform->position.z;
    if ((to_exit_x * to_exit_x) + (to_exit_z * to_exit_z) <=
            k_escape_arrival_radius * k_escape_arrival_radius ||
        !movement->get_has_target()) {
      movement->end_escape();
    }
  }
  auto const* wall_walker = world->try_get<Engine::Core::WallWalkerComponent>(id);
  if (!movement->get_escape_active() &&
      (wall_walker == nullptr ||
       wall_walker->phase == Engine::Core::WallWalkerComponent::Phase::Approaching)) {
    MovementCollision::unstick_body(*entity, *transform, delta_time);
  }

  switch (classify_movement_gate(*entity)) {
  case MovementGate::DirectControl:
    Gates::hold_direct_control(mover);
    return;
  case MovementGate::HoldMode:
    Gates::step_hold_mode(mover);
    return;
  case MovementGate::MeleeLock:
    Gates::step_melee_lock(mover, m_duel_footwork);
    return;
  case MovementGate::BuilderBypass:
    Gates::step_builder_bypass(mover);
    return;
  case MovementGate::OnWall:
    return;
  default:
    Motor::drive(mover);
    return;
  }
}

auto MovementSystem::access() const -> Engine::Core::SystemAccess {
  using namespace Engine::Core;
  return SystemAccess::declare(Reads<UnitComponent,
                                     BuildingComponent,
                                     CommanderComponent,
                                     GuardModeComponent,
                                     HoldModeComponent,
                                     ElephantComponent,
                                     RpgCommanderActionComponent,
                                     BuilderProductionComponent,
                                     WallWalkerComponent,
                                     RenderableComponent,
                                     PendingRemovalComponent>{},
                               Writes<MovementComponent,
                                      MovementFactsComponent,
                                      TransformComponent,
                                      AttackComponent,
                                      StaminaComponent,
                                      TerrainContextComponent,
                                      UnitTraversalLayoutStateComponent,
                                      FormationPresentationComponent,
                                      SoldierCasualtyAnimationComponent>{});
}

} // namespace Game::Systems