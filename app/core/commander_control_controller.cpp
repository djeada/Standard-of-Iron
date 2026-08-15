#include "commander_control_controller.h"

#include <QCursor>
#include <QQuickWindow>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "game/accessibility/motion_settings.h"
#include "game/audio/audio_cues.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/building_line_of_sight.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/combat_actions/combat_action_service.h"
#include "game/systems/combat_system/damage_processor.h"
#include "game/systems/combat_system/mounted_charge_processor.h"
#include "game/systems/nav_grid.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/rpg_combat_system/rpg_bow_aim.h"
#include "game/systems/rpg_combat_system/rpg_bow_draw.h"
#include "game/systems/rpg_combat_system/rpg_commander_damage.h"
#include "game/systems/rpg_combat_system/rpg_targeting.h"
#include "game/systems/run_stamina.h"
#include "scene/camera.h"

namespace {

constexpr float k_degrees_to_radians = 0.017453292519943295F;

auto wrap_angle_degrees(float degrees) -> float {
  degrees = std::fmod(degrees, 360.0F);
  if (degrees < 0.0F) {
    degrees += 360.0F;
  }
  return degrees;
}

auto signed_angle_delta(float target_degrees, float current_degrees) -> float {
  float diff = target_degrees - current_degrees;
  while (diff > 180.0F) {
    diff -= 360.0F;
  }
  while (diff < -180.0F) {
    diff += 360.0F;
  }
  return diff;
}

constexpr float k_commander_body_radius = 0.34F;

auto structure_blocks_commander_body(float x, float z) -> bool {
  auto const& registry = Game::Systems::BuildingCollisionRegistry::instance();
  auto blocked_by = [x, z](const Game::Systems::BuildingFootprint& footprint) {
    if (!footprint.blocks_navigation) {
      return false;
    }
    float const half_width = footprint.width * 0.5F;
    float const half_depth = footprint.depth * 0.5F;
    float const closest_x =
        std::clamp(x, footprint.center_x - half_width, footprint.center_x + half_width);
    float const closest_z =
        std::clamp(z, footprint.center_z - half_depth, footprint.center_z + half_depth);
    float const dx = x - closest_x;
    float const dz = z - closest_z;
    return ((dx * dx) + (dz * dz)) < k_commander_body_radius * k_commander_body_radius;
  };
  for (auto const& building : registry.get_all_buildings()) {
    if (blocked_by(building)) {
      return true;
    }
  }
  for (auto const& obstacle : registry.authored_obstacles()) {
    if (blocked_by(obstacle)) {
      return true;
    }
  }
  return false;
}

auto structure_padding_covers_cell(const Game::Systems::Pathfinding& pathfinder,
                                   int grid_x,
                                   int grid_z) -> bool {

  constexpr float k_cell_slack = 1.0F;
  float const cell_x = static_cast<float>(grid_x) + pathfinder.get_grid_offset_x();
  float const cell_z = static_cast<float>(grid_z) + pathfinder.get_grid_offset_z();
  auto const& registry = Game::Systems::BuildingCollisionRegistry::instance();
  auto covers = [cell_x, cell_z](const Game::Systems::BuildingFootprint& footprint) {
    if (!footprint.blocks_navigation) {
      return false;
    }
    float const half_width =
        (footprint.width * 0.5F) + footprint.grid_padding + k_cell_slack;
    float const half_depth =
        (footprint.depth * 0.5F) + footprint.grid_padding + k_cell_slack;
    return std::abs(cell_x - footprint.center_x) <= half_width &&
           std::abs(cell_z - footprint.center_z) <= half_depth;
  };
  for (auto const& building : registry.get_all_buildings()) {
    if (covers(building)) {
      return true;
    }
  }
  for (auto const& obstacle : registry.authored_obstacles()) {
    if (covers(obstacle)) {
      return true;
    }
  }
  return false;
}

constexpr float k_scatter_prop_block_radius = 0.52F;

auto scatter_prop_blocks_commander_body(const Game::Systems::Pathfinding& pathfinder,
                                        int grid_x,
                                        int grid_z,
                                        float x,
                                        float z) -> bool {
  using CellValue = Game::Systems::Pathfinding::CellValue;

  float const clearance = k_scatter_prop_block_radius + k_commander_body_radius;

  for (int offset_z = -1; offset_z <= 1; ++offset_z) {
    for (int offset_x = -1; offset_x <= 1; ++offset_x) {
      int const cell_x = grid_x + offset_x;
      int const cell_z = grid_z + offset_z;
      auto const cell = pathfinder.cell_value(cell_x, cell_z);
      if (cell != CellValue::Tree && cell != CellValue::Boulder &&
          cell != CellValue::IronOre) {
        continue;
      }
      float const dx =
          x - (static_cast<float>(cell_x) + pathfinder.get_grid_offset_x());
      float const dz =
          z - (static_cast<float>(cell_z) + pathfinder.get_grid_offset_z());
      if (((dx * dx) + (dz * dz)) < clearance * clearance) {
        return true;
      }
    }
  }
  return false;
}

auto is_walkable_at(float x, float z) -> bool {
  using CellValue = Game::Systems::Pathfinding::CellValue;

  if (auto* pathfinder = Game::Systems::NavGrid::get_pathfinder()) {
    const auto grid = Game::Systems::NavGrid::world_to_grid(x, z);
    const auto cell = pathfinder->cell_value(grid.x, grid.y);

    if (cell != CellValue::Walkable) {
      if (!pathfinder->is_terrain_walkable(grid.x, grid.y)) {
        return false;
      }
      bool const scatter_prop_cell = cell == CellValue::Tree ||
                                     cell == CellValue::Boulder ||
                                     cell == CellValue::IronOre;
      if (!scatter_prop_cell &&
          !structure_padding_covers_cell(*pathfinder, grid.x, grid.y)) {
        return false;
      }
    }
    if (scatter_prop_blocks_commander_body(*pathfinder, grid.x, grid.y, x, z)) {
      return false;
    }
    return !structure_blocks_commander_body(x, z);
  }
  auto& terrain = Game::Map::TerrainService::instance();
  if (terrain.is_initialized() &&
      !terrain.is_walkable(static_cast<int>(std::round(x)),
                           static_cast<int>(std::round(z)))) {
    return false;
  }
  return !structure_blocks_commander_body(x, z);
}

constexpr float k_fpv_walk_speed_scale = 1.25F;

constexpr float k_fov_hip = 68.0F;
constexpr float k_fov_aim = 48.0F;

constexpr float k_strike_step_reach = 1.45F;
constexpr float k_strike_acquisition_bonus = 0.55F;

constexpr float k_footstep_min_bob_amplitude = 0.25F;

constexpr float k_footstep_bob_offset = 1.5F * std::numbers::pi_v<float>;

constexpr float k_fpv_backpedal_speed_scale = 0.72F;

constexpr float k_fpv_strafe_speed_scale = 0.86F;

auto directional_speed_scale(int forward_axis, int right_axis) -> float {
  if (forward_axis < 0) {
    return k_fpv_backpedal_speed_scale;
  }
  if (forward_axis == 0 && right_axis != 0) {
    return k_fpv_strafe_speed_scale;
  }
  return 1.0F;
}

struct GroundMove {
  float x{0.0F};
  float z{0.0F};
  bool moved{false};
};

auto airborne_step(float to_x, float to_z) -> GroundMove {
  return {.x = to_x, .z = to_z, .moved = true};
}

auto resolve_ground_step(float from_x,
                         float from_z,
                         float to_x,
                         float to_z) -> GroundMove {
  if (is_walkable_at(to_x, to_z)) {
    return {.x = to_x, .z = to_z, .moved = true};
  }

  float const delta_x = to_x - from_x;
  float const delta_z = to_z - from_z;
  bool const slide_x_free = std::abs(delta_x) > 1.0e-5F && is_walkable_at(to_x, from_z);
  bool const slide_z_free = std::abs(delta_z) > 1.0e-5F && is_walkable_at(from_x, to_z);

  if (slide_x_free && (!slide_z_free || std::abs(delta_x) >= std::abs(delta_z))) {
    return {.x = to_x, .z = from_z, .moved = true};
  }
  if (slide_z_free) {
    return {.x = from_x, .z = to_z, .moved = true};
  }
  return {.x = from_x, .z = from_z, .moved = false};
}

auto resolve_reachable_ground_position(const QVector3D& start,
                                       const QVector3D& desired,
                                       unsigned int ignore_entity_id = 0) -> QVector3D {
  QVector3D candidate = desired;
  const float blocked_fraction = Game::Systems::first_building_intersection_fraction(
      start, desired, ignore_entity_id);
  if (blocked_fraction < 1.0F) {
    const float safe_fraction = std::clamp(blocked_fraction - 0.08F, 0.0F, 1.0F);
    candidate = start + (desired - start) * safe_fraction;
  }

  QVector3D best = start;
  constexpr int k_samples = 8;
  for (int sample_index = 1; sample_index <= k_samples; ++sample_index) {
    const float sample_t =
        static_cast<float>(sample_index) / static_cast<float>(k_samples);
    const QVector3D sample = start + (candidate - start) * sample_t;
    if (!is_walkable_at(sample.x(), sample.z())) {
      break;
    }
    best = sample;
  }
  return best;
}

constexpr float k_body_separation_scan_range = 3.0F;

constexpr float k_body_separation_max_push_per_second = 2.4F;

void separate_commander_from_bodies(Engine::Core::World& world,
                                    Engine::Core::Entity& commander,
                                    Engine::Core::EntityID commander_id,
                                    Engine::Core::TransformComponent& transform,
                                    float dt) {
  if (dt <= 0.0F) {
    return;
  }

  const QVector3D origin(transform.position.x, 0.0F, transform.position.z);
  QVector3D push(0.0F, 0.0F, 0.0F);

  for (auto* candidate : world.get_entities_with<Engine::Core::UnitComponent>()) {
    if (candidate == nullptr || candidate->get_id() == commander_id ||
        candidate->has_component<Engine::Core::BuildingComponent>() ||
        candidate->has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    auto const* unit = candidate->get_component<Engine::Core::UnitComponent>();
    if (unit == nullptr || unit->health <= 0) {
      continue;
    }
    auto const* candidate_transform =
        candidate->get_component<Engine::Core::TransformComponent>();
    if (candidate_transform == nullptr) {
      continue;
    }
    float const coarse_dx = candidate_transform->position.x - origin.x();
    float const coarse_dz = candidate_transform->position.z - origin.z();

    constexpr float k_formation_spread_slack = 6.0F;
    float const coarse_range = k_body_separation_scan_range + k_formation_spread_slack;
    if ((coarse_dx * coarse_dx) + (coarse_dz * coarse_dz) >
        coarse_range * coarse_range) {
      continue;
    }

    for (auto const& soldier :
         Game::Systems::RpgCombat::live_soldier_targets(*candidate)) {
      QVector3D offset =
          origin - QVector3D(soldier.position.x(), 0.0F, soldier.position.z());
      float const min_distance =
          k_commander_body_radius + std::max(soldier.body_radius, 0.05F);
      float const distance_sq = offset.lengthSquared();
      if (distance_sq >= min_distance * min_distance) {
        continue;
      }

      float distance = std::sqrt(std::max(distance_sq, 0.0F));
      if (distance > 1.0e-4F) {
        offset /= distance;
      } else {

        auto const seed = static_cast<std::uint32_t>((commander_id * 73856093U) ^
                                                     (candidate->get_id() * 19349663U));
        float const angle = static_cast<float>(seed % 6283U) * 0.001F;
        offset = QVector3D(std::cos(angle), 0.0F, std::sin(angle));
        distance = 0.0F;
      }
      push += offset * (min_distance - distance);
    }
  }

  if (push.lengthSquared() <= 1.0e-8F) {
    return;
  }

  float const max_push = k_body_separation_max_push_per_second * dt;
  if (push.lengthSquared() > max_push * max_push) {
    push = push.normalized() * max_push;
  }

  float const target_x = transform.position.x + push.x();
  float const target_z = transform.position.z + push.z();
  auto const step = resolve_ground_step(
      transform.position.x, transform.position.z, target_x, target_z);
  transform.position.x = step.x;
  transform.position.z = step.z;
}

} // namespace

void CommanderControlController::reset() {
  m_input = {};
  m_mouse_center_valid = false;
  m_last_mouse_valid = false;
  m_mouse_warp_supported = false;
  m_mouse_recentering = false;
  m_camera_rig.reset();
  m_observed_action_hit_count = 0;
  m_move_speed = 0.0F;
  m_planar_speed_smooth = 0.0F;
  m_last_move_direction = QVector3D(0.0F, 0.0F, 1.0F);
  m_move_right_axis = 0;
  m_move_forward_axis = 0;
  m_move_running = false;
  m_dodge_state = DodgeState::None;
  m_dodge_timer = 0.0F;
  m_dodge_direction = QVector3D(0.0F, 0.0F, 1.0F);
  m_requested_dodge_direction = QVector3D(0.0F, 0.0F, 0.0F);
  m_has_requested_dodge_direction = false;
  m_dodge_fov_kick = 0.0F;
  m_jump_timer = 0.0F;
  m_jump_safe_position_valid = false;
  m_jump_last_walkable_position = QVector3D(0.0F, 0.0F, 0.0F);
  m_locked_target_id = 0;
  m_locked_target_slot = Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  m_soft_target_id = 0;
  m_soft_target_slot = Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  m_primary_target_slot = Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  m_lock_lost_timer = 0.0F;
  m_guard_was_active = false;
  m_combo_miss_timer = 0.0F;
  m_primary_held_duration = 0.0F;
  m_shield_bash_cooldown = 0.0F;
  m_vanguard_rush_cooldown = 0.0F;
  m_second_wind_cooldown = 0.0F;
}

void CommanderControlController::set_view_yaw(float yaw) {
  m_view_yaw = yaw;
}

void CommanderControlController::set_view_pitch(float pitch) {
  m_view_pitch = std::clamp(pitch, -70.0F, 70.0F);
}

auto CommanderControlController::view_yaw() const -> float {
  return m_view_yaw;
}

auto CommanderControlController::view_pitch() const -> float {
  return m_view_pitch;
}

auto CommanderControlController::input() -> InputState& {
  return m_input;
}

auto CommanderControlController::input() const -> const InputState& {
  return m_input;
}

void CommanderControlController::key_down(int key) {
  switch (key) {
  case Qt::Key_W:
    m_input.forward = true;
    break;
  case Qt::Key_S:
    m_input.backward = true;
    break;
  case Qt::Key_A:
    m_input.left = true;
    break;
  case Qt::Key_D:
    m_input.right = true;
    break;
  case Qt::Key_Q:
    m_input.turn_left = true;
    break;
  case Qt::Key_E:
    m_input.turn_right = true;
    break;
  case Qt::Key_Shift:
    m_input.run = true;
    break;
  default:
    break;
  }
}

void CommanderControlController::key_up(int key) {
  switch (key) {
  case Qt::Key_W:
    m_input.forward = false;
    break;
  case Qt::Key_S:
    m_input.backward = false;
    break;
  case Qt::Key_A:
    m_input.left = false;
    break;
  case Qt::Key_D:
    m_input.right = false;
    break;
  case Qt::Key_Q:
    m_input.turn_left = false;
    break;
  case Qt::Key_E:
    m_input.turn_right = false;
    break;
  case Qt::Key_Shift:
    m_input.run = false;
    break;
  default:
    break;
  }
}

void CommanderControlController::primary_action_down() {
  m_input.primary_action = true;
  m_input.primary_action_scan_cooldown = 0.08F;
}

void CommanderControlController::primary_action_up() {
  m_input.primary_action = false;
}

void CommanderControlController::secondary_action_down() {
  m_input.secondary_action = true;
}

void CommanderControlController::secondary_action_up() {
  m_input.secondary_action = false;
}

auto CommanderControlController::look_sensitivity_scale() const -> float {

  constexpr float k_half_degrees_to_radians = 0.008726646259971648F;
  const float hip = std::tan(k_fov_hip * k_half_degrees_to_radians);
  const float current = std::tan(std::clamp(m_camera_rig.fov(), 20.0F, 110.0F) *
                                 k_half_degrees_to_radians);
  const float zoom = std::clamp(current / std::max(hip, 1.0e-4F), 0.35F, 1.0F);

  constexpr float k_aim_steadiness = 0.20F;
  return zoom *
         (1.0F - (k_aim_steadiness * std::clamp(m_camera_rig.aim_blend(), 0.0F, 1.0F)));
}

void CommanderControlController::mouse_move(qreal dx, qreal dy) {
  constexpr float k_mouse_yaw_sensitivity = 0.18F;
  constexpr float k_mouse_pitch_sensitivity = 0.12F;
  const float sensitivity = look_sensitivity_scale();
  m_view_yaw += static_cast<float>(dx) * k_mouse_yaw_sensitivity * sensitivity;
  m_view_pitch -= static_cast<float>(dy) * k_mouse_pitch_sensitivity * sensitivity;
  m_view_yaw = std::fmod(m_view_yaw, 360.0F);
  if (m_view_yaw < 0.0F) {
    m_view_yaw += 360.0F;
  }
  m_view_pitch = std::clamp(m_view_pitch, -70.0F, 70.0F);
}

void CommanderControlController::mouse_look_at(
    qreal sx, qreal sy, qreal center_sx, qreal center_sy, QQuickWindow* window) {
  const qreal dx = sx - center_sx;
  const qreal dy = sy - center_sy;
  if (m_mouse_recentering && std::abs(dx) <= 2.0 && std::abs(dy) <= 2.0) {
    m_mouse_recentering = false;
    return;
  }
  m_mouse_recentering = false;

  if (std::abs(dx) > 0.5 || std::abs(dy) > 0.5) {
    mouse_move(dx, dy);
  }
  center_mouse(center_sx, center_sy, window);
}

void CommanderControlController::center_mouse(qreal center_sx,
                                              qreal center_sy,
                                              QQuickWindow* window) {
  if (window == nullptr) {
    return;
  }

  const QPoint local_center(static_cast<int>(std::round(center_sx)),
                            static_cast<int>(std::round(center_sy)));
  m_mouse_center = local_center;
  m_mouse_center_valid = true;
  const QPoint global_center = window->mapToGlobal(local_center);
  const QPoint current_global = QCursor::pos();
  if (current_global == global_center) {
    m_last_mouse_global = global_center;
    m_last_mouse_valid = true;
    m_mouse_warp_supported = true;
    m_mouse_recentering = false;
    return;
  }

  QCursor::setPos(global_center);
  m_mouse_warp_supported = (QCursor::pos() == global_center);
  m_last_mouse_global = m_mouse_warp_supported ? global_center : current_global;
  m_last_mouse_valid = true;
  m_mouse_recentering = false;
}

void CommanderControlController::poll_mouse_look(QQuickWindow* window) {
  if (window == nullptr || !window->isActive()) {
    return;
  }

  const QPoint current_global = QCursor::pos();
  if (!m_last_mouse_valid) {
    m_last_mouse_global = current_global;
    m_last_mouse_valid = true;
    return;
  }

  const QPoint delta = current_global - m_last_mouse_global;
  if (!delta.isNull()) {
    mouse_move(delta.x(), delta.y());
  }

  if (m_mouse_warp_supported && m_mouse_center_valid) {
    const QPoint global_center = window->mapToGlobal(m_mouse_center);
    if (current_global != global_center) {
      QCursor::setPos(global_center);
      m_last_mouse_global = global_center;
      return;
    }
  }

  m_last_mouse_global = current_global;
}

void CommanderControlController::request_dodge() {
  m_has_requested_dodge_direction = false;
  m_input.dodge_requested = true;
}

void CommanderControlController::request_dodge(const QVector3D& world_direction) {
  m_requested_dodge_direction =
      QVector3D(world_direction.x(), 0.0F, world_direction.z());
  m_has_requested_dodge_direction =
      m_requested_dodge_direction.lengthSquared() > 0.0001F;
  m_input.dodge_requested = true;
}

void CommanderControlController::request_jump() {
  m_input.jump_requested = true;
}

void CommanderControlController::special_action() {
  m_input.shield_bash_requested = true;
}

void CommanderControlController::request_vanguard_rush() {
  m_input.vanguard_rush_requested = true;
}

void CommanderControlController::request_second_wind() {
  m_input.second_wind_requested = true;
}

void CommanderControlController::toggle_close_camera_mode(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id) const {
  auto* commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return;
  }
  if (auto* cmd = commander->get_component<Engine::Core::CommanderComponent>()) {
    cmd->close_camera_mode = !cmd->close_camera_mode;
  }
}

