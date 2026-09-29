#include <QVector3D>

#include <vector>

#include "command_service.h"
#include "core/component_gameplay.h"
#include "core/world.h"
#include "formation/army_formation_registry.h"
#include "movement_orders_assignment.h"
#include "movement_orders_group.h"
#include "movement_orders_prepared.h"
#include "movement_orders_targets.h"
#include "movement_system.h"
#include "order_service.h"
#include "systems/combat_rules.h"
#include "systems/formation_combat_geometry.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/pathfinding.h"
#include "systems/navigation/route_corridor_planner.h"

namespace Game::Systems {

using namespace MovementTargeting;

auto issuer_retargets(MoveOrderKind kind) -> bool {
  return kind == MoveOrderKind::AttackChase || kind == MoveOrderKind::ScriptedMove ||
         kind == MoveOrderKind::GuardReturn || kind == MoveOrderKind::RecoveryMove;
}

auto prepare_move(Engine::Core::World& world,
                  Engine::Core::EntityID unit_id,
                  const CommandService::MoveOptions& options) -> PreparedMove {
  auto* entity = world.get_entity(unit_id);
  if (entity == nullptr) {
    return {};
  }

  auto* attack = world.try_get<Engine::Core::AttackComponent>(unit_id);
  if (attack != nullptr && attack->in_melee_lock &&
      CombatRules::participates_in_rts_melee_lock(entity)) {
    Engine::Core::EntityID const locked = attack->melee_lock_target_id;
    auto const* locked_unit = world.try_get<Engine::Core::UnitComponent>(locked);
    bool const locked_to_structure = world.has<Engine::Core::BuildingComponent>(locked);
    bool const opponent_alive =
        locked_unit != nullptr && locked_unit->health > 0 &&
        !world.has<Engine::Core::PendingRemovalComponent>(locked);
    if (opponent_alive && !locked_to_structure) {
      return {};
    }
    CombatRules::clear_rts_melee_lock(entity);
  }

  OrderService::prepare_for_move(entity, options.kind, options.preserve_formation_mode);
  auto* transform = world.try_get<Engine::Core::TransformComponent>(unit_id);
  if (transform == nullptr) {
    return {};
  }
  auto* movement =
      Engine::Core::get_or_add_component<Engine::Core::MovementComponent>(entity);
  if (movement == nullptr) {
    return {};
  }
  movement->set_navigation_clearance(
      FormationCombat::formation_navigation_clearance(*entity));

  movement->begin_order();

  PreparedMove result;
  result.entity = entity;
  result.transform = transform;
  result.movement = movement;
  result.previous_vx = movement->get_vx();
  result.previous_vz = movement->get_vz();
  result.preserve_velocity =
      options.kind == MoveOrderKind::AttackChase && movement->get_has_target();
  return result;
}

auto MovementSystem::path_requests_of(Engine::Core::World& world) -> PathRequestQueue* {
  auto* system = world.get_system<MovementSystem>();
  return system != nullptr ? &system->m_path_requests : nullptr;
}

void MovementSystem::issue_move(Engine::Core::World& world,
                                Engine::Core::EntityID unit_id,
                                const QVector3D& target) {
  issue_move(world, unit_id, target, MoveOptions{});
}

void MovementSystem::issue_move(Engine::Core::World& world,
                                Engine::Core::EntityID unit_id,
                                const QVector3D& target,
                                const MoveOptions& options) {
  PreparedMove const prepared = prepare_move(world, unit_id, options);
  if (auto* requests = path_requests_of(world)) {
    requests->cancel(unit_id);
  }
  if (prepared.movement == nullptr || prepared.transform == nullptr) {
    return;
  }
  prepared.movement->precise_arrival = options.kind == MoveOrderKind::AttackChase ||
                                       options.kind == MoveOrderKind::FormationMove;
  prepared.movement->issuer_retargets = issuer_retargets(options.kind);
  Assignment::assign_navigation_target(
      NavGrid::get_pathfinder(), *prepared.transform, *prepared.movement, target);
  if (prepared.preserve_velocity && prepared.movement->get_has_target()) {
    prepared.movement->vx = prepared.previous_vx;
    prepared.movement->vz = prepared.previous_vz;
  }
}

void MovementSystem::issue_move_units(Engine::Core::World& world,
                                      const std::vector<Engine::Core::EntityID>& units,
                                      const std::vector<QVector3D>& targets) {
  issue_move_units(world, units, targets, MoveOptions{});
}

void MovementSystem::issue_move_units(Engine::Core::World& world,
                                      const std::vector<Engine::Core::EntityID>& units,
                                      const std::vector<QVector3D>& targets,
                                      const MoveOptions& options) {
  GroupIssue::issue(world, units, targets, options, path_requests_of(world));
}

void MovementSystem::issue_move_units(Engine::Core::World& world,
                                      const std::vector<MoveIntent>& intents) {
  issue_move_units(world, intents, MoveOptions{});
}

void MovementSystem::follow_formation_slot(Engine::Core::World& world,
                                           const MoveIntent& intent,
                                           const MoveOptions& options) {
  const auto* attack = world.try_get<Engine::Core::AttackComponent>(intent.unit_id);
  if (attack != nullptr && attack->in_melee_lock) {
    return;
  }
  auto* movement = world.try_get<Engine::Core::MovementComponent>(intent.unit_id);
  auto* transform = world.try_get<Engine::Core::TransformComponent>(intent.unit_id);
  if (transform == nullptr) {
    return;
  }
  bool const continuing = movement != nullptr && movement->following_formation_slot;
  if (!continuing) {
    const auto prepared = prepare_move(world, intent.unit_id, options);
    if (prepared.movement == nullptr) {
      return;
    }
    movement = prepared.movement;
    if (auto* requests = path_requests_of(world)) {
      requests->cancel(intent.unit_id);
    }
  }
  if (intent.facing_angle.has_value()) {
    transform->desired_yaw = *intent.facing_angle;
    transform->has_desired_yaw = true;
  }
  movement->following_formation_slot = true;
  movement->issuer_retargets = true;
  movement->precise_arrival = true;
  Assignment::adopt_formation_group(world, intent.unit_id, *transform, *movement);

  float const change_x = intent.target.x() - movement->goal_x;
  float const change_z = intent.target.z() - movement->goal_y;
  if (continuing && change_x * change_x + change_z * change_z < 1.0e-6F &&
      (movement->has_target ||
       std::hypot(transform->position.x - intent.target.x(),
                  transform->position.z - intent.target.z()) <= 0.1F)) {
    return;
  }

  const QVector3D current(transform->position.x, 0.0F, transform->position.z);
  auto* pathfinder = NavGrid::get_pathfinder();

  bool const direct = pathfinder == nullptr ||
                      is_direct_path_walkable(current,
                                              intent.target,
                                              passability_for(*movement),
                                              movement->get_navigation_clearance());
  if (direct) {

    Assignment::stamp_route_revision(*movement);
    movement->path = {{intent.target.x(), intent.target.z()}};
    movement->path_index = 0;
    movement->target_x = movement->goal_x = intent.target.x();
    movement->target_y = movement->goal_y = intent.target.z();
    movement->has_target = true;
    movement->has_requested_goal = false;
  } else {
    Assignment::assign_navigation_target(
        pathfinder, *transform, *movement, intent.target);
  }
}

void MovementSystem::issue_move_units(Engine::Core::World& world,
                                      const std::vector<MoveIntent>& intents,
                                      const MoveOptions& options) {
  if (options.follow_formation_slots) {
    for (const auto& intent : intents) {
      follow_formation_slot(world, intent, options);
    }
    return;
  }
  std::vector<Engine::Core::EntityID> units;
  std::vector<QVector3D> targets;
  units.reserve(intents.size());
  targets.reserve(intents.size());
  for (const auto& intent : intents) {
    units.push_back(intent.unit_id);
    targets.push_back(intent.target);
    if (!intent.facing_angle.has_value()) {
      continue;
    }
    auto* transform = world.try_get<Engine::Core::TransformComponent>(intent.unit_id);
    if (transform != nullptr) {
      transform->desired_yaw = *intent.facing_angle;
      transform->has_desired_yaw = true;
    }
  }
  if (options.kind == MoveOrderKind::AttackChase) {
    for (const auto& intent : intents) {
      issue_move(world, intent.unit_id, intent.target, options);
    }
    return;
  }
  issue_move_units(world, units, targets, options);
}

} // namespace Game::Systems
