#include "rpg_bow_draw.h"

#include <algorithm>
#include <cmath>

#include "../combat_actions/combat_action_definition.h"

namespace Game::Systems::RpgCombat {

namespace {

using Aim = Engine::Core::RpgCommanderAimComponent;

constexpr float k_early_release_rush = 3.5F;

constexpr float k_gate_margin_seconds = 0.001F;

[[nodiscard]] auto release_gate_seconds(
    const Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition) -> float {
  float const duration = action.action_duration > 0.0F ? action.action_duration
                                                       : definition.duration_seconds;
  float const normalized = Game::Systems::CombatActions::action_event_normalized_time(
      definition,
      Game::Systems::CombatActions::CombatActionEventType::ProjectileRelease,
      0.46F);
  return std::max(0.01F, duration * normalized);
}

[[nodiscard]] auto fatigue_fraction(float full_draw_hold) -> float {
  if (full_draw_hold <= Aim::k_steady_hold_seconds) {
    return 0.0F;
  }
  float const span =
      std::max(0.01F, Aim::k_max_hold_seconds - Aim::k_steady_hold_seconds);
  return std::clamp((full_draw_hold - Aim::k_steady_hold_seconds) / span, 0.0F, 1.0F);
}

[[nodiscard]] auto shot_power_for(const Aim& aim) -> float {
  float const drawn =
      Aim::k_min_shot_power +
      ((1.0F - Aim::k_min_shot_power) * std::clamp(aim.draw_progress, 0.0F, 1.0F));
  float const tired = 1.0F - ((1.0F - Aim::k_exhausted_shot_power) *
                              fatigue_fraction(aim.full_draw_hold));
  return std::clamp(drawn * tired, 0.0F, 1.0F);
}

} // namespace

auto aim_spread_degrees(const Aim& aim, float stamina_ratio) -> float {
  constexpr float k_planted_spread = 0.28F;
  constexpr float k_undrawn_penalty = 3.6F;
  constexpr float k_walk_penalty = 1.3F;
  constexpr float k_run_penalty = 2.0F;
  constexpr float k_fatigue_penalty = 3.2F;
  constexpr float k_winded_penalty = 1.2F;

  float spread = k_planted_spread;
  spread += (1.0F - std::clamp(aim.draw_progress, 0.0F, 1.0F)) * k_undrawn_penalty;
  spread += std::clamp(aim.move_speed / 3.0F, 0.0F, 1.0F) * k_walk_penalty;
  if (aim.running && aim.move_speed > 0.05F) {
    spread += k_run_penalty;
  }
  spread += fatigue_fraction(aim.full_draw_hold) * k_fatigue_penalty;
  spread += std::clamp((0.5F - stamina_ratio) * 2.0F, 0.0F, 1.0F) * k_winded_penalty;
  return spread;
}

auto update_bow_draw(
    Aim& aim,
    const Engine::Core::RpgCommanderActionComponent& action,
    const Game::Systems::CombatActions::CombatActionDefinition& definition,
    float stamina_ratio,
    float delta_time) -> BowDrawTick {
  BowDrawTick tick;
  delta_time = std::max(0.0F, delta_time);

  float const gate = release_gate_seconds(action, definition);
  float const hold_limit = std::max(0.0F, gate - k_gate_margin_seconds);
  float const elapsed = std::clamp(action.action_elapsed_time, 0.0F, gate);

  if (aim.relaxed_from_overhold && aim.draw_held) {
    aim.draw_stage = Engine::Core::BowDrawStage::None;
    aim.draw_progress = 0.0F;
    aim.spread_degrees = aim_spread_degrees(aim, stamina_ratio);
    return tick;
  }

  if (elapsed >= gate) {
    aim.draw_stage = Engine::Core::BowDrawStage::None;
    aim.draw_progress = 0.0F;
    aim.full_draw_hold = 0.0F;
    aim.spread_degrees = aim_spread_degrees(aim, stamina_ratio);
    tick.allowed_delta = delta_time;
    return tick;
  }

  bool const was_loosing = aim.draw_stage == Engine::Core::BowDrawStage::Loosing;
  bool const at_gate = elapsed >= hold_limit;
  bool const was_drawing = aim.draw_stage != Engine::Core::BowDrawStage::None;

  if (aim.draw_held && !was_loosing) {
    if (at_gate) {
      float const hold_before = aim.full_draw_hold;
      aim.draw_stage = Engine::Core::BowDrawStage::FullDraw;
      aim.draw_progress = 1.0F;
      aim.full_draw_hold += delta_time;
      tick.allowed_delta = 0.0F;
      tick.at_full_draw = true;
      tick.started_draw = !was_drawing;
      tick.reached_full_draw = aim.full_draw_hold <= delta_time;
      tick.started_straining = hold_before < Aim::k_steady_hold_seconds &&
                               aim.full_draw_hold >= Aim::k_steady_hold_seconds;
      if (aim.full_draw_hold >= Aim::k_max_hold_seconds) {

        aim.draw_stage = Engine::Core::BowDrawStage::None;
        aim.draw_progress = 0.0F;
        aim.full_draw_hold = 0.0F;
        aim.relaxed_from_overhold = true;
        tick.relaxed = true;
        tick.at_full_draw = false;
      }
    } else {
      aim.draw_stage = Engine::Core::BowDrawStage::Drawing;
      tick.started_draw = !was_drawing;
      tick.allowed_delta = std::min(delta_time, hold_limit - elapsed);
      aim.draw_progress = std::clamp((elapsed + tick.allowed_delta) / gate, 0.0F, 1.0F);
    }
    aim.spread_degrees = aim_spread_degrees(aim, stamina_ratio);
    return tick;
  }

  if (!was_loosing) {
    aim.draw_progress = at_gate ? 1.0F : std::clamp(elapsed / gate, 0.0F, 1.0F);
    aim.shot_power = shot_power_for(aim);
    aim.draw_stage = Engine::Core::BowDrawStage::Loosing;
  }

  tick.allowed_delta = at_gate ? delta_time : delta_time * k_early_release_rush;
  tick.shot_power = aim.shot_power;
  tick.loosed = elapsed + tick.allowed_delta >= gate;
  aim.spread_degrees = aim_spread_degrees(aim, stamina_ratio);
  return tick;
}

} // namespace Game::Systems::RpgCombat
