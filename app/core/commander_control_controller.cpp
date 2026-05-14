#include "commander_control_controller.h"

#include "game/core/component.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/systems/command_service.h"
#include "game/systems/owner_registry.h"
#include "game/systems/pathfinding.h"
#include "render/gl/camera.h"
#include <QCursor>
#include <QQuickWindow>
#include <QVector3D>
#include <algorithm>
#include <cmath>
#include <vector>

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
  m_combo_miss_timer = 0.0F;
  m_primary_held_duration = 0.0F;
  m_special_cooldown = 0.0F;
}

void CommanderControlController::set_view_yaw(float yaw) { m_view_yaw = yaw; }

void CommanderControlController::set_view_pitch(float pitch) {
  m_view_pitch = std::clamp(pitch, -70.0F, 70.0F);
}

auto CommanderControlController::view_yaw() const -> float {
  return m_view_yaw;
}

auto CommanderControlController::view_pitch() const -> float {
  return m_view_pitch;
}

auto CommanderControlController::input() -> InputState & { return m_input; }

auto CommanderControlController::input() const -> const InputState & {
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

void CommanderControlController::mouse_look_at(qreal sx, qreal sy,
                                               qreal center_sx, qreal center_sy,
                                               QQuickWindow *window) {
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

void CommanderControlController::center_mouse(qreal center_sx, qreal center_sy,
                                              QQuickWindow *window) {
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

void CommanderControlController::poll_mouse_look(QQuickWindow *window) {
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
  m_input.special_action = true;
}

auto CommanderControlController::locked_target_id() const
    -> Engine::Core::EntityID {
  return m_locked_target_id;
}

void CommanderControlController::cycle_lock_on_target(
    Engine::Core::World &world, Engine::Core::EntityID commander_id,
    int local_owner_id) {
  auto *commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return;
  }
  auto *transform =
      commander->get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  constexpr float k_lock_range = 15.0F;
  constexpr float k_lock_range_sq = k_lock_range * k_lock_range;

  auto &owners = Game::Systems::OwnerRegistry::instance();
  const float yaw_rad = m_view_yaw * 0.017453292519943295F;
  const QVector3D origin(transform->position.x, 0.0F, transform->position.z);

  struct Candidate {
    Engine::Core::EntityID id;
    float angle_diff;
  };
  std::vector<Candidate> candidates;

  for (auto *candidate :
       world.get_entities_with<Engine::Core::UnitComponent>()) {
    if (candidate == nullptr || candidate->get_id() == commander_id) {
      continue;
    }
    auto *u = candidate->get_component<Engine::Core::UnitComponent>();
    auto *t = candidate->get_component<Engine::Core::TransformComponent>();
    if (u == nullptr || t == nullptr || u->health <= 0) {
      continue;
    }
    if (!owners.are_enemies(local_owner_id, u->owner_id)) {
      continue;
    }
    const float dx = t->position.x - origin.x();
    const float dz = t->position.z - origin.z();
    if (dx * dx + dz * dz > k_lock_range_sq) {
      continue;
    }
    float diff = std::atan2(dx, dz) * 57.29577951308232F - m_view_yaw;
    while (diff > 180.0F) {
      diff -= 360.0F;
    }
    while (diff < -180.0F) {
      diff += 360.0F;
    }
    candidates.push_back({candidate->get_id(), diff});
  }

  if (candidates.empty()) {
    m_locked_target_id = 0;
    return;
  }

  std::sort(candidates.begin(), candidates.end(),
            [](const Candidate &a, const Candidate &b) {
              return std::abs(a.angle_diff) < std::abs(b.angle_diff);
            });

  if (m_locked_target_id == 0) {
    m_locked_target_id = candidates[0].id;
  } else {
    auto it = std::find_if(
        candidates.begin(), candidates.end(),
        [this](const Candidate &c) { return c.id == m_locked_target_id; });
    if (it == candidates.end() || std::next(it) == candidates.end()) {
      m_locked_target_id = 0;
    } else {
      m_locked_target_id = std::next(it)->id;
    }
  }
}

void CommanderControlController::update_lock_on_yaw(
    Engine::Core::World &world, Engine::Core::Entity &commander, float dt) {
  if (m_locked_target_id == 0) {
    return;
  }

  auto *cmd_transform =
      commander.get_component<Engine::Core::TransformComponent>();
  if (cmd_transform == nullptr) {
    return;
  }

  auto *target = world.get_entity(m_locked_target_id);
  auto *target_unit =
      target ? target->get_component<Engine::Core::UnitComponent>() : nullptr;
  auto *target_transform =
      target ? target->get_component<Engine::Core::TransformComponent>()
             : nullptr;

  if (target_unit == nullptr || target_unit->health <= 0 ||
      target_transform == nullptr) {
    m_locked_target_id = 0;
    return;
  }

  const float dx = target_transform->position.x - cmd_transform->position.x;
  const float dz = target_transform->position.z - cmd_transform->position.z;
  constexpr float k_lock_drop_sq = 15.0F * 15.0F;
  if (dx * dx + dz * dz > k_lock_drop_sq) {
    m_locked_target_id = 0;
    return;
  }

  float target_yaw = std::atan2(dx, dz) * 57.29577951308232F;
  float diff = target_yaw - m_view_yaw;
  while (diff > 180.0F) {
    diff -= 360.0F;
  }
  while (diff < -180.0F) {
    diff += 360.0F;
  }
  constexpr float k_lock_spring = 8.0F;
  m_view_yaw += diff * (1.0F - std::exp(-k_lock_spring * dt));
  m_view_yaw = std::fmod(m_view_yaw, 360.0F);
  if (m_view_yaw < 0.0F) {
    m_view_yaw += 360.0F;
  }
}

auto CommanderControlController::controlled_commander(
    Engine::Core::World &world, Engine::Core::EntityID commander_id,
    int local_owner_id) const -> Engine::Core::Entity * {
  auto *entity = world.get_entity(commander_id);
  if (entity == nullptr) {
    return nullptr;
  }

  auto *unit = entity->get_component<Engine::Core::UnitComponent>();
  auto *transform = entity->get_component<Engine::Core::TransformComponent>();
  if (unit == nullptr || transform == nullptr || unit->health <= 0 ||
      unit->owner_id != local_owner_id ||
      entity->get_component<Engine::Core::CommanderComponent>() == nullptr) {
    return nullptr;
  }
  return entity;
}

auto CommanderControlController::find_primary_target(
    Engine::Core::World &world, Engine::Core::EntityID commander_id,
    int local_owner_id) -> Engine::Core::EntityID {
  auto *commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return 0;
  }

  auto *commander_transform =
      commander->get_component<Engine::Core::TransformComponent>();
  auto *commander_attack =
      commander->get_component<Engine::Core::AttackComponent>();
  if (commander_transform == nullptr) {
    return 0;
  }

  constexpr float k_degrees_to_radians = 0.017453292519943295F;
  constexpr float k_commander_attack_cone_dot = 0.45F;
  float max_range = 2.5F;
  if (commander_attack != nullptr) {
    max_range = std::max(max_range, commander_attack->melee_range + 0.65F);
  }
  const float max_range_sq = max_range * max_range;

  if (m_locked_target_id != 0) {
    auto *locked_ent = world.get_entity(m_locked_target_id);
    auto *locked_unit =
        locked_ent ? locked_ent->get_component<Engine::Core::UnitComponent>()
                   : nullptr;
    if (locked_unit == nullptr || locked_unit->health <= 0) {
      m_locked_target_id = 0;
    } else {
      auto *locked_transform =
          locked_ent->get_component<Engine::Core::TransformComponent>();
      if (locked_transform != nullptr) {
        const float dx =
            locked_transform->position.x - commander_transform->position.x;
        const float dz =
            locked_transform->position.z - commander_transform->position.z;
        if (dx * dx + dz * dz <= max_range_sq) {
          return m_locked_target_id;
        }
      }
    }
  }

  const float yaw_rad = m_view_yaw * k_degrees_to_radians;
  const QVector3D forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  const QVector3D origin(commander_transform->position.x, 0.0F,
                         commander_transform->position.z);
  auto &owners = Game::Systems::OwnerRegistry::instance();

  Engine::Core::EntityID best_id = 0;
  float best_score = -1000000.0F;

  for (auto *candidate :
       world.get_entities_with<Engine::Core::UnitComponent>()) {
    if (candidate == nullptr || candidate == commander) {
      continue;
    }
    auto *candidate_unit =
        candidate->get_component<Engine::Core::UnitComponent>();
    auto *candidate_transform =
        candidate->get_component<Engine::Core::TransformComponent>();
    if (candidate_unit == nullptr || candidate_transform == nullptr ||
        candidate_unit->health <= 0) {
      continue;
    }
    if (!owners.are_enemies(local_owner_id, candidate_unit->owner_id)) {
      continue;
    }

    const QVector3D target(candidate_transform->position.x, 0.0F,
                           candidate_transform->position.z);
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

    const float score = facing * 10.0F - distance;
    if (score > best_score) {
      best_score = score;
      best_id = candidate->get_id();
    }
  }

  return best_id;
}

auto CommanderControlController::primary_action(
    Engine::Core::World &world, Engine::Core::EntityID commander_id,
    int local_owner_id) -> bool {
  auto *commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return false;
  }

  auto *combat_state =
      commander->get_component<Engine::Core::CombatStateComponent>();
  if (combat_state != nullptr && combat_state->animation_state !=
                                     Engine::Core::CombatAnimationState::Idle) {
    return true;
  }

  auto *unit = commander->get_component<Engine::Core::UnitComponent>();
  auto *attack = commander->get_component<Engine::Core::AttackComponent>();

  if (attack != nullptr && attack->can_melee) {
    attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
  }

  if (combat_state == nullptr) {
    combat_state =
        commander->add_component<Engine::Core::CombatStateComponent>();
  }
  if (combat_state != nullptr) {
    combat_state->animation_state = Engine::Core::CombatAnimationState::Advance;
    combat_state->state_time = 0.0F;
    combat_state->state_duration =
        Engine::Core::CombatStateComponent::k_advance_duration;
    if (unit != nullptr && attack != nullptr) {
      combat_state->attack_family = Engine::Core::resolve_combat_attack_family(
          unit->spawn_type, attack->current_mode);
    } else {
      combat_state->attack_family = Engine::Core::CombatAttackFamily::None;
    }

    static std::uint8_t s_fpv_attack_seed = 0;
    combat_state->attack_offset =
        static_cast<float>(s_fpv_attack_seed % 7) * 0.022F;
    combat_state->attack_variant =
        s_fpv_attack_seed %
        Engine::Core::CombatStateComponent::k_attack_variant_seed_slots;
    ++s_fpv_attack_seed;
  }

  auto *cmd_comp = commander->get_component<Engine::Core::CommanderComponent>();
  if (cmd_comp != nullptr) {
    if (m_primary_held_duration >= 0.4F) {
      cmd_comp->power_strike_active = true;
      m_dodge_fov_kick = std::max(m_dodge_fov_kick, 10.0F);
    }
    m_combo_miss_timer = 0.0F;

    if (cmd_comp->special_cooldown_remaining > 0.0F) {
    }
  }

  const auto target_id =
      find_primary_target(world, commander_id, local_owner_id);
  if (target_id == 0) {

    return true;
  }

  if (attack != nullptr) {
    attack->time_since_last = attack->get_current_cooldown();
  }

  auto *target_comp =
      commander->get_component<Engine::Core::AttackTargetComponent>();
  if (target_comp == nullptr) {
    target_comp =
        commander->add_component<Engine::Core::AttackTargetComponent>();
  }
  if (target_comp != nullptr) {
    target_comp->target_id = target_id;
    target_comp->should_chase = false;
  }
  return true;
}

