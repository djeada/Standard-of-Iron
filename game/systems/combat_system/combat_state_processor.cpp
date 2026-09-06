#include "combat_state_processor.h"

#include <algorithm>
#include <cmath>
#include <span>
#include <vector>

#include "../../core/component_gameplay.h"
#include "../../core/simulation_timing.h"
#include "../../core/world.h"
#include "../combat_actions/combat_action_definition.h"
#include "../combat_actions/combat_action_events.h"
#include "combat_action_processor.h"
#include "mounted_charge_processor.h"
#include "spear_brace_processor.h"

namespace Game::Systems::Combat {

namespace {

using CS = Engine::Core::CombatAnimationState;
using CSC = Engine::Core::CombatStateComponent;

auto base_phase_duration(CS state) noexcept -> float {
  switch (state) {
  case CS::Advance:
    return CSC::k_advance_duration;
  case CS::WindUp:
    return CSC::k_wind_up_duration;
  case CS::Strike:
    return CSC::k_strike_duration;
  case CS::Impact:
    return CSC::k_impact_duration;
  case CS::Recover:
    return CSC::k_recover_duration;
  case CS::Reposition:
    return CSC::k_reposition_duration;
  case CS::Idle:
  default:
    return 0.0F;
  }
}

auto running_authored_action(const Engine::Core::Entity& unit) noexcept
    -> const Game::Systems::CombatActions::CombatActionDefinition* {
  auto const* action = unit.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action == nullptr || action->combat_action_id == 0U) {
    return nullptr;
  }
  return Game::Systems::CombatActions::find_combat_action_definition(
      static_cast<Game::Systems::CombatActions::CombatActionId>(
          action->combat_action_id));
}

auto commander_phase_scale(const Engine::Core::Entity& unit,
                           const Engine::Core::CombatStateComponent& combat_state,
                           CS state) noexcept -> float {
  auto const* commander = unit.get_component<Engine::Core::CommanderComponent>();
  if (commander == nullptr || !commander->fpv_controlled) {
    return 1.0F;
  }

  auto const& intent = combat_state.intent;
  float const commitment =
      1.0F + (0.20F * intent.charge) + (0.15F * (intent.follow_through - 0.50F));
  float const thrust_relief = 1.0F - (0.22F * intent.thrust_amount);

  float const arc_scale = std::clamp(
      1.0F + (0.45F * (std::abs(intent.strike_dir_y) - 0.60F)), 0.80F, 1.35F);
  float const direction_scale =
      commitment * thrust_relief * arc_scale / intent.swing_speed;

  switch (state) {
  case CS::Advance:
    return (combat_state.finisher_attack ? 1.90F : 1.50F) * direction_scale;
  case CS::WindUp:
    return (combat_state.finisher_attack ? 2.10F : 1.65F) * direction_scale;
  case CS::Strike:
    return (combat_state.finisher_attack ? 1.40F : 1.15F) * direction_scale;
  case CS::Impact:
    return (combat_state.finisher_attack ? 1.50F : 1.25F) * direction_scale;
  case CS::Recover:
    return (combat_state.finisher_attack ? 1.35F : 1.15F) * direction_scale;
  case CS::Reposition:
    return (combat_state.finisher_attack ? 1.20F : 1.0F) * direction_scale;
  case CS::Idle:
  default:
    return 1.0F;
  }
}

auto phase_duration_for_state(const Engine::Core::Entity& unit,
                              const Engine::Core::CombatStateComponent& combat_state,
                              CS state) noexcept -> float {
  auto const* commander = unit.get_component<Engine::Core::CommanderComponent>();
  if (commander != nullptr && commander->fpv_controlled) {

    if (auto const* definition = running_authored_action(unit); definition != nullptr) {

      return Game::Systems::CombatActions::authored_phase_duration(*definition, state) /
             std::clamp(combat_state.intent.swing_speed, 0.55F, 1.85F);
    }
    return base_phase_duration(state) *
           commander_phase_scale(unit, combat_state, state);
  }

  return base_phase_duration(state);
}

auto bow_draw_is_held(const Engine::Core::Entity& unit) noexcept -> bool {
  auto const* aim = unit.get_component<Engine::Core::RpgCommanderAimComponent>();
  auto const* action = unit.get_component<Engine::Core::RpgCommanderActionComponent>();
  return aim != nullptr && action != nullptr && action->action_running &&
         aim->draw_stage == Engine::Core::BowDrawStage::FullDraw &&
         static_cast<Game::Systems::CombatActions::CombatActionId>(
             action->combat_action_id) ==
             Game::Systems::CombatActions::CombatActionId::RpgBowShot;
}

auto apply_action_timeline_phase(
    Engine::Core::CombatStateComponent& combat_state,
    const Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) {
  using Game::Systems::CombatActions::CombatActionEventType;
  auto const at = [&](CombatActionEventType type, float fallback) {
    return Game::Systems::CombatActions::action_event_normalized_time(
        definition, type, fallback);
  };
  float const windup = at(CombatActionEventType::WindupStart, 0.08F);
  float const active = at(CombatActionEventType::ActiveStart, 0.35F);
  float const strike = at(CombatActionEventType::WeaponTraceStart, active);
  float const strike_end = at(CombatActionEventType::WeaponTraceEnd, strike);
  float const recovery = at(CombatActionEventType::RecoveryStart, 0.75F);
  float const exit_safe = at(CombatActionEventType::ExitSafe, 0.92F);

  if (strike_end <= strike) {

    return false;
  }

  float const t = std::clamp(action.normalized_action_time, 0.0F, 1.0F);
  CS state = CS::Reposition;
  float span_start = exit_safe;
  float span_end = 1.0F;
  if (t < windup) {
    state = CS::Advance;
    span_start = 0.0F;
    span_end = windup;
  } else if (t < strike) {
    state = CS::WindUp;
    span_start = windup;
    span_end = strike;
  } else if (t < strike_end) {
    state = CS::Strike;
    span_start = strike;
    span_end = strike_end;
  } else if (t < recovery) {
    state = CS::Impact;
    span_start = strike_end;
    span_end = recovery;
  } else if (t < exit_safe) {
    state = CS::Recover;
    span_start = recovery;
    span_end = exit_safe;
  }

  float const clock = std::max(0.001F, action.action_duration);
  combat_state.animation_state = state;
  combat_state.state_duration = std::max(0.001F, span_end - span_start) * clock;
  combat_state.state_time = std::max(0.0F, t - span_start) * clock;

  float const weight =
      definition.damage.unblockable
          ? 1.0F
          : std::clamp(0.45F + (0.55F * (definition.damage.base_multiplier - 1.0F)),
                       0.25F,
                       1.0F);
  auto const ramp = [](float value) {
    float const clamped = std::clamp(value, 0.0F, 1.0F);
    return clamped * clamped * (3.0F - (2.0F * clamped));
  };
  if (t < windup) {
    combat_state.telegraph_cue = Engine::Core::TelegraphCue::None;
    combat_state.telegraph_intensity = 0.0F;
  } else if (t < strike) {
    combat_state.telegraph_cue = Engine::Core::TelegraphCue::Warning;
    combat_state.telegraph_intensity =
        weight * ramp((t - windup) / std::max(0.001F, strike - windup));
  } else if (t < strike_end) {
    combat_state.telegraph_cue = Engine::Core::TelegraphCue::Flash;
    combat_state.telegraph_intensity = weight;
  } else if (t < recovery) {
    combat_state.telegraph_cue = Engine::Core::TelegraphCue::Impact;
    combat_state.telegraph_intensity =
        weight *
        std::lerp(1.0F,
                  0.6F,
                  ramp((t - strike_end) / std::max(0.001F, recovery - strike_end)));
  } else {
    combat_state.telegraph_cue = Engine::Core::TelegraphCue::Settling;
    combat_state.telegraph_intensity =
        weight * 0.6F *
        (1.0F - ramp((t - recovery) / std::max(0.001F, 1.0F - recovery)));
  }
  return true;
}

[[nodiscard]] auto hit_pause_holds_timeline(
    const Engine::Core::Entity& unit,
    const Engine::Core::CombatStateComponent& combat_state) -> bool {
  if (!combat_state.is_hit_paused) {
    return false;
  }

  auto const* commander = unit.get_component<Engine::Core::CommanderComponent>();
  return commander == nullptr || !commander->fpv_controlled;
}

void reset_action_events_if_present(Engine::Core::Entity& unit) {
  auto* action = unit.get_component<Engine::Core::RpgCommanderActionComponent>();
  if (action == nullptr) {
    return;
  }
  Game::Systems::CombatActions::reset_combat_action_event_runtime(*action);
}

} // namespace

