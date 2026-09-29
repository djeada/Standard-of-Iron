#include "app/commander/commander_tick_trace.h"

#include <cmath>
#include <limits>

#include "app/commander/commander_motor.h"
#include "game/core/simulation_timing.h"
#include "game/systems/combat_actions/combat_action_definition.h"

namespace App::Core {

namespace {

auto slot_or_minus_one(std::uint16_t slot) -> int {
  return slot == std::numeric_limits<std::uint16_t>::max() ? -1
                                                           : static_cast<int>(slot);
}

void record_input(CommanderInputTrace& out, const TickTraceInputs& in) {
  out = in.edges;
  out.frame_index = in.frame_index;
  out.move_forward_axis = in.forward_axis;
  out.move_right_axis = in.right_axis;
  out.run_held = in.run_held;
  out.primary_held = in.primary_held;
  out.guard_held = in.guard_held;
  out.primary_held_duration = in.primary_held_duration;
  out.look_delta_yaw = in.view_yaw - in.previous_view_yaw;
  out.look_delta_pitch = in.view_pitch - in.previous_view_pitch;
  out.view_yaw = in.view_yaw;
  out.view_pitch = in.view_pitch;
}

void record_steering_facts(CommanderMotorTrace& out,
                           const Engine::Core::Entity& commander,
                           bool direct_control) {
  auto const* facts = commander.get_component<Engine::Core::MovementFactsComponent>();
  QVector3D const contact_push = facts != nullptr
                                     ? QVector3D(facts->steering.contact_push_x,
                                                 0.0F,
                                                 facts->steering.contact_push_z)
                                     : QVector3D();
  out.separation_push = contact_push.length();
  out.dynamic_push = contact_push;
  out.dynamic_neighbors = facts != nullptr ? facts->steering.neighbor_count : 0U;
  out.dynamic_overlap = facts != nullptr ? facts->steering.body_overlap : 0.0F;
  out.movement_mode = direct_control ? CommanderMovementMode::DirectControl
                                     : CommanderMovementMode::Rts;
  out.steering_source =
      facts != nullptr ? Engine::Core::desired_motion_source_name(facts->desired.source)
                       : "None";
}

void record_motor(CommanderMotorTrace& out,
                  const Engine::Core::Entity& commander,
                  const TickTraceMotor& in) {
  auto const& transform = in.transform;
  auto const& report = in.report;
  QVector3D const position(
      transform.position.x, transform.position.y, transform.position.z);
  out.previous_position = in.previous_position;
  out.position = position;
  out.desired_velocity =
      (report.move.lengthSquared() > 0.0001F ? report.move.normalized() : QVector3D()) *
      report.requested_speed;
  out.actual_velocity =
      in.dt > 0.0F ? (position - in.previous_position) / in.dt : QVector3D();
  out.requested_speed = report.requested_speed;
  out.smoothed_speed = in.smoothed_speed;
  out.speed_error = out.actual_velocity.length() - report.requested_speed;
  out.grounded = in.grounded;
  out.blocked = report.blocked;
  out.slid = report.slid;
  record_steering_facts(out, commander, in.direct_control);
  out.static_walkable = CommanderMotor::is_walkable_at(
      commander, transform.position.x, transform.position.z);
  out.accepted_displacement = std::hypot(position.x() - in.previous_position.x(),
                                         position.z() - in.previous_position.z());
  out.lunge_distance = report.lunge_distance;
  out.snap_back_distance = report.snap_back_distance;
  out.displacement_source = report.source;
  out.dt = in.dt;
  out.presented_position =
      QVector3D(in.pose.position.x, in.pose.position.y, in.pose.position.z);
  out.presented_yaw = in.pose.yaw;
  out.presentation_alpha = in.pose.alpha;
  out.presentation_extrapolated = in.pose.extrapolated;
}

void record_costs(CommanderCostTrace& out) {
  auto const costs = Engine::Core::Timing::sample_and_reset_rpg_costs();
  constexpr float k_microseconds_to_ms = 0.001F;
  out.motor_ms = static_cast<float>(costs.motor_us) * k_microseconds_to_ms;
  out.targeting_ms = static_cast<float>(costs.targeting_us) * k_microseconds_to_ms;
  out.weapon_trace_ms =
      static_cast<float>(costs.weapon_trace_us) * k_microseconds_to_ms;
  out.engagement_ms = static_cast<float>(costs.engagement_us) * k_microseconds_to_ms;
  out.camera_ms = static_cast<float>(costs.camera_us) * k_microseconds_to_ms;
}

void record_action(CommanderCombatTrace& out,
                   const Engine::Core::RpgCommanderActionComponent& action) {
  out.action_id = action.combat_action_id;
  out.action_phase = static_cast<int>(action.phase);
  out.action_normalized_time = action.normalized_action_time;
  out.action_running = action.action_running;
  out.action_hit_count = action.hit_target_count;
  if (!action.action_running) {
    return;
  }
  auto const* definition = Game::Systems::CombatActions::find_combat_action_definition(
      static_cast<Game::Systems::CombatActions::CombatActionId>(
          action.combat_action_id));
  if (definition == nullptr) {
    return;
  }
  using Game::Systems::CombatActions::CombatActionEventType;
  out.action_window_start = Game::Systems::CombatActions::action_event_normalized_time(
      *definition, CombatActionEventType::WeaponTraceStart, 0.0F);
  out.action_window_end = Game::Systems::CombatActions::action_event_normalized_time(
      *definition, CombatActionEventType::WeaponTraceEnd, 1.0F);
}

void record_queue(CommanderCombatTrace& out,
                  const Engine::Core::CombatIntentQueueComponent& intents) {
  out.queued_intents = static_cast<int>(intents.count);
  out.queue_outcome = static_cast<int>(intents.last_outcome);
  out.queue_outcome_age = intents.last_outcome_age;
  out.queue_accepted = intents.accepted_intents;
  out.queue_buffered = intents.buffered_intents;
  out.queue_refused = intents.refused_intents;
  out.queue_expired = intents.expired_intents;
  out.queue_overflow = intents.overflow_intents;
}

void record_combat(CommanderCombatTrace& out,
                   const Engine::Core::Entity& commander,
                   const TickTraceCombat& in) {
  out = CommanderCombatTrace{};
  if (in.action != nullptr) {
    record_action(out, *in.action);
  }
  if (in.intents != nullptr) {
    record_queue(out, *in.intents);
  }
  if (auto const* defense =
          commander.get_component<Engine::Core::RpgHealthComponent>()) {
    out.blocked_contacts = defense->blocked_contacts;
    out.perfect_guard_contacts = defense->perfect_guard_contacts;
    out.dodged_contacts = defense->dodged_contacts;
    out.damaging_contacts = defense->damaging_contacts;
    out.guard_broken_contacts = defense->guard_broken_contacts;
  }
  if (auto const* guard =
          commander.get_component<Engine::Core::CommanderGuardComponent>()) {
    out.guard_active = guard->active;
    out.perfect_guard_remaining = guard->perfect_guard_remaining;
  }
  out.dodge_state = static_cast<int>(in.dodge_state);
  out.dodge_timer = in.dodge_timer;
  out.dodge_grace_remaining = in.dodge_grace_remaining;
  out.health = in.health;
  out.locked_target_id = in.locked_target_id;
  out.locked_target_slot = slot_or_minus_one(in.locked_target_slot);
  out.soft_target_id = in.soft_target_id;
  out.soft_target_slot = slot_or_minus_one(in.soft_target_slot);
  out.hit_confirm_sequence = in.hit_confirm_sequence;
  out.stamina = in.stamina;
}

} // namespace

void record_tick_trace(CommanderPresentationTrace& trace,
                       const Engine::Core::Entity& commander,
                       float dt,
                       const TickTraceInputs& input,
                       const TickTraceMotor& motor,
                       const TickTraceCombat& combat) {
  ++trace.sequence;
  trace.valid = true;
  trace.time_seconds += dt;

  record_input(trace.input, input);
  record_motor(trace.motor, commander, motor);
  trace.camera = motor.camera;
  record_costs(trace.costs);
  record_combat(trace.combat, commander, combat);
}

} // namespace App::Core
