#include "movement_system.h"

#include <QVector3D>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "../formation/army_formation_registry.h"
#include "../map/terrain_service.h"
#include "../units/troop_config.h"
#include "building_collision_registry.h"
#include "combat_rules.h"
#include "combat_system/structure_combat.h"
#include "command_service.h"
#include "core/component.h"
#include "defense_formation_service.h"
#include "formation_combat_geometry.h"
#include "gate_service.h"
#include "order_service.h"
#include "pathfinding.h"

namespace Game::Systems {

static constexpr int max_waypoint_skip_count = 4;

static constexpr float k_stuck_timeout_seconds = 3.0F;
static constexpr float k_stuck_progress_epsilon_sq = 0.15F * 0.15F;

namespace {

constexpr float hold_mode_turn_speed_degrees = 180.0F;
constexpr float desired_yaw_turn_speed_degrees = 720.0F;
constexpr float full_translation_heading_error_degrees = 20.0F;
constexpr float stopped_translation_heading_error_degrees = 100.0F;

auto formation_turn_speed_degrees(const Engine::Core::Entity& entity,
                                  const Engine::Core::UnitComponent& unit,
                                  float single_body_turn_speed) -> float {
  float const turn_radius = FormationCombat::formation_turn_radius(entity);
  if (!FormationCombat::has_formation_slots(entity) || turn_radius <= 0.5F) {
    return single_body_turn_speed;
  }

  float const max_outer_speed = std::max(2.0F, unit.speed * 1.5F);
  float const derived =
      max_outer_speed / turn_radius * 180.0F / std::numbers::pi_v<float>;
  return std::clamp(derived, 30.0F, single_body_turn_speed);
}

auto heading_translation_scale(float yaw_degrees, float vx, float vz) -> float {
  if (vx * vx + vz * vz <= 1.0e-5F) {
    return 1.0F;
  }
  float const velocity_yaw = std::atan2(vx, vz) * 180.0F / std::numbers::pi_v<float>;
  float const error =
      std::fabs(std::fmod((velocity_yaw - yaw_degrees + 540.0F), 360.0F) - 180.0F);
  return 1.0F - std::clamp((error - full_translation_heading_error_degrees) /
                               (stopped_translation_heading_error_degrees -
                                full_translation_heading_error_degrees),
                           0.0F,
                           1.0F);
}

auto should_skip_navigation(const Engine::Core::Entity& entity) -> bool {
  auto const* commander = entity.get_component<Engine::Core::CommanderComponent>();
  return commander != nullptr && (commander->jump_active || commander->fpv_controlled);
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
  return max_navigation_speed(unit, stamina) *
         DefenseFormationService::move_speed_multiplier(entity) *
         Game::Formation::ArmyFormationRuntime::move_speed_multiplier(entity);
}

auto melee_turn_speed_degrees(const Engine::Core::UnitComponent& unit) -> float {
  return Game::Units::is_cavalry(unit.spawn_type) ? 90.0F
                                                  : desired_yaw_turn_speed_degrees;
}

void apply_desired_yaw(Engine::Core::TransformComponent* transform,
                       float delta_time,
                       float turn_speed_degrees) {
  if ((transform == nullptr) || !transform->has_desired_yaw) {
    return;
  }

  float const current = transform->rotation.y;
  float const target_yaw = transform->desired_yaw;
  float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
  float const step = std::clamp(
      diff, -turn_speed_degrees * delta_time, turn_speed_degrees * delta_time);
  transform->rotation.y = current + step;

  float const remaining_diff =
      std::fmod((target_yaw - transform->rotation.y + 540.0F), 360.0F) - 180.0F;
  if (std::fabs(remaining_diff) < 0.5F) {
    transform->rotation.y = target_yaw;
    transform->has_desired_yaw = false;
  }
}

auto is_point_allowed(const QVector3D& pos,
                      const Engine::Core::Entity& entity,
                      Engine::Core::World* world) -> bool {
  if (auto const* transform = entity.get_component<Engine::Core::TransformComponent>();
      transform != nullptr &&
      GateService::blocks_move(
          QVector3D(transform->position.x, 0.0F, transform->position.z), pos)) {
    return false;
  }

  if (auto const* builder_prod =
          entity.get_component<Engine::Core::BuilderProductionComponent>();
      builder_prod != nullptr && builder_prod->in_progress &&
      builder_prod->at_construction_site && builder_prod->has_task_target &&
      builder_prod->task_target_id != 0) {
    Point const position_grid = CommandService::world_to_grid(pos.x(), pos.z());
    Point const target_grid = CommandService::world_to_grid(
        builder_prod->task_target_x, builder_prod->task_target_z);
    if (position_grid.x == target_grid.x && position_grid.y == target_grid.y) {
      return true;
    }
  }

  if (CommandService::is_world_position_walkable(pos)) {
    return true;
  }

  auto const* movement = entity.get_component<Engine::Core::MovementComponent>();
  auto* structure = world != nullptr && movement != nullptr &&
                            movement->get_structure_approach_target() != 0
                        ? world->get_entity(movement->get_structure_approach_target())
                        : nullptr;
  if (structure == nullptr ||
      !structure->has_component<Engine::Core::BuildingComponent>()) {
    return false;
  }
  float const facade_distance =
      Game::Systems::Combat::structure_surface_distance(*structure, pos);
  constexpr float k_max_final_approach_depth = 2.25F;
  constexpr float k_min_physical_clearance = 0.04F;

  return facade_distance >= k_min_physical_clearance &&
         facade_distance <= k_max_final_approach_depth &&
         !BuildingCollisionRegistry::instance().is_circle_overlapping_building(
             pos.x(), pos.z(), k_min_physical_clearance);
}

} // namespace

namespace {

void finalize_orientation(Engine::Core::Entity* entity,
                          Engine::Core::TransformComponent* transform,
                          Engine::Core::MovementComponent* movement,
                          float delta_time) {
  auto& terrain = Game::Map::TerrainService::instance();
  if (terrain.is_initialized()) {
    const Game::Map::TerrainHeightMap* hm = terrain.get_height_map();
    if (hm != nullptr) {
      const float tile = hm->get_tile_size();
      const int w = hm->get_width();
      const int h = hm->get_height();
      if (w > 0 && h > 0) {
        const float half_w = w * 0.5F - 0.5F;
        const float half_h = h * 0.5F - 0.5F;
        transform->position.x =
            std::clamp(transform->position.x, -half_w * tile, half_w * tile);
        transform->position.z =
            std::clamp(transform->position.z, -half_h * tile, half_h * tile);
      }
    }
  }

  auto* terrain_ctx = entity->get_component<Engine::Core::TerrainContextComponent>();
  if (terrain_ctx != nullptr && terrain_ctx->audio_cooldown > 0.0F) {
    terrain_ctx->audio_cooldown =
        std::max(0.0F, terrain_ctx->audio_cooldown - delta_time);
  }

  if (entity->has_component<Engine::Core::BuildingComponent>()) {
    return;
  }

  float const speed2 =
      movement->get_vx() * movement->get_vx() + movement->get_vz() * movement->get_vz();
  auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
  float const turn_speed =
      (unit != nullptr ? formation_turn_speed_degrees(
                             *entity, *unit, desired_yaw_turn_speed_degrees)
                       : desired_yaw_turn_speed_degrees) *
      DefenseFormationService::turn_speed_multiplier(*entity);
  if (speed2 > 1e-5F) {
    float const target_yaw = std::atan2(movement->get_vx(), movement->get_vz()) *
                             180.0F / std::numbers::pi_v<float>;
    float const current = transform->rotation.y;
    float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
    float const step =
        std::clamp(diff, -turn_speed * delta_time, turn_speed * delta_time);
    transform->rotation.y = current + step;
  } else if (transform->has_desired_yaw) {
    float const current = transform->rotation.y;
    float const target_yaw = transform->desired_yaw;
    float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
    float const step =
        std::clamp(diff, -turn_speed * delta_time, turn_speed * delta_time);
    transform->rotation.y = current + step;
    if (std::fabs(diff) < 0.5F) {
      transform->has_desired_yaw = false;
    }
  }
}

} // namespace

void MovementSystem::update(Engine::Core::World* world, float delta_time) {
  if (world == nullptr) {
    return;
  }

  if (auto* pathfinder = CommandService::get_pathfinder()) {
    std::uint64_t const revision = pathfinder->obstruction_revision();
    if (revision != m_obstruction_revision) {
      m_obstruction_revision = revision;

      pathfinder->update_navigation_grid();
      auto entities = world->get_entities_with<Engine::Core::MovementComponent>();
      repath_after_obstruction_release(*world, entities);
    }
  }

  process_pending_path_requests(*world);
  world->each<Engine::Core::MovementComponent>(
      [this, world, delta_time](Engine::Core::EntityID id,
                                Engine::Core::MovementComponent&) {
        move_unit(world->get_entity(id), world, delta_time);
      });
}

void MovementSystem::process_pending_path_requests(Engine::Core::World& world) {
  std::size_t processed = 0;
  while (processed < k_path_requests_per_tick && !m_pending_path_requests.empty()) {
    PendingPathRequest request = std::move(m_pending_path_requests.front());
    m_pending_path_requests.pop_front();
    auto* entity = world.get_entity(request.entity_id);
    if (entity == nullptr) {
      ++processed;
      continue;
    }
    auto* transform = entity->get_component<Engine::Core::TransformComponent>();
    auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    if (transform != nullptr && movement != nullptr) {
      float const goal_dx = movement->get_goal_x() - request.target.x();
      float const goal_dz = movement->get_goal_y() - request.target.z();
      if (goal_dx * goal_dx + goal_dz * goal_dz > 0.01F) {
        ++processed;
        continue;
      }
      assign_navigation_target(
          CommandService::get_pathfinder(), *transform, *movement, request.target);
      movement->precise_arrival = request.precise_arrival;
    }
    ++processed;
  }
}

auto MovementSystem::enqueue_pending_path_request(Engine::Core::EntityID entity_id,
                                                  const QVector3D& target,
                                                  bool precise_arrival,
                                                  std::uint64_t navigation_revision)
    -> bool {
  cancel_pending_path_request(entity_id);
  if (m_pending_path_requests.size() >= k_max_pending_path_requests) {
    return false;
  }
  m_pending_path_requests.push_back(
      {entity_id, target, navigation_revision, precise_arrival});
  return true;
}

void MovementSystem::cancel_pending_path_request(Engine::Core::EntityID entity_id) {
  std::erase_if(m_pending_path_requests, [entity_id](auto const& request) {
    return request.entity_id == entity_id;
  });
}

void MovementSystem::repath_after_obstruction_release(
    Engine::Core::World& world, const std::vector<Engine::Core::Entity*>& movers) {
  for (auto* entity : movers) {
    if (entity == nullptr ||
        entity->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }

    auto const* unit = entity->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }

    auto* movement = entity->get_component<Engine::Core::MovementComponent>();
    if (movement == nullptr || !movement->get_has_target()) {
      continue;
    }

    QVector3D const goal =
        movement->get_has_requested_goal()
            ? QVector3D(movement->get_requested_goal_x(),
                        0.0F,
                        movement->get_requested_goal_z())
            : QVector3D(movement->get_goal_x(), 0.0F, movement->get_goal_y());

    if (!retarget_unit(world, entity->get_id(), goal)) {
      continue;
    }

    movement->stuck_ref_valid = false;
    movement->stuck_timer = 0.0F;
  }
}

