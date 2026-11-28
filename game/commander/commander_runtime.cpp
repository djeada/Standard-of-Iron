#include "game/commander/commander_runtime.h"

#include <algorithm>
#include <cmath>
#include <utility>

#include "game/audio/cue_ids.h"
#include "game/commander/commander_entity_access.h"
#include "game/commander/commander_heading.h"
#include "game/commander/commander_tick_trace.h"
#include "game/core/component.h"
#include "game/core/world.h"
#include "game/systems/combat_actions/combat_action_definition.h"
#include "game/systems/combat_actions/melee_intent_solver.h"
#include "game/systems/navigation/walkability.h"
#include "game/systems/rpg_combat_system/rpg_bow_aim.h"
#include "game/systems/rpg_combat_system/rpg_commander_damage.h"
#include "game/util/checkpoint_values.h"

namespace Game::Commander {
namespace {
constexpr float k_turn_speed_degrees = 105.0F;
} // namespace

void CommanderRuntime::set_view_yaw(float yaw) {
  m_view_yaw = yaw;
}

void CommanderRuntime::set_view_pitch(float pitch) {
  m_view_pitch = std::clamp(pitch,
                            -k_commander_view_pitch_limit_degrees,
                            k_commander_view_pitch_limit_degrees);
}

void CommanderRuntime::set_view(float yaw, float pitch) {
  set_view_yaw(yaw);
  set_view_pitch(pitch);
}

auto CommanderRuntime::take_impact_kick() -> float {
  return std::exchange(m_impact_kick, 0.0F);
}

void CommanderRuntime::set_trace_enabled(bool enabled) {
  m_trace_enabled = enabled;
  if (!enabled) {
    m_trace = {};
  }
}

template <typename Runtime>
auto CommanderRuntime::checkpoint_fields(Runtime& runtime) {
  auto& move = runtime.m_locomotion;
  auto& facing = runtime.m_body_facing;
  auto& lunge = runtime.m_lunge;
  auto& targets = runtime.m_targeting;
  auto& guard = runtime.m_defence;
  auto& strike = runtime.m_strike;
  auto& input = runtime.m_input_port;
  return std::tie(runtime.m_view_yaw,
                  runtime.m_view_pitch,
                  runtime.m_previous_yaw,
                  runtime.m_previous_pitch,
                  runtime.m_camera_fov,
                  runtime.m_rally_view_yaw,
                  runtime.m_rally_look_idle,
                  runtime.m_rally_view_valid,
                  move.m_move_speed,
                  move.m_accepted_speed_smooth,
                  move.m_move_right_axis,
                  move.m_move_forward_axis,
                  move.m_move_running,
                  move.m_dodge_state,
                  move.m_dodge_timer,
                  move.m_dodge_fov_kick,
                  move.m_jump_timer,
                  move.m_jump_followup_pending,
                  facing.m_body_yaw,
                  facing.m_body_yaw_valid,
                  lunge.m_aim_offset,
                  lunge.m_aim_sequence,
                  targets.m_locked_target_id,
                  targets.m_locked_target_slot,
                  targets.m_soft_target_id,
                  targets.m_soft_target_slot,
                  targets.m_primary_target_slot,
                  targets.m_lock_lost_timer,
                  targets.m_lock_spring_yaw,
                  targets.m_lock_spring_yaw_valid,
                  targets.m_lock_manual_override_timer,
                  targets.m_hit_confirm_adopted,
                  targets.m_observed_hit_confirm_sequence,
                  guard.m_guard_was_active,
                  guard.m_contacts_adopted,
                  guard.m_observed_blocked_contacts,
                  guard.m_observed_perfect_guard_contacts,
                  guard.m_observed_dodged_contacts,
                  guard.m_observed_guard_broken_contacts,
                  strike.m_combo_miss_timer,
                  strike.m_primary_held_duration,
                  strike.m_held_restart_delay,
                  strike.m_hits_adopted,
                  strike.m_observed_action_hit_count,
                  input.m_carried_primary_press,
                  input.m_carried_jump_press,
                  input.m_carried_dodge_press,
                  input.m_carried_dodge_has_direction);
}

auto CommanderRuntime::capture_checkpoint() const -> QJsonObject {
  const auto vector = [](const QVector3D& value) {
    return Game::CheckpointValues::write(value.x(), value.y(), value.z());
  };
  return {{"state", Game::CheckpointValues::write_tuple(checkpoint_fields(*this))},
          {"velocity", vector(m_locomotion.m_planar_velocity)},
          {"dodge", vector(m_locomotion.m_dodge_direction)},
          {"carried_dodge", vector(m_input_port.m_carried_dodge_direction)}};
}

auto CommanderRuntime::restore_checkpoint(const QJsonObject& state) -> bool {
  const auto vector = [](const QJsonValue& value, QVector3D& out) {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
    if (!Game::CheckpointValues::read(value, x, y, z)) {
      return false;
    }
    out = QVector3D(x, y, z);
    return true;
  };
  return Game::CheckpointValues::read_tuple(state.value("state"),
                                            checkpoint_fields(*this)) &&
         vector(state.value("velocity"), m_locomotion.m_planar_velocity) &&
         vector(state.value("dodge"), m_locomotion.m_dodge_direction) &&
         vector(state.value("carried_dodge"), m_input_port.m_carried_dodge_direction);
}

void CommanderRuntime::release_all_input() {
  m_input_port.release_all();
  m_locomotion.release_input();
  m_strike.release_input();
  m_cancel_held_draw = true;
}

void CommanderRuntime::reset() {
  release_all_input();
  m_body_facing.reset();
  m_view_yaw = m_view_pitch = m_previous_yaw = m_previous_pitch = 0.0F;
  m_impact_kick = 0.0F;
  m_rally_view_valid = false;
  m_tick_aim_ray.reset();
  m_locomotion.reset();
  m_lunge.reset();
  m_targeting.reset();
  m_defence.reset();
  m_strike.reset();
  m_abilities.reset();
}

void CommanderRuntime::release_for_rally() {
  release_all_input();
  m_body_facing.reset();
  m_rally_view_valid = false;
  m_tick_aim_ray.reset();
  m_lunge.reset();
  m_targeting.reset();
  m_defence.reset();
  m_strike.reset();
  m_abilities.reset();
}

void CommanderRuntime::toggle_close_camera_mode(Engine::Core::World& world,
                                                Engine::Core::EntityID commander_id,
                                                int owner) const {
  auto* commander = controlled_commander(world, commander_id, owner);
  if (commander == nullptr) {
    return;
  }
  if (auto* cmd = commander->get_component<Engine::Core::CommanderComponent>()) {
    cmd->close_camera_mode = !cmd->close_camera_mode;
  }
}

void CommanderRuntime::toggle_weapon_stance(Engine::Core::World& world,
                                            Engine::Core::EntityID commander_id,
                                            int owner) {
  auto* commander = controlled_commander(world, commander_id, owner);
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
    publish_cue(owner, Game::Audio::Cue::k_combat_guard_raise);
  } else {
    publish_cue(owner, Game::Audio::Cue::k_combat_ability_refused);
  }
}