void CommanderControlController::release_guard(
    Engine::Core::World &world, Engine::Core::EntityID commander_id,
    int local_owner_id) {
  if (auto *commander =
          controlled_commander(world, commander_id, local_owner_id)) {
    if (auto *guard =
            commander->get_component<Engine::Core::CommanderGuardComponent>()) {
      guard->active = false;
    }
  }
}

auto CommanderControlController::update(Engine::Core::World &world,
                                        Engine::Core::EntityID commander_id,
                                        int local_owner_id,
                                        Render::GL::Camera &camera,
                                        float dt) -> bool {
  auto *commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return false;
  }

  auto *transform =
      commander->get_component<Engine::Core::TransformComponent>();
  auto *unit = commander->get_component<Engine::Core::UnitComponent>();
  auto *combat_state =
      commander->get_component<Engine::Core::CombatStateComponent>();
  auto *cmd_comp = commander->get_component<Engine::Core::CommanderComponent>();
  if (transform == nullptr || unit == nullptr) {
    return false;
  }

  auto *movement = commander->get_component<Engine::Core::MovementComponent>();
  if (movement == nullptr) {
    movement = commander->add_component<Engine::Core::MovementComponent>();
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

  commander->remove_component<Engine::Core::AttackTargetComponent>();

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
  m_view_yaw = std::fmod(m_view_yaw, 360.0F);
  if (m_view_yaw < 0.0F) {
    m_view_yaw += 360.0F;
  }

  const int forward_axis =
      (m_input.forward ? 1 : 0) - (m_input.backward ? 1 : 0);
  const int right_axis = (m_input.right ? 1 : 0) - (m_input.left ? 1 : 0);

  const float yaw_rad = m_view_yaw * k_degrees_to_radians;
  const QVector3D forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  const QVector3D right(-forward.z(), 0.0F, forward.x());
  QVector3D move = forward * static_cast<float>(forward_axis) +
                   right * static_cast<float>(right_axis);

  float actual_speed_for_bob = 0.0F;
  bool run_for_bob = false;

  constexpr float k_fov_kick_decay = 30.0F;
  m_dodge_fov_kick = std::max(0.0F, m_dodge_fov_kick - k_fov_kick_decay * dt);
  constexpr float k_jump_duration = 0.58F;
  constexpr float k_jump_peak_height = 0.34F;

  const bool jump_blocked_by_action =
      m_dodge_state != DodgeState::None || m_input.primary_action ||
      m_input.secondary_action || m_input.special_action ||
      (combat_state != nullptr && combat_state->animation_state !=
                                      Engine::Core::CombatAnimationState::Idle);
  const bool should_jump =
      m_input.jump_requested && m_jump_timer <= 0.0F && !jump_blocked_by_action;
  m_input.jump_requested = false;
  if (should_jump) {
    m_jump_timer = k_jump_duration;
    m_jump_safe_position_valid = true;
    m_jump_last_walkable_position = QVector3D(
        transform->position.x, transform->position.y, transform->position.z);
  }

  float jump_phase = 0.0F;
  float jump_height_offset = 0.0F;
  if (m_jump_timer > 0.0F) {
    m_jump_timer = std::max(0.0F, m_jump_timer - dt);
    jump_phase = 1.0F - (m_jump_timer / k_jump_duration);
    const float normalized_phase = std::clamp(jump_phase, 0.0F, 1.0F);
    jump_height_offset = k_jump_peak_height * 4.0F * normalized_phase *
                         (1.0F - normalized_phase);
  }
  bool const jump_active = m_jump_timer > 0.0F;
  if (cmd_comp != nullptr) {
    cmd_comp->jump_active = jump_active;
    cmd_comp->jump_phase = jump_phase;
    cmd_comp->jump_height_offset = jump_height_offset;
  }

  const bool should_dodge = m_input.dodge_requested &&
                            m_dodge_state == DodgeState::None &&
                            m_jump_timer <= 0.0F;
  m_input.dodge_requested = false;

  if (should_dodge) {
    m_dodge_direction =
        (move.lengthSquared() > 0.0001F) ? move.normalized() : forward;
    m_dodge_state = DodgeState::Rolling;
    constexpr float k_dodge_roll_duration = 0.22F;
    m_dodge_timer = k_dodge_roll_duration;
    m_dodge_fov_kick = 8.0F;
    if (auto *rpg =
            commander->get_component<Engine::Core::RpgHealthComponent>()) {
      rpg->dodge_invincible = true;
    }
  }

  auto can_move_to = [](float nx, float nz) -> bool {
    if (auto *pathfinder = Game::Systems::CommandService::get_pathfinder()) {
      const auto grid = Game::Systems::CommandService::world_to_grid(nx, nz);
      return pathfinder->is_walkable(grid.x, grid.y);
    }
    auto &terrain = Game::Map::TerrainService::instance();
    if (terrain.is_initialized()) {
      return terrain.is_walkable(static_cast<int>(std::round(nx)),
                                 static_cast<int>(std::round(nz)));
    }
    return true;
  };
  auto mark_jump_safe_position = [&](float x, float z) {
    if (!jump_active || !can_move_to(x, z)) {
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
    if (can_move_to(nx, nz)) {
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
      if (auto *rpg =
              commander->get_component<Engine::Core::RpgHealthComponent>()) {
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
      if (can_move_to(nx, nz)) {
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
      if (jump_active || can_move_to(nx, nz)) {
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
    if (!can_move_to(transform->position.x, transform->position.z)) {
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
  m_move_running = run_for_bob;

  if (movement != nullptr && actual_speed_for_bob > 0.05F) {
    movement->has_target = true;
    movement->goal_x = transform->position.x;
    movement->goal_y = transform->position.z;
    movement->target_x = transform->position.x;
    movement->target_y = transform->position.z;

    if (auto *stamina =
            commander->get_component<Engine::Core::StaminaComponent>()) {
      stamina->run_requested = m_move_running;
    }
  } else if (auto *stamina =
                 commander->get_component<Engine::Core::StaminaComponent>()) {
    stamina->run_requested = false;
  }

  transform->rotation.y = m_view_yaw;
  transform->desired_yaw = m_view_yaw;
  transform->has_desired_yaw = true;

  auto *guard =
      commander->get_component<Engine::Core::CommanderGuardComponent>();
  if (m_input.secondary_action) {
    if (guard == nullptr) {
      guard = commander->add_component<Engine::Core::CommanderGuardComponent>();
    }
    if (guard != nullptr) {
      guard->active = true;
    }
  } else if (guard != nullptr) {
    guard->active = false;
  }

  if (m_special_cooldown > 0.0F) {
    m_special_cooldown = std::max(0.0F, m_special_cooldown - dt);
    if (cmd_comp != nullptr) {
      cmd_comp->special_cooldown_remaining = m_special_cooldown;
    }
  }

  constexpr float k_bash_range = 2.5F;
  constexpr float k_bash_stagger_duration = 0.5F;
  constexpr float k_bash_cooldown = 3.0F;
  const bool bash_active = m_input.special_action && guard != nullptr &&
                           guard->active && m_special_cooldown <= 0.0F &&
                           m_jump_timer <= 0.0F;
  if (bash_active) {
    auto *transform =
        commander->get_component<Engine::Core::TransformComponent>();
    if (transform != nullptr) {
      const QVector3D cmd_pos{transform->position.x, transform->position.y,
                              transform->position.z};
      for (auto *entity :
           world.get_entities_with<Engine::Core::UnitComponent>()) {
        if (entity->get_id() == commander_id) {
          continue;
        }
        auto *unit = entity->get_component<Engine::Core::UnitComponent>();
        auto *ent_tf =
            entity->get_component<Engine::Core::TransformComponent>();
        if (unit == nullptr || ent_tf == nullptr || unit->health <= 0) {
          continue;
        }
        if (unit->owner_id == local_owner_id) {
          continue;
        }
        const QVector3D epos{ent_tf->position.x, ent_tf->position.y,
                             ent_tf->position.z};
        if ((epos - cmd_pos).length() <= k_bash_range) {
          auto *existing_stagger =
              entity->get_component<Engine::Core::StaggerComponent>();
          if (existing_stagger != nullptr) {
            existing_stagger->remaining =
                std::max(existing_stagger->remaining, k_bash_stagger_duration);
          } else {
            entity->add_component<Engine::Core::StaggerComponent>(
                k_bash_stagger_duration);
          }
        }
      }
    }
    m_special_cooldown = k_bash_cooldown;
    if (cmd_comp != nullptr) {
      cmd_comp->special_cooldown_remaining = m_special_cooldown;
    }
  }
  m_input.special_action = false;

  if (m_input.primary_action) {
    m_combo_miss_timer = 0.0F;
    m_primary_held_duration += dt;
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

  if (m_dodge_state != DodgeState::Rolling && m_jump_timer <= 0.0F &&
      m_input.primary_action && m_input.primary_action_scan_cooldown <= 0.0F) {
    if (!primary_action(world, commander_id, local_owner_id)) {
      return false;
    }
    m_input.primary_action_scan_cooldown = 0.08F;
  }

  update_camera(*commander, camera, dt);
  return true;
}

void CommanderControlController::update_camera(Engine::Core::Entity &commander,
                                               Render::GL::Camera &camera,
                                               float dt) {
  auto *transform = commander.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  constexpr float k_pi = 3.14159265358979F;
  constexpr float k_deg2rad = 0.017453292519943295F;

  constexpr float k_focus_height = 1.45F;
  constexpr float k_camera_back_offset = 2.15F;
  constexpr float k_camera_up_offset = 0.65F;
  constexpr float k_target_distance = 6.0F;

  constexpr float k_bob_freq = 3.2F;
  constexpr float k_bob_vert_amp = 0.055F;
  constexpr float k_bob_run_mult = 1.45F;
  constexpr float k_bob_lat_amp = 0.018F;
  constexpr float k_bob_decay = 5.5F;

  constexpr float k_breath_freq = 0.2F;
  constexpr float k_breath_vert_amp = 0.008F;

  constexpr float k_cam_spring = 14.0F;

  constexpr float k_lean_max_deg = 1.2F;
  constexpr float k_lean_follow = 5.0F;

  constexpr float k_fov_walk = 75.0F;
  constexpr float k_fov_run_boost = 4.0F;
  constexpr float k_fov_lerp = 4.0F;

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

  const float bob_l = std::sin(m_bob_phase * 0.5F) * k_bob_lat_amp *
                      bob_run_factor * m_bob_amplitude;

  m_breath_phase += k_breath_freq * 2.0F * k_pi * std::max(dt, 0.0F);
  const float breath_idle = 1.0F - m_bob_amplitude;
  const float breath_v =
      std::sin(m_breath_phase) * k_breath_vert_amp * breath_idle;

  const float lean_target =
      -static_cast<float>(m_move_right_axis) * k_lean_max_deg;
  m_strafe_lean += (lean_target - m_strafe_lean) *
                   (1.0F - std::exp(-k_lean_follow * std::max(dt, 0.0F)));

  const float fov_target =
      k_fov_walk +
      ((m_move_running && m_move_speed > 0.05F) ? k_fov_run_boost : 0.0F) +
      m_dodge_fov_kick;
  m_fov_current += (fov_target - m_fov_current) *
                   (1.0F - std::exp(-k_fov_lerp * std::max(dt, 0.0F)));
  camera.set_perspective(m_fov_current, camera.get_aspect(), camera.get_near(),
                         camera.get_far());

  const float yaw_rad = m_view_yaw * k_deg2rad;
  const float pitch_rad = m_view_pitch * k_deg2rad;
  const float pitch_cos = std::cos(pitch_rad);
  const QVector3D forward_vec(std::sin(yaw_rad) * pitch_cos,
                              std::sin(pitch_rad),
                              std::cos(yaw_rad) * pitch_cos);
  const QVector3D flat_forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  const QVector3D flat_right(-flat_forward.z(), 0.0F, flat_forward.x());

  float jump_height_offset = 0.0F;
  if (auto const *cmd =
          commander.get_component<Engine::Core::CommanderComponent>()) {
    jump_height_offset = cmd->jump_height_offset;
  }
  const QVector3D pivot(transform->position.x,
                        transform->position.y + jump_height_offset +
                            k_focus_height,
                        transform->position.z);

  const QVector3D eye_desired =
      pivot - flat_forward * k_camera_back_offset +
      QVector3D(0.0F, k_camera_up_offset + bob_v + breath_v, 0.0F) +
      flat_right * bob_l;
  const QVector3D target_desired = pivot + forward_vec * k_target_distance;

  if (!m_cam_smooth_valid) {
    m_cam_eye_smooth = eye_desired;
    m_cam_target_smooth = target_desired;
    m_cam_smooth_valid = true;
  } else {
    const float alpha = 1.0F - std::exp(-k_cam_spring * std::max(dt, 0.0F));
    m_cam_eye_smooth += (eye_desired - m_cam_eye_smooth) * alpha;
    m_cam_target_smooth += (target_desired - m_cam_target_smooth) * alpha;
  }

  const float lean_rad = m_strafe_lean * k_deg2rad;
  const QVector3D world_up(0.0F, 1.0F, 0.0F);

  const QVector3D right_world =
      QVector3D::crossProduct(forward_vec.normalized(), world_up).normalized();
  const QVector3D up_leaned =
      (world_up + right_world * std::sin(lean_rad)).normalized();

  constexpr float k_shake_freq = 22.0F;
  constexpr float k_shake_decay = 9.5F;
  constexpr float k_shake_lateral = 0.14F;
  constexpr float k_shake_vert = 0.05F;

  if (auto const *fb =
          commander.get_component<Engine::Core::HitFeedbackComponent>()) {

    if (fb->is_reacting && fb->reaction_time < 0.05F &&
        fb->reaction_intensity > m_hit_trauma * 0.5F) {
      m_hit_trauma = fb->reaction_intensity;
      m_hit_shake_phase = 0.0F;
    }
  }
  m_hit_shake_phase += k_shake_freq * std::max(dt, 0.0F);
  m_hit_trauma *= std::exp(-k_shake_decay * std::max(dt, 0.0F));

  const float shake_lat =
      std::sin(m_hit_shake_phase) * m_hit_trauma * k_shake_lateral;
  const float shake_v = std::abs(std::sin(m_hit_shake_phase * 0.6F)) *
                        m_hit_trauma * k_shake_vert;
  const QVector3D shake_offset =
      flat_right * shake_lat - QVector3D(0.0F, shake_v, 0.0F);

  if (auto const *cmd =
          commander.get_component<Engine::Core::CommanderComponent>()) {
    if (cmd->just_struck_enemy) {
      m_strike_punch_fwd = 0.25F;
    }
  }
  constexpr float k_punch_decay = 14.0F;
  m_strike_punch_fwd *= std::exp(-k_punch_decay * std::max(dt, 0.0F));
  const QVector3D punch_offset = forward_vec * m_strike_punch_fwd;

  camera.look_at(m_cam_eye_smooth + shake_offset + punch_offset,
                 m_cam_target_smooth + shake_offset + punch_offset, up_leaned);
}
