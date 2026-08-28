#include "app/commander/commander_control_controller.h"

#include <QCursor>
#include <QQuickWindow>
#include <QVector3D>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <mutex>
#include <numbers>
#include <vector>

#include "app/commander/commander_abilities.h"
#include "app/commander/commander_motor.h"
#include "game/accessibility/commander_input_settings.h"
#include "game/accessibility/motion_settings.h"
#include "game/audio/audio_cues.h"
#include "game/core/component.h"
#include "game/core/simulation_timing.h"
#include "game/core/world.h"
#include "game/map/terrain_service.h"
#include "game/session/session_context.h"
#include "game/systems/building_collision_registry.h"
#include "game/systems/building_line_of_sight.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/combat_actions/combat_action_service.h"
#include "game/systems/combat_actions/commander_defense_timeline.h"
#include "game/systems/combat_actions/melee_intent_solver.h"
#include "game/systems/combat_system/damage_application.h"
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

constexpr float k_ability_rescan_cooldown = 0.18F;

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

auto buildings_of(const Engine::Core::World& world)
    -> const Game::Systems::BuildingCollisionRegistry& {
  return Game::Session::session_for(world).building_collision();
}

constexpr float k_fpv_walk_speed_scale = 1.25F;

constexpr float k_move_blocked_decay_rate = 16.0F;

constexpr float k_turn_in_place_threshold_degrees = 50.0F;
constexpr float k_turn_in_place_rate_degrees = 260.0F;
constexpr float k_body_travel_turn_rate_degrees = 900.0F;

constexpr float k_fov_hip = 68.0F;
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

constexpr float k_body_separation_scan_range = 3.0F;

constexpr float k_body_separation_max_push_per_second = 2.4F;

void separate_commander_from_bodies(Engine::Core::World& world,
                                    Engine::Core::Entity& commander,
                                    Engine::Core::EntityID commander_id,
                                    Engine::Core::TransformComponent& transform,
                                    App::Core::CommanderMotor& motor,
                                    float dt) {
  if (dt <= 0.0F) {
    return;
  }

  const QVector3D origin(transform.position.x, 0.0F, transform.position.z);
  QVector3D push(0.0F, 0.0F, 0.0F);

  for (auto* candidate : world.collect_entities_with<Engine::Core::UnitComponent>()) {
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
      float const min_distance = App::Core::CommanderMotor::body_radius() +
                                 std::max(soldier.body_radius, 0.05F);
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

  QVector3D const from(
      transform.position.x, transform.position.y, transform.position.z);
  static_cast<void>(
      motor.advance(Game::Session::session_for(world),
                    transform,
                    {.from = from,
                     .to = from + QVector3D(push.x(), 0.0F, push.z()),
                     .source = App::Core::CommanderDisplacementSource::BodySeparation,
                     .airborne = false,
                     .dt = dt}));
}

} // namespace

void CommanderControlController::publish_presentation_sample(
    Engine::Core::Entity& commander,
    const Engine::Core::TransformComponent& transform,
    float dt) {
  auto* sample = Engine::Core::get_or_add_component<
      Engine::Core::CommanderPresentationSampleComponent>(&commander);
  if (sample == nullptr) {
    return;
  }

  auto const previous = sample->position;
  float const step_x = transform.position.x - previous.x;
  float const step_z = transform.position.z - previous.z;
  bool const teleported =
      !sample->valid || ((step_x * step_x) + (step_z * step_z)) >
                            (Engine::Core::k_presentation_teleport_threshold *
                             Engine::Core::k_presentation_teleport_threshold);

  sample->previous_position = sample->valid ? previous : transform.position;
  sample->previous_yaw = sample->valid ? sample->yaw : transform.rotation.y;
  sample->position = transform.position;
  sample->yaw = transform.rotation.y;
  sample->tick_seconds = dt;
  sample->snap = teleported || m_presentation_snap_requested;
  sample->valid = true;
  ++sample->tick_sequence;
  m_presentation_snap_requested = false;
}

auto CommanderControlController::advance_presentation_pose(
    Engine::Core::Entity& commander,
    const Engine::Core::TransformComponent& transform,
    float dt) -> Engine::Core::PresentationPose {
  auto const* sample =
      commander.get_component<Engine::Core::CommanderPresentationSampleComponent>();
  if (sample == nullptr || !sample->valid) {
    m_presentation_pose.position = transform.position;
    m_presentation_pose.yaw = transform.rotation.y;
    m_presentation_pose.alpha = 1.0F;
    m_presentation_pose.extrapolated = false;
    return m_presentation_pose;
  }

  if (sample->tick_sequence != m_presentation_seen_sequence) {
    m_presentation_seen_sequence = sample->tick_sequence;
    m_presentation_age = 0.0F;
  }

  float const frame_dt = std::max(0.0F, dt);
  float const max_age =
      frame_dt >= sample->tick_seconds
          ? sample->tick_seconds
          : sample->tick_seconds *
                (1.0F + Engine::Core::k_presentation_max_extrapolation);
  m_presentation_age = std::min(m_presentation_age + frame_dt, max_age);
  m_presentation_pose =
      Engine::Core::resolve_presentation_pose(*sample, m_presentation_age);
  return m_presentation_pose;
}

void CommanderControlController::snap_presentation_pose() {
  m_presentation_snap_requested = true;
  m_presentation_age = 0.0F;
}

auto CommanderControlController::take_input_snapshot() -> CommanderInputSnapshot {
  const std::lock_guard<std::mutex> guard(m_input_mutex);

  CommanderInputSnapshot snapshot;
  snapshot.forward = m_input.forward;
  snapshot.backward = m_input.backward;
  snapshot.left = m_input.left;
  snapshot.right = m_input.right;
  snapshot.turn_left = m_input.turn_left;
  snapshot.turn_right = m_input.turn_right;
  snapshot.run = m_input.run;
  snapshot.primary_held = m_input.primary_action;
  snapshot.guard_held = m_input.secondary_action;

  snapshot.primary_pressed = m_primary_press_pending;
  snapshot.heavy_pressed = m_input.heavy_action_requested;
  snapshot.dodge_pressed = m_input.dodge_requested;
  snapshot.jump_pressed = m_input.jump_requested;
  snapshot.special_pressed = m_input.special_action_requested;
  snapshot.shield_bash_pressed = m_input.shield_bash_requested;
  snapshot.vanguard_rush_pressed = m_input.vanguard_rush_requested;
  snapshot.second_wind_pressed = m_input.second_wind_requested;

  snapshot.has_dodge_direction = m_has_requested_dodge_direction;
  snapshot.dodge_direction = m_requested_dodge_direction;

  m_primary_press_pending = false;
  m_input.heavy_action_requested = false;
  m_input.dodge_requested = false;
  m_input.jump_requested = false;
  m_input.special_action_requested = false;
  m_input.shield_bash_requested = false;
  m_input.vanguard_rush_requested = false;
  m_input.second_wind_requested = false;
  m_has_requested_dodge_direction = false;
  m_requested_dodge_direction = QVector3D(0.0F, 0.0F, 0.0F);

  snapshot.sequence = ++m_input_snapshot_sequence;
  return snapshot;
}

void CommanderControlController::discard_input_edges(CommanderInputSnapshot& snapshot) {
  if (m_carried_primary_press) {
    ++m_edges.primary_dropped_sequence;
    m_carried_primary_press = false;
  }
  if (snapshot.primary_pressed) {
    ++m_edges.primary_dropped_sequence;
    snapshot.primary_pressed = false;
  }
  snapshot.heavy_pressed = false;
  if (snapshot.dodge_pressed) {
    ++m_edges.dodge_refused_sequence;
    snapshot.dodge_pressed = false;
  }
  if (snapshot.jump_pressed) {
    ++m_edges.jump_refused_sequence;
    snapshot.jump_pressed = false;
  }
  snapshot.special_pressed = false;
  snapshot.shield_bash_pressed = false;
  snapshot.vanguard_rush_pressed = false;
  snapshot.second_wind_pressed = false;
  snapshot.has_dodge_direction = false;
  snapshot.dodge_direction = QVector3D(0.0F, 0.0F, 0.0F);
}