void CommanderRuntime::cycle_lock_on_target(Engine::Core::World& world,
                                            Engine::Core::EntityID commander_id,
                                            int owner) {
  if (!m_targeting.cycle_lock(world, commander_id, owner, m_view_yaw)) {
    m_abilities.refuse("commander.cycle_lock_on", owner);
  }
}

void CommanderRuntime::toggle_lock_on_target(Engine::Core::World& world,
                                             Engine::Core::EntityID commander_id,
                                             int owner) {
  if (m_targeting.toggle_lock(world, commander_id, owner, m_view_yaw) ==
      CommanderTargeting::LockToggle::NoTarget) {
    m_abilities.refuse("commander.cycle_lock_on", owner);
  }
}

void CommanderRuntime::switch_lock_on_target(Engine::Core::World& world,
                                             Engine::Core::EntityID commander_id,
                                             int owner,
                                             int direction) {
  if (!m_targeting.switch_lock(world, commander_id, owner, direction)) {
    m_abilities.refuse("commander.cycle_lock_on", owner);
  }
}

void CommanderRuntime::note_refusal(const char* action) {
  m_abilities.refuse(action);
}

auto CommanderRuntime::controlled_commander(Engine::Core::World& world,
                                            Engine::Core::EntityID commander_id,
                                            int owner) -> Engine::Core::Entity* {
  return Game::Commander::controlled_commander(world, commander_id, owner);
}

auto CommanderRuntime::find_primary_target(Engine::Core::World& world,
                                           Engine::Core::EntityID commander_id,
                                           int owner,
                                           float extra_reach)
    -> Engine::Core::EntityID {
  return m_targeting.find_primary_target(
      world, commander_id, owner, m_view_yaw, extra_reach);
}