void CommanderControlController::toggle_weapon_stance(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id) const {
  auto* commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return;
  }
  if (Game::Systems::RpgCombat::toggle_weapon_stance(*commander)) {
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_guard_raise);
  } else {
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_ability_refused);
  }
}

auto CommanderControlController::locked_target_id() const -> Engine::Core::EntityID {
  return m_locked_target_id;
}

auto CommanderControlController::focus_target_id() const -> Engine::Core::EntityID {
  return m_locked_target_id;
}

void CommanderControlController::cycle_lock_on_target(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id) {
  auto* commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return;
  }
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  constexpr float k_lock_range = 12.0F;
  constexpr float k_lock_range_sq = k_lock_range * k_lock_range;
  constexpr float k_lock_max_angle_degrees = 70.0F;

  auto& owners = Game::Systems::OwnerRegistry::instance();
  const QVector3D origin(transform->position.x, 0.0F, transform->position.z);

  struct Candidate {
    Engine::Core::EntityID id = 0;
    std::uint16_t soldier_slot{
        Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot};
    float angle_diff = 0.0F;
    float distance_sq = 0.0F;
    bool visible = false;
  };
  std::vector<Candidate> candidates;

  for (auto* candidate : world.get_entities_with<Engine::Core::UnitComponent>()) {
    if (candidate == nullptr || candidate->get_id() == commander_id) {
      continue;
    }
    auto* u = candidate->get_component<Engine::Core::UnitComponent>();
    auto* t = candidate->get_component<Engine::Core::TransformComponent>();
    if (u == nullptr || t == nullptr || u->health <= 0) {
      continue;
    }
    if (!owners.are_enemies(local_owner_id, u->owner_id)) {
      continue;
    }
    std::optional<Candidate> best_soldier;
    for (auto const& soldier :
         Game::Systems::RpgCombat::live_soldier_targets(*candidate)) {
      const float dx = soldier.position.x() - origin.x();
      const float dz = soldier.position.z() - origin.z();
      const float distance_sq = dx * dx + dz * dz;
      if (distance_sq > k_lock_range_sq) {
        continue;
      }
      const float angle_diff =
          signed_angle_delta(std::atan2(dx, dz) * 57.29577951308232F, m_view_yaw);
      if (std::abs(angle_diff) > k_lock_max_angle_degrees ||
          !Game::Systems::has_clear_building_los(origin, soldier.position)) {
        continue;
      }
      Candidate const resolved{
          candidate->get_id(), soldier.soldier_slot, angle_diff, distance_sq, true};
      if (!best_soldier.has_value() ||
          std::abs(resolved.angle_diff) < std::abs(best_soldier->angle_diff) ||
          (std::abs(resolved.angle_diff) == std::abs(best_soldier->angle_diff) &&
           resolved.distance_sq < best_soldier->distance_sq)) {
        best_soldier = resolved;
      }
    }
    if (best_soldier.has_value()) {
      candidates.push_back(*best_soldier);
    }
  }

  if (candidates.empty()) {
    m_locked_target_id = 0;
    m_locked_target_slot = Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
    m_soft_target_id = 0;
    m_soft_target_slot = Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
    m_lock_lost_timer = 0.0F;
    if (auto* targets =
            commander->get_component<Engine::Core::RpgCommanderTargetComponent>()) {
      targets->explicit_lock_target_id = 0;
      targets->explicit_lock_soldier_slot =
          Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
      targets->aim_candidate_id = 0;
      targets->aim_candidate_soldier_slot =
          Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
      targets->aim_candidate_in_range = false;
    }
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_ability_refused);
    return;
  }

  std::stable_sort(
      candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        if (a.visible != b.visible) {
          return a.visible && !b.visible;
        }
        if (std::abs(a.angle_diff) != std::abs(b.angle_diff)) {
          return std::abs(a.angle_diff) < std::abs(b.angle_diff);
        }
        return a.distance_sq < b.distance_sq;
      });

  if (m_locked_target_id == 0) {
    m_locked_target_id = candidates[0].id;
    m_locked_target_slot = candidates[0].soldier_slot;
  } else {
    auto it =
        std::find_if(candidates.begin(), candidates.end(), [this](const Candidate& c) {
          return c.id == m_locked_target_id;
        });
    if (it == candidates.end() || std::next(it) == candidates.end()) {
      m_locked_target_id = candidates[0].id;
      m_locked_target_slot = candidates[0].soldier_slot;
    } else {
      auto const& next = *std::next(it);
      m_locked_target_id = next.id;
      m_locked_target_slot = next.soldier_slot;
    }
  }
  m_soft_target_id = m_locked_target_id;
  m_soft_target_slot = m_locked_target_slot;
  m_lock_lost_timer = 0.0F;
  Game::Audio::play_cue(Game::Audio::Cue::k_combat_lock_on);
  if (auto* rpg_targets =
          Engine::Core::get_or_add_component<Engine::Core::RpgCommanderTargetComponent>(
              commander)) {
    rpg_targets->explicit_lock_target_id = m_locked_target_id;
    rpg_targets->explicit_lock_soldier_slot = m_locked_target_slot;
    rpg_targets->aim_candidate_id = m_soft_target_id;
    rpg_targets->aim_candidate_soldier_slot = m_soft_target_slot;
  }
}

