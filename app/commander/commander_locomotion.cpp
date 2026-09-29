#include "app/commander/commander_locomotion.h"

#include <algorithm>
#include <cmath>

#include "app/commander/commander_heading.h"
#include "game/audio/audio_cues.h"
#include "game/core/component.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/combat_actions/commander_defense_timeline.h"
#include "game/systems/run_stamina.h"

namespace App::Core {

namespace {

constexpr float k_jump_duration = 0.96F;
constexpr float k_jump_peak_height = 0.72F;
constexpr float k_dive_fall_velocity = -11.0F;

constexpr float k_fov_kick_decay = 22.0F;
constexpr float k_dodge_fov_kick = 14.0F;
constexpr float k_dodge_speed = 6.5F;

constexpr float k_fpv_walk_speed_scale = 1.25F;
constexpr float k_fpv_backpedal_speed_scale = 0.72F;
constexpr float k_fpv_strafe_speed_scale = 0.86F;
constexpr float k_drawn_bow_move_scale = 0.55F;
constexpr float k_commander_rest_speed = 0.05F;
constexpr float k_accepted_speed_follow_rate = 8.0F;

auto directional_speed_scale(int forward_axis, int right_axis) -> float {
  if (forward_axis < 0) {
    return k_fpv_backpedal_speed_scale;
  }
  if (forward_axis == 0 && right_axis != 0) {
    return k_fpv_strafe_speed_scale;
  }
  return 1.0F;
}

auto position_of(const Engine::Core::TransformComponent& transform) -> QVector3D {
  return {transform.position.x, transform.position.y, transform.position.z};
}

struct WalkPlan {
  float speed{0.0F};
  bool running{false};
  QVector3D direction{0.0F, 0.0F, 0.0F};
};

auto plan_walk(const CommanderHandles& body,
               const MotionTickInput& input,
               QVector3D& move) -> WalkPlan {
  WalkPlan plan;
  if (move.lengthSquared() <= 0.0001F) {
    return plan;
  }
  move.normalize();
  plan.direction = move;
  float speed = std::max(0.1F, body.unit.speed) * k_fpv_walk_speed_scale;

  auto const* stamina = Game::Systems::ensure_run_stamina(body.entity);
  plan.running = input.run_held && !input.drawing_bow && stamina != nullptr &&
                 (stamina->is_running || stamina->can_start_running());
  if (plan.running) {
    speed *= Engine::Core::StaminaComponent::k_run_speed_multiplier;
  }
  if (input.drawing_bow) {
    speed *= k_drawn_bow_move_scale;
  }
  speed *= directional_speed_scale(input.forward_axis, input.right_axis);
  plan.speed = speed;
  return plan;
}

} // namespace

auto CommanderLocomotion::move_basis(
    float view_yaw,
    int forward_axis,
    int right_axis,
    const Engine::Core::TransformComponent& transform,
    const std::optional<QVector3D>& lock_target_position) -> MoveBasis {
  const float yaw_rad = view_yaw * k_degrees_to_radians;
  const QVector3D forward(std::sin(yaw_rad), 0.0F, std::cos(yaw_rad));
  const QVector3D right(-forward.z(), 0.0F, forward.x());
  QVector3D move = forward * static_cast<float>(forward_axis) +
                   right * static_cast<float>(right_axis);

  if (lock_target_position.has_value()) {
    QVector3D away(transform.position.x - lock_target_position->x(),
                   0.0F,
                   transform.position.z - lock_target_position->z());
    if (away.lengthSquared() > 0.0001F) {
      away.normalize();
      const QVector3D tangent(-away.z(), 0.0F, away.x());
      const QVector3D radial = -away;
      move = radial * static_cast<float>(forward_axis) +
             tangent * static_cast<float>(right_axis);
    }
  }
  return {.forward = forward, .move = move};
}

auto CommanderLocomotion::follows_travel() const -> bool {
  return m_planar_velocity.length() > k_commander_rest_speed;
}

void CommanderLocomotion::decay_dodge_kick(float dt) {
  m_dodge_fov_kick = std::max(0.0F, m_dodge_fov_kick - k_fov_kick_decay * dt);
}

void CommanderLocomotion::resume_authored_jump(
    const CommanderHandles& body,
    const Engine::Core::RpgCommanderActionComponent* running_action,
    const Game::Systems::CombatActions::CombatActionDefinition* current_definition) {
  bool const authored_airborne_action =
      running_action != nullptr && running_action->action_running &&
      current_definition != nullptr &&
      current_definition->locomotion ==
          Game::Systems::CombatActions::ActionLocomotionRequirement::Airborne;
  if (m_jump_timer > 0.0F || body.commander_data == nullptr ||
      !body.commander_data->jump_active || !authored_airborne_action) {
    return;
  }

  float const authored_phase =
      std::clamp(running_action->normalized_action_time, 0.0F, 0.96F);
  m_jump_timer = std::max(0.04F, k_jump_duration * (1.0F - authored_phase));
  m_jump_safe_position_valid = true;
  m_jump_last_walkable_position = position_of(body.transform);
}

void CommanderLocomotion::begin_jump(const CommanderHandles& body, bool followup) {
  m_jump_timer = k_jump_duration;
  m_jump_followup_pending = followup;
  Game::Audio::play_cue(Game::Audio::Cue::k_combat_jump);
  m_jump_safe_position_valid = true;
  m_jump_last_walkable_position = position_of(body.transform);

  if (auto* stamina = body.entity.get_component<Engine::Core::StaminaComponent>()) {
    stamina->spend(Engine::Core::CombatStateComponent::k_stamina_cost_jump);
  }
}

void CommanderLocomotion::tick_jump_timer(const CommanderHandles& body, float dt) {
  auto* cmd_comp = body.commander_data;
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
  if (cmd_comp == nullptr) {
    return;
  }
  cmd_comp->jump_active = jump_active;
  cmd_comp->jump_phase = jump_phase;
  cmd_comp->jump_height_offset = jump_height_offset;
  if (jump_active) {
    cmd_comp->airborne_velocity =
        cmd_comp->dive_attack_active
            ? k_dive_fall_velocity
            : (4.0F * k_jump_peak_height / k_jump_duration) *
                  (1.0F - 2.0F * std::clamp(jump_phase, 0.0F, 1.0F));
  }
}

auto CommanderLocomotion::advance_jump(const CommanderHandles& body,
                                       const JumpTickInput& input) -> JumpTickResult {
  auto const* running_action =
      body.entity.get_component<Engine::Core::RpgCommanderActionComponent>();
  auto const* current_definition =
      running_action != nullptr && running_action->combat_action_id != 0U
          ? Game::Systems::CombatActions::find_combat_action_definition(
                static_cast<Game::Systems::CombatActions::CombatActionId>(
                    running_action->combat_action_id))
          : nullptr;
  resume_authored_jump(body, running_action, current_definition);

  auto const* combat_state =
      body.entity.get_component<Engine::Core::CombatStateComponent>();
  bool const can_jump_branch = current_definition != nullptr &&
                               current_definition->next_jump !=
                                   Game::Systems::CombatActions::CombatActionId::None;
  const bool jump_blocked_by_action =
      m_dodge_state != DodgeState::None || input.primary_held || input.guard_held ||
      input.ability_requested ||
      (combat_state != nullptr &&
       combat_state->animation_state != Engine::Core::CombatAnimationState::Idle &&
       !can_jump_branch);
  const bool should_jump =
      input.jump_pressed && m_jump_timer <= 0.0F && !jump_blocked_by_action;
  const bool jump_refused = input.jump_pressed && !should_jump;

  JumpTickResult result;
  if (should_jump) {
    result.edge = EdgeOutcome::Consumed;
  } else if (jump_refused) {
    result.edge = EdgeOutcome::Refused;
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_ability_refused);
  }
  if (should_jump) {
    begin_jump(body, can_jump_branch);
  }