auto CommanderRuntime::hold_for_rally(const CommanderHandles& body, float dt) -> bool {
  body.commander_data->fpv_motion_vx = 0.0F;
  body.commander_data->fpv_motion_vz = 0.0F;
  body.commander_data->fpv_motion_requested = false;

  m_input_port.release_for_rally();
  m_locomotion.hold_for_rally();
  follow_body_while_rallying(wrap_angle_degrees(body.transform.rotation.y), dt);

  if (body.movement != nullptr) {
    body.movement->set_manual_velocity(0.0F, 0.0F);
  }
  m_defence.hold_for_rally(body.entity, dt);

  return true;
}

void CommanderRuntime::follow_body_while_rallying(float body_yaw, float dt) {
  constexpr float k_idle_before_follow_seconds = 0.8F;
  constexpr float k_follow_rate = 2.5F;
  float const yaw = m_view_yaw;
  bool const looked = !m_rally_view_valid ||
                      std::abs(signed_angle_delta(yaw, m_rally_view_yaw)) > 0.01F;
  m_rally_look_idle = looked ? 0.0F : m_rally_look_idle + dt;
  if (m_rally_look_idle >= k_idle_before_follow_seconds) {
    set_view_yaw(wrap_angle_degrees(yaw + (signed_angle_delta(body_yaw, yaw) *
                                           (1.0F - std::exp(-k_follow_rate * dt)))));
  }
  m_rally_view_yaw = m_view_yaw;
  m_rally_view_valid = true;
}

void CommanderRuntime::steer_view(Engine::Core::World& world,
                                  Engine::Core::Entity& commander,
                                  float dt) {
  auto const& tick = m_input_port.tick();
  set_view_yaw(m_targeting.steer_view_toward_lock(world,
                                                  commander,
                                                  {.view_yaw = m_view_yaw,
                                                   .run = tick.run,
                                                   .backward = tick.backward,
                                                   .dodge_pressed = tick.dodge_pressed,
                                                   .dt = dt}));

  if (m_targeting.locked_id() == 0) {

    if (tick.turn_left) {
      set_view_yaw(m_view_yaw + k_turn_speed_degrees * dt);
    }
    if (tick.turn_right) {
      set_view_yaw(m_view_yaw - k_turn_speed_degrees * dt);
    }
  }
  set_view_yaw(wrap_angle_degrees(m_view_yaw));
}

auto CommanderRuntime::advance_jump_stage(const CommanderHandles& body,
                                          const TickFacts& facts,
                                          float dt) -> bool {
  auto const& tick = m_input_port.tick();
  bool const hold_press =
      tick.jump_pressed &&
      (facts.recovering_soon() ||
       m_locomotion.frees_within(k_commander_movement_buffer_seconds));
  auto const jump = m_locomotion.advance_jump(
      body,
      {.jump_pressed = tick.jump_pressed && !hold_press,
       .primary_held = tick.primary_held,
       .guard_held = tick.guard_held,
       .ability_requested = tick.shield_bash_pressed || tick.vanguard_rush_pressed ||
                            tick.second_wind_pressed,
       .staggered = facts.staggered(),
       .dt = dt});
  m_input_port.record_jump(jump.edge);
  if (hold_press) {
    m_input_port.carry_jump_edge();
  } else {
    m_input_port.clear_jump_edge();
  }
  return jump.active;
}

auto CommanderRuntime::start_dodge_stage(Engine::Core::World& world,
                                         const CommanderHandles& body,
                                         const TickFacts& facts,
                                         const MotionOutcome& motion) -> MoveBasis {
  auto const& tick = m_input_port.tick();
  auto const basis =
      CommanderLocomotion::move_basis(m_view_yaw,
                                      motion.forward_axis,
                                      motion.right_axis,
                                      body.transform,
                                      m_targeting.locked_position(world));
  bool const hold_press =
      tick.dodge_pressed &&
      (facts.recovering_soon() ||
       m_locomotion.frees_within(k_commander_movement_buffer_seconds));
  m_input_port.record_dodge(
      m_locomotion.try_start_dodge(body,
                                   {.pressed = tick.dodge_pressed && !hold_press,
                                    .staggered = facts.staggered(),
                                    .has_direction = tick.has_dodge_direction,
                                    .direction = tick.dodge_direction,
                                    .move = basis.move,
                                    .forward = basis.forward},
                                   m_latency_probe));
  if (hold_press) {
    m_input_port.carry_dodge_edge();
  } else {
    m_input_port.clear_dodge_edge();
  }
  return basis;
}

