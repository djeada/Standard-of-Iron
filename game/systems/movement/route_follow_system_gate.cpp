#include "route_follow_system_gate.h"

#include <algorithm>
#include <cmath>

#include "body_profile.h"
#include "formation/army_formation_registry.h"
#include "systems/builder_product_types.h"
#include "systems/combat_rules.h"
#include "systems/defensive_unit_layout_service.h"
#include "systems/navigation/nav_grid.h"
#include "systems/navigation/pathfinding.h"
#include "systems/navigation/walkability.h"

namespace Game::Systems {

constexpr float k_work_anchor_reach = 1.75F;

auto is_movement_point_allowed(const QVector3D& pos,
                               const Engine::Core::Entity& entity) -> bool {

  auto const* ground = NavGrid::get_pathfinder();
  Point const ground_cell = NavGrid::world_to_grid(pos.x(), pos.z());
  bool const on_terrain =
      ground == nullptr || ground->is_terrain_walkable(ground_cell.x, ground_cell.y);
  if (auto const* builder_prod =
          entity.get_component<Engine::Core::BuilderProductionComponent>();
      builder_prod != nullptr &&
      (builder_prod->has_construction_site || builder_prod->in_progress) &&
      is_gather_builder_product(builder_prod->product_type)) {
    auto const stands_at = [&pos](float x, float z) {
      float const dx = pos.x() - x;
      float const dz = pos.z() - z;
      return (dx * dx) + (dz * dz) <= k_work_anchor_reach * k_work_anchor_reach;
    };
    if (on_terrain &&
        stands_at(builder_prod->task_target_x, builder_prod->task_target_z)) {
      return true;
    }

    if (on_terrain && builder_prod->has_construction_site &&
        stands_at(builder_prod->construction_site_x,
                  builder_prod->construction_site_z)) {
      return true;
    }

    auto const* movement = entity.get_component<Engine::Core::MovementComponent>();
    auto const* pathfinder = NavGrid::get_pathfinder();
    if (builder_prod->has_construction_site && movement != nullptr &&
        pathfinder != nullptr && builder_prod->structure_task_entity_id != 0 &&
        is_harvest_builder_product(builder_prod->product_type)) {
      float const reach = gather_bypass_reach(movement->get_navigation_clearance());
      float const dx = pos.x() - builder_prod->task_target_x;
      float const dz = pos.z() - builder_prod->task_target_z;
      Point const target_cell = NavGrid::world_to_grid(builder_prod->task_target_x,
                                                       builder_prod->task_target_z);
      auto const value = pathfinder->cell_value(ground_cell.x, ground_cell.y);
      bool const resource_cell =
          (builder_prod->product_type == k_builder_product_cut_tree &&
           value == Pathfinding::CellValue::Tree) ||
          (builder_prod->product_type == k_builder_product_collect_stone &&
           value == Pathfinding::CellValue::Boulder) ||
          (builder_prod->product_type == k_builder_product_collect_iron_ore &&
           value == Pathfinding::CellValue::IronOre);
      if (!on_terrain && ground_cell.x == target_cell.x &&
          ground_cell.y == target_cell.y && resource_cell &&
          (dx * dx) + (dz * dz) <= reach * reach) {
        return true;
      }
    }
  }

  return Walkability::can_stand(pos, motor_profile_for(entity));
}

auto bypass_line_is_clear(const Engine::Core::Entity& entity,
                          const QVector3D& from,
                          const QVector3D& to) -> bool {
  constexpr float k_sample_spacing = 0.25F;
  auto const* ground = NavGrid::get_pathfinder();
  auto const* builder_prod =
      entity.get_component<Engine::Core::BuilderProductionComponent>();
  bool const is_gather_work_approach =
      builder_prod != nullptr && builder_prod->has_construction_site &&
      builder_prod->structure_task_entity_id != 0 &&
      is_gather_builder_product(builder_prod->product_type) &&
      std::hypot(to.x() - builder_prod->task_target_x,
                 to.z() - builder_prod->task_target_z) <= k_work_anchor_reach;
  if (ground != nullptr && !is_gather_work_approach &&
      !ground->is_terrain_segment_walkable(from, to)) {
    return false;
  }
  QVector3D const span = to - from;
  int const samples =
      std::max(1, static_cast<int>(std::ceil(span.length() / k_sample_spacing)));
  for (int sample = 1; sample <= samples; ++sample) {
    float const t = static_cast<float>(sample) / static_cast<float>(samples);
    if (!is_movement_point_allowed(from + span * t, entity)) {
      return false;
    }
  }
  return true;
}

auto max_navigation_speed(const Engine::Core::UnitComponent& unit,
                          const Engine::Core::StaminaComponent* stamina) -> float {
  float speed = std::max(0.1F, unit.speed);
  if (stamina != nullptr && stamina->is_running) {
    speed *= Engine::Core::StaminaComponent::k_run_speed_multiplier;
  }
  return speed;
}

auto formation_navigation_speed(const Engine::Core::Entity& entity,
                                const Engine::Core::UnitComponent& unit,
                                const Engine::Core::StaminaComponent* stamina)
    -> float {

  float speed = max_navigation_speed(unit, nullptr) *
                DefensiveUnitLayoutService::move_speed_multiplier(entity) *
                Game::Formation::ArmyFormationRuntime::move_speed_multiplier(entity);
  const auto* movement = entity.get_component<Engine::Core::MovementComponent>();
  if (movement != nullptr && movement->get_declared_group_pace() > 0.0F) {
    speed = std::min(speed, movement->get_declared_group_pace());
  }
  if (!std::isfinite(speed) || speed <= 0.0F) {
    speed = max_navigation_speed(unit, nullptr);
  }
  if (stamina != nullptr && stamina->is_running) {
    speed *= Engine::Core::StaminaComponent::k_run_speed_multiplier;
  }
  return speed;
}

auto classify_movement_gate(const Engine::Core::Entity& entity) -> MovementGate {
  auto const* unit = entity.get_component<Engine::Core::UnitComponent>();
  if (unit == nullptr || unit->health <= 0 ||
      entity.has_component<Engine::Core::PendingRemovalComponent>()) {
    return MovementGate::Dead;
  }

  if (entity.has_component<Engine::Core::RaftRiderComponent>()) {
    return MovementGate::OnRaft;
  }

  auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
  if (commander != nullptr && (commander->jump_active || commander->fpv_controlled)) {
    return MovementGate::DirectControl;
  }

  auto const* hold_mode = entity.get_component<Engine::Core::HoldModeComponent>();
  if (hold_mode != nullptr && (hold_mode->active || hold_mode->exit_cooldown > 0.0F)) {
    return MovementGate::HoldMode;
  }

  if (auto const* walker = entity.get_component<Engine::Core::WallWalkerComponent>();
      walker != nullptr && walker->aloft()) {
    return MovementGate::OnWall;
  }

  auto const* attack = entity.get_component<Engine::Core::AttackComponent>();
  if (attack != nullptr && attack->in_melee_lock &&
      CombatRules::participates_in_rts_melee_lock(&entity)) {
    return MovementGate::MeleeLock;
  }

  auto const* builder_prod =
      entity.get_component<Engine::Core::BuilderProductionComponent>();
  if (builder_prod != nullptr && builder_prod->bypass_movement_active) {
    return MovementGate::BuilderBypass;
  }

  return MovementGate::RouteFollowing;
}

} // namespace Game::Systems