void MovementSystem::move_unit(Engine::Core::Entity* entity,
                               Engine::Core::World* world,
                               float delta_time) {
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  auto* movement = entity->get_component<Engine::Core::MovementComponent>();
  auto* unit = entity->get_component<Engine::Core::UnitComponent>();

  if ((transform == nullptr) || (movement == nullptr) || (unit == nullptr)) {
    return;
  }

  if (unit->health <= 0 ||
      entity->has_component<Engine::Core::PendingRemovalComponent>()) {
    return;
  }

  if (should_skip_navigation(*entity)) {
    if (auto const* commander =
            entity->get_component<Engine::Core::CommanderComponent>();
        commander != nullptr && commander->fpv_controlled && !commander->jump_active) {
      movement->has_target = false;
      movement->clear_path();
      OrderService::clear_player_order_intent(entity);
      movement->vx = 0.0F;
      movement->vz = 0.0F;
    }
    return;
  }

  auto* hold_mode = entity->get_component<Engine::Core::HoldModeComponent>();
  bool in_hold_mode = false;
  if (hold_mode != nullptr) {
    if (hold_mode->exit_cooldown > 0.0F) {
      hold_mode->exit_cooldown = std::max(0.0F, hold_mode->exit_cooldown - delta_time);
    }

    if (hold_mode->active) {
      movement->has_target = false;
      movement->clear_path();
      OrderService::clear_player_order_intent(entity);
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      in_hold_mode = true;

      if (hold_mode->kneel_duration > 0.0F && hold_mode->kneel_entry_progress < 1.0F) {
        hold_mode->kneel_entry_progress = std::min(
            1.0F,
            hold_mode->kneel_entry_progress + delta_time / hold_mode->kneel_duration);
      }
    } else {
      hold_mode->kneel_entry_progress = 0.0F;
    }

    if (hold_mode->exit_cooldown > 0.0F && !in_hold_mode) {
      movement->vx = 0.0F;
      movement->vz = 0.0F;

      return;
    }
  }

  if (in_hold_mode) {
    if (!entity->has_component<Engine::Core::BuildingComponent>()) {
      apply_desired_yaw(
          transform,
          delta_time,
          formation_turn_speed_degrees(*entity, *unit, hold_mode_turn_speed_degrees));
    }
    return;
  }

  auto* atk = entity->get_component<Engine::Core::AttackComponent>();
  if ((atk != nullptr) && atk->in_melee_lock &&
      CombatRules::participates_in_rts_melee_lock(entity)) {
    movement->has_target = false;
    OrderService::clear_player_order_intent(entity);
    movement->vx = 0.0F;
    movement->vz = 0.0F;
    movement->clear_path();
    transform->desired_yaw = transform->rotation.y;
    transform->has_desired_yaw = false;
    return;
  }

  auto* builder_prod =
      entity->get_component<Engine::Core::BuilderProductionComponent>();
  bool const bypass_mode =
      (builder_prod != nullptr) && builder_prod->bypass_movement_active;

  if (movement->has_target) {
    float const px = transform->position.x;
    float const pz = transform->position.z;
    if (!movement->stuck_ref_valid) {
      movement->stuck_ref_x = px;
      movement->stuck_ref_z = pz;
      movement->stuck_timer = 0.0F;
      movement->stuck_ref_valid = true;
    } else {
      float const moved_x = px - movement->stuck_ref_x;
      float const moved_z = pz - movement->stuck_ref_z;
      if (moved_x * moved_x + moved_z * moved_z > k_stuck_progress_epsilon_sq) {
        movement->stuck_ref_x = px;
        movement->stuck_ref_z = pz;
        movement->stuck_timer = 0.0F;
      } else {
        movement->stuck_timer += delta_time;
        if (movement->stuck_timer >= k_stuck_timeout_seconds) {
          movement->stop();
          OrderService::clear_player_order_intent(entity);
          movement->stuck_ref_valid = false;
          return;
        }
      }
    }
  } else {
    movement->stuck_ref_valid = false;
    movement->stuck_timer = 0.0F;
  }

  if (bypass_mode) {

    float const dx = builder_prod->bypass_target_x - transform->position.x;
    float const dz = builder_prod->bypass_target_z - transform->position.z;
    float const dist_sq = dx * dx + dz * dz;

    float const dist = std::sqrt(std::max(dist_sq, 0.0001F));
    float const bypass_step =
        std::max(max_navigation_speed(*unit, nullptr) * delta_time, 0.01F);

    if (dist <= bypass_step) {

      transform->position.x = builder_prod->bypass_target_x;
      transform->position.z = builder_prod->bypass_target_z;
      builder_prod->bypass_movement_active = false;
      movement->vx = 0.0F;
      movement->vz = 0.0F;
      movement->has_target = false;
      movement->clear_path();
      OrderService::clear_player_order_intent(entity);
    } else {

      float const nx = dx / dist;
      float const nz = dz / dist;
      float const base_speed = max_navigation_speed(*unit, nullptr);
      movement->vx = nx * base_speed;
      movement->vz = nz * base_speed;

      transform->position.x += movement->vx * delta_time;
      transform->position.z += movement->vz * delta_time;

      float const target_yaw =
          std::atan2(movement->vx, movement->vz) * 180.0F / std::numbers::pi_v<float>;
      float const current = transform->rotation.y;
      float const diff = std::fmod((target_yaw - current + 540.0F), 360.0F) - 180.0F;
      float const turn_speed = 720.0F;
      float const step =
          std::clamp(diff, -turn_speed * delta_time, turn_speed * delta_time);
      transform->rotation.y = current + step;
    }
    return;
  }

  QVector3D const final_goal(movement->goal_x, 0.0F, movement->goal_y);

  QVector3D const current_pos_3d(transform->position.x, 0.0F, transform->position.z);
  bool const current_position_allowed =
      is_point_allowed(current_pos_3d, *entity, world);
  bool const destination_allowed = is_point_allowed(final_goal, *entity, world);

  auto* stamina = entity->get_component<Engine::Core::StaminaComponent>();
  const float max_speed = formation_navigation_speed(*entity, *unit, stamina);
  const float accel = max_speed * 4.0F;
  const float damping = 6.0F;

  if (!current_position_allowed && MovementSystem::assign_local_recovery_move(
                                       current_pos_3d, final_goal, movement)) {
    return;
  }

  if (movement->has_target && !destination_allowed && current_position_allowed) {
    Point const requested_goal =
        CommandService::world_to_grid(final_goal.x(), final_goal.z());
    auto const nearest_goal =
        CommandService::find_nearest_walkable_grid(requested_goal, 32);
    if (nearest_goal.has_value()) {
      QVector3D const resolved_goal = CommandService::grid_to_world(*nearest_goal);
      movement->goal_x = resolved_goal.x();
      movement->goal_y = resolved_goal.z();
      MovementSystem::retarget_unit(*world, entity->get_id(), resolved_goal);
      movement->vx = 0.0F;
      movement->vz = 0.0F;
    } else {
      movement->vx = 0.0F;
      movement->vz = 0.0F;
    }
    return;
  }

  if (!movement->has_target) {
    movement->vx *= std::max(0.0F, 1.0F - damping * delta_time);
    movement->vz *= std::max(0.0F, 1.0F - damping * delta_time);
  } else {
    if (movement->has_waypoints()) {
      const auto& wp = movement->current_waypoint();
      movement->target_x = wp.first;
      movement->target_y = wp.second;
    }

    float const waypoint_arrive_radius =
        std::clamp(max_speed * delta_time * 2.0F, 0.05F, 0.25F);
    bool const current_target_is_final =
        !movement->has_waypoints() || movement->remaining_waypoints() <= 1;
    float const arrive_radius =
        current_target_is_final
            ? (movement->precise_arrival
                   ? waypoint_arrive_radius
                   : std::max(waypoint_arrive_radius,
                              std::clamp(CommandService::get_unit_radius(
                                             *world, entity->get_id()) *
                                             1.1F,
                                         0.25F,
                                         0.9F)))
            : waypoint_arrive_radius;
    float const arrive_radius_sq = arrive_radius * arrive_radius;

    float dx = movement->target_x - transform->position.x;
    float dz = movement->target_y - transform->position.z;
    float dist2 = dx * dx + dz * dz;

    int safety_counter = max_waypoint_skip_count;
    while (movement->has_target && dist2 < arrive_radius_sq && safety_counter-- > 0) {
      if (movement->has_waypoints()) {
        movement->advance_waypoint();
        if (movement->has_waypoints()) {
          const auto& wp = movement->current_waypoint();
          movement->target_x = wp.first;
          movement->target_y = wp.second;
          dx = movement->target_x - transform->position.x;
          dz = movement->target_y - transform->position.z;
          dist2 = dx * dx + dz * dz;
          continue;
        }
      }

      movement->stop();
      OrderService::clear_player_order_intent(entity);

      auto* guard_mode = entity->get_component<Engine::Core::GuardModeComponent>();
      if ((guard_mode != nullptr) && guard_mode->active &&
          guard_mode->returning_to_guard_position) {
        guard_mode->returning_to_guard_position = false;
      }

      break;
    }

    if (!movement->has_target) {
      movement->vx *= std::max(0.0F, 1.0F - damping * delta_time);
      movement->vz *= std::max(0.0F, 1.0F - damping * delta_time);
    } else {
      float const distance = std::sqrt(std::max(dist2, 0.0F));
      float const nx = dx / std::max(0.0001F, distance);
      float const nz = dz / std::max(0.0001F, distance);
      float desired_speed = max_speed;

      auto* const move_attack = entity->get_component<Engine::Core::AttackComponent>();
      bool const ranged_mode = (move_attack != nullptr) && move_attack->can_ranged &&
                               move_attack->current_mode ==
                                   Engine::Core::AttackComponent::CombatMode::Ranged;
      float const slow_radius = ranged_mode ? arrive_radius : arrive_radius * 1.5F;
      if (distance < slow_radius) {
        desired_speed = max_speed * (distance / slow_radius);
      }

      float const desired_vx = nx * desired_speed;
      float const desired_vz = nz * desired_speed;

      float const ax = (desired_vx - movement->vx) * accel;
      float const az = (desired_vz - movement->vz) * accel;
      movement->vx += ax * delta_time;
      movement->vz += az * delta_time;

      movement->vx *= std::max(0.0F, 1.0F - 0.5F * damping * delta_time);
      movement->vz *= std::max(0.0F, 1.0F - 0.5F * damping * delta_time);
    }
  }

  bool const was_on_valid_tile = current_position_allowed;

  float const old_x = transform->position.x;
  float const old_z = transform->position.z;
  float const translation_scale =
      current_position_allowed
          ? heading_translation_scale(transform->rotation.y, movement->vx, movement->vz)
          : 1.0F;

  float const translated_vx = movement->vx * translation_scale;
  float const translated_vz = movement->vz * translation_scale;
  float const new_x = old_x + translated_vx * delta_time;
  float const new_z = old_z + translated_vz * delta_time;

  auto cell_walkable = [entity, world](float wx, float wz) -> bool {
    return is_point_allowed(QVector3D(wx, 0.0F, wz), *entity, world);
  };

  auto const& collision = BuildingCollisionRegistry::instance();
  float const trapped_depth =
      was_on_valid_tile ? 0.0F : collision.blocking_penetration_depth(old_x, old_z);
  auto step_allowed = [&](float wx, float wz) -> bool {
    if (was_on_valid_tile) {
      return cell_walkable(wx, wz);
    }
    if (trapped_depth > 0.0F) {
      return collision.blocking_penetration_depth(wx, wz) < trapped_depth;
    }
    return !collision.is_point_in_blocking_building(wx, wz);
  };

  if (step_allowed(new_x, new_z)) {

    transform->position.x = new_x;
    transform->position.z = new_z;
  } else {

    bool const x_axis_clear = step_allowed(new_x, old_z);
    bool const z_axis_clear = step_allowed(old_x, new_z);

    if (x_axis_clear) {
      transform->position.x = new_x;
      movement->vz = 0.0F;
    } else if (z_axis_clear) {
      transform->position.z = new_z;
      movement->vx = 0.0F;
    } else {

      movement->vx = 0.0F;
      movement->vz = 0.0F;
    }
  }

  float const stepped_x = transform->position.x - old_x;
  float const stepped_z = transform->position.z - old_z;
  movement->travelled += std::sqrt((stepped_x * stepped_x) + (stepped_z * stepped_z));
  // Only differences are ever read, and a float that has run to five figures can no
  // longer resolve a single footfall. Wrapping costs one dropped step per lap.
  constexpr float k_travelled_wrap = 4096.0F;
  if (movement->travelled >= k_travelled_wrap) {
    movement->travelled -= k_travelled_wrap;
  }

  finalize_orientation(entity, transform, movement, delta_time);
}

} // namespace Game::Systems