void CommanderControlController::apply_strike_lunge(
    Engine::Core::World& world,
    Engine::Core::Entity& commander,
    Engine::Core::TransformComponent& transform,
    float dt) {
  if (dt <= 0.0F || m_dodge_state != DodgeState::None) {
    return;
  }

  auto const* action =
      commander.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action == nullptr || !action->action_running || action->cancel_window_active) {
    return;
  }

  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      static_cast<Game::Systems::CombatActions::CombatActionId>(
          action->combat_action_id));
  if (definition == nullptr || definition->requires_projectile_release) {
    return;
  }

  auto* target = world.get_entity(action->active_target_id);
  auto const* target_unit = target != nullptr
                                ? target->get_component<Engine::Core::UnitComponent>()
                                : nullptr;
  if (target_unit == nullptr || target_unit->health <= 0) {
    return;
  }
  auto const sample = Game::Systems::RpgCombat::resolve_soldier_target(
      *target, action->active_target_soldier_slot);
  auto const* target_transform =
      target->get_component<Engine::Core::TransformComponent>();
  if (!sample.has_value() && target_transform == nullptr) {
    return;
  }

  const float target_x =
      sample.has_value() ? sample->position.x() : target_transform->position.x;
  const float target_z =
      sample.has_value() ? sample->position.z() : target_transform->position.z;

  const float to_x = target_x - transform.position.x;
  const float to_z = target_z - transform.position.z;
  const float distance = std::hypot(to_x, to_z);

  constexpr float k_lunge_contact_margin = 0.55F;
  const float contact_distance =
      std::max(k_lunge_contact_margin, definition->hit_shape.reach * 0.72F);

  const float gap = distance - contact_distance;
  if (distance <= 0.0001F || gap <= 0.02F || gap > k_strike_step_reach) {
    return;
  }

  float trace_end = 0.60F;
  for (auto const& event : definition->events) {
    if (event.type ==
        Game::Systems::CombatActions::CombatActionEventType::WeaponTraceEnd) {
      trace_end = event.normalized_time;
      break;
    }
  }
  const float swing_progress =
      trace_end > 0.0F
          ? std::clamp(action->normalized_action_time / trace_end, 0.0F, 1.0F)
          : 1.0F;
  const float lunge_shape = std::sin(swing_progress * std::numbers::pi_v<float>);
  if (lunge_shape <= 0.01F) {
    return;
  }

  constexpr float k_lunge_speed = 4.6F;
  const float step = std::min(gap, k_lunge_speed * lunge_shape * dt);
  const float step_x = transform.position.x + (to_x / distance) * step;
  const float step_z = transform.position.z + (to_z / distance) * step;
  auto const resolved =
      resolve_ground_step(transform.position.x, transform.position.z, step_x, step_z);
  if (!resolved.moved) {
    return;
  }
  transform.position.x = resolved.x;
  transform.position.z = resolved.z;
}

