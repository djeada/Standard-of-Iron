#include "command_service.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <memory>
#include <vector>

#include "../core/component.h"
#include "../core/world.h"
#include "../game_config.h"
#include "../map/terrain_service.h"
#include "../units/troop_config.h"
#include "combat_rules.h"
#include "movement_system.h"
#include "nav_grid.h"
#include "pathfinding.h"
#include "units/spawn_type.h"

namespace Game::Systems {

namespace {
auto resolve_walkable_target(const QVector3D& target) -> QVector3D {
  return NavGrid::snap_to_walkable_ground(target, 8);
}

} // namespace

auto CommandService::resolve_move_targets(
    Engine::Core::World& world,
    const std::vector<Engine::Core::EntityID>& units,
    const QVector3D& center) -> std::vector<QVector3D> {
  std::vector<QVector3D> targets;
  targets.reserve(units.size());
  if (units.empty()) {
    return targets;
  }

  QVector3D current_center(0.0F, 0.0F, 0.0F);
  int positioned_count = 0;
  float max_radius = CommandService::k_unit_radius_threshold;
  for (auto unit_id : units) {
    float const unit_radius = CommandService::get_unit_radius(world, unit_id);
    max_radius = std::max(max_radius, unit_radius);
    auto* entity = world.get_entity(unit_id);
    auto* transform = entity != nullptr
                          ? entity->get_component<Engine::Core::TransformComponent>()
                          : nullptr;
    if (transform == nullptr) {
      continue;
    }
    current_center += QVector3D(transform->position.x, 0.0F, transform->position.z);
    ++positioned_count;
  }

  if (positioned_count > 0) {
    current_center /= static_cast<float>(positioned_count);
  } else {
    current_center = center - QVector3D(0.0F, 0.0F, 1.0F);
  }

  QVector3D const center_target = resolve_walkable_target(center);
  Point const center_grid =
      NavGrid::world_to_grid(center_target.x(), center_target.z());
  if (!NavGrid::is_grid_walkable(center_grid)) {
    targets.assign(units.size(), center_target);
    return targets;
  }

  QVector3D forward = center - current_center;
  forward.setY(0.0F);
  if (forward.lengthSquared() <= 1.0e-4F) {
    forward = QVector3D(0.0F, 0.0F, 1.0F);
  } else {
    forward.normalize();
  }

  QVector3D right(forward.z(), 0.0F, -forward.x());
  if (right.lengthSquared() <= 1.0e-4F) {
    right = QVector3D(1.0F, 0.0F, 0.0F);
  } else {
    right.normalize();
  }

  int const columns = std::max(
      1, static_cast<int>(std::ceil(std::sqrt(static_cast<float>(units.size())))));
  float const lane_center = (static_cast<float>(columns) - 1.0F) * 0.5F;
  float const spacing =
      std::max(Game::GameConfig::instance().gameplay().formation_spacing_default,
               max_radius * 2.8F + 0.8F);

  for (std::size_t idx = 0; idx < units.size(); ++idx) {
    int const col = static_cast<int>(idx % static_cast<std::size_t>(columns));
    int const row = static_cast<int>(idx / static_cast<std::size_t>(columns));
    QVector3D const offset =
        right * ((static_cast<float>(col) - lane_center) * spacing) -
        forward * (static_cast<float>(row) * spacing);
    QVector3D const candidate = center + offset;
    Point const candidate_grid = NavGrid::world_to_grid(candidate.x(), candidate.z());
    if (NavGrid::is_grid_walkable(candidate_grid)) {
      targets.push_back(resolve_walkable_target(candidate));
    } else {
      targets.push_back(center_target);
    }
  }

  return targets;
}

auto CommandService::plan_ground_move(Engine::Core::World& world,
                                      const std::vector<Engine::Core::EntityID>& units,
                                      const QVector3D& target,
                                      bool preserve_current_shape) -> GroundMovePlan {
  GroundMovePlan plan;
  if (units.empty()) {
    return plan;
  }

  plan.resolved_target = resolve_walkable_target(target);
  if (preserve_current_shape) {
    QVector3D current_center;
    int positioned_count = 0;
    for (auto const unit_id : units) {
      auto* entity = world.get_entity(unit_id);
      auto const* transform =
          entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                            : nullptr;
      if (transform != nullptr) {
        current_center += QVector3D(
            transform->position.x, transform->position.y, transform->position.z);
        ++positioned_count;
      }
    }
    if (positioned_count > 0) {
      current_center /= static_cast<float>(positioned_count);
    }
    plan.positions.reserve(units.size());
    for (auto const unit_id : units) {
      auto* entity = world.get_entity(unit_id);
      auto const* transform =
          entity != nullptr ? entity->get_component<Engine::Core::TransformComponent>()
                            : nullptr;
      QVector3D const position = transform != nullptr ? QVector3D(transform->position.x,
                                                                  transform->position.y,
                                                                  transform->position.z)
                                                      : current_center;
      QVector3D const offset = position - current_center;
      plan.positions.push_back(resolve_walkable_target(plan.resolved_target + offset));
    }
  } else {
    plan.positions = resolve_move_targets(world, units, plan.resolved_target);
  }
  plan.facing_angles.assign(units.size(), 0.0F);
  plan.preserve_formation_mode = preserve_current_shape;
  if (units.size() == 1 && !plan.positions.empty()) {
    plan.resolved_target = plan.positions.front();
  }
  return plan;
}

void CommandService::issue_ground_move(Engine::Core::World& world,
                                       const std::vector<Engine::Core::EntityID>& units,
                                       const GroundMovePlan& plan) {
  if (units.empty() || units.size() != plan.positions.size()) {
    return;
  }

  for (std::size_t i = 0; i < units.size(); ++i) {
    auto* entity = world.get_entity(units[i]);
    if (entity == nullptr) {
      continue;
    }

    auto* formation_mode =
        entity->get_component<Engine::Core::FormationModeComponent>();
    if ((formation_mode == nullptr) || !formation_mode->active ||
        i >= plan.facing_angles.size()) {
      continue;
    }

    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    if (transform != nullptr) {
      transform->desired_yaw = plan.facing_angles[i];
      transform->has_desired_yaw = true;
    }
  }

  MoveOptions opts;
  opts.kind = MoveOrderKind::FormationMove;
  opts.preserve_formation_mode = plan.preserve_formation_mode;
  move_units(world, units, plan.positions, opts);
}

auto CommandService::get_unit_radius(Engine::Core::World& world,
                                     Engine::Core::EntityID entity_id) -> float {
  auto* entity = world.get_entity(entity_id);
  if (entity == nullptr) {
    return 0.5F;
  }

  auto* unit_comp = entity->get_component<Engine::Core::UnitComponent>();
  if (unit_comp == nullptr) {
    return 0.5F;
  }

  float const selection_ring_size =
      Game::Units::TroopConfig::instance().get_selection_ring_size(
          unit_comp->spawn_type);

  return std::max(selection_ring_size * 0.5F, k_unit_radius_threshold);
}

void CommandService::move_unit(Engine::Core::World& world,
                               Engine::Core::EntityID unit_id,
                               const QVector3D& target) {
  MovementSystem::issue_move(world, unit_id, target);
}

void CommandService::move_unit(Engine::Core::World& world,
                               Engine::Core::EntityID unit_id,
                               const QVector3D& target,
                               const MoveOptions& options) {
  MovementSystem::issue_move(world, unit_id, target, options);
}

void CommandService::move_units(Engine::Core::World& world,
                                const std::vector<Engine::Core::EntityID>& units,
                                const std::vector<QVector3D>& targets) {
  MovementSystem::issue_move_units(world, units, targets);
}

void CommandService::move_units(Engine::Core::World& world,
                                const std::vector<Engine::Core::EntityID>& units,
                                const std::vector<QVector3D>& targets,
                                const MoveOptions& options) {
  MovementSystem::issue_move_units(world, units, targets, options);
}

void CommandService::move_units(Engine::Core::World& world,
                                const std::vector<MoveIntent>& intents) {
  MovementSystem::issue_move_units(world, intents);
}

void CommandService::move_units(Engine::Core::World& world,
                                const std::vector<MoveIntent>& intents,
                                const MoveOptions& options) {
  MovementSystem::issue_move_units(world, intents, options);
}

void CommandService::attack_target(Engine::Core::World& world,
                                   const std::vector<Engine::Core::EntityID>& units,
                                   Engine::Core::EntityID target_id,
                                   bool should_chase) {
  if (target_id == 0) {
    return;
  }
  for (auto unit_id : units) {
    auto* e = world.get_entity(unit_id);
    if (e == nullptr) {
      continue;
    }

    auto* atk = e->get_component<Engine::Core::AttackComponent>();
    if (atk != nullptr && atk->in_melee_lock &&
        atk->melee_lock_target_id != target_id &&
        CombatRules::participates_in_rts_melee_lock(e)) {
      auto* locked_target = world.get_entity(atk->melee_lock_target_id);
      auto const* locked_unit =
          locked_target != nullptr
              ? locked_target->get_component<Engine::Core::UnitComponent>()
              : nullptr;
      bool const locked_opponent_alive =
          locked_unit != nullptr && locked_unit->health > 0 &&
          !locked_target->has_component<Engine::Core::PendingRemovalComponent>();

      bool const leaves_structure =
          locked_target != nullptr &&
          locked_target->has_component<Engine::Core::BuildingComponent>();

      if (locked_opponent_alive && !leaves_structure) {
        continue;
      }
      CombatRules::clear_rts_melee_lock(e);
    }

    OrderService::prepare_for_attack(e);

    auto* attack_target = e->get_component<Engine::Core::AttackTargetComponent>();
    if (attack_target == nullptr) {
      attack_target = e->add_component<Engine::Core::AttackTargetComponent>();
    }
    if (attack_target == nullptr) {
      continue;
    }

    if (auto* action = e->get_component<Engine::Core::RpgCommanderActionComponent>();
        action != nullptr && action->action_running && action->active_target_id != 0 &&
        action->active_target_id != target_id) {
      action->action_running = false;
      action->action_completed = true;
      action->action_active = false;
      action->weapon_trace_active = false;
    }

    attack_target->target_id = target_id;
    attack_target->should_chase = should_chase;
    attack_target->is_player_command = true;
  }
}

} // namespace Game::Systems