void CommanderControlController::release_all_input() {
  auto pending = take_input_snapshot();
  discard_input_edges(pending);
  discard_input_edges(m_tick_input);
  if (pending.primary_held) {
    ++m_edges.primary_release_sequence;
  }
  if (pending.guard_held) {
    ++m_edges.guard_release_sequence;
  }
  {
    const std::lock_guard<std::mutex> guard(m_input_mutex);
    m_input = {};
  }
  m_tick_input = {};
  m_move_right_axis = 0;
  m_move_forward_axis = 0;
  m_move_running = false;
  m_move_speed = 0.0F;
  m_primary_held_duration = 0.0F;
  m_primary_scan_cooldown = 0.0F;
  m_last_mouse_valid = false;
  m_mouse_center_valid = false;
  m_mouse_recentering = false;
}

void CommanderControlController::reset() {
  release_all_input();
  snap_presentation_pose();
  m_presentation_seen_sequence = 0;
  m_body_yaw_valid = false;
  m_turning_in_place = false;
  m_frame_intent = {};
  m_intent_sample_valid = false;
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
  m_jump_followup_pending = false;
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
  m_abilities.reset();
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
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  if (m_latency_probe != nullptr) {
    m_latency_probe->note_input();
  }
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
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
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
  if (m_latency_probe != nullptr) {
    m_latency_probe->note_input();
  }
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  ++m_edges.primary_press_sequence;
  m_input.primary_action = true;
  m_primary_press_pending = true;
}

void CommanderControlController::primary_action_up() {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  if (m_input.primary_action) {
    ++m_edges.primary_release_sequence;
  }
  m_input.primary_action = false;
}

void CommanderControlController::request_heavy_action() {
  if (m_latency_probe != nullptr) {
    m_latency_probe->note_input();
  }
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  m_input.heavy_action_requested = true;
}

void CommanderControlController::secondary_action_down() {
  if (m_latency_probe != nullptr) {
    m_latency_probe->note_input();
  }
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  if (Game::Accessibility::CommanderInput::guard_is_toggle()) {

    if (m_input.secondary_action) {
      ++m_edges.guard_release_sequence;
      m_input.secondary_action = false;
    } else {
      ++m_edges.guard_press_sequence;
      m_input.secondary_action = true;
    }
    return;
  }
  if (!m_input.secondary_action) {
    ++m_edges.guard_press_sequence;
  }
  m_input.secondary_action = true;
}