void CommanderControlController::update_lock_on_yaw(Engine::Core::World& world,
                                                    Engine::Core::Entity& commander,
                                                    float dt) {
  if (m_locked_target_id == 0) {
    return;
  }

  auto* cmd_transform = commander.get_component<Engine::Core::TransformComponent>();
  if (cmd_transform == nullptr) {
    return;
  }

  auto* target = world.get_entity(m_locked_target_id);
  auto* target_unit = (target != nullptr)
                          ? target->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
  auto target_sample = target != nullptr
                           ? Game::Systems::RpgCombat::resolve_soldier_target(
                                 *target, m_locked_target_slot)
                           : std::nullopt;
  if (target_unit == nullptr || target_unit->health <= 0 ||
      !target_sample.has_value()) {
    m_locked_target_id = 0;
    m_locked_target_slot = Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
    m_lock_lost_timer = 0.0F;
    return;
  }
  m_locked_target_slot = target_sample->soldier_slot;

  const float dx = target_sample->position.x() - cmd_transform->position.x;
  const float dz = target_sample->position.z() - cmd_transform->position.z;
  constexpr float k_lock_drop_sq = 18.0F * 18.0F;
  const QVector3D origin(cmd_transform->position.x, 0.0F, cmd_transform->position.z);
  const QVector3D target_pos = target_sample->position;
  const bool target_visible = Game::Systems::has_clear_building_los(origin, target_pos);

  const float target_yaw = std::atan2(dx, dz) * 57.29577951308232F;
  const float diff = signed_angle_delta(target_yaw, m_view_yaw);
  const bool escape_input =
      (m_input.run && m_input.backward) ||
      (m_input.dodge_requested && (m_input.backward || m_input.run));
  if (escape_input || (m_input.run && std::abs(diff) > 95.0F)) {
    m_locked_target_id = 0;
    m_locked_target_slot = Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
    m_soft_target_id = 0;
    m_soft_target_slot = Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
    m_lock_lost_timer = 0.0F;
    return;
  }

  if (dx * dx + dz * dz > k_lock_drop_sq) {
    m_lock_lost_timer += dt * 2.0F;
  } else if (!target_visible) {
    m_lock_lost_timer += dt;
  } else {
    m_lock_lost_timer = 0.0F;
  }
  if (m_lock_lost_timer > 0.35F) {
    m_locked_target_id = 0;
    m_locked_target_slot = Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
    m_lock_lost_timer = 0.0F;
    return;
  }

  const float k_lock_spring = target_visible ? 8.5F : 3.5F;
  m_view_yaw += diff * (1.0F - std::exp(-k_lock_spring * dt));
  m_view_yaw = wrap_angle_degrees(m_view_yaw);
}

auto CommanderControlController::controlled_commander(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id) const -> Engine::Core::Entity* {
  auto* entity = world.get_entity(commander_id);
  if (entity == nullptr) {
    return nullptr;
  }

  auto* unit = entity->get_component<Engine::Core::UnitComponent>();
  auto* transform = entity->get_component<Engine::Core::TransformComponent>();
  if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
      unit->owner_id != local_owner_id ||
      entity->get_component<Engine::Core::CommanderComponent>() == nullptr) {
    return nullptr;
  }
  return entity;
}

auto CommanderControlController::find_primary_target(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id,
    float extra_reach) -> Engine::Core::EntityID {
  using Target = Game::Systems::RpgCombat::SoldierTarget;
  constexpr auto k_no_slot =
      Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  m_primary_target_slot = k_no_slot;

  auto* commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return 0;
  }

  auto* commander_transform =
      commander->get_component<Engine::Core::TransformComponent>();
  auto* commander_attack = commander->get_component<Engine::Core::AttackComponent>();
  if (commander_transform == nullptr) {
    return 0;
  }

  constexpr float k_commander_attack_cone_dot = 0.45F;
  float max_range = 2.05F;
  if (commander_attack != nullptr) {
    auto const* commander_unit =
        commander->get_component<Engine::Core::UnitComponent>();
    auto const family = commander_unit != nullptr
                            ? Engine::Core::resolve_combat_attack_family(
                                  commander_unit->spawn_type,
                                  Engine::Core::AttackComponent::CombatMode::Melee)
                            : Engine::Core::CombatAttackFamily::Sword;
    if (family == Engine::Core::CombatAttackFamily::Spear) {
      max_range = 2.75F;
    } else if (family == Engine::Core::CombatAttackFamily::Bow &&
               commander_attack->can_ranged) {
      max_range = commander_attack->range;
    } else {
      max_range = std::max(max_range, commander_attack->melee_range);
    }
  }
  max_range += std::max(0.0F, extra_reach);

  const float yaw_rad = m_view_yaw * k_degrees_to_radians;
  const QVector3D forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  const QVector3D origin(
      commander_transform->position.x, 0.0F, commander_transform->position.z);
  auto& owners = Game::Systems::OwnerRegistry::instance();

  auto eligible_samples = [&](Engine::Core::EntityID entity_id) -> std::vector<Target> {
    auto* entity = world.get_entity(entity_id);
    auto* unit = entity != nullptr
                     ? entity->get_component<Engine::Core::UnitComponent>()
                     : nullptr;
    if (entity == nullptr || unit == nullptr || unit->health <= 0 ||
        entity->has_component<Engine::Core::BuildingComponent>() ||
        !owners.are_enemies(local_owner_id, unit->owner_id)) {
      return {};
    }
    return Game::Systems::RpgCombat::live_soldier_targets(*entity);
  };

  auto choose_sample = [&](Engine::Core::EntityID entity_id,
                           std::uint16_t preferred_slot,
                           float minimum_facing) -> std::optional<Target> {
    auto samples = eligible_samples(entity_id);
    std::optional<Target> best;
    float best_score = -1000000.0F;
    for (auto const& sample : samples) {
      QVector3D to_target = sample.position - origin;
      to_target.setY(0.0F);
      float const distance = to_target.length();
      if (distance <= 0.0001F ||
          distance > max_range + std::max(0.0F, sample.body_radius) ||
          !Game::Systems::has_clear_building_los(origin, sample.position)) {
        continue;
      }
      to_target /= distance;
      float const facing = QVector3D::dotProduct(forward, to_target);
      if (facing < minimum_facing) {
        continue;
      }
      float score = facing * 10.0F - distance;
      if (sample.soldier_slot == preferred_slot) {
        score += 3.0F;
      }
      if (!best.has_value() || score > best_score) {
        best = sample;
        best_score = score;
      }
    }
    return best;
  };

  auto accept = [&](const Target& target) -> Engine::Core::EntityID {
    m_primary_target_slot = target.soldier_slot;
    m_soft_target_id = target.entity_id;
    m_soft_target_slot = target.soldier_slot;
    return target.entity_id;
  };

  if (m_locked_target_id != 0) {
    if (auto target = choose_sample(m_locked_target_id, m_locked_target_slot, -0.05F);
        target.has_value()) {
      m_locked_target_slot = target->soldier_slot;
      return accept(*target);
    }
    m_soft_target_id = 0;
    m_soft_target_slot = k_no_slot;
    return 0;
  }

  if (m_soft_target_id != 0) {
    if (auto target = choose_sample(m_soft_target_id, m_soft_target_slot, 0.15F);
        target.has_value()) {
      return accept(*target);
    }
  }

  if (auto* engagement =
          commander->get_component<Engine::Core::RpgEngagementComponent>()) {
    std::array<Engine::Core::EntityID, 3> const ring_targets = {
        engagement->front_attacker_id,
        engagement->left_threat_id,
        engagement->right_threat_id};
    for (auto const ring_target_id : ring_targets) {
      if (auto target = choose_sample(ring_target_id, k_no_slot, 0.05F);
          target.has_value()) {
        return accept(*target);
      }
    }
  }

  std::optional<Target> best;
  float best_score = -1000000.0F;
  for (auto* candidate : world.get_entities_with<Engine::Core::UnitComponent>()) {
    if (candidate == nullptr || candidate == commander) {
      continue;
    }
    auto target =
        choose_sample(candidate->get_id(), k_no_slot, k_commander_attack_cone_dot);
    if (!target.has_value()) {
      continue;
    }
    QVector3D to_target = target->position - origin;
    to_target.setY(0.0F);
    float const distance = to_target.length();
    to_target /= std::max(distance, 0.0001F);
    float const score = QVector3D::dotProduct(forward, to_target) * 10.0F - distance;
    if (score > best_score) {
      best_score = score;
      best = *target;
    }
  }

  if (best.has_value()) {
    return accept(*best);
  }
  m_soft_target_id = 0;
  m_soft_target_slot = k_no_slot;
  return 0;
}

auto CommanderControlController::primary_action(Engine::Core::World& world,
                                                Engine::Core::EntityID commander_id,
                                                int local_owner_id) -> bool {
  auto* commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return false;
  }

  auto const* aim = commander->get_component<Engine::Core::RpgCommanderAimComponent>();
  bool const shooting =
      aim != nullptr && aim->stance == Engine::Core::FpvWeaponStance::Bow;

  Engine::Core::EntityID target_id = 0;
  if (shooting) {
    m_primary_target_slot =
        Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  } else {
    target_id = find_primary_target(
        world, commander_id, local_owner_id, k_strike_acquisition_bonus);
  }
  auto const attack_result =
      Game::Systems::CombatActions::CombatActionService::request_attack(
          world,
          {.attacker_id = commander_id,
           .target_hint_id = target_id,
           .target_soldier_slot = m_primary_target_slot,
           .move_right_axis = m_move_right_axis,
           .move_forward_axis = m_move_forward_axis,
           .primary_held_duration = m_primary_held_duration});
  if (!attack_result.accepted) {
    return false;
  }

  if (auto* cmd_comp = commander->get_component<Engine::Core::CommanderComponent>();
      cmd_comp != nullptr && !attack_result.buffered) {
    m_combo_miss_timer = 0.0F;
  }
  return true;
}

void CommanderControlController::release_guard(Engine::Core::World& world,
                                               Engine::Core::EntityID commander_id,
                                               int local_owner_id) {
  if (auto* commander = controlled_commander(world, commander_id, local_owner_id)) {
    if (auto* guard =
            commander->get_component<Engine::Core::CommanderGuardComponent>()) {
      guard->active = false;
      guard->perfect_guard_remaining = 0.0F;
    }
  }
  m_guard_was_active = false;
}