auto CommanderRuntime::advance_motion(Engine::Core::World& world,
                                      const CommanderHandles& body,
                                      const TickFacts& facts,
                                      float dt) -> MotionOutcome {
  auto const& tick = m_input_port.tick();
  MotionOutcome out;
  out.forward_axis = tick.forward_axis();
  out.right_axis = tick.right_axis();
  out.previous_position = QVector3D(
      body.transform.position.x, body.transform.position.y, body.transform.position.z);

  m_locomotion.decay_dodge_kick(dt);
  out.jump_active = advance_jump_stage(body, facts, dt);
  CommanderDefence::advance_upkeep(body.entity, tick.guard_held, dt);
  auto const basis = start_dodge_stage(world, body, facts, out);

  auto const footing =
      CommanderMotor::is_on_wall(body.entity)
          ? Game::Systems::Walkability::SlopeFooting{}
          : Game::Systems::Walkability::slope_footing(out.previous_position);
  out.report = m_locomotion.advance_motion(
      body,
      m_motor,
      {.move = facts.staggered() ? QVector3D() : basis.move,
       .forward_axis = out.forward_axis,
       .right_axis = out.right_axis,
       .run_held = tick.run,
       .drawing_bow = facts.drawing_bow,
       .jump_active = out.jump_active,
       .squared_up = squared_up(facts),
       .wading = world.try_get<Engine::Core::WadingComponent>(body.entity.get_id()),
       .slope_grade = footing.on_slope ? footing.grade : 0.0F,
       .slope_downhill = footing.on_slope ? footing.downhill : QVector3D(),
       .holds_footing = facts.attack_animation_active || tick.guard_held,
       .dt = dt});

  out.report.lunge_distance =
      m_lunge.apply(world,
                    body.entity,
                    body.transform,
                    m_motor,
                    m_locomotion.dodge_state() != DodgeState::None,
                    body.commander_data != nullptr && body.commander_data->jump_active,
                    m_view_yaw,
                    dt);
  if (out.report.lunge_distance > 1.0e-5F) {
    out.report.source = CommanderDisplacementSource::StrikeLunge;
  }
  m_locomotion.commit_motion(out.report, out.forward_axis, out.right_axis, dt);
  return out;
}

void CommanderRuntime::face_body(const CommanderHandles& body,
                                 const TickFacts& facts,
                                 const MotionOutcome& motion,
                                 float dt) {
  auto const& tick = m_input_port.tick();
  float action_redirect_authority = 1.0F;
  if (facts.attack_animation_active) {
    if (auto const* definition =
            Game::Systems::CombatActions::find_combat_action_definition(
                static_cast<Game::Systems::CombatActions::CombatActionId>(
                    facts.active_action->combat_action_id))) {
      action_redirect_authority =
          Game::Systems::CombatActions::melee_interruption_at(
              *definition, facts.active_action->normalized_action_time)
              .redirect_authority;
    }
  }

  bool const dodging = m_locomotion.dodge_state() != DodgeState::None;
  std::optional<float> travel_yaw;
  if (!dodging && !motion.jump_active && motion.report.move.lengthSquared() > 1.0e-4F) {
    travel_yaw = std::atan2(motion.report.move.x(), motion.report.move.z()) *
                 k_radians_to_degrees;
  }

  float const body_yaw =
      m_body_facing.advance({.view_yaw = m_view_yaw,
                             .attack_animation_active = facts.attack_animation_active,
                             .action_redirect_authority = action_redirect_authority,
                             .strike_aim_offset = m_lunge.aim_offset(),
                             .must_face_view = !dodging && squared_up(facts),
                             .snap_to_view = !dodging && tick.primary_held,
                             .travel_yaw = travel_yaw,
                             .dt = dt});
  body.transform.rotation.y = body_yaw;
  body.transform.desired_yaw = body_yaw;
  body.transform.has_desired_yaw = true;
}

