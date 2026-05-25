#include "commander_control_controller.h"

#include <QCursor>
#include <QQuickWindow>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/combat_system/damage_processor.h"
#include "game/systems/command_service.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "game/systems/rpg_combat_system/rpg_combat_processor.h"
#include "game/systems/rpg_combat_system/rpg_commander_damage.h"
#include "render/gl/camera.h"

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

auto segment_intersection_fraction(float start,
                                   float delta,
                                   float min_bound,
                                   float max_bound,
                                   float& t_enter,
                                   float& t_exit) -> bool {
  constexpr float k_epsilon = 0.00001F;
  if (std::abs(delta) <= k_epsilon) {
    return start >= min_bound && start <= max_bound;
  }

  const float inv_delta = 1.0F / delta;
  float t0 = (min_bound - start) * inv_delta;
  float t1 = (max_bound - start) * inv_delta;
  if (t0 > t1) {
    std::swap(t0, t1);
  }
  t_enter = std::max(t_enter, t0);
  t_exit = std::min(t_exit, t1);
  return t_enter <= t_exit;
}

auto first_building_intersection_fraction(const QVector3D& start,
                                          const QVector3D& end,
                                          unsigned int ignore_entity_id = 0) -> float {
  auto const& buildings =
      Game::Systems::BuildingCollisionRegistry::instance().get_all_buildings();
  float best_fraction = 1.0F;
  const QVector3D delta = end - start;
  for (const auto& building : buildings) {
    if (ignore_entity_id != 0 && building.entity_id == ignore_entity_id) {
      continue;
    }

    float t_enter = 0.0F;
    float t_exit = 1.0F;
    const float half_width = building.width * 0.5F;
    const float half_depth = building.depth * 0.5F;
    if (!segment_intersection_fraction(start.x(),
                                       delta.x(),
                                       building.center_x - half_width,
                                       building.center_x + half_width,
                                       t_enter,
                                       t_exit) ||
        !segment_intersection_fraction(start.z(),
                                       delta.z(),
                                       building.center_z - half_depth,
                                       building.center_z + half_depth,
                                       t_enter,
                                       t_exit)) {
      continue;
    }

    if (t_enter >= 0.0F && t_enter <= 1.0F) {
      best_fraction = std::min(best_fraction, t_enter);
    }
  }
  return best_fraction;
}

auto has_clear_building_los(const QVector3D& start,
                            const QVector3D& end,
                            unsigned int ignore_entity_id = 0) -> bool {
  return first_building_intersection_fraction(start, end, ignore_entity_id) >= 1.0F;
}

constexpr std::uint8_t k_commander_sword_sway_default = 0U;
constexpr std::uint8_t k_commander_sword_sway_reverse = 1U;
constexpr std::uint8_t k_commander_sword_sway_finisher = 2U;
constexpr std::uint8_t k_commander_sword_sway_overhead = 3U;
constexpr std::uint8_t k_commander_sword_sway_thrust = 4U;

auto select_commander_sword_sway(const Engine::Core::CommanderComponent* commander,
                                 std::uint8_t attack_sequence,
                                 int move_right_axis,
                                 int move_forward_axis,
                                 bool finisher_attack) -> std::uint8_t {
  if (finisher_attack) {
    return k_commander_sword_sway_finisher;
  }
  // Thrust on forward input
  if (move_forward_axis > 0) {
    return k_commander_sword_sway_thrust;
  }
  // Overhead on backward input or power strike
  if (move_forward_axis < 0 ||
      (move_right_axis == 0 && commander != nullptr &&
       commander->power_strike_active)) {
    return k_commander_sword_sway_overhead;
  }
  // More varied directional slash selection based on sequence
  // Cycles through: left, right, left-high, right-high, overhead-light
  switch (attack_sequence % 5U) {
  case 0:
    return k_commander_sword_sway_default; // Left slash
  case 1:
    return k_commander_sword_sway_reverse; // Right slash
  case 2:
    return (move_right_axis > 0) ? k_commander_sword_sway_reverse
                                 : k_commander_sword_sway_default;
  case 3:
    return (move_right_axis < 0) ? k_commander_sword_sway_default
                                 : k_commander_sword_sway_reverse;
  case 4:
    return k_commander_sword_sway_overhead; // Occasional overhead in neutral
  default:
    return k_commander_sword_sway_default;
  }
}

auto attack_direction_from_sway(std::uint8_t sway) -> Engine::Core::AttackDirection {
  switch (sway) {
  case k_commander_sword_sway_default:
    return Engine::Core::AttackDirection::LeftSlash;
  case k_commander_sword_sway_reverse:
    return Engine::Core::AttackDirection::RightSlash;
  case k_commander_sword_sway_overhead:
    return Engine::Core::AttackDirection::Overhead;
  case k_commander_sword_sway_finisher:
    return Engine::Core::AttackDirection::HeavyOverhead;
  case k_commander_sword_sway_thrust:
    return Engine::Core::AttackDirection::Thrust;
  default:
    return Engine::Core::AttackDirection::LeftSlash;
  }
}

auto is_walkable_at(float x, float z) -> bool {
  if (auto* pathfinder = Game::Systems::CommandService::get_pathfinder()) {
    const auto grid = Game::Systems::CommandService::world_to_grid(x, z);
    return pathfinder->is_walkable(grid.x, grid.y);
  }
  auto& terrain = Game::Map::TerrainService::instance();
  if (terrain.is_initialized()) {
    return terrain.is_walkable(static_cast<int>(std::round(x)),
                               static_cast<int>(std::round(z)));
  }
  return true;
}

