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

void CommanderControlController::reset() {
  m_input = {};
  m_mouse_center_valid = false;
  m_last_mouse_valid = false;
  m_mouse_warp_supported = false;
  m_mouse_recentering = false;
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

auto CommanderControlController::controlled_commander(
    Engine::Core::World &world, Engine::Core::EntityID commander_id,
    int local_owner_id) const -> Engine::Core::Entity * {
  if (commander_id == 0) {
    return nullptr;
  }

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
    int local_owner_id) const -> Engine::Core::EntityID {
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

  const auto target_id =
      find_primary_target(world, commander_id, local_owner_id);
  if (target_id == 0) {
    return true;
  }

  if (auto *attack =
          commander->get_component<Engine::Core::AttackComponent>()) {
    if (attack->can_melee) {
      attack->current_mode = Engine::Core::AttackComponent::CombatMode::Melee;
    }
  }

  auto *target =
      commander->get_component<Engine::Core::AttackTargetComponent>();
  if (target == nullptr) {
    target = commander->add_component<Engine::Core::AttackTargetComponent>();
  }
  if (target != nullptr) {
    target->target_id = target_id;
    target->should_chase = false;
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

  constexpr float k_degrees_to_radians = 0.017453292519943295F;
  constexpr float k_turn_speed_degrees = 105.0F;
  if (m_input.turn_left) {
    m_view_yaw -= k_turn_speed_degrees * dt;
  }
  if (m_input.turn_right) {
    m_view_yaw += k_turn_speed_degrees * dt;
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
  if (move.lengthSquared() > 0.0001F) {
    move.normalize();
    float speed = std::max(0.1F, unit->speed);
    if (m_input.run) {
      speed *= Engine::Core::StaminaComponent::k_run_speed_multiplier;
    }

    const QVector3D current(transform->position.x, transform->position.y,
                            transform->position.z);
    const QVector3D next = current + move * speed * dt;
    bool can_move = true;
    if (auto *pathfinder = Game::Systems::CommandService::get_pathfinder()) {
      const auto grid =
          Game::Systems::CommandService::world_to_grid(next.x(), next.z());
      can_move = pathfinder->is_walkable(grid.x, grid.y);
    } else {
      auto &terrain_service = Game::Map::TerrainService::instance();
      if (terrain_service.is_initialized()) {
        can_move =
            terrain_service.is_walkable(static_cast<int>(std::round(next.x())),
                                        static_cast<int>(std::round(next.z())));
      }
    }

    if (can_move) {
      transform->position.x = next.x();
      transform->position.z = next.z();
      if (movement != nullptr) {
        movement->vx = move.x() * speed;
        movement->vz = move.z() * speed;
      }
    } else if (movement != nullptr) {
      movement->vx = 0.0F;
      movement->vz = 0.0F;
    }
  } else if (movement != nullptr) {
    movement->vx = 0.0F;
    movement->vz = 0.0F;
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

  if (m_input.primary_action_scan_cooldown > 0.0F) {
    m_input.primary_action_scan_cooldown =
        std::max(0.0F, m_input.primary_action_scan_cooldown - dt);
  }
  if (m_input.primary_action && m_input.primary_action_scan_cooldown <= 0.0F) {
    if (!primary_action(world, commander_id, local_owner_id)) {
      return false;
    }
    m_input.primary_action_scan_cooldown = 0.08F;
  }

  update_camera(*commander, camera);
  return true;
}

void CommanderControlController::update_camera(Engine::Core::Entity &commander,
                                               Render::GL::Camera &camera) {
  auto *transform = commander.get_component<Engine::Core::TransformComponent>();
  if (transform == nullptr) {
    return;
  }

  constexpr float k_degrees_to_radians = 0.017453292519943295F;
  constexpr float k_focus_height = 1.4F;
  constexpr float k_camera_back_offset = 2.15F;
  constexpr float k_camera_up_offset = 0.7F;
  constexpr float k_target_distance = 5.0F;
  set_view_pitch(m_view_pitch);

  const float yaw_rad = m_view_yaw * k_degrees_to_radians;
  const float pitch_rad = m_view_pitch * k_degrees_to_radians;
  const float pitch_cos = std::cos(pitch_rad);
  const QVector3D forward(std::sin(yaw_rad) * pitch_cos, std::sin(pitch_rad),
                          std::cos(yaw_rad) * pitch_cos);
  const QVector3D flat_forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  const QVector3D pivot(transform->position.x,
                        transform->position.y + k_focus_height,
                        transform->position.z);
  const QVector3D eye = pivot - flat_forward * k_camera_back_offset +
                        QVector3D(0.0F, k_camera_up_offset, 0.0F);
  camera.look_at(eye, pivot + forward * k_target_distance,
                 QVector3D(0.0F, 1.0F, 0.0F));
}