auto CommanderRuntime::sync_direct_control(
    const CommanderHandles& body, const MotionOutcome& motion) -> DirectControlSync {
  DirectControlSync sync;
  sync.aim = Game::Systems::RpgCombat::sync_commander_aim(
      body.entity,
      {.view_yaw_degrees = m_view_yaw,
       .view_pitch_degrees = m_view_pitch,
       .move_speed = m_locomotion.move_speed(),
       .running = m_locomotion.move_running(),
       .primary_held = m_input_port.tick().primary_held,
       .camera_origin =
           m_tick_aim_ray.has_value() ? m_tick_aim_ray->first : QVector3D(),
       .camera_origin_valid = m_tick_aim_ray.has_value(),
       .camera_forward = m_tick_aim_ray.has_value() ? m_tick_aim_ray->second
                                                    : QVector3D(0.0F, 0.0F, 1.0F),
       .camera_forward_valid = m_tick_aim_ray.has_value(),
       .camera_fov_degrees = m_camera_fov});
  m_locomotion.publish_fpv_motion(body, motion.report.requested_speed);
  sync.traced_stamina = m_locomotion.engage_manual_motion(body, m_latency_probe);
  return sync;
}

void CommanderRuntime::activate_abilities(Engine::Core::World& world,
                                          const CommanderHandles& body,
                                          Engine::Core::EntityID commander_id,
                                          int owner) {
  auto const& tick = m_input_port.tick();
  CommanderAbilityRequest request;
  request.shield_bash = tick.shield_bash_pressed;
  request.vanguard_rush = tick.vanguard_rush_pressed;
  request.second_wind = tick.second_wind_pressed;
  m_input_port.clear_ability_edges();

  if (!request.any()) {
    return;
  }
  CommanderAbilityContext context;
  context.world = &world;
  context.commander = &body.entity;
  context.commander_id = commander_id;
  context.local_owner_id = owner;
  context.view_yaw = m_view_yaw;
  context.dodging = m_locomotion.dodge_state() != DodgeState::None;
  context.airborne = m_locomotion.jump_airborne();
  context.locked_target_id = m_targeting.locked_id();
  context.soft_target_id = m_targeting.soft_id();
  context.motor = &m_motor;
  static_cast<void>(m_abilities.activate(request, context));
}

void CommanderRuntime::queue_pressed_intents(
    const CommanderHandles& body,
    Engine::Core::CombatIntentQueueComponent& intents,
    const Engine::Core::RpgCommanderAimComponent* aim) {
  auto const& tick = m_input_port.tick();
  auto const* guard =
      body.entity.get_component<Engine::Core::CommanderGuardComponent>();
  bool const primary_was_pressed = tick.primary_pressed;
  bool const queued = m_strike.queue_intents(
      body.entity,
      intents,
      tick,
      {.dodge_clear = m_locomotion.dodge_state() == DodgeState::None,
       .guarding = guard != nullptr && guard->active,
       .bow_stance =
           aim != nullptr && aim->stance == Engine::Core::FpvWeaponStance::Bow,
       .jump_followup_pending = m_locomotion.jump_followup_pending()});
  if (queued) {
    m_input_port.consume_press_edges(primary_was_pressed);
    m_locomotion.clear_jump_followup();
  }
}

auto CommanderRuntime::dispatch_front_intent(
    Engine::Core::World& world,
    const CommanderHandles& body,
    Engine::Core::CombatIntentQueueComponent& intents,
    Engine::Core::EntityID commander_id,
    int owner) -> bool {
  return m_strike.dispatch_pending(
      {.world = world,
       .commander_id = commander_id,
       .local_owner_id = owner,
       .view_yaw = m_view_yaw,
       .move_right_axis = m_locomotion.move_right_axis(),
       .move_forward_axis = m_locomotion.move_forward_axis(),
       .commander_position = QVector3D(body.transform.position.x,
                                       body.transform.position.y,
                                       body.transform.position.z)},
      m_targeting,
      body.entity,
      intents,
      {.feedback = m_feedback, .probe = m_latency_probe});
}

void CommanderRuntime::advance_swing(const CommanderHandles& body, float dt) {
  auto const& tick = m_input_port.tick();
  m_strike.advance_melee(
      body.entity,
      {.aim_delta_x = rightward_look_delta(m_view_yaw, m_previous_yaw),
       .aim_delta_y = m_view_pitch - m_previous_pitch,
       .view_pitch = m_view_pitch,
       .move_right_axis = m_locomotion.move_right_axis(),
       .move_forward_axis = m_locomotion.move_forward_axis(),
       .primary_held = tick.primary_held,
       .guard_held = tick.guard_held,
       .dt = dt});
  m_previous_yaw = m_view_yaw;
  m_previous_pitch = m_view_pitch;

  if (auto const kick = m_strike.observe_hits(body.entity); kick.has_value()) {
    m_impact_kick += *kick;
  }
}