void process_combat_state(Engine::Core::World* world, float delta_time) {
  Engine::Core::Timing::ScopedAccumulator const scope(
      Engine::Core::Timing::combat_state_update());
  process_spear_brace_state(world, delta_time);
  process_mounted_charge_intents(world, delta_time);

  for (auto [unit, combat_state] :
       world->entity_view<Engine::Core::CombatStateComponent>()) {
    if (unit.has_component<Engine::Core::PendingRemovalComponent>() ||
        !combat_state.is_hit_paused) {
      continue;
    }
    combat_state.hit_pause_remaining -= delta_time;
    if (combat_state.hit_pause_remaining <= 0.0F) {
      combat_state.is_hit_paused = false;
      combat_state.hit_pause_remaining = 0.0F;
    }
  }

  for (auto [unit, action] :
       world->entity_view<Engine::Core::RpgCommanderActionComponent>()) {
    (void)action;
    if (unit.has_component<Engine::Core::PendingRemovalComponent>()) {
      continue;
    }
    auto* presentation_state = unit.get_component<Engine::Core::CombatStateComponent>();
    if (presentation_state != nullptr &&
        hit_pause_holds_timeline(unit, *presentation_state)) {
      continue;
    }
    process_authored_combat_action(world, unit, presentation_state, delta_time);
  }

  for (auto [unit, combat_state] :
       world->entity_view<Engine::Core::CombatStateComponent>()) {
    if (unit.has_component<Engine::Core::PendingRemovalComponent>() ||
        hit_pause_holds_timeline(unit, combat_state)) {
      continue;
    }

    if (bow_draw_is_held(unit)) {

      continue;
    }

    if (auto const* definition = running_authored_action(unit); definition != nullptr) {
      auto const* action =
          unit.get_component<Engine::Core::RpgCommanderActionComponent>();
      if (action != nullptr && action->action_running &&
          apply_action_timeline_phase(combat_state, *action, *definition)) {
        continue;
      }
      if (action != nullptr && action->action_completed &&
          apply_action_timeline_phase(combat_state, *action, *definition)) {

        combat_state.animation_state = CS::Idle;
        combat_state.state_time = 0.0F;
        combat_state.state_duration = 0.0F;
        combat_state.input_buffered = false;
        combat_state.telegraph_cue = Engine::Core::TelegraphCue::None;
        combat_state.telegraph_intensity = 0.0F;
        continue;
      }
    }

    combat_state.state_time += delta_time;

    int transitions = 0;
    while (combat_state.state_duration > 0.0F &&
           combat_state.state_time >= combat_state.state_duration && transitions < 8) {
      ++transitions;
      float const carry = combat_state.state_time - combat_state.state_duration;
      switch (combat_state.animation_state) {
      case CS::Advance:
        combat_state.animation_state = CS::WindUp;
        combat_state.state_duration =
            phase_duration_for_state(unit, combat_state, combat_state.animation_state);
        break;
      case CS::WindUp:
        combat_state.animation_state = CS::Strike;
        combat_state.state_duration =
            phase_duration_for_state(unit, combat_state, combat_state.animation_state);
        break;
      case CS::Strike:
        combat_state.animation_state = CS::Impact;
        combat_state.state_duration =
            phase_duration_for_state(unit, combat_state, combat_state.animation_state);

        break;
      case CS::Impact:
        combat_state.animation_state = CS::Recover;
        combat_state.state_duration =
            phase_duration_for_state(unit, combat_state, combat_state.animation_state);
        break;
      case CS::Recover:

        if (combat_state.input_buffered) {
          combat_state.animation_state = CS::Advance;
          combat_state.state_duration = phase_duration_for_state(
              unit, combat_state, combat_state.animation_state);
          combat_state.input_buffered = false;
          combat_state.damage_dealt_this_swing = false;
          reset_action_events_if_present(unit);
        } else {
          combat_state.animation_state = CS::Reposition;
          combat_state.state_duration = phase_duration_for_state(
              unit, combat_state, combat_state.animation_state);
        }
        break;
      case CS::Reposition:
      case CS::Idle:
      default:
        combat_state.animation_state = CS::Idle;
        combat_state.state_duration = 0.0F;
        combat_state.input_buffered = false;
        break;
      }
      combat_state.state_time = carry;
    }
  }
}

} // namespace Game::Systems::Combat