auto CommanderControlController::resolve_ability_target(Engine::Core::World& world,
                                                        Engine::Core::Entity& commander,
                                                        int local_owner_id,
                                                        float max_range) const
    -> Engine::Core::EntityID {
  auto* transform = commander.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return 0;
  }

  const QVector3D origin(transform->position.x, 0.0F, transform->position.z);
  const float max_range_sq = max_range * max_range;
  auto& owners = Game::Systems::OwnerRegistry::instance();

  auto qualifies = [&](Engine::Core::EntityID candidate_id) -> bool {
    auto* candidate = world.get_entity(candidate_id);
    auto* candidate_unit = (candidate != nullptr)
                               ? candidate->get_component<Engine::Core::UnitComponent>()
                               : nullptr;
    auto* candidate_transform =
        (candidate != nullptr)
            ? candidate->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    if (candidate_unit == nullptr || candidate_transform == nullptr ||
        candidate_unit->health <= 0 ||
        !owners.are_enemies(local_owner_id, candidate_unit->owner_id)) {
      return false;
    }

    const QVector3D target(
        candidate_transform->position.x, 0.0F, candidate_transform->position.z);
    return (target - origin).lengthSquared() <= max_range_sq &&
           Game::Systems::has_clear_building_los(origin, target);
  };

  if (m_locked_target_id != 0 && qualifies(m_locked_target_id)) {
    return m_locked_target_id;
  }
  if (m_soft_target_id != 0 && qualifies(m_soft_target_id)) {
    return m_soft_target_id;
  }
  return 0;
}

void CommanderControlController::update_ability_cooldowns(
    Engine::Core::CommanderComponent* commander, float dt) {
  auto decay = [dt](float& cooldown) {
    if (cooldown > 0.0F) {
      cooldown = std::max(0.0F, cooldown - dt);
    }
  };
  decay(m_shield_bash_cooldown);
  decay(m_vanguard_rush_cooldown);
  decay(m_second_wind_cooldown);

  if (commander != nullptr) {
    commander->shield_bash_cooldown_remaining = m_shield_bash_cooldown;
    commander->vanguard_rush_cooldown_remaining = m_vanguard_rush_cooldown;
    commander->second_wind_cooldown_remaining = m_second_wind_cooldown;
  }
}

void CommanderControlController::try_activate_shield_bash(
    Engine::Core::World& world,
    Engine::Core::Entity& commander,
    Engine::Core::EntityID commander_id,
    int local_owner_id) {
  const bool bash_requested = m_input.shield_bash_requested;
  m_input.shield_bash_requested = false;
  if (!bash_requested) {
    return;
  }

  auto* guard = commander.get_component<Engine::Core::CommanderGuardComponent>();
  auto* cmd_comp = commander.get_component<Engine::Core::CommanderComponent>();
  constexpr float k_bash_range = 2.5F;
  constexpr float k_bash_stagger_duration = 0.5F;
  constexpr float k_bash_cooldown = 3.0F;
  if (guard == nullptr || !guard->active || m_shield_bash_cooldown > 0.0F ||
      m_jump_timer > 0.0F) {
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_ability_refused);
    return;
  }

  auto* transform = commander.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  auto& owners = Game::Systems::OwnerRegistry::instance();
  const QVector3D cmd_pos(
      transform->position.x, transform->position.y, transform->position.z);
  for (auto* entity : world.get_entities_with<Engine::Core::UnitComponent>()) {
    if (entity == nullptr || entity->get_id() == commander_id) {
      continue;
    }
    auto* unit = entity->get_component<Engine::Core::UnitComponent>();
    auto* ent_tf = entity->get_component<Engine::Core::TransformComponent>();
    if (unit == nullptr || ent_tf == nullptr || unit->health <= 0 ||
        !owners.are_enemies(local_owner_id, unit->owner_id)) {
      continue;
    }
    const QVector3D epos(ent_tf->position.x, ent_tf->position.y, ent_tf->position.z);
    if ((epos - cmd_pos).length() > k_bash_range) {
      continue;
    }
    if (auto* existing_stagger =
            entity->get_component<Engine::Core::StaggerComponent>()) {
      existing_stagger->remaining =
          std::max(existing_stagger->remaining, k_bash_stagger_duration);
    } else {
      entity->add_component<Engine::Core::StaggerComponent>(k_bash_stagger_duration);
    }
    if (auto* enemy_cmd = entity->get_component<Engine::Core::CommanderComponent>()) {
      enemy_cmd->punish_window_remaining =
          std::max(enemy_cmd->punish_window_remaining, 0.75F);
    }
  }

  m_shield_bash_cooldown = k_bash_cooldown;
  Game::Audio::play_cue(Game::Audio::Cue::k_combat_shield_bash);
  if (cmd_comp != nullptr) {
    cmd_comp->shield_bash_cooldown_remaining = m_shield_bash_cooldown;
  }
}

void CommanderControlController::try_activate_vanguard_rush(
    Engine::Core::World& world,
    Engine::Core::Entity& commander,
    Engine::Core::EntityID commander_id,
    int local_owner_id) {
  const bool rush_requested = m_input.vanguard_rush_requested;
  m_input.vanguard_rush_requested = false;
  if (!rush_requested) {
    return;
  }
  if (m_vanguard_rush_cooldown > 0.0F || m_dodge_state != DodgeState::None ||
      m_jump_timer > 0.0F) {
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_ability_refused);
    return;
  }

  auto* transform = commander.get_component<Engine::Core::TransformComponent>();
  auto* unit = commander.get_component<Engine::Core::UnitComponent>();
  auto* movement = commander.get_component<Engine::Core::MovementComponent>();
  auto* combat_state = commander.get_component<Engine::Core::CombatStateComponent>();
  auto* cmd_comp = commander.get_component<Engine::Core::CommanderComponent>();
  if (transform == nullptr || unit == nullptr ||
      (combat_state != nullptr &&
       combat_state->animation_state != Engine::Core::CombatAnimationState::Idle)) {
    return;
  }

  constexpr float k_rush_cooldown = 4.5F;
  constexpr float k_rush_max_range = 8.0F;
  constexpr float k_rush_stop_distance = 1.35F;
  constexpr float k_rush_default_distance = 3.6F;
  constexpr int k_rush_damage = 18;
  constexpr float k_rush_stagger_duration = 0.35F;

  const QVector3D start(
      transform->position.x, transform->position.y, transform->position.z);
  const float yaw_rad = m_view_yaw * k_degrees_to_radians;
  QVector3D rush_direction(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  float rush_distance = k_rush_default_distance;

  if (Game::Units::is_cavalry(unit->spawn_type) && movement != nullptr) {
    movement->set_manual_velocity(rush_direction.x() * 8.0F, rush_direction.z() * 8.0F);
    if (Game::Systems::Combat::request_mounted_charge(
            commander, Engine::Core::MountedChargeIntentSource::Player)) {
      m_vanguard_rush_cooldown = k_rush_cooldown;
      Game::Audio::play_cue(Game::Audio::Cue::k_combat_vanguard_rush);
      m_input.primary_action_scan_cooldown = 0.18F;
      if (cmd_comp != nullptr) {
        cmd_comp->vanguard_rush_cooldown_remaining = m_vanguard_rush_cooldown;
      }
    }
    return;
  }

  Engine::Core::Entity* target = nullptr;
  const auto target_id =
      resolve_ability_target(world, commander, local_owner_id, k_rush_max_range);
  if (target_id != 0) {
    target = world.get_entity(target_id);
    auto* target_transform =
        (target != nullptr) ? target->get_component<Engine::Core::TransformComponent>()
                            : nullptr;
    if (target_transform != nullptr) {
      QVector3D const to_target(target_transform->position.x - start.x(),
                                0.0F,
                                target_transform->position.z - start.z());
      if (to_target.lengthSquared() > 0.0001F) {
        const float target_distance = std::sqrt(to_target.lengthSquared());
        rush_direction = to_target / target_distance;
        rush_distance = std::clamp(target_distance - k_rush_stop_distance,
                                   1.4F,
                                   k_rush_default_distance + 0.4F);
      }
    }
  }

  const QVector3D desired = start + rush_direction * rush_distance;
  const QVector3D resolved =
      resolve_reachable_ground_position(start, desired, commander_id);
  transform->position.x = resolved.x();
  transform->position.z = resolved.z();
  if (movement != nullptr) {
    movement->set_manual_velocity(rush_direction.x() * 8.0F, rush_direction.z() * 8.0F);
  }
  m_input.primary_action_scan_cooldown = 0.18F;

  if (target != nullptr) {
    auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
    auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
    if (target_unit != nullptr && target_transform != nullptr &&
        target_unit->health > 0) {
      const QVector3D target_pos(target_transform->position.x,
                                 target_transform->position.y,
                                 target_transform->position.z);
      if ((target_pos - resolved).length() <= 2.35F &&
          Game::Systems::has_clear_building_los(resolved, target_pos)) {
        Game::Systems::RpgCombat::deal_commander_attack_damage(
            &world, target, k_rush_damage, commander_id);
        if (target_unit->health > 0) {
          if (auto* stagger = target->get_component<Engine::Core::StaggerComponent>()) {
            stagger->remaining = std::max(stagger->remaining, k_rush_stagger_duration);
          } else {
            target->add_component<Engine::Core::StaggerComponent>(
                k_rush_stagger_duration);
          }
          if (auto* target_cmd =
                  target->get_component<Engine::Core::CommanderComponent>()) {
            target_cmd->punish_window_remaining =
                std::max(target_cmd->punish_window_remaining, 0.85F);
          }
        }
      }
    }
  }

  m_vanguard_rush_cooldown = k_rush_cooldown;
  Game::Audio::play_cue(Game::Audio::Cue::k_combat_vanguard_rush);
  if (cmd_comp != nullptr) {
    cmd_comp->vanguard_rush_cooldown_remaining = m_vanguard_rush_cooldown;
  }
}