auto CommanderRuntime::advance_strike(Engine::Core::World& world,
                                      const CommanderHandles& body,
                                      const TickFacts& facts,
                                      Engine::Core::RpgCommanderAimComponent* aim,
                                      Engine::Core::EntityID commander_id,
                                      int owner,
                                      float dt) -> std::optional<QueueOutcome> {
  m_strike.advance_hold_timers(body.commander_data,
                               m_input_port.tick().primary_held,
                               facts.attack_animation_active,
                               dt);

  QueueOutcome queue;
  queue.intents = CommanderStrike::open_queue(body.entity, dt);
  queue.dodge_grace_remaining = CommanderDefence::advance_dodge_grace(body.entity, dt);

  bool const waiting_for_release = aim != nullptr && aim->relaxed_from_overhold;
  bool const body_can_take_input =
      m_locomotion.dodge_state() != DodgeState::Rolling && !waiting_for_release;

  if (queue.intents != nullptr && body_can_take_input) {
    queue_pressed_intents(body, *queue.intents, aim);
  }
  m_input_port.settle_primary_press();

  if (queue.intents != nullptr && !queue.intents->empty() && body_can_take_input &&
      !dispatch_front_intent(world, body, *queue.intents, commander_id, owner)) {
    return std::nullopt;
  }

  advance_swing(body, dt);
  return queue;
}

void CommanderRuntime::advance_targeting(Engine::Core::World& world,
                                         const CommanderHandles& body,
                                         Engine::Core::RpgCommanderAimComponent* aim,
                                         Engine::Core::EntityID commander_id,
                                         int owner,
                                         float dt) {
  auto const aim_candidate_id = m_targeting.resolve_aim_candidate(
      world, body.entity, commander_id, owner, aim, m_view_yaw);

  m_defence.publish_resolved_feedback(
      body.entity, commander_id, body.transform, m_feedback);

  if (auto const kick = m_targeting.publish_targets(body.entity, aim_candidate_id, dt);
      kick.has_value()) {
    m_impact_kick += *kick;
  }
}

void CommanderRuntime::record_trace(const CommanderHandles& body,
                                    const TickFacts& facts,
                                    const MotionOutcome& motion,
                                    const QueueOutcome& queue,
                                    float traced_stamina,
                                    float dt) {
  auto const& tick = m_input_port.tick();
  record_tick_trace(
      m_trace,
      body.entity,
      dt,
      {.edges = m_input_port.edges(),
       .frame_index = m_input_port.tick().sequence,
       .forward_axis = motion.forward_axis,
       .right_axis = motion.right_axis,
       .run_held = tick.run,
       .primary_held = tick.primary_held,
       .guard_held = tick.guard_held,
       .primary_held_duration = m_strike.primary_held_duration(),
       .view_yaw = m_view_yaw,
       .view_pitch = m_view_pitch,
       .previous_view_yaw = m_previous_yaw,
       .previous_view_pitch = m_previous_pitch},
      {.transform = body.transform,
       .direct_control =
           body.commander_data != nullptr && body.commander_data->fpv_controlled,
       .report = motion.report,
       .previous_position = motion.previous_position,
       .smoothed_speed = m_locomotion.planar_speed(),
       .grounded = !motion.jump_active,
       .dt = dt,
       .pose = {},
       .camera = {}},
      {.action = facts.active_action,
       .intents = queue.intents,
       .dodge_state = m_locomotion.dodge_state(),
       .dodge_timer = m_locomotion.dodge_timer(),
       .dodge_grace_remaining = queue.dodge_grace_remaining,
       .locked_target_id = m_targeting.locked_id(),
       .locked_target_slot = m_targeting.locked_slot(),
       .soft_target_id = m_targeting.soft_id(),
       .soft_target_slot = m_targeting.soft_slot(),
       .hit_confirm_sequence = m_targeting.observed_hit_confirm_sequence(),
       .stamina = traced_stamina,
       .health = body.unit.health});
}

auto CommanderRuntime::resolve_handles(Engine::Core::Entity& commander,
                                       Engine::Core::TransformComponent& transform,
                                       Engine::Core::UnitComponent& unit)
    -> CommanderHandles {
  auto* movement = commander.get_component<Engine::Core::MovementComponent>();
  if (movement == nullptr) {
    movement = commander.add_component<Engine::Core::MovementComponent>();
  }
  return {commander,
          transform,
          unit,
          commander.get_component<Engine::Core::CommanderComponent>(),
          movement};
}