  tick_jump_timer(body, input.dt);
  result.active = m_jump_timer > 0.0F;
  return result;
}

auto CommanderLocomotion::try_start_dodge(const CommanderHandles& body,
                                          const DodgeStartInput& input,
                                          CommanderLatencyProbe* probe) -> EdgeOutcome {
  const bool should_dodge = input.pressed && m_dodge_state == DodgeState::None &&
                            m_jump_timer <= 0.0F &&
                            body_allows_now(body.entity).accepts_dodge;
  const bool dodge_refused = input.pressed && !should_dodge;
  EdgeOutcome outcome = EdgeOutcome::None;
  if (should_dodge) {
    outcome = EdgeOutcome::Consumed;
    cancel_current_attack(body.entity);
  } else if (dodge_refused) {
    outcome = EdgeOutcome::Refused;
    Game::Audio::play_cue(Game::Audio::Cue::k_combat_ability_refused);
  }
  if (!should_dodge) {
    return outcome;
  }

  m_dodge_direction = input.has_direction ? input.direction.normalized()
                                          : ((input.move.lengthSquared() > 0.0001F)
                                                 ? input.move.normalized()
                                                 : input.forward);
  m_dodge_state = DodgeState::Rolling;
  if (probe != nullptr) {
    probe->note_dodge_start();
    probe->note_pose_response();
  }
  Game::Audio::play_cue(Game::Audio::Cue::k_combat_dodge);
  m_dodge_timer = Game::Systems::CombatActions::k_commander_dodge_timeline.roll_seconds;
  m_dodge_fov_kick = k_dodge_fov_kick;
  if (auto* rpg = body.entity.get_component<Engine::Core::RpgHealthComponent>()) {
    rpg->dodge_grace_remaining =
        Game::Systems::CombatActions::k_commander_dodge_timeline.invulnerable_seconds();
    rpg->dodge_dir_x = m_dodge_direction.x();
    rpg->dodge_dir_z = m_dodge_direction.z();
  }

  if (auto* stamina = body.entity.get_component<Engine::Core::StaminaComponent>()) {
    stamina->spend(Engine::Core::CombatStateComponent::k_stamina_cost_dodge);
  }
  return outcome;
}