void CommanderControlController::try_activate_second_wind(
    Engine::Core::Entity& commander) {
  const bool second_wind_requested = m_input.second_wind_requested;
  m_input.second_wind_requested = false;
  if (!second_wind_requested) {
    return;
  }
  if (m_second_wind_cooldown > 0.0F || m_dodge_state != DodgeState::None ||
      m_jump_timer > 0.0F) {
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_ability_refused);
    return;
  }

  auto* cmd_comp = commander.get_component<Engine::Core::CommanderComponent>();
  auto* combat_state = commander.get_component<Engine::Core::CombatStateComponent>();
  if (cmd_comp == nullptr ||
      (combat_state != nullptr &&
       combat_state->animation_state != Engine::Core::CombatAnimationState::Idle)) {
    return;
  }

  constexpr float k_second_wind_cooldown = 8.0F;
  constexpr float k_second_wind_posture_restore = 55.0F;
  constexpr float k_second_wind_stamina_restore = 35.0F;
  constexpr float k_second_wind_guard_window = 0.35F;

  cmd_comp->posture = std::max(0.0F, cmd_comp->posture - k_second_wind_posture_restore);
  if (auto* stamina = commander.get_component<Engine::Core::StaminaComponent>()) {
    stamina->stamina = std::min(stamina->max_stamina,
                                stamina->stamina + k_second_wind_stamina_restore);
  }
  auto* guard = commander.get_component<Engine::Core::CommanderGuardComponent>();
  if (guard == nullptr) {
    guard = commander.add_component<Engine::Core::CommanderGuardComponent>();
  }
  if (guard != nullptr && guard->guard_break_remaining <= 0.0F) {
    guard->perfect_guard_remaining =
        std::max(guard->perfect_guard_remaining, k_second_wind_guard_window);
  }

  m_second_wind_cooldown = k_second_wind_cooldown;
  Game::Audio::play_cue(Game::Audio::Cue::k_combat_second_wind);
  cmd_comp->second_wind_cooldown_remaining = m_second_wind_cooldown;
}