auto resolve_reachable_ground_position(const QVector3D& start,
                                       const QVector3D& desired,
                                       unsigned int ignore_entity_id = 0) -> QVector3D {
  QVector3D candidate = desired;
  const float blocked_fraction =
      first_building_intersection_fraction(start, desired, ignore_entity_id);
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

} // namespace

void CommanderControlController::reset() {
  m_input = {};
  m_mouse_center_valid = false;
  m_last_mouse_valid = false;
  m_mouse_warp_supported = false;
  m_mouse_recentering = false;
  m_bob_phase = 0.0F;
  m_bob_amplitude = 0.0F;
  m_breath_phase = 0.0F;
  m_strafe_lean = 0.0F;
  m_fov_current = 75.0F;
  m_cam_smooth_valid = false;
  m_move_speed = 0.0F;
  m_move_right_axis = 0;
  m_move_forward_axis = 0;
  m_move_running = false;
  m_hit_trauma = 0.0F;
  m_hit_shake_phase = 0.0F;
  m_strike_punch_fwd = 0.0F;
  m_dodge_state = DodgeState::None;
  m_dodge_timer = 0.0F;
  m_dodge_fov_kick = 0.0F;
  m_jump_timer = 0.0F;
  m_jump_safe_position_valid = false;
  m_jump_last_walkable_position = QVector3D(0.0F, 0.0F, 0.0F);
  m_locked_target_id = 0;
  m_soft_target_id = 0;
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

void CommanderControlController::mouse_move(qreal dx, qreal dy) {
  constexpr float k_mouse_yaw_sensitivity = 0.18F;
  constexpr float k_mouse_pitch_sensitivity = 0.12F;
  m_view_yaw += static_cast<float>(dx) * k_mouse_yaw_sensitivity;
  m_view_pitch -= static_cast<float>(dy) * k_mouse_pitch_sensitivity;
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
    const float dx = t->position.x - origin.x();
    const float dz = t->position.z - origin.z();
    const float distance_sq = dx * dx + dz * dz;
    if (distance_sq > k_lock_range_sq) {
      continue;
    }
    const float angle_diff =
        signed_angle_delta(std::atan2(dx, dz) * 57.29577951308232F, m_view_yaw);
    if (std::abs(angle_diff) > k_lock_max_angle_degrees) {
      continue;
    }
    const QVector3D target_pos(t->position.x, 0.0F, t->position.z);
    const bool visible = has_clear_building_los(origin, target_pos);
    if (!visible) {
      continue;
    }
    candidates.push_back({candidate->get_id(), angle_diff, distance_sq, visible});
  }

  if (candidates.empty()) {
    m_locked_target_id = 0;
    m_soft_target_id = 0;
    m_lock_lost_timer = 0.0F;
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
  } else {
    auto it =
        std::find_if(candidates.begin(), candidates.end(), [this](const Candidate& c) {
          return c.id == m_locked_target_id;
        });
    if (it == candidates.end() || std::next(it) == candidates.end()) {
      m_locked_target_id = candidates[0].id;
    } else {
      m_locked_target_id = std::next(it)->id;
    }
  }
  m_soft_target_id = m_locked_target_id;
  m_lock_lost_timer = 0.0F;
  if (auto* rpg_targets =
          Engine::Core::get_or_add_component<Engine::Core::RpgCommanderTargetComponent>(
              commander)) {
    rpg_targets->explicit_lock_target_id = m_locked_target_id;
    rpg_targets->aim_candidate_id = m_soft_target_id;
  }
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
  auto* target_transform =
      (target != nullptr) ? target->get_component<Engine::Core::TransformComponent>()
                          : nullptr;

  if (target_unit == nullptr || target_unit->health <= 0 ||
      target_transform == nullptr) {
    m_locked_target_id = 0;
    m_lock_lost_timer = 0.0F;
    return;
  }

  const float dx = target_transform->position.x - cmd_transform->position.x;
  const float dz = target_transform->position.z - cmd_transform->position.z;
  constexpr float k_lock_drop_sq = 18.0F * 18.0F;
  const QVector3D origin(cmd_transform->position.x, 0.0F, cmd_transform->position.z);
  const QVector3D target_pos(
      target_transform->position.x, 0.0F, target_transform->position.z);
  const bool target_visible = has_clear_building_los(origin, target_pos);

  const float target_yaw = std::atan2(dx, dz) * 57.29577951308232F;
  const float diff = signed_angle_delta(target_yaw, m_view_yaw);
  const bool escape_input =
      (m_input.run && m_input.backward) ||
      (m_input.dodge_requested && (m_input.backward || m_input.run));
  if (escape_input || (m_input.run && std::abs(diff) > 95.0F)) {
    m_locked_target_id = 0;
    m_soft_target_id = 0;
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
    int local_owner_id) -> Engine::Core::EntityID {
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
  float max_range = 2.5F;
  if (commander_attack != nullptr) {
    max_range = std::max(max_range, commander_attack->melee_range + 0.65F);
  }
  const float max_range_sq = max_range * max_range;

  if (m_locked_target_id != 0) {
    auto* locked_ent = world.get_entity(m_locked_target_id);
    auto* locked_unit = (locked_ent != nullptr)
                            ? locked_ent->get_component<Engine::Core::UnitComponent>()
                            : nullptr;
    if (locked_unit == nullptr || locked_unit->health <= 0) {
      m_locked_target_id = 0;
    } else {
      auto* locked_transform =
          locked_ent->get_component<Engine::Core::TransformComponent>();
      if (locked_transform != nullptr) {
        const float dx = locked_transform->position.x - commander_transform->position.x;
        const float dz = locked_transform->position.z - commander_transform->position.z;
        const QVector3D locked_pos(
            locked_transform->position.x, 0.0F, locked_transform->position.z);
        const QVector3D origin(
            commander_transform->position.x, 0.0F, commander_transform->position.z);
        if (dx * dx + dz * dz <= max_range_sq &&
            has_clear_building_los(origin, locked_pos)) {
          return m_locked_target_id;
        }
      }
    }
  }

  const float yaw_rad = m_view_yaw * k_degrees_to_radians;
  const QVector3D forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  const QVector3D origin(
      commander_transform->position.x, 0.0F, commander_transform->position.z);
  auto& owners = Game::Systems::OwnerRegistry::instance();

  if (m_soft_target_id != 0) {
    auto* soft_ent = world.get_entity(m_soft_target_id);
    auto* soft_unit = (soft_ent != nullptr)
                          ? soft_ent->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
    auto* soft_transform =
        (soft_ent != nullptr)
            ? soft_ent->get_component<Engine::Core::TransformComponent>()
            : nullptr;
    if (soft_unit == nullptr || soft_unit->health <= 0 || soft_transform == nullptr) {
      m_soft_target_id = 0;
    } else {
      const QVector3D soft_target(
          soft_transform->position.x, 0.0F, soft_transform->position.z);
      QVector3D to_target = soft_target - origin;
      const float dist_sq = to_target.lengthSquared();
      if (dist_sq > 0.0001F && dist_sq <= max_range_sq &&
          has_clear_building_los(origin, soft_target)) {
        const float distance = std::sqrt(dist_sq);
        to_target /= distance;
        if (QVector3D::dotProduct(forward, to_target) >= 0.15F) {
          return m_soft_target_id;
        }
      }
    }
  }

  Game::Systems::RpgCombat::refresh_commander_engagement(&world, commander_id);
  if (auto* engagement =
          commander->get_component<Engine::Core::RpgEngagementComponent>()) {
    std::array<Engine::Core::EntityID, 3> const ring_targets = {
        engagement->front_attacker_id,
        engagement->left_threat_id,
        engagement->right_threat_id};
    for (auto ring_target_id : ring_targets) {
      auto* ring_target = world.get_entity(ring_target_id);
      auto* ring_unit = ring_target != nullptr
                            ? ring_target->get_component<Engine::Core::UnitComponent>()
                            : nullptr;
      auto* ring_transform =
          ring_target != nullptr
              ? ring_target->get_component<Engine::Core::TransformComponent>()
              : nullptr;
      if (ring_unit == nullptr || ring_transform == nullptr || ring_unit->health <= 0 ||
          !owners.are_enemies(local_owner_id, ring_unit->owner_id)) {
        continue;
      }
      const QVector3D ring_pos(
          ring_transform->position.x, 0.0F, ring_transform->position.z);
      QVector3D to_ring_target = ring_pos - origin;
      float const dist_sq = to_ring_target.lengthSquared();
      if (dist_sq > 0.0001F && dist_sq <= max_range_sq &&
          has_clear_building_los(origin, ring_pos)) {
        to_ring_target.normalize();
        if (QVector3D::dotProduct(forward, to_ring_target) >= 0.05F) {
          return ring_target_id;
        }
      }
    }
  }

  Engine::Core::EntityID best_id = 0;
  float best_score = -1000000.0F;

  for (auto* candidate : world.get_entities_with<Engine::Core::UnitComponent>()) {
    if (candidate == nullptr || candidate == commander) {
      continue;
    }
    auto* candidate_unit = candidate->get_component<Engine::Core::UnitComponent>();
    auto* candidate_transform =
        candidate->get_component<Engine::Core::TransformComponent>();
    if (candidate_unit == nullptr || candidate_transform == nullptr ||
        candidate_unit->health <= 0) {
      continue;
    }
    if (!owners.are_enemies(local_owner_id, candidate_unit->owner_id)) {
      continue;
    }

    const QVector3D target(
        candidate_transform->position.x, 0.0F, candidate_transform->position.z);
    QVector3D to_target = target - origin;
    const float dist_sq = to_target.lengthSquared();
    if (dist_sq <= 0.0001F || dist_sq > max_range_sq) {
      continue;
    }
    const float distance = std::sqrt(dist_sq);
    to_target /= distance;
    const float facing = QVector3D::dotProduct(forward, to_target);
    if (facing < k_commander_attack_cone_dot) {
      continue;
    }
    if (!has_clear_building_los(origin, target)) {
      continue;
    }

    const float score = facing * 10.0F - distance;
    if (score > best_score) {
      best_score = score;
      best_id = candidate->get_id();
    }
  }

  return best_id;
}

auto CommanderControlController::primary_action(Engine::Core::World& world,
                                                Engine::Core::EntityID commander_id,
                                                int local_owner_id) -> bool {
  auto* commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return false;
  }

  auto* combat_state = commander->get_component<Engine::Core::CombatStateComponent>();
  if (combat_state != nullptr &&
      combat_state->animation_state != Engine::Core::CombatAnimationState::Idle) {
    // Input buffering: allow queuing next attack during Recover/Reposition phases
    if (combat_state->animation_state == Engine::Core::CombatAnimationState::Recover ||
        combat_state->animation_state ==
            Engine::Core::CombatAnimationState::Reposition) {
      combat_state->input_buffered = true;
    }
    return true;
  }

  auto* unit = commander->get_component<Engine::Core::UnitComponent>();
  auto* attack = commander->get_component<Engine::Core::AttackComponent>();

  if (attack != nullptr && attack->can_melee) {
    attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  }

  if (combat_state == nullptr) {
    combat_state = commander->add_component<Engine::Core::CombatStateComponent>();
  }
  auto* cmd_comp = commander->get_component<Engine::Core::CommanderComponent>();
  bool const finisher_attack = cmd_comp != nullptr && cmd_comp->combo_step >= 3;
  Engine::Core::CombatAttackFamily attack_family =
      Engine::Core::CombatAttackFamily::None;
  if (combat_state != nullptr) {
    combat_state->animation_state = Engine::Core::CombatAnimationState::Advance;
    combat_state->state_time = 0.0F;
    combat_state->state_duration =
        Engine::Core::CombatStateComponent::k_advance_duration *
        (finisher_attack ? 1.70F : 1.35F);
    if (unit != nullptr && attack != nullptr) {
      attack_family = Engine::Core::resolve_combat_attack_family(unit->spawn_type,
                                                                 attack->current_mode);
    }
    combat_state->attack_family = attack_family;
    combat_state->finisher_attack = finisher_attack;

    static std::uint8_t s_fpv_attack_seed = 0;
    combat_state->attack_offset = static_cast<float>(s_fpv_attack_seed % 7) * 0.022F;
    combat_state->attack_variant =
        s_fpv_attack_seed %
        Engine::Core::CombatStateComponent::k_attack_variant_seed_slots;
    ++s_fpv_attack_seed;
  }

  if (cmd_comp != nullptr) {
    if (auto* action = Engine::Core::get_or_add_component<
            Engine::Core::RpgCommanderActionComponent>(commander)) {
      action->phase = Engine::Core::RpgCommanderActionPhase::Strike;
      action->melee_attack_style = 0;
      action->active_target_id = 0;
      action->phase_time = 0.0F;
      if (combat_state != nullptr &&
          attack_family == Engine::Core::CombatAttackFamily::Sword) {
        action->melee_attack_style =
            select_commander_sword_sway(cmd_comp,
                                        action->melee_attack_sequence,
                                        m_move_right_axis,
                                        m_move_forward_axis,
                                        finisher_attack);
        combat_state->attack_variant = action->melee_attack_style;
        combat_state->attack_direction =
            attack_direction_from_sway(action->melee_attack_style);
        action->melee_attack_sequence =
            static_cast<std::uint8_t>((action->melee_attack_sequence + 1U) % 5U);
      }
    }
    if (finisher_attack) {
      m_dodge_fov_kick = std::max(m_dodge_fov_kick, 16.0F);
    }
    if (m_primary_held_duration >= 0.4F) {
      cmd_comp->power_strike_active = true;
      m_dodge_fov_kick = std::max(m_dodge_fov_kick, 14.0F);
    }
    m_combo_miss_timer = 0.0F;
  }

  // Consume stamina for the attack
  if (auto* stamina = commander->get_component<Engine::Core::StaminaComponent>()) {
    float cost = (cmd_comp != nullptr && cmd_comp->power_strike_active)
                     ? Engine::Core::CombatStateComponent::k_stamina_cost_heavy_attack
                     : Engine::Core::CombatStateComponent::k_stamina_cost_light_attack;
    stamina->stamina = std::max(0.0F, stamina->stamina - cost);
  }

  // Mark that damage has not been dealt yet for this swing (deferred to Strike phase)
  if (combat_state != nullptr) {
    combat_state->damage_dealt_this_swing = false;
  }

  const auto target_id = find_primary_target(world, commander_id, local_owner_id);
  if (target_id == 0) {

    return true;
  }

  // Store pending target for deferred damage at Strike phase
  if (auto* action =
          commander->get_component<Engine::Core::RpgCommanderActionComponent>()) {
    action->active_target_id = target_id;
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
           has_clear_building_los(origin, target);
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
  if (cmd_comp != nullptr) {
    cmd_comp->shield_bash_cooldown_remaining = m_shield_bash_cooldown;
  }
  m_dodge_fov_kick = std::max(m_dodge_fov_kick, 4.5F);
}

void CommanderControlController::try_activate_vanguard_rush(
    Engine::Core::World& world,
    Engine::Core::Entity& commander,
    Engine::Core::EntityID commander_id,
    int local_owner_id) {
  const bool rush_requested = m_input.vanguard_rush_requested;
  m_input.vanguard_rush_requested = false;
  if (!rush_requested || m_vanguard_rush_cooldown > 0.0F ||
      m_dodge_state != DodgeState::None || m_jump_timer > 0.0F) {
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
    movement->vx = rush_direction.x() * 8.0F;
    movement->vz = rush_direction.z() * 8.0F;
  }
  m_input.primary_action_scan_cooldown = 0.18F;
  m_dodge_fov_kick = std::max(m_dodge_fov_kick, 6.0F);

  if (target != nullptr) {
    auto* target_unit = target->get_component<Engine::Core::UnitComponent>();
    auto* target_transform = target->get_component<Engine::Core::TransformComponent>();
    if (target_unit != nullptr && target_transform != nullptr &&
        target_unit->health > 0) {
      const QVector3D target_pos(target_transform->position.x,
                                 target_transform->position.y,
                                 target_transform->position.z);
      if ((target_pos - resolved).length() <= 2.35F &&
          has_clear_building_los(resolved, target_pos)) {
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
  if (cmd_comp != nullptr) {
    cmd_comp->vanguard_rush_cooldown_remaining = m_vanguard_rush_cooldown;
  }
}

void CommanderControlController::try_activate_second_wind(
    Engine::Core::Entity& commander) {
  const bool second_wind_requested = m_input.second_wind_requested;
  m_input.second_wind_requested = false;
  if (!second_wind_requested || m_second_wind_cooldown > 0.0F ||
      m_dodge_state != DodgeState::None || m_jump_timer > 0.0F) {
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

  m_hit_trauma *= 0.35F;
  m_second_wind_cooldown = k_second_wind_cooldown;
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
      movement->vx = 0.0F;
      movement->vz = 0.0F;
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
    movement->has_target = false;
    movement->path_pending = false;
    movement->pending_request_id = 0;
    movement->goal_x = transform->position.x;
    movement->goal_y = transform->position.z;
    movement->target_x = transform->position.x;
    movement->target_y = transform->position.z;
    movement->clear_path();
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
  constexpr float k_shake_decay = 18.0F;
  m_impact_shake = std::max(0.0F, m_impact_shake - k_shake_decay * dt);
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
  m_input.jump_requested = false;
  if (should_jump) {
    m_jump_timer = k_jump_duration;
    m_jump_safe_position_valid = true;
    m_jump_last_walkable_position =
        QVector3D(transform->position.x, transform->position.y, transform->position.z);
    // Stamina cost for jump
    if (auto* stamina = commander->get_component<Engine::Core::StaminaComponent>()) {
      stamina->stamina = std::max(
          0.0F,
          stamina->stamina -
              Engine::Core::CombatStateComponent::k_stamina_cost_jump);
    }
  }

  float jump_phase = 0.0F;
  float jump_height_offset = 0.0F;
  if (m_jump_timer > 0.0F) {
    m_jump_timer = std::max(0.0F, m_jump_timer - dt);
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
    auto* lock_tf = (lock_ent != nullptr)
                        ? lock_ent->get_component<Engine::Core::TransformComponent>()
                        : nullptr;
    auto* lock_unit = (lock_ent != nullptr)
                          ? lock_ent->get_component<Engine::Core::UnitComponent>()
                          : nullptr;
    if (lock_tf != nullptr && lock_unit != nullptr && lock_unit->health > 0) {
      QVector3D away(transform->position.x - lock_tf->position.x,
                     0.0F,
                     transform->position.z - lock_tf->position.z);
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
  m_input.dodge_requested = false;

  if (should_dodge) {
    m_dodge_direction = (move.lengthSquared() > 0.0001F) ? move.normalized() : forward;
    m_dodge_state = DodgeState::Rolling;
    constexpr float k_dodge_roll_duration = 0.22F;
    m_dodge_timer = k_dodge_roll_duration;
    m_dodge_fov_kick = 14.0F;
    if (auto* rpg = commander->get_component<Engine::Core::RpgHealthComponent>()) {
      rpg->dodge_invincible = true;
    }
    // Stamina cost for dodge
    if (auto* stamina = commander->get_component<Engine::Core::StaminaComponent>()) {
      stamina->stamina = std::max(
          0.0F,
          stamina->stamina -
              Engine::Core::CombatStateComponent::k_stamina_cost_dodge);
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
    if (is_walkable_at(nx, nz)) {
      transform->position.x = nx;
      transform->position.z = nz;
    }
    if (movement != nullptr) {
      movement->vx = m_dodge_direction.x() * k_dodge_speed;
      movement->vz = m_dodge_direction.z() * k_dodge_speed;
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
      if (is_walkable_at(nx, nz)) {
        transform->position.x = nx;
        transform->position.z = nz;
        if (movement != nullptr) {
          movement->vx = move.x() * speed;
          movement->vz = move.z() * speed;
        }
        actual_speed_for_bob = speed;
      } else if (movement != nullptr) {
        movement->vx = 0.0F;
        movement->vz = 0.0F;
      }
    } else if (movement != nullptr) {
      movement->vx = 0.0F;
      movement->vz = 0.0F;
    }
  } else {

    if (move.lengthSquared() > 0.0001F) {
      move.normalize();
      float speed = std::max(0.1F, unit->speed);
      if (m_input.run) {
        speed *= Engine::Core::StaminaComponent::k_run_speed_multiplier;
      }
      const float nx = transform->position.x + move.x() * speed * dt;
      const float nz = transform->position.z + move.z() * speed * dt;
      if (jump_active || is_walkable_at(nx, nz)) {
        transform->position.x = nx;
        transform->position.z = nz;
        mark_jump_safe_position(nx, nz);
        if (movement != nullptr) {
          movement->vx = move.x() * speed;
          movement->vz = move.z() * speed;
        }
        actual_speed_for_bob = speed;
        run_for_bob = m_input.run;
      } else if (movement != nullptr) {
        movement->vx = 0.0F;
        movement->vz = 0.0F;
      }
    } else if (movement != nullptr) {
      movement->vx = 0.0F;
      movement->vz = 0.0F;
    }
  }
  if (m_jump_safe_position_valid && !jump_active) {
    if (!is_walkable_at(transform->position.x, transform->position.z)) {
      transform->position.x = m_jump_last_walkable_position.x();
      transform->position.z = m_jump_last_walkable_position.z();
      if (movement != nullptr) {
        movement->vx = 0.0F;
        movement->vz = 0.0F;
      }
      actual_speed_for_bob = 0.0F;
      run_for_bob = false;
    }
    m_jump_safe_position_valid = false;
  }

  m_move_speed = actual_speed_for_bob;
  m_move_right_axis = right_axis;
  m_move_forward_axis = forward_axis;
  m_move_running = run_for_bob;
  if (cmd_comp != nullptr) {
    cmd_comp->fpv_motion_vx = (movement != nullptr) ? movement->vx : 0.0F;
    cmd_comp->fpv_motion_vz = (movement != nullptr) ? movement->vz : 0.0F;
  }

  if (movement != nullptr && actual_speed_for_bob > 0.05F) {
    movement->has_target = true;
    movement->goal_x = transform->position.x;
    movement->goal_y = transform->position.z;
    movement->target_x = transform->position.x;
    movement->target_y = transform->position.z;

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

  // Stamina drain while guarding
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

  bool const attack_animation_active =
      combat_state != nullptr &&
      combat_state->animation_state != Engine::Core::CombatAnimationState::Idle;
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
      if (auto* action =
              commander->get_component<Engine::Core::RpgCommanderActionComponent>()) {
        action->melee_attack_sequence = 0;
      }
      m_combo_miss_timer = 0.0F;
    }
  }

  if (m_input.primary_action_scan_cooldown > 0.0F) {
    m_input.primary_action_scan_cooldown =
        std::max(0.0F, m_input.primary_action_scan_cooldown - dt);
  }

  if (m_dodge_state != DodgeState::Rolling && m_jump_timer <= 0.0F &&
      m_input.primary_action && m_input.primary_action_scan_cooldown <= 0.0F) {
    if (!primary_action(world, commander_id, local_owner_id)) {
      return false;
    }
    m_input.primary_action_scan_cooldown = 0.08F;
  }

  if (m_locked_target_id != 0) {
    m_soft_target_id = m_locked_target_id;
  } else {
    constexpr float k_soft_target_range = 8.0F;
    constexpr float k_soft_target_range_sq = k_soft_target_range * k_soft_target_range;
    constexpr float k_soft_target_min_facing = 0.35F;
    auto& owners = Game::Systems::OwnerRegistry::instance();
    const QVector3D origin(transform->position.x, 0.0F, transform->position.z);
    Engine::Core::EntityID best_id = 0;
    float best_score = -1000000.0F;
    for (auto* candidate : world.get_entities_with<Engine::Core::UnitComponent>()) {
      if (candidate == nullptr || candidate->get_id() == commander_id) {
        continue;
      }
      auto* candidate_unit = candidate->get_component<Engine::Core::UnitComponent>();
      auto* candidate_transform =
          candidate->get_component<Engine::Core::TransformComponent>();
      if (candidate_unit == nullptr || candidate_transform == nullptr ||
          candidate_unit->health <= 0 ||
          !owners.are_enemies(local_owner_id, candidate_unit->owner_id)) {
        continue;
      }
      const QVector3D target(
          candidate_transform->position.x, 0.0F, candidate_transform->position.z);
      QVector3D to_target = target - origin;
      const float dist_sq = to_target.lengthSquared();
      if (dist_sq <= 0.0001F || dist_sq > k_soft_target_range_sq ||
          !has_clear_building_los(origin, target)) {
        continue;
      }
      const float distance = std::sqrt(dist_sq);
      to_target /= distance;
      const float facing = QVector3D::dotProduct(forward, to_target);
      if (facing < k_soft_target_min_facing) {
        continue;
      }
      float score = facing * 120.0F - distance * 3.5F;
      if (candidate->get_id() == m_soft_target_id) {
        score += 12.0F;
      }
      if (score > best_score) {
        best_score = score;
        best_id = candidate->get_id();
      }
    }
    m_soft_target_id = best_id;
  }

  if (auto* rpg_targets =
          Engine::Core::get_or_add_component<Engine::Core::RpgCommanderTargetComponent>(
              commander)) {
    rpg_targets->explicit_lock_target_id = m_locked_target_id;
    rpg_targets->aim_candidate_id = m_soft_target_id;
    rpg_targets->recent_hit_timer = std::max(0.0F, rpg_targets->recent_hit_timer - dt);
    if (rpg_targets->recent_hit_timer <= 0.0F) {
      rpg_targets->recent_hit_target_id = 0;
    }
  }
  if (auto* action =
          commander->get_component<Engine::Core::RpgCommanderActionComponent>()) {
    action->phase_time += dt;
    if (combat_state == nullptr ||
        combat_state->animation_state == Engine::Core::CombatAnimationState::Idle) {
      action->phase = Engine::Core::RpgCommanderActionPhase::None;
      action->active_target_id = 0;
    }
  }

  update_camera(world, *commander, camera, dt);
  return true;
}

void CommanderControlController::update_camera(Engine::Core::World& world,
                                               Engine::Core::Entity& commander,
                                               Render::GL::Camera& camera,
                                               float dt) {
  auto* transform = commander.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  constexpr float k_pi = std::numbers::pi_v<float>;
  constexpr float k_deg2rad = 0.017453292519943295F;

  constexpr float k_focus_height = 1.45F;
  constexpr float k_camera_back_offset = 2.15F;
  constexpr float k_camera_up_offset = 0.65F;
  constexpr float k_target_distance = 6.0F;
  constexpr float k_close_camera_back_offset = 1.25F;
  constexpr float k_close_camera_up_offset = 0.38F;
  constexpr float k_close_target_distance = 5.2F;

  constexpr float k_bob_freq = 3.5F;
  constexpr float k_bob_vert_amp = 0.075F;
  constexpr float k_bob_run_mult = 1.65F;
  constexpr float k_bob_lat_amp = 0.028F;
  constexpr float k_bob_decay = 5.5F;

  constexpr float k_breath_freq = 0.2F;
  constexpr float k_breath_vert_amp = 0.008F;

  constexpr float k_cam_spring = 14.0F;

  constexpr float k_lean_max_deg = 2.2F;
  constexpr float k_lean_follow = 6.5F;

  constexpr float k_fov_walk = 75.0F;
  constexpr float k_fov_run_boost = 7.0F;
  constexpr float k_fov_lerp = 5.0F;

  set_view_pitch(m_view_pitch);

  const float bob_amp_target = (m_move_speed > 0.05F) ? 1.0F : 0.0F;
  m_bob_amplitude += (bob_amp_target - m_bob_amplitude) *
                     (1.0F - std::exp(-k_bob_decay * std::max(dt, 0.0F)));
  if (m_move_speed > 0.05F) {
    m_bob_phase += m_move_speed * k_bob_freq * dt;
  }
  const float bob_run_factor = m_move_running ? k_bob_run_mult : 1.0F;
  const float bob_v =
      std::sin(m_bob_phase) * k_bob_vert_amp * bob_run_factor * m_bob_amplitude;

  const float bob_l =
      std::sin(m_bob_phase * 0.5F) * k_bob_lat_amp * bob_run_factor * m_bob_amplitude;

  m_breath_phase += k_breath_freq * 2.0F * k_pi * std::max(dt, 0.0F);
  const float breath_idle = 1.0F - m_bob_amplitude;
  const float breath_v = std::sin(m_breath_phase) * k_breath_vert_amp * breath_idle;

  const float lean_target = -static_cast<float>(m_move_right_axis) * k_lean_max_deg;
  m_strafe_lean += (lean_target - m_strafe_lean) *
                   (1.0F - std::exp(-k_lean_follow * std::max(dt, 0.0F)));

  bool close_camera_mode = false;
  std::uint8_t last_strike_combo_step = 0U;
  float jump_height_offset = 0.0F;
  if (auto const* cmd = commander.get_component<Engine::Core::CommanderComponent>()) {
    jump_height_offset = cmd->jump_height_offset;
    close_camera_mode = cmd->close_camera_mode;
    last_strike_combo_step = cmd->last_strike_combo_step;
  }

  const float fov_target =
      (close_camera_mode ? 72.0F : k_fov_walk) +
      ((m_move_running && m_move_speed > 0.05F) ? k_fov_run_boost : 0.0F) +
      m_dodge_fov_kick;
  m_fov_current += (fov_target - m_fov_current) *
                   (1.0F - std::exp(-k_fov_lerp * std::max(dt, 0.0F)));
  constexpr float k_commander_near_plane = 0.05F;
  camera.set_perspective(
      m_fov_current, camera.get_aspect(), k_commander_near_plane, camera.get_far());

  const float yaw_rad = m_view_yaw * k_deg2rad;
  const float pitch_rad = m_view_pitch * k_deg2rad;
  const float pitch_cos = std::cos(pitch_rad);
  const QVector3D forward_vec(std::sin(yaw_rad) * pitch_cos,
                              std::sin(pitch_rad),
                              std::cos(yaw_rad) * pitch_cos);
  const QVector3D flat_forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  const QVector3D flat_right(-flat_forward.z(), 0.0F, flat_forward.x());

  const QVector3D pivot(transform->position.x,
                        transform->position.y + jump_height_offset + k_focus_height,
                        transform->position.z);
  const float back_offset =
      close_camera_mode ? k_close_camera_back_offset : k_camera_back_offset;
  const float up_offset =
      close_camera_mode ? k_close_camera_up_offset : k_camera_up_offset;
  const float target_distance =
      close_camera_mode ? k_close_target_distance : k_target_distance;
  QVector3D eye_desired = pivot - flat_forward * back_offset +
                          QVector3D(0.0F, up_offset + bob_v + breath_v, 0.0F) +
                          flat_right * bob_l;
  // Combat impact micro-shake for crisp hit feedback
  if (m_impact_shake > 0.1F) {
    float const shake_intensity = m_impact_shake * 0.012F;
    float const shake_t = m_impact_shake_seed + m_impact_shake * 7.0F;
    float const shake_x = std::sin(shake_t * 23.0F) * shake_intensity;
    float const shake_y = std::cos(shake_t * 31.0F) * shake_intensity * 0.7F;
    eye_desired += flat_right * shake_x + QVector3D(0.0F, shake_y, 0.0F);
  }
  QVector3D target_desired = pivot + forward_vec * target_distance;

  const Engine::Core::EntityID focus_id = locked_target_id();
  if (focus_id != 0) {
    auto* target = world.get_entity(focus_id);
    auto* target_transform =
        (target != nullptr) ? target->get_component<Engine::Core::TransformComponent>()
                            : nullptr;
    auto* target_unit = (target != nullptr)
                            ? target->get_component<Engine::Core::UnitComponent>()
                            : nullptr;
    if (target_transform != nullptr && target_unit != nullptr &&
        target_unit->health > 0) {
      const QVector3D enemy_focus(target_transform->position.x,
                                  target_transform->position.y + 1.1F,
                                  target_transform->position.z);
      QVector3D to_enemy = enemy_focus - pivot;
      if (to_enemy.lengthSquared() > 0.0001F) {
        const float enemy_distance = std::sqrt(to_enemy.lengthSquared());
        to_enemy /= enemy_distance;
        target_desired =
            (pivot + forward_vec * target_distance) * 0.40F +
            (enemy_focus + to_enemy * std::min(1.2F, enemy_distance * 0.15F)) * 0.60F;
        eye_desired += flat_right * (close_camera_mode ? 0.10F : 0.22F);
      }
    }
  }

  const float blocked_fraction =
      first_building_intersection_fraction(pivot, eye_desired);
  const bool camera_blocked = blocked_fraction < 1.0F;
  if (camera_blocked) {
    const float safe_fraction =
        std::clamp(blocked_fraction - 0.06F, close_camera_mode ? 0.12F : 0.22F, 1.0F);
    eye_desired = pivot + (eye_desired - pivot) * safe_fraction;
  }

  if (!m_cam_smooth_valid) {
    m_cam_eye_smooth = eye_desired;
    m_cam_target_smooth = target_desired;
    m_cam_smooth_valid = true;
  } else {
    const float alpha = 1.0F - std::exp(-k_cam_spring * std::max(dt, 0.0F));
    if (camera_blocked) {
      m_cam_eye_smooth = eye_desired;
    } else {
      m_cam_eye_smooth += (eye_desired - m_cam_eye_smooth) * alpha;
    }
    m_cam_target_smooth += (target_desired - m_cam_target_smooth) * alpha;
  }

  const float lean_rad = m_strafe_lean * k_deg2rad;
  const QVector3D world_up(0.0F, 1.0F, 0.0F);

  const QVector3D right_world =
      QVector3D::crossProduct(forward_vec.normalized(), world_up).normalized();
  const QVector3D up_leaned =
      (world_up + right_world * std::sin(lean_rad)).normalized();

  constexpr float k_shake_freq = 28.0F;
  constexpr float k_shake_decay = 7.5F;
  constexpr float k_shake_lateral = 0.22F;
  constexpr float k_shake_vert = 0.09F;

  if (auto const* fb = commander.get_component<Engine::Core::HitFeedbackComponent>()) {

    if (fb->is_reacting && fb->reaction_time < 0.05F &&
        fb->reaction_intensity > m_hit_trauma * 0.5F) {
      m_hit_trauma = fb->reaction_intensity;
      m_hit_shake_phase = 0.0F;
    }
  }
  m_hit_shake_phase += k_shake_freq * std::max(dt, 0.0F);
  m_hit_trauma *= std::exp(-k_shake_decay * std::max(dt, 0.0F));

  const float shake_lat = std::sin(m_hit_shake_phase) * m_hit_trauma * k_shake_lateral;
  const float shake_v =
      std::abs(std::sin(m_hit_shake_phase * 0.6F)) * m_hit_trauma * k_shake_vert;
  const float camera_effect_scale = close_camera_mode ? 0.35F : 1.0F;
  const QVector3D shake_offset =
      (flat_right * shake_lat - QVector3D(0.0F, shake_v, 0.0F)) * camera_effect_scale;

  if (auto const* cmd = commander.get_component<Engine::Core::CommanderComponent>()) {
    if (cmd->just_struck_enemy) {
      m_strike_punch_fwd =
          (last_strike_combo_step >= 3 ? 0.52F : 0.38F) * camera_effect_scale;
      // Trigger impact micro-shake for crisp hit feedback
      m_impact_shake = last_strike_combo_step >= 3 ? 5.5F : 3.2F;
      m_impact_shake_seed = static_cast<float>(last_strike_combo_step) * 1.7F;
    }
  }
  constexpr float k_punch_decay = 11.0F;
  m_strike_punch_fwd *= std::exp(-k_punch_decay * std::max(dt, 0.0F));
  const QVector3D punch_offset = forward_vec * m_strike_punch_fwd;

  // Dodge roll camera tilt
  float dodge_tilt_rad = 0.0F;
  if (m_dodge_state == DodgeState::Rolling) {
    const float dodge_progress =
        1.0F - std::clamp(m_dodge_timer / 0.22F, 0.0F, 1.0F);
    const float tilt_curve =
        std::sin(dodge_progress * k_pi) * 0.12F; // ~7 degree roll
    const float dot_right =
        m_dodge_direction.x() * flat_right.x() +
        m_dodge_direction.z() * flat_right.z();
    dodge_tilt_rad = tilt_curve * (dot_right > 0.0F ? 1.0F : -1.0F);
  }

  const QVector3D up_final =
      (up_leaned + right_world * dodge_tilt_rad).normalized();

  camera.look_at(m_cam_eye_smooth + shake_offset + punch_offset,
                 m_cam_target_smooth + shake_offset + punch_offset,
                 up_final);
}