void CommanderLocomotion::mark_jump_safe_position(const CommanderHandles& body,
                                                  bool jump_active,
                                                  float x,
                                                  float z) {
  if (!jump_active || !CommanderMotor::is_walkable_at(body.entity, x, z)) {
    return;
  }
  m_jump_safe_position_valid = true;
  m_jump_last_walkable_position = QVector3D(x, body.transform.position.y, z);
}

void CommanderLocomotion::roll(const CommanderHandles& body,
                               CommanderMotor& motor,
                               float dt,
                               MotorReport& report) {
  auto& transform = body.transform;
  const float roll_dt = std::min(dt, m_dodge_timer);
  m_dodge_timer -= dt;
  report.dodge_pose_active = true;
  report.dodge_pose_phase =
      1.0F -
      std::clamp(
          m_dodge_timer /
              Game::Systems::CombatActions::k_commander_dodge_timeline.roll_seconds,
          0.0F,
          1.0F);

  const float nx =
      transform.position.x + m_dodge_direction.x() * k_dodge_speed * roll_dt;
  const float nz =
      transform.position.z + m_dodge_direction.z() * k_dodge_speed * roll_dt;
  auto const step = motor.advance(body.entity,
                                  transform,
                                  {.from = position_of(transform),
                                   .to = QVector3D(nx, transform.position.y, nz),
                                   .source = CommanderDisplacementSource::DodgeRoll,
                                   .airborne = false,
                                   .dt = dt});
  report.source = step.source;
  report.requested_speed = k_dodge_speed;
  report.blocked = step.blocked;
  report.slid = step.slid;
  m_planar_velocity = m_dodge_direction * k_dodge_speed;
  if (body.movement != nullptr) {
    body.movement->set_manual_velocity(m_dodge_direction.x() * k_dodge_speed,
                                       m_dodge_direction.z() * k_dodge_speed);
  }
  report.actual_speed = k_dodge_speed;
  report.running = true;

  if (m_dodge_timer <= 0.0F) {
    m_dodge_state = DodgeState::Recovering;
    m_dodge_timer =
        Game::Systems::CombatActions::k_commander_dodge_timeline.recovery_seconds;
    if (auto* rpg = body.entity.get_component<Engine::Core::RpgHealthComponent>()) {
      rpg->dodge_grace_remaining = 0.0F;
    }
  }
}