auto CommanderControlController::update(Engine::Core::World& world,
                                        Engine::Core::EntityID commander_id,
                                        int local_owner_id,
                                        Render::GL::Camera& camera,
                                        float dt) -> bool {
  auto* commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return false;
  }

  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  auto* unit = commander->get_component<Engine::Core::UnitComponent>();
  auto* combat_state = commander->get_component<Engine::Core::CombatStateComponent>();
  auto* cmd_comp = commander->get_component<Engine::Core::CommanderComponent>();
  if (transform == nullptr || unit == nullptr) {
    return false;
  }

  auto* movement = commander->get_component<Engine::Core::MovementComponent>();
  if (movement == nullptr) {
    movement = commander->add_component<Engine::Core::MovementComponent>();
  }
  auto* guard = commander->get_component<Engine::Core::CommanderGuardComponent>();

  auto const* aim_state =
      commander->get_component<Engine::Core::RpgCommanderAimComponent>();
  bool const bow_stance =
      aim_state != nullptr && aim_state->stance == Engine::Core::FpvWeaponStance::Bow;
  bool const drawing_bow = aim_state != nullptr && aim_state->is_drawing();

  if (cmd_comp != nullptr && cmd_comp->flag_rally_in_progress &&
      !cmd_comp->fpv_controlled) {
    update_ability_cooldowns(cmd_comp, dt);
    cmd_comp->fpv_motion_vx = 0.0F;
    cmd_comp->fpv_motion_vz = 0.0F;

    m_input.primary_action = false;
    m_input.secondary_action = false;
    m_input.dodge_requested = false;
    m_input.jump_requested = false;
    m_input.shield_bash_requested = false;
    m_input.vanguard_rush_requested = false;
    m_input.second_wind_requested = false;
    m_move_speed = 0.0F;
    m_move_right_axis = 0;
    m_move_forward_axis = 0;
    m_move_running = false;
    m_guard_was_active = false;
    m_view_yaw = wrap_angle_degrees(transform->rotation.y);

    if (movement != nullptr) {
      movement->set_manual_velocity(0.0F, 0.0F);
    }
    if (guard != nullptr) {
      guard->active = false;
      guard->perfect_guard_remaining =
          std::max(0.0F, guard->perfect_guard_remaining - dt);
      guard->guard_break_remaining = std::max(0.0F, guard->guard_break_remaining - dt);
      guard->rearm_requires_release = false;
    }
    if (auto* rpg = commander->get_component<Engine::Core::RpgHealthComponent>()) {
      rpg->dodge_invincible = false;
    }

    update_camera(world, *commander, camera, dt);
    return true;
  }

  if (movement != nullptr) {
    movement->stop();
    movement->set_rest_position(transform->position.x, transform->position.z);
  }

  update_lock_on_yaw(world, *commander, dt);

  constexpr float k_degrees_to_radians = 0.017453292519943295F;
  constexpr float k_turn_speed_degrees = 105.0F;
  if (m_locked_target_id == 0) {
    if (m_input.turn_left) {
      m_view_yaw -= k_turn_speed_degrees * dt;
    }
    if (m_input.turn_right) {
      m_view_yaw += k_turn_speed_degrees * dt;
    }
  }
  m_view_yaw = wrap_angle_degrees(m_view_yaw);

  const int forward_axis = (m_input.forward ? 1 : 0) - (m_input.backward ? 1 : 0);
  const int right_axis = (m_input.right ? 1 : 0) - (m_input.left ? 1 : 0);

  const float yaw_rad = m_view_yaw * k_degrees_to_radians;
  const QVector3D forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  const QVector3D right(-forward.z(), 0.0F, forward.x());
  QVector3D move = forward * static_cast<float>(forward_axis) +
                   right * static_cast<float>(right_axis);

  float actual_speed_for_bob = 0.0F;
  bool run_for_bob = false;

  constexpr float k_fov_kick_decay = 22.0F;
  m_dodge_fov_kick = std::max(0.0F, m_dodge_fov_kick - k_fov_kick_decay * dt);
  constexpr float k_jump_duration = 0.58F;
  constexpr float k_jump_peak_height = 0.34F;

  const bool ability_requested = m_input.shield_bash_requested ||
                                 m_input.vanguard_rush_requested ||
                                 m_input.second_wind_requested;
  const bool jump_blocked_by_action =
      m_dodge_state != DodgeState::None || m_input.primary_action ||
      m_input.secondary_action || ability_requested ||
      (combat_state != nullptr &&
       combat_state->animation_state != Engine::Core::CombatAnimationState::Idle);
  const bool should_jump =
      m_input.jump_requested && m_jump_timer <= 0.0F && !jump_blocked_by_action;
  const bool jump_refused = m_input.jump_requested && !should_jump;
  m_input.jump_requested = false;
  if (jump_refused) {
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_ability_refused);
  }
  if (should_jump) {
    m_jump_timer = k_jump_duration;
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_jump);
    m_jump_safe_position_valid = true;
    m_jump_last_walkable_position =
        QVector3D(transform->position.x, transform->position.y, transform->position.z);

    if (auto* stamina = commander->get_component<Engine::Core::StaminaComponent>()) {
      stamina->stamina = std::max(
          0.0F,
          stamina->stamina - Engine::Core::CombatStateComponent::k_stamina_cost_jump);
    }
  }

  float jump_phase = 0.0F;
  float jump_height_offset = 0.0F;
  if (m_jump_timer > 0.0F) {
    m_jump_timer = std::max(0.0F, m_jump_timer - dt);
    if (m_jump_timer <= 0.0F) {
      Game::Audio::play_cue(Game::Audio::Cue::k_combat_land);
    }
    jump_phase = 1.0F - (m_jump_timer / k_jump_duration);
    const float normalized_phase = std::clamp(jump_phase, 0.0F, 1.0F);
    jump_height_offset =
        k_jump_peak_height * 4.0F * normalized_phase * (1.0F - normalized_phase);
  }
  bool const jump_active = m_jump_timer > 0.0F;
  if (cmd_comp != nullptr) {
    cmd_comp->jump_active = jump_active;
    cmd_comp->jump_phase = jump_phase;
    cmd_comp->jump_height_offset = jump_height_offset;
    cmd_comp->punish_window_remaining =
        std::max(0.0F, cmd_comp->punish_window_remaining - dt);
    cmd_comp->posture = std::max(
        0.0F,
        cmd_comp->posture -
            ((m_guard_was_active || m_input.secondary_action) ? 8.0F : 18.0F) * dt);
  }

  if (guard != nullptr) {
    guard->perfect_guard_remaining =
        std::max(0.0F, guard->perfect_guard_remaining - dt);
    guard->guard_break_remaining = std::max(0.0F, guard->guard_break_remaining - dt);
    if (!m_input.secondary_action) {
      guard->rearm_requires_release = false;
    }
  }

  if (m_locked_target_id != 0) {
    auto* lock_ent = world.get_entity(m_locked_target_id);
    auto* lock_unit = (lock_ent != nullptr)
                          ? lock_ent->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
    auto const lock_target = lock_ent != nullptr
                                 ? Game::Systems::RpgCombat::resolve_soldier_target(
                                       *lock_ent, m_locked_target_slot)
                                 : std::nullopt;
    if (lock_target.has_value() && lock_unit != nullptr && lock_unit->health > 0) {
      QVector3D away(transform->position.x - lock_target->position.x(),
                     0.0F,
                     transform->position.z - lock_target->position.z());
      if (away.lengthSquared() > 0.0001F) {
        away.normalize();
        const QVector3D tangent(-away.z(), 0.0F, away.x());
        const QVector3D radial = -away;
        move = radial * static_cast<float>(forward_axis) +
               tangent * static_cast<float>(right_axis);
      }
    }
  }

  const bool should_dodge = m_input.dodge_requested &&
                            m_dodge_state == DodgeState::None && m_jump_timer <= 0.0F;
  const bool dodge_refused = m_input.dodge_requested && !should_dodge;
  QVector3D const requested_dodge_direction = m_requested_dodge_direction;
  bool const has_requested_dodge_direction = m_has_requested_dodge_direction;
  m_input.dodge_requested = false;
  if (dodge_refused) {
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_ability_refused);
  }
  m_has_requested_dodge_direction = false;
  m_requested_dodge_direction = QVector3D(0.0F, 0.0F, 0.0F);

  if (should_dodge) {
    m_dodge_direction =
        has_requested_dodge_direction
            ? requested_dodge_direction.normalized()
            : ((move.lengthSquared() > 0.0001F) ? move.normalized() : forward);
    m_dodge_state = DodgeState::Rolling;
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_dodge);
    constexpr float k_dodge_roll_duration = 0.22F;
    m_dodge_timer = k_dodge_roll_duration;
    m_dodge_fov_kick = 14.0F;
    if (auto* rpg = commander->get_component<Engine::Core::RpgHealthComponent>()) {
      rpg->dodge_invincible = true;
    }

    if (auto* stamina = commander->get_component<Engine::Core::StaminaComponent>()) {
      stamina->stamina = std::max(
          0.0F,
          stamina->stamina - Engine::Core::CombatStateComponent::k_stamina_cost_dodge);
    }
  }

  auto mark_jump_safe_position = [&](float x, float z) {
    if (!jump_active || !is_walkable_at(x, z)) {
      return;
    }
    m_jump_safe_position_valid = true;
    m_jump_last_walkable_position = QVector3D(x, transform->position.y, z);
  };
  mark_jump_safe_position(transform->position.x, transform->position.z);

  if (m_dodge_state == DodgeState::Rolling) {
    constexpr float k_dodge_speed = 6.5F;
    constexpr float k_dodge_roll_duration = 0.22F;
    const float roll_dt = std::min(dt, m_dodge_timer);
    m_dodge_timer -= dt;

    const float nx =
        transform->position.x + m_dodge_direction.x() * k_dodge_speed * roll_dt;
    const float nz =
        transform->position.z + m_dodge_direction.z() * k_dodge_speed * roll_dt;
    auto const step =
        resolve_ground_step(transform->position.x, transform->position.z, nx, nz);
    transform->position.x = step.x;
    transform->position.z = step.z;
    if (movement != nullptr) {
      movement->set_manual_velocity(m_dodge_direction.x() * k_dodge_speed,
                                    m_dodge_direction.z() * k_dodge_speed);
    }
    actual_speed_for_bob = k_dodge_speed;
    run_for_bob = true;

    if (m_dodge_timer <= 0.0F) {
      m_dodge_state = DodgeState::Recovering;
      constexpr float k_dodge_recover_duration = 0.18F;
      m_dodge_timer = k_dodge_recover_duration;
      if (auto* rpg = commander->get_component<Engine::Core::RpgHealthComponent>()) {
        rpg->dodge_invincible = false;
      }
    }
  } else if (m_dodge_state == DodgeState::Recovering) {
    m_dodge_timer -= dt;
    if (m_dodge_timer <= 0.0F) {
      m_dodge_state = DodgeState::None;
      m_dodge_timer = 0.0F;
    }

    if (move.lengthSquared() > 0.0001F) {
      move.normalize();
      const float speed = std::max(0.1F, unit->speed) * 0.4F;
      const float nx = transform->position.x + move.x() * speed * dt;
      const float nz = transform->position.z + move.z() * speed * dt;
      auto const step =
          resolve_ground_step(transform->position.x, transform->position.z, nx, nz);
      if (step.moved) {
        float const step_vx = dt > 0.0F ? (step.x - transform->position.x) / dt : 0.0F;
        float const step_vz = dt > 0.0F ? (step.z - transform->position.z) / dt : 0.0F;
        transform->position.x = step.x;
        transform->position.z = step.z;
        if (movement != nullptr) {
          movement->set_manual_velocity(step_vx, step_vz);
        }
        actual_speed_for_bob = std::hypot(step_vx, step_vz);
      } else if (movement != nullptr) {
        movement->set_manual_velocity(0.0F, 0.0F);
      }
    } else if (movement != nullptr) {
      movement->set_manual_velocity(0.0F, 0.0F);
    }
  } else {

    float target_speed = 0.0F;
    bool running = false;
    if (move.lengthSquared() > 0.0001F) {
      move.normalize();
      m_last_move_direction = move;
      float speed = std::max(0.1F, unit->speed) * k_fpv_walk_speed_scale;

      auto const* stamina = Game::Systems::ensure_run_stamina(*commander);
      running = m_input.run && !drawing_bow && stamina != nullptr &&
                (stamina->is_running || stamina->can_start_running());
      if (running) {
        speed *= Engine::Core::StaminaComponent::k_run_speed_multiplier;
      }
      if (drawing_bow) {

        constexpr float k_drawn_bow_move_scale = 0.55F;
        speed *= k_drawn_bow_move_scale;
      }
      speed *= directional_speed_scale(forward_axis, right_axis);
      target_speed = speed;
    }

    constexpr float k_move_accel_rate = 12.0F;
    constexpr float k_move_decel_rate = 16.0F;
    float const approach_rate =
        target_speed > m_planar_speed_smooth ? k_move_accel_rate : k_move_decel_rate;
    m_planar_speed_smooth += (target_speed - m_planar_speed_smooth) *
                             (1.0F - std::exp(-approach_rate * std::max(dt, 0.0F)));
    if (target_speed <= 0.0F && m_planar_speed_smooth < 0.05F) {
      m_planar_speed_smooth = 0.0F;
    }

    if (m_planar_speed_smooth > 0.01F) {
      QVector3D const direction =
          move.lengthSquared() > 0.0001F ? move : m_last_move_direction;
      const float nx =
          transform->position.x + direction.x() * m_planar_speed_smooth * dt;
      const float nz =
          transform->position.z + direction.z() * m_planar_speed_smooth * dt;
      auto const step = jump_active
                            ? airborne_step(nx, nz)
                            : resolve_ground_step(
                                  transform->position.x, transform->position.z, nx, nz);
      if (step.moved) {
        float const step_vx = dt > 0.0F ? (step.x - transform->position.x) / dt : 0.0F;
        float const step_vz = dt > 0.0F ? (step.z - transform->position.z) / dt : 0.0F;
        transform->position.x = step.x;
        transform->position.z = step.z;
        mark_jump_safe_position(step.x, step.z);
        if (movement != nullptr) {
          movement->set_manual_velocity(step_vx, step_vz);
        }
        actual_speed_for_bob = std::hypot(step_vx, step_vz);
        run_for_bob = running;
      } else {
        m_planar_speed_smooth = 0.0F;
        if (movement != nullptr) {
          movement->set_manual_velocity(0.0F, 0.0F);
        }
      }
    } else if (movement != nullptr) {
      movement->set_manual_velocity(0.0F, 0.0F);
    }
  }
  if (m_jump_safe_position_valid && !jump_active) {
    if (!is_walkable_at(transform->position.x, transform->position.z)) {
      transform->position.x = m_jump_last_walkable_position.x();
      transform->position.z = m_jump_last_walkable_position.z();
      if (movement != nullptr) {
        movement->set_manual_velocity(0.0F, 0.0F);
      }
      actual_speed_for_bob = 0.0F;
      run_for_bob = false;
    }
    m_jump_safe_position_valid = false;
  }

  if (!jump_active) {
    apply_strike_lunge(world, *commander, *transform, dt);
    separate_commander_from_bodies(world, *commander, commander_id, *transform, dt);
  }

  m_move_speed = actual_speed_for_bob;
  m_move_right_axis = right_axis;
  m_move_forward_axis = forward_axis;
  m_move_running = run_for_bob;

  update_camera(world, *commander, camera, dt);

  auto* aim = Game::Systems::RpgCombat::sync_commander_aim(
      *commander,
      {.view_yaw_degrees = m_view_yaw,
       .view_pitch_degrees = m_view_pitch,
       .move_speed = m_move_speed,
       .running = m_move_running,
       .primary_held = m_input.primary_action,
       .camera_origin = m_camera_rig.eye(),
       .camera_origin_valid = m_camera_rig.eye_valid(),
       .camera_forward = m_camera_rig.forward(),
       .camera_forward_valid = m_camera_rig.forward_valid(),
       .camera_fov_degrees = m_camera_rig.fov()});
  if (cmd_comp != nullptr) {
    cmd_comp->fpv_motion_vx = (movement != nullptr) ? movement->get_vx() : 0.0F;
    cmd_comp->fpv_motion_vz = (movement != nullptr) ? movement->get_vz() : 0.0F;
  }

  if (movement != nullptr && actual_speed_for_bob > 0.05F) {
    movement->engage_manual_move(transform->position.x, transform->position.z);

    if (auto* stamina = commander->get_component<Engine::Core::StaminaComponent>()) {
      stamina->run_requested = m_move_running;
    }
  } else if (auto* stamina =
                 commander->get_component<Engine::Core::StaminaComponent>()) {
    stamina->run_requested = false;
  }

  transform->rotation.y = m_view_yaw;
  transform->desired_yaw = m_view_yaw;
  transform->has_desired_yaw = true;

  guard = commander->get_component<Engine::Core::CommanderGuardComponent>();
  if (m_input.secondary_action) {
    if (guard == nullptr) {
      guard = commander->add_component<Engine::Core::CommanderGuardComponent>();
    }
    if (guard != nullptr && guard->guard_break_remaining <= 0.0F &&
        !guard->rearm_requires_release && m_dodge_state == DodgeState::None &&
        !jump_active) {
      guard->active = true;
      if (!m_guard_was_active) {
        guard->perfect_guard_remaining = 0.16F;
        Game::Audio::play_cue(Game::Audio::Cue::k_combat_guard_raise);
      }
    } else if (guard != nullptr) {
      guard->active = false;
    }
  } else if (guard != nullptr) {
    guard->active = false;
  }
  if (guard != nullptr && guard->guard_break_remaining > 0.0F) {
    guard->active = false;
  }
  m_guard_was_active = (guard != nullptr) && guard->active;

  if (guard != nullptr && guard->active) {
    if (auto* stamina = commander->get_component<Engine::Core::StaminaComponent>()) {
      stamina->stamina = std::max(
          0.0F,
          stamina->stamina -
              Engine::Core::CombatStateComponent::k_stamina_cost_guard_per_second * dt);
    }
  }

  update_ability_cooldowns(cmd_comp, dt);
  try_activate_shield_bash(world, *commander, commander_id, local_owner_id);
  try_activate_vanguard_rush(world, *commander, commander_id, local_owner_id);
  try_activate_second_wind(*commander);

  auto const* active_action =
      commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  bool const attack_animation_active =
      active_action != nullptr && active_action->action_running;
  if (m_input.primary_action) {
    m_combo_miss_timer = 0.0F;
    m_primary_held_duration += dt;
  } else if (attack_animation_active) {
    m_primary_held_duration = 0.0F;
  } else {
    m_primary_held_duration = 0.0F;
    m_combo_miss_timer += dt;
    constexpr float k_combo_reset_window = 1.0F;
    if (m_combo_miss_timer >= k_combo_reset_window && cmd_comp != nullptr) {

      cmd_comp->combo_step = 0;
      m_combo_miss_timer = 0.0F;
    }
  }

  if (m_input.primary_action_scan_cooldown > 0.0F) {
    m_input.primary_action_scan_cooldown =
        std::max(0.0F, m_input.primary_action_scan_cooldown - dt);
  }

  bool const waiting_for_release = aim != nullptr && aim->relaxed_from_overhold;
  if (m_dodge_state != DodgeState::Rolling && m_jump_timer <= 0.0F &&
      !waiting_for_release && m_input.primary_action &&
      m_input.primary_action_scan_cooldown <= 0.0F) {
    if (!primary_action(world, commander_id, local_owner_id)) {
      return false;
    }
    m_input.primary_action_scan_cooldown = 0.08F;
  }

  if (auto const* struck =
          commander->get_component<Engine::Core::RpgCommanderActionComponent>()) {
    if (!struck->action_running) {
      m_observed_action_hit_count = 0;
    } else if (struck->hit_target_count > m_observed_action_hit_count) {
      m_observed_action_hit_count = struck->hit_target_count;
      m_camera_rig.add_impact_kick(1.0F);
    } else if (struck->hit_target_count < m_observed_action_hit_count) {
      m_observed_action_hit_count = struck->hit_target_count;
    }
  } else {
    m_observed_action_hit_count = 0;
  }

  Engine::Core::EntityID aim_candidate_id = 0;
  if (aim != nullptr && aim->stance == Engine::Core::FpvWeaponStance::Bow) {

    if (!aim->is_drawing()) {

      auto const* stamina = commander->get_component<Engine::Core::StaminaComponent>();
      aim->spread_degrees = Game::Systems::RpgCombat::aim_spread_degrees(
          *aim, stamina != nullptr ? stamina->get_stamina_ratio() : 1.0F);
    }

    constexpr float k_crosshair_forgiveness = 0.22F;

    constexpr float k_aim_overshoot_range = 12.0F;
    auto const* commander_attack =
        commander->get_component<Engine::Core::AttackComponent>();
    float const range =
        (commander_attack != nullptr ? commander_attack->range : 12.0F) +
        k_aim_overshoot_range;
    auto const hit = Game::Systems::RpgCombat::raycast_enemy_bodies(
        world,
        *commander,
        Game::Systems::RpgCombat::commander_aim_ray(*commander, *aim),
        range,
        k_crosshair_forgiveness);
    if (hit.has_value()) {
      aim_candidate_id = hit->entity_id;
      m_primary_target_slot = hit->soldier_slot;
    } else {
      m_primary_target_slot =
          Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
    }
  } else {
    aim_candidate_id = find_primary_target(world, commander_id, local_owner_id);
  }

  if (auto* rpg_targets =
          Engine::Core::get_or_add_component<Engine::Core::RpgCommanderTargetComponent>(
              commander)) {
    rpg_targets->explicit_lock_target_id = m_locked_target_id;
    rpg_targets->explicit_lock_soldier_slot = m_locked_target_slot;
    rpg_targets->aim_candidate_id = aim_candidate_id;
    rpg_targets->aim_candidate_soldier_slot =
        aim_candidate_id != 0
            ? m_primary_target_slot
            : Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
    rpg_targets->aim_candidate_in_range = aim_candidate_id != 0;

    if (rpg_targets->hit_confirm_sequence != m_observed_hit_confirm_sequence) {
      m_observed_hit_confirm_sequence = rpg_targets->hit_confirm_sequence;
      m_camera_rig.add_impact_kick(rpg_targets->recent_hit_killed ? 1.0F : 0.7F);
    }
    rpg_targets->recent_hit_timer = std::max(0.0F, rpg_targets->recent_hit_timer - dt);
    if (rpg_targets->recent_hit_timer <= 0.0F) {
      rpg_targets->recent_hit_target_id = 0;
      rpg_targets->recent_hit_soldier_slot =
          Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
    }
  }
  return true;
}