void CommanderControlController::secondary_action_up() {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  if (Game::Accessibility::CommanderInput::guard_is_toggle()) {

    return;
  }
  if (m_input.secondary_action) {
    ++m_edges.guard_release_sequence;
  }
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
  if (m_latency_probe != nullptr && (std::abs(dx) > 0.0 || std::abs(dy) > 0.0)) {
    m_latency_probe->note_input();
  }
  constexpr float k_mouse_yaw_sensitivity = 0.18F;
  constexpr float k_mouse_pitch_sensitivity = 0.12F;
  const float sensitivity = look_sensitivity_scale();
  const float user_x = Game::Accessibility::CommanderInput::look_sensitivity_x();
  const float user_y = Game::Accessibility::CommanderInput::look_sensitivity_y();
  const float pitch_sign =
      Game::Accessibility::CommanderInput::invert_look_y() ? 1.0F : -1.0F;
  m_view_yaw += static_cast<float>(dx) * k_mouse_yaw_sensitivity * sensitivity * user_x;
  m_view_pitch += pitch_sign * static_cast<float>(dy) * k_mouse_pitch_sensitivity *
                  sensitivity * user_y;
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

auto CommanderControlController::sample_frame_intent(QQuickWindow* window)
    -> CommanderFrameIntent {
  poll_mouse_look(window);

  if (!m_intent_sample_valid) {
    m_intent_sample_yaw = m_view_yaw;
    m_intent_sample_pitch = m_view_pitch;
    m_intent_sample_valid = true;
  }

  m_frame_intent.look_delta =
      QVector2D(signed_angle_delta(m_view_yaw, m_intent_sample_yaw),
                m_view_pitch - m_intent_sample_pitch);
  m_intent_sample_yaw = m_view_yaw;
  m_intent_sample_pitch = m_view_pitch;
  m_frame_intent.view_yaw = m_view_yaw;
  m_frame_intent.view_pitch = m_view_pitch;
  {
    const std::lock_guard<std::mutex> input_guard(m_input_mutex);
    m_frame_intent.move = QVector2D(
        static_cast<float>((m_input.right ? 1 : 0) - (m_input.left ? 1 : 0)),
        static_cast<float>((m_input.forward ? 1 : 0) - (m_input.backward ? 1 : 0)));
    m_frame_intent.guard = m_input.secondary_action;
    m_frame_intent.attack_held = m_input.primary_action;
    m_frame_intent.run = m_input.run;
    m_frame_intent.dodge_pressed = m_input.dodge_requested;
    m_frame_intent.jump_pressed = m_input.jump_requested;
  }
  ++m_frame_intent.frame_index;
  return m_frame_intent;
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
  if (m_latency_probe != nullptr) {
    m_latency_probe->note_input();
  }
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  ++m_edges.dodge_request_sequence;
  m_has_requested_dodge_direction = false;
  m_input.dodge_requested = true;
}

void CommanderControlController::request_dodge(const QVector3D& world_direction) {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  ++m_edges.dodge_request_sequence;
  m_requested_dodge_direction =
      QVector3D(world_direction.x(), 0.0F, world_direction.z());
  m_has_requested_dodge_direction =
      m_requested_dodge_direction.lengthSquared() > 0.0001F;
  m_input.dodge_requested = true;
}

void CommanderControlController::request_jump() {
  if (m_latency_probe != nullptr) {
    m_latency_probe->note_input();
  }
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  ++m_edges.jump_request_sequence;
  m_input.jump_requested = true;
}

void CommanderControlController::special_action() {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  if (m_input.secondary_action) {
    m_input.shield_bash_requested = true;
  } else {
    m_input.special_action_requested = true;
  }
}

void CommanderControlController::request_vanguard_rush() {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
  m_input.vanguard_rush_requested = true;
}

void CommanderControlController::request_second_wind() {
  const std::lock_guard<std::mutex> input_guard(m_input_mutex);
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
    int local_owner_id) {
  auto* commander = controlled_commander(world, commander_id, local_owner_id);
  if (commander == nullptr) {
    return;
  }
  if (Game::Systems::RpgCombat::toggle_weapon_stance(*commander)) {
    if (auto* intents = Engine::Core::get_or_add_component<
            Engine::Core::CombatIntentQueueComponent>(commander)) {
      Engine::Core::CombatActionIntent transition;
      transition.type = Engine::Core::CommanderCombatIntentType::WeaponSwitch;
      transition.pressed_at = intents->clock;
      intents->push(transition);
    }
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
  Engine::Core::Timing::ScopedAccumulator const lock_scope(
      Engine::Core::Timing::commander_targeting());
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

  auto& owners = Game::Session::session_for(world).owners();
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

  for (auto* candidate : world.collect_entities_with<Engine::Core::UnitComponent>()) {
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
          !Game::Systems::has_clear_building_los(
              buildings_of(world), origin, soldier.position)) {
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

  if (m_locked_target_id == 0) {

    auto const nearest_to_centre =
        std::min_element(candidates.begin(),
                         candidates.end(),
                         [](const Candidate& a, const Candidate& b) {
                           if (a.visible != b.visible) {
                             return a.visible && !b.visible;
                           }
                           if (std::abs(a.angle_diff) != std::abs(b.angle_diff)) {
                             return std::abs(a.angle_diff) < std::abs(b.angle_diff);
                           }
                           return a.distance_sq < b.distance_sq;
                         });
    m_locked_target_id = nearest_to_centre->id;
    m_locked_target_slot = nearest_to_centre->soldier_slot;
  } else {

    std::stable_sort(candidates.begin(),
                     candidates.end(),
                     [](const Candidate& a, const Candidate& b) {
                       if (a.angle_diff != b.angle_diff) {
                         return a.angle_diff < b.angle_diff;
                       }
                       return a.distance_sq < b.distance_sq;
                     });
    auto it =
        std::find_if(candidates.begin(), candidates.end(), [this](const Candidate& c) {
          return c.id == m_locked_target_id;
        });
    if (it == candidates.end() || std::next(it) == candidates.end()) {
      m_locked_target_id = candidates.front().id;
      m_locked_target_slot = candidates.front().soldier_slot;
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

namespace {

auto body_allows_now(const Engine::Core::Entity& commander)
    -> Game::Systems::CombatActions::MeleeInterruption {
  auto const* action =
      commander.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action == nullptr || !action->action_running || action->combat_action_id == 0U) {
    return {};
  }
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      static_cast<Game::Systems::CombatActions::CombatActionId>(
          action->combat_action_id));
  if (definition == nullptr) {
    return {};
  }
  return Game::Systems::CombatActions::melee_interruption_at(
      *definition, action->normalized_action_time);
}

void cancel_current_attack(Engine::Core::Entity& commander) {
  auto* action = commander.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action != nullptr && action->action_running) {
    action->action_running = false;
    action->action_active = false;
    action->weapon_trace_active = false;
    action->action_completed = true;
    action->phase = Engine::Core::RpgCommanderActionPhase::None;
  }
  if (auto* combat = commander.get_component<Engine::Core::CombatStateComponent>()) {
    combat->animation_state = Engine::Core::CombatAnimationState::Idle;
    combat->state_time = 0.0F;
    combat->state_duration = 0.0F;
    combat->is_hit_paused = false;
    combat->hit_pause_remaining = 0.0F;
  }
}

} // namespace

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
  if (action == nullptr || !action->action_running) {
    return;
  }

  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      static_cast<Game::Systems::CombatActions::CombatActionId>(
          action->combat_action_id));
  if (definition == nullptr) {
    return;
  }

  if (definition->requires_projectile_release) {
    float const movement_start = definition->movement.start_normalized;
    float const movement_end = definition->movement.end_normalized;
    if (std::abs(definition->movement.distance) <= 0.01F ||
        movement_end <= movement_start ||
        action->normalized_action_time < movement_start ||
        action->normalized_action_time > movement_end) {
      return;
    }
    float const yaw = transform.rotation.y * 0.017453292519943295F;
    float const direction = definition->movement.distance >= 0.0F ? 1.0F : -1.0F;
    QVector3D const forward(std::sin(yaw) * direction, 0.0F, std::cos(yaw) * direction);
    float const movement_seconds =
        std::max(0.05F, (movement_end - movement_start) * action->action_duration);
    float const step =
        (std::abs(definition->movement.distance) / movement_seconds) * dt;
    QVector3D const from(
        transform.position.x, transform.position.y, transform.position.z);
    static_cast<void>(m_motor.advance(
        Game::Session::session_for(world),
        transform,
        {.from = from,
         .to = from + forward * step,
         .source = App::Core::CommanderDisplacementSource::StrikeLunge,
         .airborne =
             commander.get_component<Engine::Core::CommanderComponent>()->jump_active,
         .dt = dt}));
    return;
  }

  if (Game::Systems::CombatActions::melee_interruption_at(
          *definition, action->normalized_action_time)
          .phase == Game::Systems::CombatActions::MeleePhase::FollowThrough) {
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

  if (distance > 0.0001F && definition->target_assist.maximum_turn_degrees > 0.0F) {
    float const target_yaw = std::atan2(to_x, to_z) * 57.29577951308232F;
    float const yaw_delta = signed_angle_delta(target_yaw, transform.rotation.y);
    float const turn_rate = definition->target_assist.maximum_turn_degrees /
                            std::max(0.08F, action->action_duration * 0.45F);
    float const correction = std::clamp(yaw_delta, -turn_rate * dt, turn_rate * dt);
    transform.rotation.y = wrap_angle_degrees(transform.rotation.y + correction);
    transform.desired_yaw = transform.rotation.y;
    transform.has_desired_yaw = true;
  }

  constexpr float k_lunge_contact_margin = 0.55F;
  const float contact_distance =
      std::max(k_lunge_contact_margin, definition->hit_shape.reach * 0.72F);

  const float gap = distance - contact_distance;
  float const authored_distance = std::abs(definition->movement.distance);
  float const maximum_lunge =
      authored_distance > 0.01F ? authored_distance : k_strike_step_reach;
  if (distance <= 0.0001F || gap <= 0.02F) {
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
  float const movement_start =
      definition->movement.end_normalized > definition->movement.start_normalized
          ? definition->movement.start_normalized
          : 0.0F;
  float const movement_end = definition->movement.end_normalized > movement_start
                                 ? definition->movement.end_normalized
                                 : trace_end;
  if (action->normalized_action_time < movement_start ||
      action->normalized_action_time > movement_end) {
    return;
  }
  const float swing_progress =
      movement_end > movement_start
          ? std::clamp((action->normalized_action_time - movement_start) /
                           (movement_end - movement_start),
                       0.0F,
                       1.0F)
          : 1.0F;
  const float lunge_shape = std::sin(swing_progress * std::numbers::pi_v<float>);
  if (lunge_shape <= 0.01F) {
    return;
  }

  float const movement_seconds =
      std::max(0.05F, (movement_end - movement_start) * action->action_duration);
  float const lunge_speed =
      authored_distance > 0.01F ? authored_distance / movement_seconds : 4.6F;
  float const remaining_authored_lunge = std::min(gap, maximum_lunge);
  const float step =
      std::min(remaining_authored_lunge, lunge_speed * (0.5F + lunge_shape) * dt);
  QVector3D const from(
      transform.position.x, transform.position.y, transform.position.z);
  static_cast<void>(m_motor.advance(
      Game::Session::session_for(world),
      transform,
      {.from = from,
       .to = from + QVector3D((to_x / distance) * step, 0.0F, (to_z / distance) * step),
       .source = App::Core::CommanderDisplacementSource::StrikeLunge,
       .airborne =
           commander.get_component<Engine::Core::CommanderComponent>()->jump_active,
       .dt = dt}));
}

void CommanderControlController::update_lock_on_yaw(Engine::Core::World& world,
                                                    Engine::Core::Entity& commander,
                                                    float dt) {
  if (m_locked_target_id == 0) {
    m_lock_spring_yaw_valid = false;
    m_lock_manual_override_timer = 0.0F;
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
  const bool target_visible =
      Game::Systems::has_clear_building_los(buildings_of(world), origin, target_pos);

  const float target_yaw = std::atan2(dx, dz) * 57.29577951308232F;
  const float diff = signed_angle_delta(target_yaw, m_view_yaw);
  const bool escape_input =
      (m_tick_input.run && m_tick_input.backward) ||
      (m_tick_input.dodge_pressed && (m_tick_input.backward || m_tick_input.run));
  if (escape_input || (m_tick_input.run && std::abs(diff) > 95.0F)) {
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

  constexpr float k_lock_minimum_distance = 0.75F;
  constexpr float k_lock_full_authority_distance = 2.0F;
  constexpr float k_lock_max_turn_degrees_per_second = 220.0F;
  float const target_distance = std::sqrt((dx * dx) + (dz * dz));
  if (target_distance < k_lock_minimum_distance) {

    return;
  }
  float const close_range_authority =
      std::clamp((target_distance - k_lock_minimum_distance) /
                     (k_lock_full_authority_distance - k_lock_minimum_distance),
                 0.0F,
                 1.0F);

  constexpr float k_manual_override_seconds = 0.35F;
  float const manual_look = std::abs(signed_angle_delta(m_view_yaw, m_lock_spring_yaw));
  if (m_lock_spring_yaw_valid && manual_look > 0.05F) {
    m_lock_manual_override_timer = k_manual_override_seconds;
  }
  if (m_lock_manual_override_timer > 0.0F) {
    m_lock_manual_override_timer = std::max(0.0F, m_lock_manual_override_timer - dt);
    m_lock_spring_yaw = m_view_yaw;
    m_lock_spring_yaw_valid = true;
    return;
  }

  const float k_lock_spring = target_visible ? 8.5F : 3.5F;
  float step = diff * (1.0F - std::exp(-k_lock_spring * dt)) * close_range_authority;
  float const step_limit = k_lock_max_turn_degrees_per_second * dt;
  step = std::clamp(step, -step_limit, step_limit);
  m_view_yaw += step;
  m_view_yaw = wrap_angle_degrees(m_view_yaw);
  m_lock_spring_yaw = m_view_yaw;
  m_lock_spring_yaw_valid = true;
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
  Engine::Core::Timing::ScopedAccumulator const scope(
      Engine::Core::Timing::commander_targeting());
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
  auto& owners = Game::Session::session_for(world).owners();

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
          !Game::Systems::has_clear_building_los(
              buildings_of(world), origin, sample.position)) {
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
    for (auto const& slot : engagement->engagement_slots) {
      if (!slot.pressing) {
        continue;
      }
      if (auto target = choose_sample(slot.entity_id, k_no_slot, 0.05F);
          target.has_value()) {
        return accept(*target);
      }
    }
  }

  std::optional<Target> best;
  float best_score = -1000000.0F;
  for (auto* candidate : world.collect_entities_with<Engine::Core::UnitComponent>()) {
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

  auto const* queued =
      commander->get_component<Engine::Core::CombatIntentQueueComponent>();
  auto const* pending =
      queued != nullptr && queued->count > 0U ? &queued->entries[0] : nullptr;

  Engine::Core::EntityID target_id = 0;
  if (shooting) {
    m_primary_target_slot =
        Engine::Core::RpgCommanderTargetComponent::k_no_soldier_slot;
  } else {
    float acquisition_bonus = k_strike_acquisition_bonus;
    if (pending != nullptr &&
        pending->type == Engine::Core::CommanderCombatIntentType::Heavy) {
      acquisition_bonus = 3.0F;
    } else if (pending != nullptr &&
               pending->type == Engine::Core::CommanderCombatIntentType::Special) {
      acquisition_bonus = 1.2F;
    }
    target_id =
        find_primary_target(world, commander_id, local_owner_id, acquisition_bonus);
  }
  auto const attack_result =
      Game::Systems::CombatActions::CombatActionService::request_attack(
          world,
          {.attacker_id = commander_id,
           .target_hint_id = target_id,
           .target_soldier_slot = m_primary_target_slot,
           .move_right_axis = m_move_right_axis,
           .move_forward_axis = m_move_forward_axis,
           .primary_held_duration =
               pending != nullptr ? pending->held_duration : m_primary_held_duration,
           .intent_type = pending != nullptr
                              ? pending->type
                              : Engine::Core::CommanderCombatIntentType::Light,
           .has_swing = pending != nullptr && pending->has_swing,
           .swing = pending != nullptr ? pending->swing : Engine::Core::MeleeIntent{}});

  if (attack_result.outcome == Engine::Core::CombatIntentOutcome::Accepted) {
    m_combo_miss_timer = 0.0F;
  }
  return true;
}

auto CommanderControlController::queued_intent_count(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    int local_owner_id) const -> int {
  auto const* commander = controlled_commander(world, commander_id, local_owner_id);
  auto const* intents =
      commander != nullptr
          ? commander->get_component<Engine::Core::CombatIntentQueueComponent>()
          : nullptr;
  return intents != nullptr ? static_cast<int>(intents->count) : 0;
}

void CommanderControlController::publish_resolved_defense_feedback(
    Engine::Core::Entity& commander,
    Engine::Core::EntityID commander_id,
    const Engine::Core::TransformComponent& transform) {
  auto const* rpg = commander.get_component<Engine::Core::RpgHealthComponent>();
  if (rpg == nullptr) {
    return;
  }

  QVector3D const at(transform.position.x, transform.position.y, transform.position.z);
  auto publish = [&](App::Core::PlayerFeedbackType type, const char* reason) {
    if (m_feedback == nullptr) {
      return;
    }
    App::Core::PlayerFeedbackEvent event;
    event.type = type;
    event.entity = commander_id;
    event.has_world_position = true;
    event.world_position = at;
    event.reason = QString::fromLatin1(reason);
    m_feedback->publish(std::move(event));
  };

  if (rpg->perfect_guard_contacts != m_observed_perfect_guard_contacts) {
    m_observed_perfect_guard_contacts = rpg->perfect_guard_contacts;
    publish(App::Core::PlayerFeedbackType::PerfectGuard, "perfect_guard");
  }
  if (rpg->dodged_contacts != m_observed_dodged_contacts) {
    m_observed_dodged_contacts = rpg->dodged_contacts;
    publish(App::Core::PlayerFeedbackType::DodgeSuccess, "iframe_reject");
  }
  if (rpg->blocked_contacts != m_observed_blocked_contacts) {
    m_observed_blocked_contacts = rpg->blocked_contacts;
    publish(App::Core::PlayerFeedbackType::WeaponContact, "guard_block");
  }
  if (rpg->guard_broken_contacts != m_observed_guard_broken_contacts) {
    m_observed_guard_broken_contacts = rpg->guard_broken_contacts;
    publish(App::Core::PlayerFeedbackType::GuardBroken, "guard_break");
  }
}

auto CommanderControlController::update(Engine::Core::World& world,
                                        Engine::Core::EntityID commander_id,
                                        int local_owner_id,
                                        Render::GL::Camera& camera,
                                        float dt) -> bool {
  return update_impl(world, commander_id, local_owner_id, &camera, dt);
}

auto CommanderControlController::update_simulation(Engine::Core::World& world,
                                                   Engine::Core::EntityID commander_id,
                                                   int local_owner_id,
                                                   float dt) -> bool {
  return update_impl(world, commander_id, local_owner_id, nullptr, dt);
}

void CommanderControlController::update_camera_presentation(
    Engine::Core::World& world,
    Engine::Core::EntityID commander_id,
    Render::GL::Camera& camera,
    float dt) {
  auto* commander = world.get_entity(commander_id);
  if (commander == nullptr) {
    return;
  }
  update_camera(world, *commander, camera, dt);
}

auto CommanderControlController::update_impl(Engine::Core::World& world,
                                             Engine::Core::EntityID commander_id,
                                             int local_owner_id,
                                             Render::GL::Camera* camera,
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

  m_tick_input = take_input_snapshot();
  m_tick_input.primary_pressed =
      m_tick_input.primary_pressed || m_carried_primary_press;
  m_carried_primary_press = false;

  auto const* active_action =
      commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  bool const attack_animation_active =
      active_action != nullptr && active_action->action_running;

  auto* movement = commander->get_component<Engine::Core::MovementComponent>();
  if (movement == nullptr) {
    movement = commander->add_component<Engine::Core::MovementComponent>();
  }
  auto* guard = commander->get_component<Engine::Core::CommanderGuardComponent>();

  auto const* aim_state =
      commander->get_component<Engine::Core::RpgCommanderAimComponent>();
  bool const drawing_bow = aim_state != nullptr && aim_state->is_drawing();

  if (cmd_comp != nullptr && cmd_comp->flag_rally_in_progress &&
      !cmd_comp->fpv_controlled) {
    m_abilities.advance_cooldowns(cmd_comp, dt);
    cmd_comp->fpv_motion_vx = 0.0F;
    cmd_comp->fpv_motion_vz = 0.0F;
    cmd_comp->fpv_motion_requested = false;

    discard_input_edges(m_tick_input);
    if (m_tick_input.primary_held) {
      ++m_edges.primary_release_sequence;
    }
    if (m_tick_input.guard_held) {
      ++m_edges.guard_release_sequence;
    }
    {
      const std::lock_guard<std::mutex> input_guard(m_input_mutex);
      m_input.primary_action = false;
      m_input.secondary_action = false;
    }
    m_tick_input = {};
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
      rpg->dodge_grace_remaining = 0.0F;
    }

    publish_presentation_sample(*commander, *transform, dt);
    if (camera != nullptr) {
      update_camera(world, *commander, *camera, dt);
    }
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
    if (m_tick_input.turn_left) {
      m_view_yaw -= k_turn_speed_degrees * dt;
    }
    if (m_tick_input.turn_right) {
      m_view_yaw += k_turn_speed_degrees * dt;
    }
  }
  m_view_yaw = wrap_angle_degrees(m_view_yaw);

  const int forward_axis =
      (m_tick_input.forward ? 1 : 0) - (m_tick_input.backward ? 1 : 0);
  const int right_axis = (m_tick_input.right ? 1 : 0) - (m_tick_input.left ? 1 : 0);

  const float yaw_rad = m_view_yaw * k_degrees_to_radians;
  const QVector3D forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  const QVector3D right(-forward.z(), 0.0F, forward.x());
  QVector3D move = forward * static_cast<float>(forward_axis) +
                   right * static_cast<float>(right_axis);

  float actual_speed_for_bob = 0.0F;
  bool run_for_bob = false;

  float traced_stamina = -1.0F;
  QVector3D const motor_previous_position(
      transform->position.x, transform->position.y, transform->position.z);
  auto motor_source = App::Core::CommanderDisplacementSource::None;
  bool motor_blocked = false;
  bool motor_slid = false;
  float motor_requested_speed = 0.0F;
  float motor_separation_push = 0.0F;
  float motor_lunge_distance = 0.0F;
  float motor_snap_back_distance = 0.0F;

  constexpr float k_fov_kick_decay = 22.0F;
  m_dodge_fov_kick = std::max(0.0F, m_dodge_fov_kick - k_fov_kick_decay * dt);
  constexpr float k_jump_duration = 0.96F;
  constexpr float k_jump_peak_height = 0.72F;

  const bool ability_requested = m_tick_input.shield_bash_pressed ||
                                 m_tick_input.vanguard_rush_pressed ||
                                 m_tick_input.second_wind_pressed;
  auto const* running_action =
      commander->get_component<Engine::Core::RpgCommanderActionComponent>();
  auto const* running_definition =
      running_action != nullptr && running_action->action_running
          ? Game::Systems::CombatActions::find_combat_action_definition(
                static_cast<Game::Systems::CombatActions::CombatActionId>(
                    running_action->combat_action_id))
          : nullptr;
  bool const can_jump_branch = running_definition != nullptr &&
                               running_definition->next_jump !=
                                   Game::Systems::CombatActions::CombatActionId::None;
  const bool jump_blocked_by_action =
      m_dodge_state != DodgeState::None || m_tick_input.primary_held ||
      m_tick_input.guard_held || ability_requested ||
      (combat_state != nullptr &&
       combat_state->animation_state != Engine::Core::CombatAnimationState::Idle &&
       !can_jump_branch);
  const bool should_jump =
      m_tick_input.jump_pressed && m_jump_timer <= 0.0F && !jump_blocked_by_action;
  const bool jump_refused = m_tick_input.jump_pressed && !should_jump;
  if (should_jump) {
    ++m_edges.jump_consumed_sequence;
  } else if (jump_refused) {
    ++m_edges.jump_refused_sequence;
  }
  m_tick_input.jump_pressed = false;
  if (jump_refused) {
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_ability_refused);
  }
  if (should_jump) {
    m_jump_timer = k_jump_duration;
    m_jump_followup_pending = can_jump_branch;
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_jump);
    m_jump_safe_position_valid = true;
    m_jump_last_walkable_position =
        QVector3D(transform->position.x, transform->position.y, transform->position.z);

    if (auto* stamina = commander->get_component<Engine::Core::StaminaComponent>()) {
      stamina->spend(Engine::Core::CombatStateComponent::k_stamina_cost_jump);
    }
  }

  float jump_phase = 0.0F;
  float jump_height_offset = 0.0F;
  if (m_jump_timer > 0.0F) {
    if (cmd_comp != nullptr && cmd_comp->dive_attack_active) {
      m_jump_timer = std::min(m_jump_timer, k_jump_duration * 0.42F);
      m_jump_timer = std::max(0.0F, m_jump_timer - dt * 2.8F);
    }
    m_jump_timer = std::max(0.0F, m_jump_timer - dt);
    if (m_jump_timer <= 0.0F) {
      Game::Audio::play_cue(Game::Audio::Cue::k_combat_land);
      if (cmd_comp != nullptr) {
        cmd_comp->dive_attack_active = false;
        cmd_comp->airborne_velocity = 0.0F;
      }
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
            ((m_guard_was_active || m_tick_input.guard_held) ? 8.0F : 18.0F) * dt);
  }

  if (guard != nullptr) {
    guard->perfect_guard_remaining =
        std::max(0.0F, guard->perfect_guard_remaining - dt);
    guard->guard_break_remaining = std::max(0.0F, guard->guard_break_remaining - dt);
    if (!m_tick_input.guard_held) {
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

  const bool should_dodge = m_tick_input.dodge_pressed &&
                            m_dodge_state == DodgeState::None && m_jump_timer <= 0.0F &&
                            body_allows_now(*commander).accepts_dodge;
  const bool dodge_refused = m_tick_input.dodge_pressed && !should_dodge;
  if (should_dodge) {
    ++m_edges.dodge_consumed_sequence;
    cancel_current_attack(*commander);
  } else if (dodge_refused) {
    ++m_edges.dodge_refused_sequence;
  }
  QVector3D const requested_dodge_direction = m_tick_input.dodge_direction;
  bool const has_requested_dodge_direction = m_tick_input.has_dodge_direction;
  m_tick_input.dodge_pressed = false;
  if (dodge_refused) {
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_ability_refused);
  }
  m_tick_input.has_dodge_direction = false;
  m_tick_input.dodge_direction = QVector3D(0.0F, 0.0F, 0.0F);

  if (should_dodge) {
    m_dodge_direction =
        has_requested_dodge_direction
            ? requested_dodge_direction.normalized()
            : ((move.lengthSquared() > 0.0001F) ? move.normalized() : forward);
    m_dodge_state = DodgeState::Rolling;
    if (m_latency_probe != nullptr) {
      m_latency_probe->note_dodge_start();
      m_latency_probe->note_pose_response();
    }
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_dodge);
    m_dodge_timer =
        Game::Systems::CombatActions::k_commander_dodge_timeline.roll_seconds;
    m_dodge_fov_kick = 14.0F;
    if (auto* rpg = commander->get_component<Engine::Core::RpgHealthComponent>()) {

      rpg->dodge_grace_remaining =
          Game::Systems::CombatActions::k_commander_dodge_timeline
              .invulnerable_seconds();
      rpg->dodge_dir_x = m_dodge_direction.x();
      rpg->dodge_dir_z = m_dodge_direction.z();
    }

    if (auto* stamina = commander->get_component<Engine::Core::StaminaComponent>()) {
      stamina->spend(Engine::Core::CombatStateComponent::k_stamina_cost_dodge);
    }
  }

  auto mark_jump_safe_position = [&](float x, float z) {
    if (!jump_active || !App::Core::CommanderMotor::is_walkable_at(
                            Game::Session::session_for(world), x, z)) {
      return;
    }
    m_jump_safe_position_valid = true;
    m_jump_last_walkable_position = QVector3D(x, transform->position.y, z);
  };
  mark_jump_safe_position(transform->position.x, transform->position.z);

  if (m_dodge_state == DodgeState::Rolling) {
    constexpr float k_dodge_speed = 6.5F;
    const float roll_dt = std::min(dt, m_dodge_timer);
    m_dodge_timer -= dt;

    const float nx =
        transform->position.x + m_dodge_direction.x() * k_dodge_speed * roll_dt;
    const float nz =
        transform->position.z + m_dodge_direction.z() * k_dodge_speed * roll_dt;
    auto const step = m_motor.advance(
        Game::Session::session_for(world),
        *transform,
        {.from = QVector3D(
             transform->position.x, transform->position.y, transform->position.z),
         .to = QVector3D(nx, transform->position.y, nz),
         .source = App::Core::CommanderDisplacementSource::DodgeRoll,
         .airborne = false,
         .dt = dt});
    motor_source = step.source;
    motor_requested_speed = k_dodge_speed;
    motor_blocked = step.blocked;
    motor_slid = step.slid;
    if (movement != nullptr) {
      movement->set_manual_velocity(m_dodge_direction.x() * k_dodge_speed,
                                    m_dodge_direction.z() * k_dodge_speed);
    }
    actual_speed_for_bob = k_dodge_speed;
    run_for_bob = true;

    if (m_dodge_timer <= 0.0F) {
      m_dodge_state = DodgeState::Recovering;
      m_dodge_timer =
          Game::Systems::CombatActions::k_commander_dodge_timeline.recovery_seconds;
      if (auto* rpg = commander->get_component<Engine::Core::RpgHealthComponent>()) {
        rpg->dodge_grace_remaining = 0.0F;
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
      auto const step = m_motor.advance(
          Game::Session::session_for(world),
          *transform,
          {.from = QVector3D(
               transform->position.x, transform->position.y, transform->position.z),
           .to = QVector3D(nx, transform->position.y, nz),
           .source = App::Core::CommanderDisplacementSource::DodgeRecover,
           .airborne = false,
           .dt = dt});
      motor_source = step.source;
      motor_requested_speed = speed;
      motor_blocked = step.blocked;
      motor_slid = step.slid;
      if (step.moved) {
        if (movement != nullptr) {
          movement->set_manual_velocity(step.velocity.x(), step.velocity.z());
        }
        actual_speed_for_bob = step.velocity.length();
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
      running = m_tick_input.run && !drawing_bow && stamina != nullptr &&
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
    motor_requested_speed = target_speed;

    if (m_planar_speed_smooth > 0.01F) {
      QVector3D const direction =
          move.lengthSquared() > 0.0001F ? move : m_last_move_direction;
      const float nx =
          transform->position.x + direction.x() * m_planar_speed_smooth * dt;
      const float nz =
          transform->position.z + direction.z() * m_planar_speed_smooth * dt;
      auto const step = m_motor.advance(
          Game::Session::session_for(world),
          *transform,
          {.from = QVector3D(
               transform->position.x, transform->position.y, transform->position.z),
           .to = QVector3D(nx, transform->position.y, nz),
           .source = jump_active ? App::Core::CommanderDisplacementSource::Airborne
                                 : App::Core::CommanderDisplacementSource::Walk,
           .airborne = jump_active,
           .dt = dt});
      motor_source = step.source;
      motor_blocked = step.blocked;
      motor_slid = step.slid;
      if (step.moved) {
        mark_jump_safe_position(step.position.x(), step.position.z());
        if (movement != nullptr) {
          movement->set_manual_velocity(step.velocity.x(), step.velocity.z());
        }
        actual_speed_for_bob = step.velocity.length();
        run_for_bob = running;
      } else {

        m_planar_speed_smooth *=
            std::exp(-k_move_blocked_decay_rate * std::max(dt, 0.0F));
        if (m_planar_speed_smooth < 0.05F) {
          m_planar_speed_smooth = 0.0F;
        }
        if (movement != nullptr) {
          movement->set_manual_velocity(0.0F, 0.0F);
        }
      }
    } else if (movement != nullptr) {
      movement->set_manual_velocity(0.0F, 0.0F);
    }
  }
  if (m_jump_safe_position_valid && !jump_active) {
    if (!App::Core::CommanderMotor::is_walkable_at(Game::Session::session_for(world),
                                                   transform->position.x,
                                                   transform->position.z)) {
      motor_snap_back_distance =
          std::hypot(m_jump_last_walkable_position.x() - transform->position.x,
                     m_jump_last_walkable_position.z() - transform->position.z);
      motor_source = App::Core::CommanderDisplacementSource::JumpRecovery;
      static_cast<void>(
          m_motor.teleport(*transform,
                           m_jump_last_walkable_position,
                           App::Core::CommanderDisplacementSource::JumpRecovery));
      if (movement != nullptr) {
        movement->set_manual_velocity(0.0F, 0.0F);
      }
      actual_speed_for_bob = 0.0F;
      run_for_bob = false;
    }
    m_jump_safe_position_valid = false;
  }

  if (!jump_active) {
    float const before_lunge_x = transform->position.x;
    float const before_lunge_z = transform->position.z;
    apply_strike_lunge(world, *commander, *transform, dt);
    motor_lunge_distance = std::hypot(transform->position.x - before_lunge_x,
                                      transform->position.z - before_lunge_z);
    if (motor_lunge_distance > 1.0e-5F) {
      motor_source = App::Core::CommanderDisplacementSource::StrikeLunge;
    }
    float const before_push_x = transform->position.x;
    float const before_push_z = transform->position.z;
    separate_commander_from_bodies(
        world, *commander, commander_id, *transform, m_motor, dt);
    motor_separation_push = std::hypot(transform->position.x - before_push_x,
                                       transform->position.z - before_push_z);
    if (motor_separation_push > 1.0e-5F &&
        motor_source == App::Core::CommanderDisplacementSource::None) {
      motor_source = App::Core::CommanderDisplacementSource::BodySeparation;
    }
  }

  m_move_speed = actual_speed_for_bob;
  m_move_right_axis = right_axis;
  m_move_forward_axis = forward_axis;
  m_move_running = run_for_bob;

  float action_redirect_authority = 1.0F;
  if (attack_animation_active) {
    if (auto const* definition =
            Game::Systems::CombatActions::find_combat_action_definition(
                static_cast<Game::Systems::CombatActions::CombatActionId>(
                    active_action->combat_action_id))) {
      action_redirect_authority =
          Game::Systems::CombatActions::melee_interruption_at(
              *definition, active_action->normalized_action_time)
              .redirect_authority;
    }
  }

  bool const body_must_face_view =
      m_tick_input.primary_held || m_tick_input.guard_held ||
      m_dodge_state != DodgeState::None || jump_active || drawing_bow ||
      m_locked_target_id != 0 || attack_animation_active;
  bool const body_follows_travel = m_planar_speed_smooth > 0.05F;

  if (!m_body_yaw_valid) {
    m_body_yaw = m_view_yaw;
    m_body_yaw_valid = true;
  }

  if (attack_animation_active && action_redirect_authority < 1.0F) {

    m_turning_in_place = false;
    float const offset = signed_angle_delta(m_view_yaw, m_body_yaw);
    float const step = k_body_travel_turn_rate_degrees * action_redirect_authority *
                       std::max(0.0F, dt);
    if (step <= 0.0F) {

    } else if (std::abs(offset) <= step) {
      m_body_yaw = m_view_yaw;
    } else {
      m_body_yaw = wrap_angle_degrees(m_body_yaw + std::copysign(step, offset));
    }
  } else if (body_must_face_view) {
    m_body_yaw = m_view_yaw;
    m_turning_in_place = false;
  } else {
    float const offset = signed_angle_delta(m_view_yaw, m_body_yaw);
    float turn_rate = 0.0F;
    if (body_follows_travel) {
      turn_rate = k_body_travel_turn_rate_degrees;
      m_turning_in_place = false;
    } else {
      if (std::abs(offset) > k_turn_in_place_threshold_degrees) {
        m_turning_in_place = true;
      }
      turn_rate = m_turning_in_place ? k_turn_in_place_rate_degrees : 0.0F;
    }

    float const step = turn_rate * std::max(0.0F, dt);
    if (step <= 0.0F) {

    } else if (std::abs(offset) <= step) {
      m_body_yaw = m_view_yaw;
      m_turning_in_place = false;
    } else {
      m_body_yaw = wrap_angle_degrees(m_body_yaw + std::copysign(step, offset));
    }
  }

  transform->rotation.y = m_body_yaw;
  transform->desired_yaw = m_body_yaw;
  transform->has_desired_yaw = true;

  publish_presentation_sample(*commander, *transform, dt);

  set_view_pitch(m_view_pitch);
  if (camera != nullptr) {
    update_camera(world, *commander, *camera, dt);
  }

  auto* aim = Game::Systems::RpgCombat::sync_commander_aim(
      *commander,
      {.view_yaw_degrees = m_view_yaw,
       .view_pitch_degrees = m_view_pitch,
       .move_speed = m_move_speed,
       .running = m_move_running,
       .primary_held = m_tick_input.primary_held,
       .camera_origin = m_camera_rig.eye(),
       .camera_origin_valid = m_camera_rig.eye_valid(),
       .camera_forward = m_camera_rig.forward(),
       .camera_forward_valid = m_camera_rig.forward_valid(),
       .camera_fov_degrees = m_camera_rig.fov()});
  if (cmd_comp != nullptr) {
    cmd_comp->fpv_motion_vx = (movement != nullptr) ? movement->get_vx() : 0.0F;
    cmd_comp->fpv_motion_vz = (movement != nullptr) ? movement->get_vz() : 0.0F;
    cmd_comp->fpv_motion_requested =
        motor_requested_speed > 0.0F || m_planar_speed_smooth > 0.05F;
  }

  if (movement != nullptr && actual_speed_for_bob > 0.05F) {
    movement->engage_manual_move(transform->position.x, transform->position.z);
    if (m_latency_probe != nullptr) {
      m_latency_probe->note_movement_response();
      m_latency_probe->note_pose_response();
    }

    if (auto* stamina = commander->get_component<Engine::Core::StaminaComponent>()) {
      stamina->run_requested = m_move_running;
    }
  } else if (auto* stamina =
                 commander->get_component<Engine::Core::StaminaComponent>()) {
    stamina->run_requested = false;
  }
  if (auto const* stamina =
          commander->get_component<Engine::Core::StaminaComponent>()) {
    traced_stamina = stamina->stamina;
  }

  guard = commander->get_component<Engine::Core::CommanderGuardComponent>();
  if (m_tick_input.guard_held) {
    if (guard == nullptr) {
      guard = commander->add_component<Engine::Core::CommanderGuardComponent>();
    }
    if (guard != nullptr && guard->guard_break_remaining <= 0.0F &&
        !guard->rearm_requires_release && m_dodge_state == DodgeState::None &&
        !jump_active && body_allows_now(*commander).accepts_guard) {
      guard->active = true;
      if (!m_guard_was_active) {
        cancel_current_attack(*commander);
        guard->perfect_guard_remaining =
            Game::Systems::CombatActions::k_commander_guard_timeline
                .perfect_window_seconds;
        Game::Audio::play_cue(Game::Audio::Cue::k_combat_guard_raise);
        if (m_latency_probe != nullptr) {
          m_latency_probe->note_guard_start();
          m_latency_probe->note_pose_response();
        }
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
      stamina->spend(
          Engine::Core::CombatStateComponent::k_stamina_cost_guard_per_second * dt);
    }
  }

  App::Core::CommanderAbilityRequest ability_request;
  ability_request.shield_bash = m_tick_input.shield_bash_pressed;
  ability_request.vanguard_rush = m_tick_input.vanguard_rush_pressed;
  ability_request.second_wind = m_tick_input.second_wind_pressed;
  m_tick_input.shield_bash_pressed = false;
  m_tick_input.vanguard_rush_pressed = false;
  m_tick_input.second_wind_pressed = false;

  m_abilities.advance_cooldowns(cmd_comp, dt);
  if (ability_request.any()) {
    App::Core::CommanderAbilityContext ability_context;
    ability_context.world = &world;
    ability_context.commander = commander;
    ability_context.commander_id = commander_id;
    ability_context.local_owner_id = local_owner_id;
    ability_context.view_yaw = m_view_yaw;
    ability_context.dodging = m_dodge_state != DodgeState::None;
    ability_context.airborne = m_jump_timer > 0.0F;
    ability_context.locked_target_id = m_locked_target_id;
    ability_context.soft_target_id = m_soft_target_id;
    ability_context.motor = &m_motor;
    if (m_abilities.activate(ability_request, ability_context).rescan_primary_target) {
      m_primary_scan_cooldown = k_ability_rescan_cooldown;
    }
  }

  if (m_tick_input.primary_held) {
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

  if (m_primary_scan_cooldown > 0.0F) {
    m_primary_scan_cooldown = std::max(0.0F, m_primary_scan_cooldown - dt);
  }

  auto* intents =
      Engine::Core::get_or_add_component<Engine::Core::CombatIntentQueueComponent>(
          commander);
  if (intents != nullptr) {
    Game::Systems::CombatActions::expire_stale_intents(*intents, dt);
  }

  float traced_dodge_grace_remaining = 0.0F;
  if (auto* rpg = commander->get_component<Engine::Core::RpgHealthComponent>()) {
    rpg->dodge_grace_remaining = std::max(0.0F, rpg->dodge_grace_remaining - dt);
    traced_dodge_grace_remaining = rpg->dodge_grace_remaining;
  }

  bool const waiting_for_release = aim != nullptr && aim->relaxed_from_overhold;
  bool const body_can_take_input =
      m_dodge_state != DodgeState::Rolling && !waiting_for_release;

  if (intents != nullptr && body_can_take_input) {

    auto const* held_action =
        commander->get_component<Engine::Core::RpgCommanderActionComponent>();
    auto const* held_definition =
        held_action != nullptr && held_action->action_running
            ? Game::Systems::CombatActions::find_combat_action_definition(
                  static_cast<Game::Systems::CombatActions::CombatActionId>(
                      held_action->combat_action_id))
            : nullptr;
    bool const held_melee_combo =
        m_tick_input.primary_held && !m_tick_input.primary_pressed &&
        intents->empty() && held_definition != nullptr &&
        (held_definition->weapon_family ==
             Game::Systems::CombatActions::WeaponFamily::Sword ||
         held_definition->weapon_family ==
             Game::Systems::CombatActions::WeaponFamily::Spear) &&
        held_action->normalized_action_time >=
            Game::Systems::CombatActions::action_event_normalized_time(
                *held_definition,
                Game::Systems::CombatActions::CombatActionEventType::RecoveryStart,
                0.75F);
    if (held_melee_combo) {
      auto const* body =
          commander->get_component<Engine::Core::CommanderBodyControlComponent>();
      Engine::Core::CombatActionIntent continuation;
      continuation.type = Engine::Core::CommanderCombatIntentType::Light;
      continuation.has_swing = body != nullptr;
      if (body != nullptr) {
        continuation.swing = body->steered_intent;
        continuation.swing.charge = 0.0F;
      }
      continuation.pressed_at = intents->clock;
      continuation.held_duration = 0.0F;
      intents->push(continuation);
    }

    if (m_tick_input.primary_pressed || m_tick_input.heavy_pressed ||
        m_tick_input.special_pressed || m_jump_followup_pending) {
      auto const* body =
          commander->get_component<Engine::Core::CommanderBodyControlComponent>();
      Engine::Core::CombatActionIntent intent;
      intent.type =
          m_jump_followup_pending
              ? Engine::Core::CommanderCombatIntentType::Jump
              : (m_tick_input.special_pressed
                     ? Engine::Core::CommanderCombatIntentType::Special
                     : (m_tick_input.heavy_pressed
                            ? Engine::Core::CommanderCombatIntentType::Heavy
                            : Engine::Core::CommanderCombatIntentType::Light));
      intent.has_swing = body != nullptr;
      if (body != nullptr) {
        intent.swing = body->steered_intent;
      }
      intent.pressed_at = intents->clock;
      intents->push(intent);
      if (m_tick_input.primary_pressed) {
        ++m_edges.primary_consumed_sequence;
      }
      m_tick_input.primary_pressed = false;
      m_tick_input.heavy_pressed = false;
      m_tick_input.special_pressed = false;
      m_jump_followup_pending = false;
      m_primary_scan_cooldown = 0.08F;
    }
  }
  if (!m_tick_input.primary_held) {
    if (m_tick_input.primary_pressed) {
      ++m_edges.primary_dropped_sequence;
    }
    m_tick_input.primary_pressed = false;
  } else {
    m_carried_primary_press = m_tick_input.primary_pressed;
  }
  m_tick_input.heavy_pressed = false;
  m_tick_input.special_pressed = false;

  if (intents != nullptr && !intents->empty() && body_can_take_input) {
    auto* pending = intents->front();
    if (pending != nullptr) {
      pending->held_duration =
          pending->type == Engine::Core::CommanderCombatIntentType::Heavy
              ? m_primary_held_duration
              : 0.0F;
    }
    if (!primary_action(world, commander_id, local_owner_id)) {
      return false;
    }
    if (auto const* queue =
            commander->get_component<Engine::Core::CombatIntentQueueComponent>();
        queue != nullptr &&
        queue->last_outcome == Engine::Core::CombatIntentOutcome::Accepted) {
      intents->pop_front();
      if (m_feedback != nullptr) {
        App::Core::PlayerFeedbackEvent event;
        event.type = App::Core::PlayerFeedbackType::AttackCommitted;
        event.entity = commander_id;
        event.has_world_position = true;
        event.world_position = QVector3D(
            transform->position.x, transform->position.y, transform->position.z);
        m_feedback->publish(std::move(event));
      }
      if (m_latency_probe != nullptr) {
        m_latency_probe->note_attack_start();
        m_latency_probe->note_pose_response();
      }
      m_primary_scan_cooldown = 0.08F;
    }
  }

  Game::Systems::CombatActions::advance_melee_control(
      *commander,
      {.aim_delta_x = signed_angle_delta(m_view_yaw, m_previous_view_yaw),
       .aim_delta_y = m_view_pitch - m_previous_view_pitch,
       .view_pitch_degrees = m_view_pitch,
       .move_right_axis = m_move_right_axis,
       .move_forward_axis = m_move_forward_axis,
       .held_duration = m_primary_held_duration,
       .primary_held = m_tick_input.primary_held,
       .guard_held = m_tick_input.guard_held,
       .delta_time = dt});
  m_previous_view_yaw = m_view_yaw;
  m_previous_view_pitch = m_view_pitch;

  if (auto const* struck =
          commander->get_component<Engine::Core::RpgCommanderActionComponent>()) {
    if (!struck->action_running) {
      m_observed_action_hit_count = 0;
    } else if (struck->hit_target_count > m_observed_action_hit_count) {
      m_observed_action_hit_count = struck->hit_target_count;
      float role_boost = 1.0F;
      if (auto const* definition =
              Game::Systems::CombatActions::find_combat_action_definition(
                  static_cast<Game::Systems::CombatActions::CombatActionId>(
                      struck->combat_action_id));
          definition != nullptr) {
        using Game::Systems::CombatActions::CommanderActionRole;
        if (definition->role == CommanderActionRole::Dive ||
            definition->role == CommanderActionRole::Finisher) {
          role_boost = 1.55F;
        } else if (definition->role == CommanderActionRole::Launcher ||
                   definition->role == CommanderActionRole::Special) {
          role_boost = 1.3F;
        }
      }
      m_camera_rig.add_impact_kick(
          role_boost * std::clamp(struck->last_contact_speed /
                                      Game::Systems::Combat::k_reference_weapon_speed,
                                  0.35F,
                                  1.8F));
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

  publish_resolved_defense_feedback(*commander, commander_id, *transform);

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
  if (m_latency_probe != nullptr) {
    m_latency_probe->note_simulation_response();
  }

  if (m_trace_enabled) {
    ++m_trace.sequence;
    m_trace.valid = true;
    m_trace.time_seconds += dt;

    m_trace.input = m_edges;
    m_trace.input.frame_index = m_frame_intent.frame_index;
    m_trace.input.move_forward_axis = forward_axis;
    m_trace.input.move_right_axis = right_axis;
    m_trace.input.run_held = m_tick_input.run;
    m_trace.input.primary_held = m_tick_input.primary_held;
    m_trace.input.guard_held = m_tick_input.guard_held;
    m_trace.input.primary_held_duration = m_primary_held_duration;
    m_trace.input.look_delta_yaw = m_view_yaw - m_previous_view_yaw;
    m_trace.input.look_delta_pitch = m_view_pitch - m_previous_view_pitch;
    m_trace.input.view_yaw = m_view_yaw;
    m_trace.input.view_pitch = m_view_pitch;

    QVector3D const motor_position(
        transform->position.x, transform->position.y, transform->position.z);
    m_trace.motor.previous_position = motor_previous_position;
    m_trace.motor.position = motor_position;
    m_trace.motor.desired_velocity =
        (move.lengthSquared() > 0.0001F ? move.normalized() : QVector3D()) *
        motor_requested_speed;
    m_trace.motor.actual_velocity =
        dt > 0.0F ? (motor_position - motor_previous_position) / dt : QVector3D();
    m_trace.motor.requested_speed = motor_requested_speed;
    m_trace.motor.smoothed_speed = m_planar_speed_smooth;
    m_trace.motor.speed_error =
        m_trace.motor.actual_velocity.length() - motor_requested_speed;
    m_trace.motor.grounded = !jump_active;
    m_trace.motor.blocked = motor_blocked;
    m_trace.motor.slid = motor_slid;
    m_trace.motor.separation_push = motor_separation_push;
    m_trace.motor.lunge_distance = motor_lunge_distance;
    m_trace.motor.snap_back_distance = motor_snap_back_distance;
    m_trace.motor.displacement_source = motor_source;
    m_trace.motor.dt = dt;
    m_trace.motor.presented_position = QVector3D(m_presentation_pose.position.x,
                                                 m_presentation_pose.position.y,
                                                 m_presentation_pose.position.z);
    m_trace.motor.presented_yaw = m_presentation_pose.yaw;
    m_trace.motor.presentation_alpha = m_presentation_pose.alpha;
    m_trace.motor.presentation_extrapolated = m_presentation_pose.extrapolated;

    m_trace.camera = m_camera_rig.trace();

    auto const costs = Engine::Core::Timing::sample_and_reset_rpg_costs();
    constexpr float k_microseconds_to_ms = 0.001F;
    m_trace.costs.motor_ms = static_cast<float>(costs.motor_us) * k_microseconds_to_ms;
    m_trace.costs.targeting_ms =
        static_cast<float>(costs.targeting_us) * k_microseconds_to_ms;
    m_trace.costs.weapon_trace_ms =
        static_cast<float>(costs.weapon_trace_us) * k_microseconds_to_ms;
    m_trace.costs.engagement_ms =
        static_cast<float>(costs.engagement_us) * k_microseconds_to_ms;
    m_trace.costs.camera_ms =
        static_cast<float>(costs.camera_us) * k_microseconds_to_ms;

    m_trace.combat = App::Core::CommanderCombatTrace{};
    if (active_action != nullptr) {
      m_trace.combat.action_phase = static_cast<int>(active_action->phase);
      m_trace.combat.action_normalized_time = active_action->normalized_action_time;
      m_trace.combat.action_running = active_action->action_running;
      m_trace.combat.action_hit_count = active_action->hit_target_count;
    }
    if (intents != nullptr) {
      m_trace.combat.queued_intents = static_cast<int>(intents->count);
      m_trace.combat.queue_outcome = static_cast<int>(intents->last_outcome);
      m_trace.combat.queue_outcome_age = intents->last_outcome_age;
      m_trace.combat.queue_accepted = intents->accepted_intents;
      m_trace.combat.queue_buffered = intents->buffered_intents;
      m_trace.combat.queue_refused = intents->refused_intents;
      m_trace.combat.queue_expired = intents->expired_intents;
      m_trace.combat.queue_overflow = intents->overflow_intents;
    }
    if (auto const* defense =
            commander->get_component<Engine::Core::RpgHealthComponent>()) {
      m_trace.combat.blocked_contacts = defense->blocked_contacts;
      m_trace.combat.perfect_guard_contacts = defense->perfect_guard_contacts;
      m_trace.combat.dodged_contacts = defense->dodged_contacts;
      m_trace.combat.damaging_contacts = defense->damaging_contacts;
      m_trace.combat.guard_broken_contacts = defense->guard_broken_contacts;
    }
    if (active_action != nullptr && active_action->action_running) {
      if (auto const* definition =
              Game::Systems::CombatActions::find_combat_action_definition(
                  static_cast<Game::Systems::CombatActions::CombatActionId>(
                      active_action->combat_action_id))) {
        m_trace.combat.action_window_start =
            Game::Systems::CombatActions::action_event_normalized_time(
                *definition,
                Game::Systems::CombatActions::CombatActionEventType::WeaponTraceStart,
                0.0F);
        m_trace.combat.action_window_end =
            Game::Systems::CombatActions::action_event_normalized_time(
                *definition,
                Game::Systems::CombatActions::CombatActionEventType::WeaponTraceEnd,
                1.0F);
      }
    }
    if (guard != nullptr) {
      m_trace.combat.guard_active = guard->active;
      m_trace.combat.perfect_guard_remaining = guard->perfect_guard_remaining;
    }
    m_trace.combat.dodge_state = static_cast<int>(m_dodge_state);
    m_trace.combat.dodge_timer = m_dodge_timer;
    m_trace.combat.dodge_grace_remaining = traced_dodge_grace_remaining;
    m_trace.combat.health = unit->health;
    m_trace.combat.locked_target_id = m_locked_target_id;
    m_trace.combat.locked_target_slot =
        m_locked_target_slot == std::numeric_limits<std::uint16_t>::max()
            ? -1
            : static_cast<int>(m_locked_target_slot);
    m_trace.combat.soft_target_id = m_soft_target_id;
    m_trace.combat.soft_target_slot =
        m_soft_target_slot == std::numeric_limits<std::uint16_t>::max()
            ? -1
            : static_cast<int>(m_soft_target_slot);
    m_trace.combat.hit_confirm_sequence = m_observed_hit_confirm_sequence;
    m_trace.combat.stamina = traced_stamina;
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
  Engine::Core::EntityID nearest_pressing_id = 0;
  if (auto const* engagement =
          commander.get_component<Engine::Core::RpgEngagementComponent>()) {
    fight_context = engagement->fight_context;

    float pressure_bias = 0.0F;
    for (auto const& slot : engagement->engagement_slots) {
      if (!slot.pressing) {
        continue;
      }
      if (nearest_pressing_id == 0) {
        nearest_pressing_id = slot.entity_id;
      }
      pressure_bias += slot.signed_angle_degrees < 0.0F ? 1.0F : -1.0F;
    }
    if (fight_context == Engine::Core::FightContext::Skirmish) {
      threat_side_bias = std::clamp(pressure_bias, -1.0F, 1.0F);
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
  if (!lock_target_position.has_value() && nearest_pressing_id != 0) {
    if (auto* front = world.get_entity(nearest_pressing_id)) {
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
  auto const pose = advance_presentation_pose(commander, *transform, dt);
  inputs.commander_position =
      QVector3D(pose.position.x, pose.position.y, pose.position.z);
  inputs.lock_target_position = lock_target_position;
  inputs.soft_focus_position = soft_focus_position;
  inputs.fight_context = fight_context;
  inputs.threat_side_bias = threat_side_bias;
  auto& camera_session = Game::Session::session_for(world);
  inputs.terrain = &camera_session.terrain();
  inputs.buildings = &camera_session.building_collision();

  float const previous_bob_phase = m_camera_rig.update(camera, inputs);
  if (m_latency_probe != nullptr) {
    m_latency_probe->note_camera_response();
  }
  play_footstep_if_stride_landed(commander, previous_bob_phase);
}
