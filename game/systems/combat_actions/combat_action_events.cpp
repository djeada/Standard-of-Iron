#include "combat_action_events.h"

#include <algorithm>

namespace Game::Systems::CombatActions {

namespace {

void apply_event_side_effect(Engine::Core::RpgCommanderActionComponent& action,
                             CombatActionEventType event_type) {
  action.last_event_type = static_cast<std::uint8_t>(event_type);
  action.last_event_valid = true;

  switch (event_type) {
  case CombatActionEventType::ActiveStart:
    action.action_active = true;
    break;
  case CombatActionEventType::WeaponTraceStart:
    action.action_active = true;
    action.weapon_trace_active = true;
    break;
  case CombatActionEventType::WeaponTraceEnd:
    action.action_active = false;
    action.weapon_trace_active = false;
    break;
  case CombatActionEventType::RecoveryStart:
    action.action_active = false;
    action.weapon_trace_active = false;
    action.cancel_window_active = true;
    break;
  case CombatActionEventType::CancelWindowStart:
    action.cancel_window_active = true;
    break;
  case CombatActionEventType::CancelWindowEnd:
  case CombatActionEventType::ExitSafe:
    action.action_active = false;
    action.weapon_trace_active = false;
    action.cancel_window_active = false;
    break;
  default:
    break;
  }
}

} // namespace

auto crossed_events_between(const CombatActionDefinition& definition,
                            float previous_normalized_time,
                            float current_normalized_time,
                            std::uint8_t first_event_index)
    -> std::vector<CombatActionEvent> {
  std::vector<CombatActionEvent> crossed;
  if (definition.events.empty()) {
    return crossed;
  }

  previous_normalized_time = std::clamp(previous_normalized_time, 0.0F, 1.0F);
  current_normalized_time = std::clamp(current_normalized_time, 0.0F, 1.0F);
  if (current_normalized_time < previous_normalized_time) {
    previous_normalized_time = 0.0F;
  }

  for (std::size_t i = first_event_index; i < definition.events.size(); ++i) {
    auto const& event = definition.events[i];
    if (event.normalized_time > current_normalized_time) {
      break;
    }
    if (event.normalized_time >= previous_normalized_time) {
      crossed.push_back(event);
    }
  }
  return crossed;
}

auto advance_combat_action_events(Engine::Core::RpgCommanderActionComponent& action,
                                  float delta_time,
                                  const CombatActionDefinition& definition)
    -> std::vector<CombatActionEvent> {
  if (!action.action_running && !action.action_completed &&
      action.action_duration <= 0.0F) {
    action.action_duration = definition.duration_seconds;
    action.action_running = true;
  }
  if (!action.action_running || action.action_completed) {
    return {};
  }
  action.action_duration = action.action_duration > 0.0F ? action.action_duration
                                                         : definition.duration_seconds;
  action.action_elapsed_time = std::min(
      action.action_duration, action.action_elapsed_time + std::max(0.0F, delta_time));
  float const current =
      action.action_duration > 0.0F
          ? std::clamp(action.action_elapsed_time / action.action_duration, 0.0F, 1.0F)
          : 1.0F;
  float previous = action.normalized_action_time;
  if (current < action.normalized_action_time) {
    action.next_event_index = 0U;
    action.action_active = false;
    action.weapon_trace_active = false;
    previous = 0.0F;
    action.normalized_action_time = 0.0F;
  }

  auto crossed =
      crossed_events_between(definition, previous, current, action.next_event_index);
  for (auto const& event : crossed) {
    apply_event_side_effect(action, event.type);
    while (action.next_event_index < definition.events.size() &&
           definition.events[action.next_event_index].normalized_time <=
               event.normalized_time) {
      ++action.next_event_index;
    }
  }

  action.previous_normalized_action_time = previous;
  action.normalized_action_time = current;
  if (current >= 1.0F) {
    action.action_running = false;
    action.action_completed = true;
    action.action_active = false;
    action.weapon_trace_active = false;
    action.phase = Engine::Core::RpgCommanderActionPhase::None;
    action.active_target_id = 0;
  }
  return crossed;
}

void reset_combat_action_event_runtime(
    Engine::Core::RpgCommanderActionComponent& action) {
  action.normalized_action_time = 0.0F;
  action.previous_normalized_action_time = 0.0F;
  action.action_elapsed_time = 0.0F;
  action.next_event_index = 0U;
  action.last_event_type = 0U;
  action.last_event_valid = false;
  action.action_active = false;
  action.weapon_trace_active = false;
  action.action_running = true;
  action.action_completed = false;
  action.cancel_window_active = false;
  action.input_buffered = false;
  action.hit_target_ids.fill(0U);
  action.hit_target_count = 0U;
}

} // namespace Game::Systems::CombatActions