void CommanderControlController::play_footstep_if_stride_landed(
    const Engine::Core::Entity& commander, float previous_bob_phase) {
  if (m_camera_rig.bob_amplitude() < k_footstep_min_bob_amplitude) {
    return;
  }

  const auto stride_index = [](float phase) {
    return static_cast<long long>(std::floor((phase - k_footstep_bob_offset) /
                                             (2.0F * std::numbers::pi_v<float>)));
  };
  if (stride_index(m_camera_rig.bob_phase()) == stride_index(previous_bob_phase)) {
    return;
  }

  if (m_move_running) {
    Game::Audio::play_cue(Game::Audio::Cue::k_move_footstep_run);
    return;
  }

  const auto* terrain =
      commander.get_component<Engine::Core::TerrainContextComponent>();
  const bool hard_ground = terrain != nullptr && terrain->is_on_bridge;
  Game::Audio::play_cue(hard_ground ? Game::Audio::Cue::k_move_footstep_hard
                                    : Game::Audio::Cue::k_move_footstep);
}

void CommanderControlController::update_camera(Engine::Core::World& world,
                                               Engine::Core::Entity& commander,
                                               Render::GL::Camera& camera,
                                               float dt) {
  auto* transform = commander.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  set_view_pitch(m_view_pitch);

  auto const* aim_state =
      commander.get_component<Engine::Core::RpgCommanderAimComponent>();
  auto const* bow_action =
      commander.get_component<Engine::Core::RpgCommanderActionComponent>();
  bool const bow_stance =
      aim_state != nullptr && aim_state->stance == Engine::Core::FpvWeaponStance::Bow;
  bool const aiming_bow =
      aim_state != nullptr &&
      (aim_state->is_drawing() ||
       (bow_stance && bow_action != nullptr && bow_action->action_running));

  bool close_camera_mode = false;
  float jump_height_offset = 0.0F;
  if (auto const* cmd = commander.get_component<Engine::Core::CommanderComponent>()) {
    jump_height_offset = cmd->jump_height_offset;
    close_camera_mode = cmd->close_camera_mode;
  }

  auto fight_context = Engine::Core::FightContext::None;
  float threat_side_bias = 0.0F;
  Engine::Core::EntityID front_attacker_id = 0;
  if (auto const* engagement =
          commander.get_component<Engine::Core::RpgEngagementComponent>()) {
    fight_context = engagement->fight_context;
    front_attacker_id = engagement->front_attacker_id;
    if (fight_context == Engine::Core::FightContext::Skirmish) {
      bool const left = engagement->left_threat_id != 0;
      bool const right = engagement->right_threat_id != 0;
      if (left != right) {
        threat_side_bias = left ? 1.0F : -1.0F;
      }
    }
  }

  std::optional<QVector3D> lock_target_position;
  const Engine::Core::EntityID focus_id = locked_target_id();
  if (focus_id != 0) {
    auto* target = world.get_entity(focus_id);
    auto* target_unit = (target != nullptr)
                            ? target->get_component<Engine::Core::UnitComponent>()
                            : nullptr;
    auto const target_sample = target != nullptr
                                   ? Game::Systems::RpgCombat::resolve_soldier_target(
                                         *target, m_locked_target_slot)
                                   : std::nullopt;
    if (target_sample.has_value() && target_unit != nullptr &&
        target_unit->health > 0) {
      lock_target_position = target_sample->position;
    }
  }

  std::optional<QVector3D> soft_focus_position;
  if (!lock_target_position.has_value() && front_attacker_id != 0) {
    if (auto* front = world.get_entity(front_attacker_id)) {
      if (auto const sample = Game::Systems::RpgCombat::resolve_soldier_target(
              *front, Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot);
          sample.has_value()) {
        soft_focus_position = sample->position;
      }
    }
  }

  App::Core::CommanderCameraInputs inputs;
  inputs.dt = dt;
  inputs.view_yaw_degrees = m_view_yaw;
  inputs.view_pitch_degrees = m_view_pitch;
  inputs.move_speed = m_move_speed;
  inputs.move_right_axis = m_move_right_axis;
  inputs.move_running = m_move_running;
  inputs.aiming_bow = aiming_bow;
  inputs.close_camera_mode = close_camera_mode;
  inputs.lock_target_active = lock_target_position.has_value();
  inputs.jump_height_offset = jump_height_offset;
  inputs.dodge_fov_kick = m_dodge_fov_kick;
  inputs.dodge_rolling = m_dodge_state == DodgeState::Rolling;
  inputs.dodge_tilt_progress = 1.0F - std::clamp(m_dodge_timer / 0.22F, 0.0F, 1.0F);
  inputs.dodge_direction = m_dodge_direction;
  inputs.commander_position =
      QVector3D(transform->position.x, transform->position.y, transform->position.z);
  inputs.lock_target_position = lock_target_position;
  inputs.soft_focus_position = soft_focus_position;
  inputs.fight_context = fight_context;
  inputs.threat_side_bias = threat_side_bias;

  float const previous_bob_phase = m_camera_rig.update(camera, inputs);
  play_footstep_if_stride_landed(commander, previous_bob_phase);
}