void CommanderLocomotion::recover_roll(const CommanderHandles& body,
                                       CommanderMotor& motor,
                                       const MotionTickInput& input,
                                       MotorReport& report) {
  auto& transform = body.transform;
  m_dodge_timer -= input.dt;
  if (m_dodge_timer <= 0.0F) {
    m_dodge_state = DodgeState::None;
    m_dodge_timer = 0.0F;
  }

  if (report.move.lengthSquared() <= 0.0001F) {
    m_planar_velocity = QVector3D(0.0F, 0.0F, 0.0F);
    if (body.movement != nullptr) {
      body.movement->set_manual_velocity(0.0F, 0.0F);
    }
    return;
  }

  report.move.normalize();
  const float speed = std::max(0.1F, body.unit.speed) * 0.4F;
  const float nx = transform.position.x + report.move.x() * speed * input.dt;
  const float nz = transform.position.z + report.move.z() * speed * input.dt;
  auto const step = motor.advance(body.entity,
                                  transform,
                                  {.from = position_of(transform),
                                   .to = QVector3D(nx, transform.position.y, nz),
                                   .source = CommanderDisplacementSource::DodgeRecover,
                                   .airborne = false,
                                   .dt = input.dt});
  report.source = step.source;
  report.requested_speed = speed;
  report.blocked = step.blocked;
  report.slid = step.slid;
  if (step.moved) {
    m_planar_velocity = QVector3D(step.velocity.x(), 0.0F, step.velocity.z());
    if (body.movement != nullptr) {
      body.movement->set_manual_velocity(step.velocity.x(), step.velocity.z());
    }
    report.actual_speed = step.velocity.length();
  } else if (body.movement != nullptr) {
    m_planar_velocity = QVector3D(0.0F, 0.0F, 0.0F);
    body.movement->set_manual_velocity(0.0F, 0.0F);
  }
}

void CommanderLocomotion::accelerate_toward(const QVector3D& desired_velocity,
                                            float target_speed,
                                            float dt) {
  QVector3D const velocity_delta = desired_velocity - m_planar_velocity;
  float const velocity_delta_length = velocity_delta.length();
  float const acceleration_limit =
      (target_speed > 0.0F ? k_commander_ground_acceleration_mps2
                           : k_commander_ground_deceleration_mps2) *
      std::max(dt, 0.0F);
  if (velocity_delta_length <= acceleration_limit || velocity_delta_length <= 1.0e-5F) {
    m_planar_velocity = desired_velocity;
  } else {
    m_planar_velocity += velocity_delta * (acceleration_limit / velocity_delta_length);
  }
  if (target_speed <= 0.0F && m_planar_velocity.length() < k_commander_rest_speed) {
    m_planar_velocity = QVector3D(0.0F, 0.0F, 0.0F);
  }
}

void CommanderLocomotion::walk(const CommanderHandles& body,
                               CommanderMotor& motor,
                               const MotionTickInput& input,
                               MotorReport& report) {
  auto& transform = body.transform;
  auto const plan = plan_walk(body, input, report.move);
  accelerate_toward(plan.direction * plan.speed, plan.speed, input.dt);
  report.requested_speed = plan.speed;

  if (m_planar_velocity.length() <= 0.01F) {
    if (body.movement != nullptr) {
      body.movement->set_manual_velocity(0.0F, 0.0F);
    }
    return;
  }

  const float nx = transform.position.x + m_planar_velocity.x() * input.dt;
  const float nz = transform.position.z + m_planar_velocity.z() * input.dt;
  auto const step =
      motor.advance(body.entity,
                    transform,
                    {.from = position_of(transform),
                     .to = QVector3D(nx, transform.position.y, nz),
                     .source = input.jump_active ? CommanderDisplacementSource::Airborne
                                                 : CommanderDisplacementSource::Walk,
                     .airborne = input.jump_active,
                     .dt = input.dt});
  report.source = step.source;
  report.blocked = step.blocked;
  report.slid = step.slid;
  if (step.moved) {
    mark_jump_safe_position(
        body, input.jump_active, step.position.x(), step.position.z());

    m_planar_velocity = QVector3D(step.velocity.x(), 0.0F, step.velocity.z());
    if (body.movement != nullptr) {
      body.movement->set_manual_velocity(step.velocity.x(), step.velocity.z());
    }
    report.actual_speed = step.velocity.length();
    report.running = plan.running;
  } else {
    m_planar_velocity = QVector3D(0.0F, 0.0F, 0.0F);
    if (body.movement != nullptr) {
      body.movement->set_manual_velocity(0.0F, 0.0F);
    }
  }
}

auto CommanderLocomotion::advance_motion(const CommanderHandles& body,
                                         CommanderMotor& motor,
                                         const MotionTickInput& input) -> MotorReport {
  MotorReport report;
  report.move = input.move;

  mark_jump_safe_position(
      body, input.jump_active, body.transform.position.x, body.transform.position.z);

  if (m_dodge_state == DodgeState::Rolling) {
    roll(body, motor, input.dt, report);
  } else if (m_dodge_state == DodgeState::Recovering) {
    recover_roll(body, motor, input, report);
  } else {
    walk(body, motor, input, report);
  }

  if (body.commander_data != nullptr) {
    body.commander_data->dodge_active = report.dodge_pose_active;
    body.commander_data->dodge_phase = report.dodge_pose_phase;
  }
  return report;
}