auto CommanderRuntime::squared_up(const TickFacts& facts) const -> bool {
  auto const& tick = m_input_port.tick();
  return tick.primary_held || tick.guard_held || facts.drawing_bow ||
         facts.attack_animation_active || m_targeting.locked_id() != 0;
}

auto CommanderRuntime::read_tick_facts(const Engine::Core::World& world,
                                       const Engine::Core::Entity& commander)
    -> TickFacts {
  auto const* aim_state =
      commander.get_component<Engine::Core::RpgCommanderAimComponent>();
  auto const* active_action =
      commander.get_component<Engine::Core::RpgCommanderActionComponent>();
  auto const* stagger =
      world.try_get<Engine::Core::StaggerComponent>(commander.get_id());
  return {.active_action = active_action,
          .attack_animation_active =
              active_action != nullptr && active_action->action_running,
          .drawing_bow = aim_state != nullptr && aim_state->is_drawing(),
          .stagger_remaining =
              stagger != nullptr ? std::max(0.0F, stagger->remaining) : 0.0F};
}

auto CommanderRuntime::advance_combat(Engine::Core::World& world,
                                      const CommanderHandles& body,
                                      const TickFacts& facts,
                                      const MotionOutcome& motion,
                                      Engine::Core::EntityID commander_id,
                                      int owner,
                                      float dt) -> bool {
  auto const sync = sync_direct_control(body, motion);
  m_defence.apply_guard(body.entity,
                        {.guard_held = m_input_port.tick().guard_held,
                         .dodging = m_locomotion.dodge_state() != DodgeState::None,
                         .airborne = motion.jump_active,
                         .staggered = facts.staggered(),
                         .dt = dt},
                        m_latency_probe);
  activate_abilities(world, body, commander_id, owner);

  auto const queue =
      advance_strike(world, body, facts, sync.aim, commander_id, owner, dt);
  if (!queue.has_value()) {
    return false;
  }
  advance_targeting(world, body, sync.aim, commander_id, owner, dt);
  if (m_latency_probe != nullptr) {
    m_latency_probe->note_simulation_response();
  }
  if (m_trace_enabled) {
    record_trace(body, facts, motion, *queue, sync.traced_stamina, dt);
  }
  return true;
}

auto CommanderRuntime::advance(Engine::Core::World& world,
                               Engine::Core::EntityID commander_id,
                               int owner,
                               const Game::Command::CommanderInputFrame& frame,
                               float dt) -> bool {
  auto* commander = controlled_commander(world, commander_id, owner);
  if (commander == nullptr) {
    return false;
  }
  auto* transform = commander->get_component<Engine::Core::TransformComponent>();
  auto* unit = commander->get_component<Engine::Core::UnitComponent>();
  if (transform == nullptr || unit == nullptr) {
    return false;
  }

  if (std::exchange(m_cancel_held_draw, false)) {
    cancel_held_bow_draw(*commander);
  }
  set_view(frame.view_yaw, frame.view_pitch);
  m_camera_fov = frame.camera_fov;
  m_input_port.apply_snapshot(CommanderInputSnapshot::from_record(frame));
  m_tick_aim_ray.reset();
  if (frame.held(Game::Command::CommanderInputFrame::HasAimRay)) {
    m_tick_aim_ray = std::make_pair(frame.aim_origin, frame.aim_forward);
  }
  m_impact_kick = 0.0F;
  auto const body = resolve_handles(*commander, *transform, *unit);
  auto const facts = read_tick_facts(world, *commander);

  if (body.commander_data != nullptr && body.commander_data->flag_rally_in_progress &&
      !body.commander_data->fpv_controlled) {
    return hold_for_rally(body, dt);
  }
  m_rally_view_valid = false;

  if (body.movement != nullptr) {
    body.movement->stop();
    body.movement->set_rest_position(transform->position.x, transform->position.z);
  }

  steer_view(world, *commander, dt);
  m_motor.bind_world(&world);
  auto const motion = advance_motion(world, body, facts, dt);
  face_body(body, facts, motion, dt);
  return advance_combat(world, body, facts, motion, commander_id, owner, dt);
}

} // namespace Game::Commander