void CommanderLocomotion::recover_from_fall(const CommanderHandles& body,
                                            CommanderMotor& motor,
                                            bool jump_active,
                                            MotorReport& report) {
  if (!m_jump_safe_position_valid || jump_active) {
    return;
  }
  auto& transform = body.transform;
  if (!CommanderMotor::is_walkable_at(
          body.entity, transform.position.x, transform.position.z)) {
    report.snap_back_distance =
        std::hypot(m_jump_last_walkable_position.x() - transform.position.x,
                   m_jump_last_walkable_position.z() - transform.position.z);
    report.source = CommanderDisplacementSource::JumpRecovery;
    static_cast<void>(motor.teleport(transform,
                                     m_jump_last_walkable_position,
                                     CommanderDisplacementSource::JumpRecovery));
    if (body.movement != nullptr) {
      body.movement->set_manual_velocity(0.0F, 0.0F);
    }
    report.actual_speed = 0.0F;
    report.running = false;
  }
  m_jump_safe_position_valid = false;
}

void CommanderLocomotion::commit_motion(const MotorReport& report,
                                        int forward_axis,
                                        int right_axis,
                                        float dt) {
  m_move_speed = report.actual_speed;
  m_accepted_speed_smooth +=
      (report.actual_speed - m_accepted_speed_smooth) *
      (1.0F - std::exp(-k_accepted_speed_follow_rate * std::max(dt, 0.0F)));
  m_move_right_axis = right_axis;
  m_move_forward_axis = forward_axis;
  m_move_running = report.running;
}

void CommanderLocomotion::publish_fpv_motion(const CommanderHandles& body,
                                             float requested_speed) const {
  auto* cmd_comp = body.commander_data;
  if (cmd_comp == nullptr) {
    return;
  }
  cmd_comp->fpv_motion_vx = (body.movement != nullptr) ? body.movement->get_vx() : 0.0F;
  cmd_comp->fpv_motion_vz = (body.movement != nullptr) ? body.movement->get_vz() : 0.0F;

  cmd_comp->fpv_motion_requested =
      (requested_speed > 0.0F &&
       m_accepted_speed_smooth >
           Engine::Core::CommanderComponent::k_direct_control_gait_floor_speed *
               0.5F) ||
      m_planar_velocity.length() >
          Engine::Core::CommanderComponent::k_direct_control_gait_floor_speed;
}

auto CommanderLocomotion::engage_manual_motion(
    const CommanderHandles& body, CommanderLatencyProbe* probe) const -> float {
  auto* stamina = body.entity.get_component<Engine::Core::StaminaComponent>();
  if (body.movement != nullptr && m_move_speed > 0.05F) {
    body.movement->engage_manual_move(body.transform.position.x,
                                      body.transform.position.z);
    if (probe != nullptr) {
      probe->note_movement_response();
      probe->note_pose_response();
    }
    if (stamina != nullptr) {
      stamina->run_requested = m_move_running;
    }
  } else if (stamina != nullptr) {
    stamina->run_requested = false;
  }
  return stamina != nullptr ? stamina->stamina : -1.0F;
}

void CommanderLocomotion::hold_for_rally() {
  m_move_speed = 0.0F;
  m_planar_velocity = QVector3D(0.0F, 0.0F, 0.0F);
  m_move_right_axis = 0;
  m_move_forward_axis = 0;
  m_move_running = false;
}

void CommanderLocomotion::release_input() {
  m_move_right_axis = 0;
  m_move_forward_axis = 0;
  m_move_running = false;
  m_move_speed = 0.0F;
}

void CommanderLocomotion::reset() {
  m_move_speed = 0.0F;
  m_planar_velocity = QVector3D(0.0F, 0.0F, 0.0F);
  m_accepted_speed_smooth = 0.0F;
  m_move_right_axis = 0;
  m_move_forward_axis = 0;
  m_move_running = false;
  m_dodge_state = DodgeState::None;
  m_dodge_timer = 0.0F;
  m_dodge_direction = QVector3D(0.0F, 0.0F, 1.0F);
  m_dodge_fov_kick = 0.0F;
  m_jump_timer = 0.0F;
  m_jump_safe_position_valid = false;
  m_jump_followup_pending = false;
  m_jump_last_walkable_position = QVector3D(0.0F, 0.0F, 0.0F);
}

} // namespace App::Core
